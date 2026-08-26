/** @file
    Bounce buffer implementation for the Hyper-V IOMMU driver.

    Provides host-visible bounce buffer management for DMA operations in
    isolated Hyper-V virtual machines. Adapted from the NvmExpressBounce
    implementation to be generic and usable by any DMA-capable driver.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "IoMmuBounce.h"

#include <IsolationTypes.h>
#include <Library/PcdLib.h>
#include <UefiConstants.h>
//
// Module globals for host visibility and shared GPA translation.
//
EFI_HV_IVM_PROTOCOL   *mHvIvm;
EFI_PHYSICAL_ADDRESS  mSharedGpaBoundary;
UINT64                mCanonicalizationMask;

//
// Allocation tracking.
//
LIST_ENTRY  mAllocContextListHead;

//
// Pre-allocated bounce block pool. Each block is a contiguous host-visible
// region; Map() sub-allocates contiguous page runs from a block via the
// per-block AllocBitmap.
//
LIST_ENTRY  mBounceBlockListHead;

//
// Bounce dispatch state, resolved once at driver entry. A set of
// IOMMU_BOUNCE_MODE_* flags describing which per-region obligations
// (host visibility, pinning) wrap the bounced DMA. See
// IoMmuComputeBounceMode for the resolution policy and IOMMU_BOUNCE_MODE
// in IoMmuBounce.h for why this is a bitmask rather than an enum.
//
IOMMU_BOUNCE_MODE  mBounceMode = IOMMU_BOUNCE_MODE_NONE;

/**
  Resolve the bounce dispatch flags. Cached in mBounceMode at init and
  consulted by all subsequent dispatch decisions. See IOMMU_BOUNCE_MODE
  in IoMmuBounce.h for the two-tier composition model.

  Hard-requirement predicates (compose):
    - IsIsolated() -> BOUNCE | HOST_VISIBILITY
    - PcdDmaPinningRequired -> BOUNCE | PINNING

  Soft override from PcdForceDmaBounceEnabled: when TRUE, force BOUNCE on
  regardless of the hard-requirement predicates above.

  @retval IOMMU_BOUNCE_MODE   Bitmask of resolved dispatch flags.
**/
IOMMU_BOUNCE_MODE
IoMmuComputeBounceMode (
  VOID
  )
{
  IOMMU_BOUNCE_MODE  Mode;

  Mode = IOMMU_BOUNCE_MODE_NONE;

  //
  // Hard requirement: isolated VM.
  //
  if (IsIsolated ()) {
    DEBUG ((DEBUG_INFO, "%a: VM is isolated\n", __FUNCTION__));
    Mode |= IOMMU_BOUNCE_MODE_BOUNCE | IOMMU_BOUNCE_MODE_HOST_VISIBILITY;
  }

  //
  // Hard requirement: hypervisor requires DMA buffers to be pinned.
  // PcdDmaPinningRequired is set at PEI by the hypervisor capability
  // probe (e.g. VA-backed VMs advertising HvCallPin/UnpinGpaPageRanges).
  //
  if (PcdGetBool (PcdDmaPinningRequired)) {
    DEBUG ((DEBUG_INFO, "%a: Hypervisor requires DMA pinning\n", __FUNCTION__));
    Mode |= IOMMU_BOUNCE_MODE_BOUNCE | IOMMU_BOUNCE_MODE_PINNING;
  }

  //
  // Soft override: VMM-configured intent.
  //
  if (PcdGetBool (PcdForceDmaBounceEnabled)) {
    DEBUG ((DEBUG_INFO, "%a: Forcing DMA bounce\n", __FUNCTION__));
    Mode |= IOMMU_BOUNCE_MODE_BOUNCE;
  }

  return Mode;
}

