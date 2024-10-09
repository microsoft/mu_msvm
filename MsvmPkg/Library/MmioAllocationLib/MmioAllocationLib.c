/** @file
  MMIO allocation library

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MmioAllocationLib.h>
#include <Library/PcdLib.h>

// The base address to allocate from
static UINT64 mMmioFreeBaseAddress = 0;

// The currently allocated amount of space
static UINT64 mMmioAllocatedSpace = 0;

// The amount of free space remaining
static UINT64 mMmioFreeSpaceRemaining = 0;

EFI_STATUS
EFIAPI
MmioAllocationLibConstructor(
    IN EFI_HANDLE                          ImageHandle,
    IN EFI_SYSTEM_TABLE                   *SystemTable
    )
{
    // Load the PCD for the high mmio gap
    mMmioFreeBaseAddress = PcdGet64(PcdHighMmioGapBasePageNumber) * EFI_PAGE_SIZE;
    mMmioFreeSpaceRemaining = PcdGet64(PcdHighMmioGapSizeInPages) * EFI_PAGE_SIZE;

    return EFI_SUCCESS;
}

//
// \brief      Allocate Mmio space from the high gap.
//
// \param[in]  NumberOfPages  The number of pages to allocate from the mmio gap
//
// \return     A pointer to the region of mmio space, or NULL on failure.
//
VOID*
AllocateMmioPages(
    IN UINT64 NumberOfPages
    )
{
    // This allocator is very simple. All we do is check if we have enough free space,
    // and give it away. We don't support reclaim since the only devices needing
    // dynamic MMIO arbitration is the VPCI VSC and NVME devices.
    //
    // In the future, if reclaim is needed the allocator could become much smarter.
    // But for now, it doesn't need to be.
    UINT64 spaceToAllocate = NumberOfPages * EFI_PAGE_SIZE;
    VOID* baseAddress = NULL;
    UINT64 alignedAllocationBase = ALIGN_VALUE(mMmioFreeBaseAddress, spaceToAllocate);
    UINT64 totalAllocationSize = (alignedAllocationBase - mMmioFreeBaseAddress) + spaceToAllocate;

    if (totalAllocationSize > mMmioFreeSpaceRemaining ||
        totalAllocationSize == 0)
    {
        // Not enough free space.
        return NULL;
    }

    // TODO:    This is a static lib for now and the VPCI vsc does not need thread safety,
    //          but if it ever becomes a full DXE driver, it will need to raise/lower
    //          TPL to synchronize across different callers.
    baseAddress = (VOID*) alignedAllocationBase;
    mMmioFreeBaseAddress += totalAllocationSize;
    mMmioAllocatedSpace += totalAllocationSize;
    mMmioFreeSpaceRemaining -= totalAllocationSize;

    return baseAddress;
}