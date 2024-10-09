/** @file
    Bounce buffer routines for NVMe device

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "NvmExpress.h"
#include "NvmExpressBounce.h"

EFI_HV_IVM_PROTOCOL *mHvIvm;
UINTN mSharedGpaBoundary;
UINT64 mCanonicalizationMask;

/**
  Return true if NVMe should use bounce buffering.

  @param None.

  @returns TRUE if bounce buffering should be used.
**/
EFI_STATUS
NvmExpressInitializeBounce(
    VOID
    )
{
  mSharedGpaBoundary = (UINTN)PcdGet64(PcdIsolationSharedGpaBoundary);
  mCanonicalizationMask = PcdGet64(PcdIsolationSharedGpaCanonicalizationBitmask);
  return gBS->LocateProtocol(&gEfiHvIvmProtocolGuid, NULL, (VOID **)&mHvIvm);
}

/**
  Return true if NVMe should use bounce buffering.

  @param None.

  @returns TRUE if bounce buffering should be used.
**/
BOOLEAN
NvmExpressIsBounceActive(
    VOID
    )
{
  return IsIsolated();
}


/**
  Allocate a large block of memory from EFI for I/O. Mark the memory as host-
  visible. Allocate tracking structures to sub-allocate the block into
  individual pages.

  @param[in]  Private             Pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.
  @param[in]  BlockByteCount      Number of bytes to allocate for I/O. Must be a multiple of EFI_PAGE_SIZE.

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES    Memory allocation failures or other failures from hypervisor
                                  page visibility call.
**/
EFI_STATUS
NvmExpressAllocateBounceBlock(
    IN NVME_CONTROLLER_PRIVATE_DATA  *Private,
    IN UINT32 BlockByteCount
    )
{
  EFI_STATUS status = EFI_INVALID_PARAMETER;
  UINT32 pageCount = 0;
  UINT32 i = 0;
  PNVME_BOUNCE_BLOCK bounceBlock = NULL;
  UINT8* nextVa;
  UINT64 nextPa;

  DEBUG((EFI_D_VERBOSE,
    "%a(%d) Context=%p ByteCount=0x%x\n",
    __FUNCTION__,
    __LINE__,
    Private,
    BlockByteCount));

  if (BlockByteCount % EFI_PAGE_SIZE) {
    status = EFI_INVALID_PARAMETER;
    goto Cleanup;
  }

  pageCount = BlockByteCount / EFI_PAGE_SIZE;

  bounceBlock = AllocatePool(sizeof(*bounceBlock));
  if (bounceBlock == NULL) {
    status = EFI_OUT_OF_RESOURCES;
    goto Cleanup;
  }

  ZeroMem(bounceBlock, sizeof(*bounceBlock));

  // Allocate the bounce page memory

  bounceBlock->BlockBase = AllocatePages(pageCount);
  if (bounceBlock->BlockBase == NULL) {
    status = EFI_OUT_OF_RESOURCES;
    goto Cleanup;
  }

  bounceBlock->BlockPageCount = pageCount;
  ZeroMem(bounceBlock->BlockBase, pageCount * EFI_PAGE_SIZE);

  // Allocate the tracking structures as one
  bounceBlock->BouncePageStructureBase = AllocatePool(pageCount * sizeof(NVME_BOUNCE_PAGE));
  if (bounceBlock->BouncePageStructureBase == NULL) {
    status = EFI_OUT_OF_RESOURCES;
    goto Cleanup;
  }

  bounceBlock->FreePageListHead = bounceBlock->BouncePageStructureBase;
  nextVa = bounceBlock->BlockBase;
  nextPa = (UINT64)nextVa;

  //
  // Make these pages visible to the host
  //

  if (IsIsolated()) {
    status = mHvIvm->MakeAddressRangeHostVisible(mHvIvm,
                                                 HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE,
                                                 bounceBlock->BlockBase,
                                                 pageCount * EFI_PAGE_SIZE,
                                                 FALSE,
                                                 &bounceBlock->ProtectionHandle);

    if (EFI_ERROR(status)) {
      goto Cleanup;
    }

    //
    // Adjust the address above the shared GPA boundary if required.
    //
    nextPa += mSharedGpaBoundary;

    //
    // Canonicalize the VA.
    //
    nextVa = (VOID *)(mCanonicalizationMask | nextPa);
    bounceBlock->IsHostVisible = TRUE;
  }

  for (i = 0; i < pageCount; i++) {
    if (i == (pageCount - 1)) {
      bounceBlock->BouncePageStructureBase[i].NextBouncePage = NULL;
    } else {
      bounceBlock->BouncePageStructureBase[i].NextBouncePage =
        &bounceBlock->BouncePageStructureBase[i + 1];
    }

    bounceBlock->BouncePageStructureBase[i].BounceBlock = bounceBlock;
    bounceBlock->BouncePageStructureBase[i].PageVA = nextVa;
    bounceBlock->BouncePageStructureBase[i].HostVisiblePA = nextPa;
    nextVa += EFI_PAGE_SIZE;
    nextPa += EFI_PAGE_SIZE;
  }

  InsertTailList(&Private->BounceBlockListHead, &bounceBlock->BlockListEntry);
  status = EFI_SUCCESS;

  Cleanup:
  DEBUG((EFI_D_INFO,
    "%a (%d) Context=%p bounceBlock=%p status=0x%x\n",
    __FUNCTION__,
    __LINE__,
    Private,
    bounceBlock,
    status));

  if (EFI_ERROR(status)) {
    if (bounceBlock) {
      NvmExpressFreeBounceBlock(bounceBlock);
      bounceBlock = NULL;
    }
  }
  return status;
}

