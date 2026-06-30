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

//
// Bounce dispatch state, resolved once at driver entry. A set of
// IOMMU_BOUNCE_MODE_* flags describing which per-region obligations
// (host visibility, pinning) wrap the bounced DMA. See
// IoMmuComputeBounceMode for the resolution policy and IOMMU_BOUNCE_MODE
// in IoMmuBounce.h for why this is a bitmask rather than an enum.
//
IOMMU_BOUNCE_MODE  mBounceMode = IOMMU_BOUNCE_MODE_NONE;


/**
  VM-property predicate: is this guest VA-backed (host pages may be
  uncommitted until faulted in) and therefore obligated to pin any
  region the host will DMA into?

  Sibling of IsIsolated() from IsolationLib. Both predicates abstract
  the underlying source of truth so that the policy code in
  IoMmuComputeBounceMode reads as a list of VM-property questions
  rather than as raw wire-format checks.

  TODO: Phase 2 wires this to a dedicated PCD set by PlatformPei -
  the moral equivalent of PcdIsolationArchitecture for IsIsolated().
  The PCD will itself be sourced from a hypervisor capability/feature
  register (and/or a dedicated UEFI_CONFIG_FLAGS bit), keeping this
  predicate independent of PcdForceDmaBounceEnabled (which is a VMM configuration
  knob, not a VM-property claim). For now this returns FALSE so the
  PINNING obligation is never asserted; the BOUNCE half of VA-backed
  scenarios is reachable via the soft override below.

  TODO: Promote to IsolationLib (or a new VmPropertiesLib) when a second
  consumer appears.

  @retval TRUE    The VM is VA-backed; bounce regions must be pinned.
  @retval FALSE   Not VA-backed (or not knowable from current sources).
**/
STATIC
BOOLEAN
IsVaBacked (
    VOID
    )
{
    // TODO: Implement the logic to determine if the VM is VA-backed, or
    // do this in an IsolationLib
    return FALSE;
}


/**
  Resolve the bounce dispatch flags. Cached in mBounceMode at init and
  consulted by all subsequent dispatch decisions. See IOMMU_BOUNCE_MODE
  in IoMmuBounce.h for the two-tier composition model.

  Hard-requirement predicates (compose):
    - IsIsolated() -> BOUNCE | HOST_VISIBILITY
    - IsVaBacked() -> PINNING            (PINNING in Phase 2)

  Soft override from PcdForceDmaBounceEnabled (the VMM's wire-format
  intent): when TRUE, force BOUNCE on regardless of the hard-requirement
  predicates above.

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
        DEBUG((DEBUG_INFO, "%a: VM is isolated\n", __FUNCTION__));
        Mode |= IOMMU_BOUNCE_MODE_BOUNCE | IOMMU_BOUNCE_MODE_HOST_VISIBILITY;
    }

    //
    // Hard requirement: VA-backed VM. In Phase 1 IsVaBacked() always
    // returns FALSE and the only obligation this branch would set
    // (PINNING) isn't defined yet, so the branch is a no-op stub. A
    // VA-backed VM that needs bounce buffering will already satisfy
    // the IsIsolated() requirement above, or have BOUNCE forced on
    // via the soft override below.
    //
    if (IsVaBacked ()) {
        DEBUG((DEBUG_INFO, "%a: VM is VA-backed\n", __FUNCTION__));
        //
        // TODO: Phase 2:
        //   if (PcdGetBool (PcdGpaPinningSupported)) {
        //       Mode |= IOMMU_BOUNCE_MODE_PINNING;
        //   }
        //
    }

    //
    // Soft override: VMM-configured intent.
    //
    if (PcdGetBool (PcdForceDmaBounceEnabled)) {
        DEBUG((DEBUG_INFO, "%a: Forcing DMA bounce\n", __FUNCTION__));
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

    mSharedGpaBoundary = (EFI_PHYSICAL_ADDRESS)PcdGet64 (PcdIsolationSharedGpaBoundary);
    mCanonicalizationMask = PcdGet64 (PcdIsolationSharedGpaCanonicalizationBitmask);

    if (!IoMmuRequiresHostVisibility ()) {
        //
        // The HV IVM protocol is only used to make bounce regions
        // host-visible. Skip the locate when that obligation is not set
        // so the driver can come up in pure-bounce and pass-through
        // modes even when no provider exposes the protocol.
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

    //
    // Always clear the protection handle first so the matching
    // IoMmuMakeAddressRangePrivate call is a safe no-op if the
    // HOST_VISIBILITY obligation is not set in mBounceMode.
    //
    if (VisibilityContext != NULL) {
        VisibilityContext->RangeProtectionHandle = NULL;
    }

    if (IoMmuRequiresHostVisibility ()) {
        Status = mHvIvm->MakeAddressRangeHostVisible (
                     mHvIvm,
                     HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE,
                     BaseAddress,
                     PageCount * EFI_PAGE_SIZE,
                     FALSE,
                     &VisibilityContext->RangeProtectionHandle
                     );
        if (EFI_ERROR (Status)) {
            return Status;
        }
    }

    //
    // TODO: Phase 2: if ((mBounceMode & IOMMU_BOUNCE_MODE_PINNING) != 0) issue
    // HvCallPinGpaPageRanges here and stash unpin state in
    // VisibilityContext. On failure, unwind the host-visibility step
    // above before returning so the caller sees a clean error.
    //

    return EFI_SUCCESS;
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
    //
    // Symmetric to IoMmuMakeAddressRangeShared. Each obligation is
    // unwound only if it was applied in the first place; the bits in
    // mBounceMode are the source of truth (and the protection handle is
    // NULL when HOST_VISIBILITY was not set, so the hypercall would be
    // a no-op anyway).
    //
    // TODO: Phase 2: unpin BEFORE revoking host visibility so the host stops
    // touching the pages first.
    //   if ((mBounceMode & IOMMU_BOUNCE_MODE_PINNING) != 0) {
    //       HvCallUnpinGpaPageRanges (...);
    //   }
    //
    if (IoMmuRequiresHostVisibility ()) {
        mHvIvm->MakeAddressRangeNotHostVisible (mHvIvm, &VisibilityContext->RangeProtectionHandle);
    }
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
