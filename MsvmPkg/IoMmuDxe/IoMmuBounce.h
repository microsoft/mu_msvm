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
// Bounce buffer initialization.
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