/**
  Free the block of memory allocated for I/O. Marks the memory as not host-visible.

  @param[in]  Block               Bounce block that needs to be freed.

**/
VOID
NvmExpressFreeBounceBlock(
    IN PNVME_BOUNCE_BLOCK Block
    )
{
  if (Block->IsHostVisible)  {
    mHvIvm->MakeAddressRangeNotHostVisible(mHvIvm, Block->ProtectionHandle);
  }

  if (Block->BouncePageStructureBase) {
    FreePool(Block->BouncePageStructureBase);
    Block->BouncePageStructureBase = NULL;
  }

  if (Block->BlockBase) {
    FreePages(Block->BlockBase, Block->BlockPageCount);
    Block->BlockBase = NULL;
    Block->BlockPageCount = 0;
  }

  FreePool(Block);
}

/**
  Free all of the large blocks of memory allocated for I/O. Marks the memory
  as not host-visible. Frees the associated tracking structures.

  @param[in]  Private             Pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.

**/
VOID
NvmExpressFreeAllBounceBlocks(
    IN NVME_CONTROLLER_PRIVATE_DATA  *Private
    )

{
  PNVME_BOUNCE_BLOCK block;
  LIST_ENTRY* entry;

  while (!IsListEmpty(&Private->BounceBlockListHead)) {

    entry = GetFirstNode(&Private->BounceBlockListHead);
    RemoveEntryList(entry);

    block = BASE_CR(entry, NVME_BOUNCE_BLOCK, BlockListEntry);

    DEBUG((EFI_D_WARN,
      "%a (%d) Context=%p block=%p IsHostVis=%d InUsePageCount=%d BlockBase=%p PageCount=0x%x\n",
      __FUNCTION__,
      __LINE__,
      Private,
      block,
      block->IsHostVisible,
      block->InUsePageCount,
      block->BlockBase,
      block->BlockPageCount));

    NvmExpressFreeBounceBlock(block);
    block = NULL;
  }
}