/**
  Initialize the bounce buffer subsystem. Allocates pool tracking state
  and locates the hypervisor IVM protocol when isolation visibility is
  required. The caller must have already assigned mBounceMode (via
  IoMmuComputeBounceMode) before invoking this.

  @retval EFI_SUCCESS           Initialization successful.
  @retval other                 Failed to locate the HV IVM protocol.
**/
EFI_STATUS
IoMmuInitializeBounce (
  VOID
  )
{
  InitializeListHead (&mAllocContextListHead);
  InitializeListHead (&mBounceBlockListHead);

  mSharedGpaBoundary    = (EFI_PHYSICAL_ADDRESS)PcdGet64 (PcdIsolationSharedGpaBoundary);
  mCanonicalizationMask = PcdGet64 (PcdIsolationSharedGpaCanonicalizationBitmask);

  if (!IoMmuRequiresHostVisibility () && !IoMmuRequiresPinning ()) {
    //
    // The HV IVM protocol is only used for per-range host visibility
    // and pinning. Skip the locate when neither obligation is set so
    // the driver can come up in pure-bounce and pass-through modes
    // even when no provider exposes the protocol.
    //
    mHvIvm = NULL;
    return EFI_SUCCESS;
  }

  return gBS->LocateProtocol (&gEfiHvIvmProtocolGuid, NULL, (VOID **)&mHvIvm);
}

/**
  Return TRUE if bounce buffering should be used for DMA operations.
  Callers should not assume the VM is isolated when this returns TRUE
  (bouncing also runs in non-isolated VA-backed scenarios).

  @retval TRUE    Bounce buffering is active.
  @retval FALSE   DMA can access guest memory directly; no bouncing needed.
**/
BOOLEAN
IoMmuIsBounceActive (
  VOID
  )
{
  return (mBounceMode & IOMMU_BOUNCE_MODE_BOUNCE) != 0;
}

/**
  Return TRUE if bounce regions must be made host-visible via the HV
  IVM protocol. Mirrors IoMmuIsBounceActive for the HOST_VISIBILITY
  obligation bit so dispatch sites read as a policy question rather
  than a raw bitmask test.

  @retval TRUE    Bounce regions require host-visibility hypercalls.
  @retval FALSE   No host-visibility obligation; the HV IVM protocol
                  is not consulted.
**/
BOOLEAN
IoMmuRequiresHostVisibility (
  VOID
  )
{
  return (mBounceMode & IOMMU_BOUNCE_MODE_HOST_VISIBILITY) != 0;
}

/**
  Return TRUE if bounce regions must be pinned via the HV IVM protocol.

  @retval TRUE    Bounce regions require pinning hypercalls.
  @retval FALSE   No pinning obligation; the HV IVM protocol is not
                  consulted for pinning.
**/
BOOLEAN
IoMmuRequiresPinning (
  VOID
  )
{
  return (mBounceMode & IOMMU_BOUNCE_MODE_PINNING) != 0;
}

/**
  Given an address (VA or PA), strip canonicalization and return the
  shared GPA above the shared GPA boundary.

  @param[in]  Address   Input address.

  @retval     The shared physical address.
**/
EFI_PHYSICAL_ADDRESS
IoMmuGetSharedPa (
  IN VOID  *Address
  )
{
  EFI_PHYSICAL_ADDRESS  Addr;

  Addr  = (EFI_PHYSICAL_ADDRESS)(UINTN)Address;
  Addr &= ~mCanonicalizationMask;
  if (Addr < mSharedGpaBoundary) {
    Addr += mSharedGpaBoundary;
  }

  return Addr;
}

/**
  Given an address (VA or PA), return a canonicalized pointer to the
  shared GPA alias.

  @param[in]  Address   Input address.

  @retval     Canonicalized shared VA pointer.
**/
VOID *
IoMmuGetSharedVa (
  IN VOID  *Address
  )
{
  return (VOID *)(UINTN)(IoMmuGetSharedPa (Address) | mCanonicalizationMask);
}

