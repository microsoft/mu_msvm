/** @file
  This file contains routines to initialize and install TPM2 ACPI table.
  Specific to the MSFT0101 virtual TPM device.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Protocol/AcpiTable.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/IoLib.h>

#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/Tpm2Acpi.h>

#include <TpmInterface.h>

EFI_TPM2_ACPI_TABLE mTpm20AcpiTable = {0};

VOID
Tpm2InitializeAcpiTable()
/*++

Routine Description:

    This routine fills in the TPM20 ACPI table entries.
    See "TCG PC Client Platform TPM Profile Specification for TPM 2.0"
    for details about control and locality register offsets.
    For compatibility reasons, not all VMs support control registers
    at spec compliant offsets.

Arguments:

    None

Return Value:

    EFI_STATUS.

--*/
{
    UINT64 TpmBaseAddress = FixedPcdGet64(PcdTpmBaseAddress);
    TpmBaseAddress += PcdGetBool(PcdTpmLocalityRegsEnabled) ? 0x40 : 0;

    ZeroMem(&mTpm20AcpiTable, sizeof(EFI_TPM2_ACPI_TABLE));

    mTpm20AcpiTable.Header.Signature = 0x324D5054;          // 'TPM2'
    mTpm20AcpiTable.Header.Length = sizeof(EFI_TPM2_ACPI_TABLE);
    mTpm20AcpiTable.Header.Revision = 3;
    CopyMem(mTpm20AcpiTable.Header.OemId, PcdGetPtr(PcdAcpiDefaultOemId), sizeof(mTpm20AcpiTable.Header.OemId));
    mTpm20AcpiTable.Header.OemTableId = 0x202020204D505456; // 'VTPM    '
    mTpm20AcpiTable.Header.OemRevision = 0x1;
    mTpm20AcpiTable.Header.CreatorId = 0x5446534D;          // 'MSFT'
    mTpm20AcpiTable.Header.CreatorRevision = 0x00000001;
    mTpm20AcpiTable.StartMethod = EFI_TPM2_ACPI_TABLE_START_METHOD_COMMAND_RESPONSE_BUFFER_INTERFACE;
    mTpm20AcpiTable.AddressOfControlArea = TpmBaseAddress;

    mTpm20AcpiTable.Header.Checksum = CalculateCheckSum8((UINT8*)&mTpm20AcpiTable, sizeof(EFI_TPM2_ACPI_TABLE));
}


/*++

    This routine initializes and installs TPM2 ACPI table.

Arguments:

    NONE

Return Value:

    EFI_STATUS

--*/
EFI_STATUS
EFIAPI
InstallTpm2AcpiTable (
    VOID
    )
{
    EFI_STATUS                  Status = EFI_SUCCESS;
    UINTN                       TableHandle;
    EFI_ACPI_TABLE_PROTOCOL     *AcpiTableProtocol;

    // TODO: Register a callback for when this protocol is available, if EFI_NOT_FOUND.
    Status = gBS->LocateProtocol(&gEfiAcpiTableProtocolGuid, NULL, (VOID**) &AcpiTableProtocol);
    if (EFI_ERROR(Status))
    {
        goto Cleanup;
    }

    Tpm2InitializeAcpiTable();

    Status = AcpiTableProtocol->InstallAcpiTable(AcpiTableProtocol,
                                                 &mTpm20AcpiTable,
                                                 sizeof(mTpm20AcpiTable),
                                                 &TableHandle);
    if (EFI_ERROR(Status))
    {
        goto Cleanup;
    }

    Status = EFI_SUCCESS;

Cleanup:

    return Status;
}