/**
  Remove 'PageCount' pre-allocated NVME_BOUNCE_PAGE structures from the
  NVME_CONTROLLER_PRIVATE_DATA context and return them in a linked-list.
  These PAGE structures will be used in an I/O.

  @param[in]  Private             Pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.
  @param[in]  PageCount           Number of NVME_BOUNCE_PAGE structures to acquire from the
                                  NVME_CONTROLLER_PRIVATE_DATA context structure and return to
                                  the caller.

  @retval Linked list of NVME_BOUNCE_PAGE structures or NULL on failure.
**/
PNVME_BOUNCE_PAGE
NvmExpressAcquireBouncePages(
    IN NVME_CONTROLLER_PRIVATE_DATA  *Private,
    IN UINT32 PageCount
    )
{
  PNVME_BOUNCE_PAGE listHead = NULL;
  UINT32 pagesToGo = PageCount;
  EFI_STATUS status;

  DEBUG((EFI_D_VERBOSE,
    "%a(%d) Context=%p PageCount=%d\n",
    __FUNCTION__,
    __LINE__,
    Private,
    PageCount));

Retry:
  if (!IsListEmpty(&Private->BounceBlockListHead)) {
    LIST_ENTRY* blockListEntry;

    for (blockListEntry = Private->BounceBlockListHead.ForwardLink;
         blockListEntry != &Private->BounceBlockListHead;
         blockListEntry = blockListEntry->ForwardLink) {

      PNVME_BOUNCE_BLOCK bounceBlock;
      bounceBlock = BASE_CR(blockListEntry, NVME_BOUNCE_BLOCK, BlockListEntry);

      while (bounceBlock->FreePageListHead && pagesToGo) {

        PNVME_BOUNCE_PAGE bouncePage;

        bouncePage = bounceBlock->FreePageListHead;
        bounceBlock->FreePageListHead = bouncePage->NextBouncePage;

        bouncePage->NextBouncePage = listHead;
        listHead = bouncePage;

        bounceBlock->InUsePageCount++;

        pagesToGo--;
      }

      if (pagesToGo == 0) {
        break;
      }
    }

    if (pagesToGo) {
      UINT32 allocSize = MAX(pagesToGo * EFI_PAGE_SIZE, 32 * EFI_PAGE_SIZE);

      status = NvmExpressAllocateBounceBlock(Private, allocSize);
      if (EFI_ERROR(status)) {
      DEBUG((EFI_D_WARN,
        "%a(%d) Context=%p Bounce block allocation failure\n",
        __FUNCTION__,
        __LINE__,
        Private));
      goto Exit;
      }
      goto Retry;
    }
  }

Exit:
  if (pagesToGo) {
    // failed
    NvmExpressReleaseBouncePages(Private, listHead);

    DEBUG((EFI_D_WARN,
      "%a(%d) Context=%p PageCount=%d Returning=NULL\n",
      __FUNCTION__,
      __LINE__,
      Private,
      PageCount));
  } else {
    DEBUG((EFI_D_VERBOSE,
      "%a(%d) Context=%p PageCount=%d Returning=%p\n",
      __FUNCTION__,
      __LINE__,
      Private,
      PageCount,
      listHead));
  }
  return listHead;
}

/**
  Return NVME_BOUNCE_PAGES from a linked list to their 'home' NVME_BOUNCE_BLOCK lists.
  Effectively frees these temporary pages for use by another I/O.

  @param[in]  Private             Pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.
  @param[in]  BounceListHead      Linked list of one more NVME_BOUNCE_PAGE structures.

**/
VOID
NvmExpressReleaseBouncePages(
    IN NVME_CONTROLLER_PRIVATE_DATA  *Private,
    IN OUT PNVME_BOUNCE_PAGE BounceListHead
    )
{
  PNVME_BOUNCE_PAGE page;
  UINT32 count = 0;

  while (BounceListHead) {
    page = BounceListHead;
    BounceListHead = BounceListHead->NextBouncePage;

    page->BounceBlock->InUsePageCount--;
    count++;

    page->NextBouncePage = page->BounceBlock->FreePageListHead;
    page->BounceBlock->FreePageListHead = page;
  }

  DEBUG((EFI_D_VERBOSE,
    "%a(%d) Context=%p released PageCount=%d\n",
    __FUNCTION__,
    __LINE__,
    Private,
    count));

  BounceListHead = NULL;
}

