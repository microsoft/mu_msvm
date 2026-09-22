/** @file
  This module is responsible for runtime initialization of the entropy
  table.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <IsolationTypes.h>
#include "AcpiPlatform.h"

//
// Entry point
//

EFI_STATUS
Oem0InitializeTable (
  IN OUT  EFI_ACPI_DESCRIPTION_HEADER  *Table
  )

/*++

Routine Description:

    Initializes the OEM0 table.

Arguments:

    Table - The Oem0 Table, expressed as an EFI_ACPI_DESCRIPTION_HEADER*.

Return Value:

    EFI_SUCCESS       The entropy table was initialized, or no configured entropy was available.

--*/
{
  EFI_STATUS             status;
  VM_ACPI_ENTROPY_TABLE  *table;
  UINT64                 entropyAddress;
  UINT32                 IsolationType;

  table = (VM_ACPI_ENTROPY_TABLE *)Table;

  // Confidential VMs should not depend on the VMM for entropy and instead should use hardware-generated entropy.
  IsolationType = GetIsolationType ();
  if ((IsolationType == UefiIsolationTypeTdx) || (IsolationType == UefiIsolationTypeSnp)) {
    status = Oem0GetHardwareEntropy (table->Data, ConfigLibEntropyDataSize);
    if (EFI_ERROR (status)) {
      DEBUG ((DEBUG_ERROR, "Oem0InitializeTable: Failed to generate confidential guest entropy: %r\n", status));
      ASSERT_EFI_ERROR (status);
    }

    return EFI_SUCCESS;
  }

  entropyAddress = PcdGet64 (PcdEntropyPtr);
  if (entropyAddress == 0) {
    DEBUG ((DEBUG_ERROR, "Oem0InitializeTable: PcdEntropyPtr is NULL. Entropy table will not be initialized.\n"));
    ASSERT (entropyAddress != 0);
    return EFI_SUCCESS;
  }

  //
  // Copy the entropy data from the configuration.
  //
  CopyMem (table->Data, (VOID *)(UINTN)entropyAddress, ConfigLibEntropyDataSize);

  return EFI_SUCCESS;
}