/**
  Apply the configured hypervisor obligations for DMA over an address range.

  @param[in]  BaseAddress   Base address of the range.
  @param[in]  PageCount     Number of pages in the range.
  @param[out] DmaContext    Context for releasing DMA preparation later.

  @retval EFI_SUCCESS              Range is ready for host DMA.
  @retval EFI_INVALID_PARAMETER    DmaContext is NULL.
  @retval other                    Hypervisor call failed.
**/
EFI_STATUS
IoMmuPrepareAddressRangeForDma (
  IN  VOID                     *BaseAddress,
  IN  UINT32                   PageCount,
  OUT IOMMU_DMA_RANGE_CONTEXT  *DmaContext
  )
{
  EFI_STATUS  Status;

  if (DmaContext == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Always clear the hypervisor state first so the matching
  // IoMmuReleaseAddressRangeFromDma call unwinds only obligations that
  // were applied successfully.
  //
  DmaContext->RangeProtectionHandle = NULL;
  DmaContext->BaseAddress           = BaseAddress;
  DmaContext->PageCount             = PageCount;
  DmaContext->PinApplied            = FALSE;

  if (IoMmuRequiresHostVisibility ()) {
    Status = mHvIvm->MakeAddressRangeHostVisible (
                       mHvIvm,
                       HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE,
                       BaseAddress,
                       PageCount * EFI_PAGE_SIZE,
                       FALSE,
                       &DmaContext->RangeProtectionHandle
                       );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (IoMmuRequiresPinning ()) {
    Status = mHvIvm->PinAddressRange (
                       mHvIvm,
                       BaseAddress,
                       PageCount * EFI_PAGE_SIZE,
                       &DmaContext->PinApplied
                       );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to pin address range: %r\n", __func__, Status));
      if (IoMmuRequiresHostVisibility ()) {
        mHvIvm->MakeAddressRangeNotHostVisible (
                  mHvIvm,
                  &DmaContext->RangeProtectionHandle
                  );
      }

      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
  Release the configured hypervisor obligations for a DMA address range.

  @param[in]  DmaContext   Context from a prior IoMmuPrepareAddressRangeForDma call.
**/
VOID
IoMmuReleaseAddressRangeFromDma (
  IN IOMMU_DMA_RANGE_CONTEXT  *DmaContext
  )
{
  //
  // Symmetric to IoMmuPrepareAddressRangeForDma. Each obligation is
  // unwound only if it was applied in the first place; unpin before
  // revoking host visibility so the host stops touching the pages first.
  //
  if (DmaContext->PinApplied) {
    mHvIvm->UnpinAddressRange (
              mHvIvm,
              DmaContext->BaseAddress,
              DmaContext->PageCount * EFI_PAGE_SIZE
              );
    DmaContext->PinApplied = FALSE;
  }

  if (IoMmuRequiresHostVisibility ()) {
    mHvIvm->MakeAddressRangeNotHostVisible (mHvIvm, &DmaContext->RangeProtectionHandle);
  }
}

//
// ---------------------------------------------------------------------------
// Pre-allocated bounce block pool.
//
// Each IOMMU_BOUNCE_BLOCK is a contiguous, DMA-prepared region of pages
// allocated below 4GB. Map() requests are satisfied by sub-allocating a
// contiguous run of free pages from one of the blocks (via the per-block
// AllocBitmap). If no existing block can satisfy a request, a new block is
// allocated and prepared for DMA (one set of hypervisor calls per new
// block, not per Map). Blocks are kept around for the lifetime of the
// driver to amortize the cost across many DMA operations.
// ---------------------------------------------------------------------------
//

/**
  Find the lowest contiguous run of `RunLength` clear bits in `Bitmap`,
  considering only bits [0, BitmapSize). Returns the starting bit index
  via *StartBit on success.
**/
STATIC
BOOLEAN
FindFreeRun (
  IN  UINT64  *Bitmap,
  IN  UINT32  BitmapSize,
  IN  UINT32  RunLength,
  OUT UINT32  *StartBit
  )
{
  UINT32  i;
  UINT32  Run;

  if ((RunLength == 0) || (RunLength > BitmapSize)) {
    return FALSE;
  }

  Run = 0;
  for (i = 0; i < BitmapSize; i++) {
    if ((Bitmap[i >> 6] & ((UINT64)1 << (i & 63))) == 0) {
      Run++;
      if (Run == RunLength) {
        *StartBit = i + 1 - RunLength;
        return TRUE;
      }
    } else {
      Run = 0;
    }
  }

  return FALSE;
}

/**
  Set or clear a contiguous run of `Count` bits starting at `Start` in
  `Bitmap`. `Set` selects between OR (TRUE) and AND-NOT (FALSE).
**/
STATIC
VOID
UpdateBitmapRun (
  IN OUT UINT64   *Bitmap,
  IN     UINT32   Start,
  IN     UINT32   Count,
  IN     BOOLEAN  Set
  )
{
  UINT32  i;
  UINT32  Bit;
  UINT64  Mask;

  for (i = 0; i < Count; i++) {
    Bit  = Start + i;
    Mask = (UINT64)1 << (Bit & 63);
    if (Set) {
      Bitmap[Bit >> 6] |= Mask;
    } else {
      Bitmap[Bit >> 6] &= ~Mask;
    }
  }
}

/**
  Allocate a new bounce block of `PageCount` pages, prepare it for DMA,
  and insert it at the tail of the bounce block list.

  @param[in]   PageCount   Number of pages in the new block.
  @param[out]  BlockOut    The newly allocated block on success.

  @retval EFI_SUCCESS            Block allocated and registered.
  @retval EFI_OUT_OF_RESOURCES   Allocation or hypercall failed.
**/
EFI_STATUS
IoMmuPreAllocateBounceBlock (
  IN  UINT32               PageCount,
  OUT PIOMMU_BOUNCE_BLOCK  *BlockOut
  )
{
  EFI_STATUS            Status;
  PIOMMU_BOUNCE_BLOCK   Block;
  EFI_PHYSICAL_ADDRESS  PhysicalAddress;
  UINT32                BitmapWordCount;

  ASSERT (PageCount > 0);

  Block = AllocateZeroPool (sizeof (IOMMU_BOUNCE_BLOCK));
  if (Block == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  BitmapWordCount    = (PageCount + 63) / 64;
  Block->AllocBitmap = AllocateZeroPool ((UINTN)BitmapWordCount * sizeof (UINT64));
  if (Block->AllocBitmap == NULL) {
    FreePool (Block);
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Allocate the block below 4GB so it can satisfy both 32-bit and
  // 64-bit DMA Map() requests without further constraints.
  //
  PhysicalAddress = SIZE_4GB - 1;
  Status          = gBS->AllocatePages (
                           AllocateMaxAddress,
                           EfiBootServicesData,
                           PageCount,
                           &PhysicalAddress
                           );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "IoMmu: AllocateBounceBlock: AllocatePages(%d pages, <4GB) failed: %r\n",
      PageCount,
      Status
      ));
    FreePool (Block->AllocBitmap);
    FreePool (Block);
    return Status;
  }

  Block->Signature        = IOMMU_BOUNCE_BLOCK_SIGNATURE;
  Block->BlockBase        = (VOID *)(UINTN)PhysicalAddress;
  Block->BlockPageCount   = PageCount;
  Block->BitmapWordCount  = BitmapWordCount;
  Block->InUsePageCount   = 0;
  Block->IsPreparedForDma = FALSE;

  Status = IoMmuPrepareAddressRangeForDma (
             Block->BlockBase,
             PageCount,
             &Block->DmaContext
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "IoMmu: AllocateBounceBlock: PrepareAddressRangeForDma failed: %r\n",
      Status
      ));
    gBS->FreePages (PhysicalAddress, PageCount);
    FreePool (Block->AllocBitmap);
    FreePool (Block);
    return Status;
  }

  Block->IsPreparedForDma = TRUE;
  InsertTailList (&mBounceBlockListHead, &Block->Link);

  DEBUG ((
    DEBUG_INFO,
    "IoMmu: AllocateBounceBlock: Block=%p Base=%p Pages=%d\n",
    Block,
    Block->BlockBase,
    PageCount
    ));

  *BlockOut = Block;
  return EFI_SUCCESS;
}

EFI_STATUS
IoMmuAcquireBouncePages (
  IN  UINT32               PageCount,
  OUT PIOMMU_BOUNCE_BLOCK  *Block,
  OUT UINT32               *StartPageIndex,
  OUT VOID                 **BounceBase
  )
{
  LIST_ENTRY           *Entry;
  PIOMMU_BOUNCE_BLOCK  Candidate;
  UINT32               StartBit;
  EFI_STATUS           Status;
  UINT32               NewBlockPages;

  if (PageCount == 0) {
    return EFI_INVALID_PARAMETER;
  }

  StartBit = 0;

  //
  // Try to satisfy from an existing pooled block.
  //
  for (Entry = GetFirstNode (&mBounceBlockListHead);
       !IsNull (&mBounceBlockListHead, Entry);
       Entry = GetNextNode (&mBounceBlockListHead, Entry))
  {
    Candidate = BASE_CR (Entry, IOMMU_BOUNCE_BLOCK, Link);

    if (PageCount > Candidate->BlockPageCount) {
      continue;
    }

    if (FindFreeRun (Candidate->AllocBitmap, Candidate->BlockPageCount, PageCount, &StartBit)) {
      UpdateBitmapRun (Candidate->AllocBitmap, StartBit, PageCount, TRUE);
      Candidate->InUsePageCount += PageCount;

      *Block          = Candidate;
      *StartPageIndex = StartBit;
      *BounceBase     = (VOID *)((UINTN)Candidate->BlockBase + ((UINTN)StartBit * EFI_PAGE_SIZE));
      return EFI_SUCCESS;
    }
  }

  //
  // No existing block can fit. Allocate a new pooled block large enough
  // for this request (at least the default block size to leave room for
  // future sub-allocations). The new block requires one
  // MakeAddressRangeHostVisible hypercall; subsequent same-or-smaller
  // requests reuse the block with no hypercall.
  //
  NewBlockPages = PageCount;
  if (NewBlockPages < IOMMU_BOUNCE_GROWTH_BLOCK_PAGES) {
    NewBlockPages = IOMMU_BOUNCE_GROWTH_BLOCK_PAGES;
  }

  Status = IoMmuPreAllocateBounceBlock (NewBlockPages, &Candidate);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG ((
    DEBUG_INFO,
    "IoMmuAcquireBouncePages: Allocated new bounce block %p with %d pages to satisfy request for %d pages\n",
    Candidate,
    Candidate->BlockPageCount,
    PageCount
    ));
  UpdateBitmapRun (Candidate->AllocBitmap, 0, PageCount, TRUE);
  Candidate->InUsePageCount = PageCount;

  *Block          = Candidate;
  *StartPageIndex = 0;
  *BounceBase     = Candidate->BlockBase;
  return EFI_SUCCESS;
}

VOID
IoMmuReleaseBouncePages (
  IN PIOMMU_BOUNCE_BLOCK  Block,
  IN UINT32               StartPageIndex,
  IN UINT32               PageCount
  )
{
  ASSERT (Block != NULL);
  ASSERT (Block->Signature == IOMMU_BOUNCE_BLOCK_SIGNATURE);
  ASSERT (PageCount > 0);
  ASSERT (StartPageIndex + PageCount <= Block->BlockPageCount);

  UpdateBitmapRun (Block->AllocBitmap, StartPageIndex, PageCount, FALSE);
  Block->InUsePageCount -= PageCount;
}
