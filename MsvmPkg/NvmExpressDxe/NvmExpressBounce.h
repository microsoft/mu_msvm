/** @file
    Bounce buffer routines for NVMe device

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Protocol/EfiHv.h>

#include <IsolationTypes.h>

#include "NvmExpress.h"

//
// Nvme bounce buffer support.
//

#define NVME_BOUNCE_BLOCK_SIZE  (32 * EFI_PAGE_SIZE)

typedef struct _NVME_BOUNCE_BLOCK
{
    LIST_ENTRY                  BlockListEntry;

    struct _NVME_BOUNCE_PAGE *FreePageListHead;

    UINT32                      InUsePageCount;
    BOOLEAN                     IsHostVisible;

    VOID *                      BlockBase;
    UINT32                      BlockPageCount;
    EFI_HV_PROTECTION_HANDLE    ProtectionHandle;

    // Allocate associated _NVME_BOUNCE_PAGE as a large block
    struct _NVME_BOUNCE_PAGE    *BouncePageStructureBase;
} NVME_BOUNCE_BLOCK, *PNVME_BOUNCE_BLOCK;

//
// NVME_BOUNCE_PAGE - represents one guest physical page of a block.
// Units of pages are allocated to a vmbus packet as required and
// returned to the 'block pool' when not in use.
//
typedef struct _NVME_BOUNCE_PAGE
{
    struct _NVME_BOUNCE_PAGE*   NextBouncePage;
    struct _NVME_BOUNCE_BLOCK*  BounceBlock;
    VOID *                      PageVA;
    UINT64                      HostVisiblePA;
} NVME_BOUNCE_PAGE, *PNVME_BOUNCE_PAGE;

typedef struct _NVME_PAGE_VISIBILITY_CONTEXT
{
    EFI_HV_PROTECTION_HANDLE    RangeProtectionHandle;
} NVME_HOST_VISIBILITY_CONTEXT;


EFI_STATUS
NvmExpressInitializeBounce(
    VOID
    );

/**
  Return true if NVMe should use bounce buffering.

  @param None.

  @returns TRUE if bounce buffering should be used.
**/
BOOLEAN
NvmExpressIsBounceActive(
    VOID
    );

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
    );

/**
  Free the block of memory allocated for I/O. Marks the memory as not host-visible.

  @param[in]  Block               Bounce block that needs to be freed.

**/
VOID
NvmExpressFreeBounceBlock(
    IN PNVME_BOUNCE_BLOCK Block
    );

/**
  Free all of the large blocks of memory allocated for I/O. Marks the memory
  as not host-visible. Frees the associated tracking structures.

  @param[in]  Private             Pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.

**/
VOID
NvmExpressFreeAllBounceBlocks(
    IN NVME_CONTROLLER_PRIVATE_DATA  *Private
    );

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
    );

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
    );

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
    );

/**
  Zero initialize host-visible bounce page list.

  @param[in]  BouncePageList      List of bounce pages (shared with host).

**/
VOID
NvmExpressZeroBouncePageList(
    IN PNVME_BOUNCE_PAGE BouncePageList
    );


//
// Page visibility
//
UINTN
NvmExpressGetSharedPa(
    VOID* Address
    );

VOID*
NvmExpressGetSharedVa(
    VOID* Address
    );

EFI_STATUS
NvmExpressMakeAddressRangeShared(
    IN OUT  NVME_HOST_VISIBILITY_CONTEXT    *HostVisibilityContext,
    IN OUT  VOID                            *BaseAddress,
    IN      UINT32                          ByteCount
    );

VOID
NvmExpressMakeAddressRangePrivate(
    IN      NVME_HOST_VISIBILITY_CONTEXT    *HostVisibilityContext,
    IN OUT  VOID                            *AddressRange
    );