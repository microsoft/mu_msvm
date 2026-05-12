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

//
// Module globals for host visibility and shared GPA translation.
//
EFI_HV_IVM_PROTOCOL  *mHvIvm;
EFI_PHYSICAL_ADDRESS mSharedGpaBoundary;
UINT64               mCanonicalizationMask;

//
// Allocation tracking.
//
LIST_ENTRY           mAllocContextListHead;

//
// Pre-allocated bounce block pool. Each block is a contiguous host-visible
// region; Map() sub-allocates contiguous page runs from a block via the
// per-block AllocBitmap.
//
LIST_ENTRY           mBounceBlockListHead;


/**
  Initialize the bounce buffer subsystem. Caches PCDs and locates
  the hypervisor IVM protocol.

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

    mSharedGpaBoundary = (EFI_PHYSICAL_ADDRESS)PcdGet64 (PcdIsolationSharedGpaBoundary);
    mCanonicalizationMask = PcdGet64 (PcdIsolationSharedGpaCanonicalizationBitmask);

    if (!IsIsolated ()) {
        //
        // Bounce buffering and host-visibility hypercalls are not used when the
        // VM is not isolated, so don't require the HV IVM protocol.
        //
        mHvIvm = NULL;
        return EFI_SUCCESS;
    }

    return gBS->LocateProtocol (&gEfiHvIvmProtocolGuid, NULL, (VOID **)&mHvIvm);
}


/**
  Return TRUE if bounce buffering should be used for DMA operations.

  @retval TRUE    The VM is isolated and bounce buffering is required.
  @retval FALSE   No isolation; DMA can access all memory directly.
**/
BOOLEAN
IoMmuIsBounceActive (
    VOID
    )
{
    return IsIsolated ();
}


/**
  Given an address (VA or PA), strip canonicalization and return the
  shared GPA above the shared GPA boundary.

  @param[in]  Address   Input address.

  @retval     The shared physical address.
**/
EFI_PHYSICAL_ADDRESS
IoMmuGetSharedPa (
    IN VOID     *Address
    )
{
    EFI_PHYSICAL_ADDRESS Addr;

    Addr = (EFI_PHYSICAL_ADDRESS)(UINTN)Address;
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
    IN VOID     *Address
    )
{
    return (VOID *)(UINTN)(IoMmuGetSharedPa (Address) | mCanonicalizationMask);
}


/**
  Make an address range host-visible for DMA.

  @param[in]  BaseAddress         Base address of the range.
  @param[in]  PageCount           Number of pages in the range.
  @param[out] VisibilityContext   Context for revoking visibility later.

  @retval EFI_SUCCESS             Range is now host-visible.
  @retval other                   Hypervisor call failed.
**/
EFI_STATUS
IoMmuMakeAddressRangeShared (
    IN  VOID                            *BaseAddress,
    IN  UINT32                          PageCount,
    OUT IOMMU_HOST_VISIBILITY_CONTEXT   *VisibilityContext
    )
{
    EFI_STATUS  Status;

    if (!IsIsolated ()) {
        //
        // No isolation: host already has access to all guest memory, so
        // there is no visibility hypercall to make. Clear the protection
        // handle so a later IoMmuMakeAddressRangePrivate is a no-op too.
        //
        if (VisibilityContext != NULL) {
            VisibilityContext->RangeProtectionHandle = NULL;
        }
        return EFI_SUCCESS;
    }

    Status = mHvIvm->MakeAddressRangeHostVisible (
                 mHvIvm,
                 HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE,
                 BaseAddress,
                 PageCount * EFI_PAGE_SIZE,
                 FALSE,
                 &VisibilityContext->RangeProtectionHandle
                 );

    return Status;
}


/**
  Revoke host visibility for an address range.

  @param[in]  VisibilityContext   Context from a prior MakeAddressRangeShared call.
**/
VOID
IoMmuMakeAddressRangePrivate (
    IN IOMMU_HOST_VISIBILITY_CONTEXT    *VisibilityContext
    )
{
    if (!IsIsolated ()) {
        //
        // No isolation: nothing was made host-visible, so nothing to revoke.
        //
        return;
    }

    mHvIvm->MakeAddressRangeNotHostVisible (mHvIvm, &VisibilityContext->RangeProtectionHandle);
}