/**
  Copy between the memory pages in the bounce buffers and the client's
  buffer respecting the page offsets of the client's buffer. This function
  will zero the partial pages at the beginning and end of the BouncePageList.

  @param[in]  ExternalBuffer      The EFI client's data buffer. Can start at any
  @param[in]  BufferSize          Size of EFI client's data buffer.
  @param[in]  BouncePageList      List of bounce pages (shared with host)
  @param[in]  CopyToBounce        If TRUE, copy from ExternalBuffer into the BouncePageList.
                                  If FALSE, copy from the BouncePageList into ExternalBuffer.

**/
VOID
NvmExpressCopyBouncePagesToExternalBuffer(
    IN VOID * ExternalBuffer,
    IN UINT32 BufferSize,
    IN PNVME_BOUNCE_PAGE BouncePageList,
    IN BOOLEAN CopyToBounce
    )
{
  UINT64 pageOffset;
  PNVME_BOUNCE_PAGE bouncePage;
  UINT8* bounceBuffer;
  UINT8* bounceBufferEnd;
  UINT8* extBuffer;
  UINT32 transferToGo;
  UINT32 copySize;

  DEBUG((EFI_D_INFO,
    "%a(%d) ExternalBuffer.Buffer=%p Size=0x%x BouncePageList=%p CopyToBounce=%d\n",
    __FUNCTION__,
    __LINE__,
    ExternalBuffer,
    BufferSize,
    BouncePageList,
    CopyToBounce));

  ASSERT (BouncePageList);

  bouncePage = BouncePageList;
  pageOffset = (UINT64)ExternalBuffer % EFI_PAGE_SIZE;

  extBuffer = ExternalBuffer;
  transferToGo = BufferSize;

  while (transferToGo) {
    ASSERT(bouncePage);

    bounceBuffer = (UINT8*)bouncePage->PageVA;

    // Zero any unused space in buffer we are sharing with the host.
    if (CopyToBounce && pageOffset) {
      DEBUG((EFI_D_VERBOSE,
        "%a(%d) Zero %p size=0x%x\n",
        __FUNCTION__,
        __LINE__,
        bouncePage->PageVA,
        pageOffset));
      ZeroMem(bouncePage->PageVA, pageOffset);
    }

    // First page offset
    bounceBuffer += pageOffset;
    copySize = EFI_PAGE_SIZE - (UINT32)pageOffset;
    pageOffset = 0; // no more offsets

    copySize = MIN(copySize, transferToGo);
    bounceBufferEnd = bounceBuffer + copySize;

    if (CopyToBounce) {
      DEBUG((EFI_D_VERBOSE,
        "%a(%d) CopyToBounce dst=%p src=%p size=0x%x\n",
        __FUNCTION__,
        __LINE__,
        bounceBuffer,
        extBuffer,
        copySize));

      CopyMem(bounceBuffer, extBuffer, copySize);
    } else {
      DEBUG((EFI_D_VERBOSE,
        "%a(%d) CopyToExtBuffer dst=%p src=%p size=0x%x\n",
        __FUNCTION__,
        __LINE__,
        extBuffer,
        bounceBuffer,
        copySize));

      CopyMem(extBuffer, bounceBuffer, copySize);
    }

    transferToGo -= copySize;
    extBuffer += copySize;

    // Zero any unused space in buffer we are sharing with the host.
    if (transferToGo == 0 &&
        CopyToBounce &&
        ((UINT64)bounceBuffer % EFI_PAGE_SIZE)) {
      UINT32 endOffset = (UINT64)bounceBufferEnd % EFI_PAGE_SIZE;
      UINT32 zeroSize = EFI_PAGE_SIZE - endOffset;

      DEBUG((EFI_D_VERBOSE,
        "%a(%d) Zero %p size=0x%x (from offset=0x%x)\n",
        __FUNCTION__,
        __LINE__,
        bounceBufferEnd,
        zeroSize,
        endOffset));

        ZeroMem(bounceBufferEnd, zeroSize);
    }
    bouncePage = bouncePage->NextBouncePage;
  }

  ASSERT(bouncePage == NULL); // should be all done
}

