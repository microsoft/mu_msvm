/** @file
  This module is responsible for runtime initialization of the WDAT APCI table.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Library/BaseMemoryLib.h>
#include "AcpiPlatform.h"
#include <Base.h>

EFI_STATUS
WdatInitializeTable(
    IN OUT  EFI_ACPI_DESCRIPTION_HEADER* Table
    )
/*++

Routine Description:

    Initializes the WDAT table based on configuration data.

Arguments:

    Table - The WDAT Table, expressed as an EFI_ACPI_DESCRIPTION_HEADER*.

Return Value:

    EFI_SUCCESS     if WDAT is requested and properly initialized.
    EFI_UNSUPPORTED if WDAT is not required. Causes table to not be added.

--*/
{

    VM_HARDWARE_WATCHDOG_ACTION_TABLE *wdat;

    //
    // Get configuration to determine if this table is needed.
    //
    BOOLEAN watchdogEnabled = PcdGetBool(PcdWatchdogEnabled);

    if (!watchdogEnabled)
    {
        return EFI_UNSUPPORTED;
    }

    //
    // Get bios base address.
    //
    UINT32 biosBaseAddress = PcdGet32(PcdBiosBaseAddress);

    //
    // Init table with actual address values.
    //
    wdat = (VM_HARDWARE_WATCHDOG_ACTION_TABLE *)Table;

    for (int i = 0; i < VM_HARDWARE_WATCHDOG_ACTION_COUNT; i++)
    {
        //
        // BiosWatchdog (for guest) has its MMIO/IO-ports at an offset w.r.t bios base address.
        // BiosWdatAddress is at offset 8, BiosWdatData is at offset 12. WDAT ACPI table is
        // populated with 0 & 4 for Address & Data respectively.
        //
        wdat->action[i].RegisterRegion.Address = biosBaseAddress + wdat->action[i].RegisterRegion.Address + 8;
    }

    return EFI_SUCCESS;
}