//
// ---------------------------------------------------------------------------
// Pre-allocated bounce block pool.
//
// Each IOMMU_BOUNCE_BLOCK is a contiguous, host-visible region of pages
// allocated below 4GB. Map() requests are satisfied by sub-allocating a
// contiguous run of free pages from one of the blocks (via the per-block
// AllocBitmap). If no existing block can satisfy a request, a new block is
// allocated and made host-visible (one hypercall per new block, not per
// Map). Blocks are kept around for the lifetime of the driver to amortize
// the cost of host-visibility hypercalls across many DMA operations.
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
  Allocate a new bounce block of `PageCount` pages, make it host-visible,
  and insert it at the tail of the bounce block list.

  @param[in]   PageCount   Number of pages in the new block.
  @param[out]  BlockOut    The newly allocated block on success.

  @retval EFI_SUCCESS            Block allocated and registered.
  @retval EFI_OUT_OF_RESOURCES   Allocation or hypercall failed.
**/
EFI_STATUS
IoMmuPreAllocateBounceBlock (
    IN  UINT32                  PageCount,
    OUT PIOMMU_BOUNCE_BLOCK     *BlockOut
    )
{
    EFI_STATUS              Status;
    PIOMMU_BOUNCE_BLOCK     Block;
    EFI_PHYSICAL_ADDRESS    PhysicalAddress;
    UINT32                  BitmapWordCount;

    ASSERT (PageCount > 0);

    Block = AllocateZeroPool (sizeof (IOMMU_BOUNCE_BLOCK));
    if (Block == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    BitmapWordCount = (PageCount + 63) / 64;
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
    Status = gBS->AllocatePages (
                     AllocateMaxAddress,
                     EfiBootServicesData,
                     PageCount,
                     &PhysicalAddress
                     );
    if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR,
            "IoMmu: AllocateBounceBlock: AllocatePages(%d pages, <4GB) failed: %r\n",
            PageCount, Status));
        FreePool (Block->AllocBitmap);
        FreePool (Block);
        return Status;
    }

    Block->Signature       = IOMMU_BOUNCE_BLOCK_SIGNATURE;
    Block->BlockBase       = (VOID *)(UINTN)PhysicalAddress;
    Block->BlockPageCount  = PageCount;
    Block->BitmapWordCount = BitmapWordCount;
    Block->InUsePageCount  = 0;
    Block->IsHostVisible   = FALSE;

    Status = IoMmuMakeAddressRangeShared (
                 Block->BlockBase,
                 PageCount,
                 &Block->VisibilityContext
                 );
    if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR,
            "IoMmu: AllocateBounceBlock: MakeAddressRangeShared failed: %r\n",
            Status));
        gBS->FreePages (PhysicalAddress, PageCount);
        FreePool (Block->AllocBitmap);
        FreePool (Block);
        return Status;
    }

    Block->IsHostVisible = TRUE;
    InsertTailList (&mBounceBlockListHead, &Block->Link);

    DEBUG ((DEBUG_INFO,
        "IoMmu: AllocateBounceBlock: Block=%p Base=%p Pages=%d\n",
        Block, Block->BlockBase, PageCount));

    *BlockOut = Block;
    return EFI_SUCCESS;
}


EFI_STATUS
IoMmuAcquireBouncePages (
    IN  UINT32                  PageCount,
    OUT PIOMMU_BOUNCE_BLOCK     *Block,
    OUT UINT32                  *StartPageIndex,
    OUT VOID                    **BounceBase
    )
{
    LIST_ENTRY              *Entry;
    PIOMMU_BOUNCE_BLOCK     Candidate;
    UINT32                  StartBit;
    EFI_STATUS              Status;
    UINT32                  NewBlockPages;

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
    DEBUG ((DEBUG_INFO,
        "IoMmuAcquireBouncePages: Allocated new bounce block %p with %d pages to satisfy request for %d pages\n",
        Candidate, Candidate->BlockPageCount, PageCount));
    UpdateBitmapRun (Candidate->AllocBitmap, 0, PageCount, TRUE);
    Candidate->InUsePageCount = PageCount;

    *Block          = Candidate;
    *StartPageIndex = 0;
    *BounceBase     = Candidate->BlockBase;
    return EFI_SUCCESS;
}


VOID
IoMmuReleaseBouncePages (
    IN PIOMMU_BOUNCE_BLOCK      Block,
    IN UINT32                   StartPageIndex,
    IN UINT32                   PageCount
    )
{
    ASSERT (Block != NULL);
    ASSERT (Block->Signature == IOMMU_BOUNCE_BLOCK_SIGNATURE);
    ASSERT (PageCount > 0);
    ASSERT (StartPageIndex + PageCount <= Block->BlockPageCount);

    UpdateBitmapRun (Block->AllocBitmap, StartPageIndex, PageCount, FALSE);
    Block->InUsePageCount -= PageCount;
}
