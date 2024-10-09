/** @file
  This module is responsible for runtime initialization of the entropy
  table.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Library/BaseMemoryLib.h>
#include "AcpiPlatform.h"

//
// Entry point
//

EFI_STATUS
Oem0InitializeTable(
    IN OUT  EFI_ACPI_DESCRIPTION_HEADER* Table
    )
/*++

Routine Description:

    Initializes the OEM0 table.

Arguments:

    Table - The Oem0 Table, expressed as an EFI_ACPI_DESCRIPTION_HEADER*.

Return Value:

    EFI_SUCCESS

--*/
{
    VM_ACPI_ENTROPY_TABLE *table;

    //
    // Copy the entropy data from the configuration.
    //
    table = (VM_ACPI_ENTROPY_TABLE *)Table;

    CopyMem(table->Data, (VOID*)(UINTN) PcdGet64(PcdEntropyPtr), ConfigLibEntropyDataSize);

    return EFI_SUCCESS;
}