/**
  Zero initialize host-visible bounce page list.

  @param[in]  BouncePageList      List of bounce pages (shared with host).

**/
VOID
NvmExpressZeroBouncePageList(
    IN PNVME_BOUNCE_PAGE BouncePageList
    )
{
    PNVME_BOUNCE_PAGE bouncePage = BouncePageList;
    UINT32 pageCount = 0;

    while (bouncePage) {
      ZeroMem(bouncePage->PageVA, EFI_PAGE_SIZE);
      bouncePage = bouncePage->NextBouncePage;
      pageCount++;
    }
    DEBUG((EFI_D_VERBOSE, "%a(%d) BouncePageList=%p zeroed %d pages\n",
      __FUNCTION__,
      __LINE__,
      BouncePageList,
      pageCount));
}


//
// Page visibility
//

UINTN
NvmExpressGetSharedPa(
    VOID* Address
    )
/**
    Given an address, which may be either a VA or a PA, removes any
    canonicalization bits and returns the shared GPA corresponding to the
    address.

    @param Address Input address.

    @returns Shared GPA.

**/
{
    UINTN addr;

    addr = (UINTN)Address;
    addr &= ~mCanonicalizationMask;
    if (addr < mSharedGpaBoundary)
    {
        addr += mSharedGpaBoundary;
    }

    return addr;
}

VOID*
NvmExpressGetSharedVa(
    VOID* Address
    )
/**
     Given an address, which may be either a VA or a PA, returns a canonicalized
    pointer pointing to the shared GPA alias.

    @param Address Input address.

    @returns Shared VA pointer.

**/
{
    return (VOID*)(NvmExpressGetSharedPa(Address) | mCanonicalizationMask);
}

EFI_STATUS
NvmExpressMakeAddressRangeShared(
    IN OUT  NVME_HOST_VISIBILITY_CONTEXT    *HostVisibilityContext,
    IN OUT  VOID                            *BaseAddress,
    IN      UINT32                          ByteCount
    )
{
  ASSERT(IsIsolated());

  EFI_STATUS status = mHvIvm->MakeAddressRangeHostVisible(mHvIvm,
                                                 HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE,
                                                 BaseAddress,
                                                 ByteCount,
                                                 FALSE,
                                                 &HostVisibilityContext->RangeProtectionHandle);

  if (EFI_ERROR(status))
  {
    return status;
  }

  //
  // Adjust the address above the shared GPA boundary.
  //
  BaseAddress = (UINT8 *)((UINTN)BaseAddress + mSharedGpaBoundary);

  return EFI_SUCCESS;
}


VOID
NvmExpressMakeAddressRangePrivate(
    IN      NVME_HOST_VISIBILITY_CONTEXT  *HostVisibilityContext,
    IN OUT  VOID                          *AddressRange
    )
{
  ASSERT(IsIsolated());

  mHvIvm->MakeAddressRangeNotHostVisible(mHvIvm, HostVisibilityContext->RangeProtectionHandle);

  AddressRange = (VOID *)((UINT64)AddressRange & ~mCanonicalizationMask);
  if ((UINTN)AddressRange >= mSharedGpaBoundary) {
    AddressRange = (VOID *)((UINTN)AddressRange - mSharedGpaBoundary);
  }
}
