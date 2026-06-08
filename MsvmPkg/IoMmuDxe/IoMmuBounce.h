/** @file
    Bounce buffer types and declarations for the Hyper-V IOMMU driver.

    This module provides bounce buffering for DMA operations in isolated
    Hyper-V virtual machines. Memory that needs to be visible to the host
    for DMA must be explicitly shared via the hypervisor.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/EfiHv.h>
#include <Protocol/IoMmu.h>
#include <IsolationTypes.h>

//
// IOMMU_BOUNCE_MODE - dispatch state for the bounce subsystem. Resolved
// once at driver init by IoMmuComputeBounceMode and consulted by the
// per-region helpers to decide which hypercalls (if any) wrap the DMA.
//
// Composed from two tiers:
//   - Hard requirements: VM-property predicates (IsIsolated, IsVaBacked)
//     each OR in the obligations their property implies. Each predicate
//     hides its own source of truth (PCD today, capability register
//     tomorrow).
//   - Soft override: PcdForceDmaBounceEnabled, read directly because it
//     *is* VMM configuration, not a VM property. When set, force BOUNCE
//     on; PINNING is left to the hard-requirement predicates above.
//
// Bitflags because the obligations are independent and compose - e.g. a
// paravisor-isolated VA-backed VM ends up with
// BOUNCE | HOST_VISIBILITY | PINNING.
//
typedef UINT32 IOMMU_BOUNCE_MODE;

//
// No bounce, no hypercalls. DMA hits guest memory directly and the
// bounce pool is never allocated.
//
#define IOMMU_BOUNCE_MODE_NONE              0x00000000

//
// Funnel DMA through the pre-allocated bounce pool (pages allocated
// below 4GB so they satisfy both 32-bit and 64-bit DMA). Set by
// IsIsolated() and by the soft PcdForceDmaBounceEnabled override.
//
#define IOMMU_BOUNCE_MODE_BOUNCE            BIT0

//
// Make each bounce region host-visible via the HV IVM protocol
// (MakeAddressRangeHostVisible / NotHostVisible). Set when IsIsolated().
//
#define IOMMU_BOUNCE_MODE_HOST_VISIBILITY   BIT1

//
// TODO: Reserved for Phase 2 (VA-backed VMs). Will pin each DMA region
// via HvCallPinGpaPageRanges before the host accesses it and unpin on
// release. Independent of BOUNCE: a VA-backed VM that supports pinning
// can pin guest pages and DMA directly into them, no bouncing required.
// Set when IsVaBacked() and the hypervisor advertises pin-hypercall
// support. Composes with BOUNCE | HOST_VISIBILITY for VA-backed
// paravisor-isolated VMs.
//
// #define IOMMU_BOUNCE_MODE_PINNING        BIT2

//
// Context for tracking host visibility of an address range.
//
typedef struct _IOMMU_HOST_VISIBILITY_CONTEXT
{
    EFI_HV_PROTECTION_HANDLE     RangeProtectionHandle;
} IOMMU_HOST_VISIBILITY_CONTEXT;

//
// Page counts for bounce blocks. Each block is allocated as a contiguous,
// host-visible region below 4GB. Map() requests are satisfied by
// sub-allocating contiguous page runs from a block.
//
// IOMMU_BOUNCE_INITIAL_BLOCK_PAGES: size of the single block pre-allocated
//   at driver entry. Sized to absorb large boot-time DMA transfers (e.g.
//   NVMe namespace reads up to ~2 MB) without a lazy allocation.
//
// IOMMU_BOUNCE_GROWTH_BLOCK_PAGES: minimum size used when the pool is
//   exhausted and a new block must be allocated on demand. The new block
//   is sized to MAX(request, growth) so small requests don't reserve a
//   disproportionately large region.
//
#define IOMMU_BOUNCE_INITIAL_BLOCK_PAGES  1024
#define IOMMU_BOUNCE_GROWTH_BLOCK_PAGES   32

//
// IOMMU_BOUNCE_BLOCK - a contiguous, host-visible region of pages backing
// Map() requests. The block is kept (and stays host-visible) for the
// lifetime of the driver. Per-page in-use state is tracked via a bitmap
// sized to BlockPageCount.
//
#define IOMMU_BOUNCE_BLOCK_SIGNATURE  SIGNATURE_32('i','o','m','b')

typedef struct _IOMMU_BOUNCE_BLOCK
{
    UINT32                        Signature;
    LIST_ENTRY                    Link;
    VOID                          *BlockBase;          // contiguous host-visible base (private VA)
    UINT32                        BlockPageCount;
    UINT32                        BitmapWordCount;     // number of UINT64 words in AllocBitmap
    UINT64                        *AllocBitmap;        // 1 bit per page; 1 = in use
    UINT32                        InUsePageCount;
    BOOLEAN                       IsHostVisible;
    IOMMU_HOST_VISIBILITY_CONTEXT VisibilityContext;
} IOMMU_BOUNCE_BLOCK, *PIOMMU_BOUNCE_BLOCK;

//
// MAP_CONTEXT - tracking structure for an active Map operation. Stored as
// the Mapping handle returned to callers. Records which bounce block and
// starting page index were sub-allocated so Unmap can release them.
//
#define IOMMU_MAP_CONTEXT_SIGNATURE  SIGNATURE_32('i','o','m','c')

typedef struct _IOMMU_MAP_CONTEXT
{
    UINT32                        Signature;
    EDKII_IOMMU_OPERATION         Operation;
    VOID                          *HostAddress;
    UINTN                         NumberOfBytes;
    VOID                          *BounceBase;         // private VA of the sub-allocated bounce region
    UINT32                        BouncePageCount;
    PIOMMU_BOUNCE_BLOCK           BounceBlock;         // owning block
    UINT32                        BounceStartPage;     // starting page index within block
} IOMMU_MAP_CONTEXT, *PIOMMU_MAP_CONTEXT;

//
// ALLOC_CONTEXT - tracking structure for an active AllocateBuffer allocation.
// Used by FreeBuffer to revoke host visibility.
//
typedef struct _IOMMU_ALLOC_CONTEXT
{
    LIST_ENTRY                    Link;
    VOID                          *OriginalAddress;
    UINTN                         Pages;
    IOMMU_HOST_VISIBILITY_CONTEXT VisibilityContext;
} IOMMU_ALLOC_CONTEXT, *PIOMMU_ALLOC_CONTEXT;


//
// Bounce dispatch state, resolved once at driver entry. Consulted by
// IoMmuIsBounceActive() / IoMmuRequiresHostVisibility() and by the
// dispatch sites in IoMmuDxe.c. Assign via IoMmuComputeBounceMode().
//
extern IOMMU_BOUNCE_MODE  mBounceMode;

//
// Resolve the bounce dispatch flags from VM-property predicates and the
// PcdForceDmaBounceEnabled soft override. The driver entry point assigns
// the result to mBounceMode before any IoMmuIsBounceActive() consumer.
//
IOMMU_BOUNCE_MODE
IoMmuComputeBounceMode (
    VOID
    );

//
// Bounce buffer initialization. Allocates pool tracking state and locates
// the HV IVM protocol when host visibility is required. Assumes mBounceMode
// has already been assigned.
//
EFI_STATUS
IoMmuInitializeBounce (
    VOID
    );

//
// Returns TRUE if bounce buffering is active (isolated VM with IOMMU).
//
BOOLEAN
IoMmuIsBounceActive (
    VOID
    );

//
// Returns TRUE if bounce regions must be made host-visible via the HV
// IVM protocol (i.e. the HOST_VISIBILITY obligation is set in the
// resolved bounce mode).
//
BOOLEAN
IoMmuRequiresHostVisibility (
    VOID
    );

//
// Address translation helpers for shared GPA.
//
EFI_PHYSICAL_ADDRESS
IoMmuGetSharedPa (
    IN VOID     *Address
    );

VOID *
IoMmuGetSharedVa (
    IN VOID     *Address
    );

//
// Host visibility management.
//
EFI_STATUS
IoMmuMakeAddressRangeShared (
    IN VOID                             *BaseAddress,
    IN UINT32                           PageCount,
    OUT IOMMU_HOST_VISIBILITY_CONTEXT   *VisibilityContext
    );

VOID
IoMmuMakeAddressRangePrivate (
    IN IOMMU_HOST_VISIBILITY_CONTEXT    *VisibilityContext
    );

//
// Bounce block pool - pre-allocated, host-visible contiguous regions used
// to satisfy Map() requests without per-Map hypercalls.
//

/**
  Pre-allocate a bounce block of `PageCount` pages, make it host-visible,
  and add it to the pool. Used to populate the pool at driver init so the
  common-case Map() path needs no hypercall.

  @param[in]   PageCount   Number of pages in the new block.
  @param[out]  BlockOut    The newly allocated block on success.

  @retval EFI_SUCCESS            Block allocated and registered.
  @retval EFI_OUT_OF_RESOURCES   Allocation or hypercall failed.
**/
EFI_STATUS
IoMmuPreAllocateBounceBlock (
    IN  UINT32                  PageCount,
    OUT PIOMMU_BOUNCE_BLOCK     *BlockOut
    );

/**
  Acquire a contiguous run of bounce pages from the pool. Allocates a new
  bounce block (and makes it host-visible) on demand if no existing block
  has a sufficient contiguous free run.

  @param[in]   PageCount        Number of contiguous pages required.
  @param[out]  Block            Owning bounce block.
  @param[out]  StartPageIndex   Starting page index within the block.
  @param[out]  BounceBase       Private VA of the start of the run.

  @retval EFI_SUCCESS           Pages acquired.
  @retval EFI_OUT_OF_RESOURCES  Could not acquire/allocate pages.
**/
EFI_STATUS
IoMmuAcquireBouncePages (
    IN  UINT32                  PageCount,
    OUT PIOMMU_BOUNCE_BLOCK     *Block,
    OUT UINT32                  *StartPageIndex,
    OUT VOID                    **BounceBase
    );

/**
  Release a previously acquired contiguous run of bounce pages back to the
  pool. The pages remain host-visible (the owning block stays host-visible
  for its lifetime).

  @param[in]  Block            Owning bounce block.
  @param[in]  StartPageIndex   Starting page index within the block.
  @param[in]  PageCount        Number of pages to release.
**/
VOID
IoMmuReleaseBouncePages (
    IN PIOMMU_BOUNCE_BLOCK      Block,
    IN UINT32                   StartPageIndex,
    IN UINT32                   PageCount
    );
