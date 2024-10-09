/** @file
  Tpm2 intialization hooks specific to the MSFT0101 virtual TPM device.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/Tpm2DeviceLib.h>

#include <TpmInterface.h>           // Definitions specific to MSFT0101


// Prototype for function in Tpm2Acpi.c
EFI_STATUS
EFIAPI
InstallTpm2AcpiTable (
    VOID
    );

UINT32
ReadTpmPort(
    IN UINT32 AddressRegisterValue
);

/**
  Constructor for the lib.
  Important that this runs prior to Tcg2Dxe because it may disable some of
  the intended functionality.

  @retval EFI_SUCCESS     The entry point executed successfully.
  @retval other           Some error occured when executing this entry point.

**/
EFI_STATUS
EFIAPI
MsvmTpm2InitLibConstructorDxe (
  IN    EFI_HANDLE                  ImageHandle,
  IN    EFI_SYSTEM_TABLE            *SystemTable
  )
{
  EFI_STATUS    Status = EFI_SUCCESS;
  UINT64        TpmBaseAddress;
  UINT32        TcgProtocolVersion;

  DEBUG(( DEBUG_INFO, __FUNCTION__"()\n" ));

  //
  // If the TPM not enabled, don't perform any more TPM init.
  if (CompareGuid (PcdGetPtr(PcdTpmInstanceGuid), &gEfiTpmDeviceInstanceNoneGuid) ||
      CompareGuid (PcdGetPtr(PcdTpmInstanceGuid), &gEfiTpmDeviceInstanceTpm12Guid)){
    DEBUG ((DEBUG_INFO, "No TPM2 instance required!\n"));
    return EFI_SUCCESS;
  }

  TpmBaseAddress = FixedPcdGet64(PcdTpmBaseAddress);
  TpmBaseAddress += PcdGetBool(PcdTpmLocalityRegsEnabled) ? 0x40 : 0;

  //
  // Query the Tcg protocol version
  TcgProtocolVersion = ReadTpmPort(TpmIoGetTcgProtocolVersion);

  if ((TcgProtocolVersion != TcgProtocolTrEE) && (TcgProtocolVersion != TcgProtocolTcg2)) {
    DEBUG(( DEBUG_ERROR, __FUNCTION__" - TPM device reports bad version! 0x%X\n", TcgProtocolVersion ));
    return EFI_DEVICE_ERROR;
  }

  // If we're good, we need to make sure that our instance of Tpm2DeviceLib
  // can talk with the device.
  Tpm2RegisterTpm2DeviceLib( (TPM2_DEVICE_INTERFACE*)TpmBaseAddress );
  Status = InstallTpm2AcpiTable();

  // NOTE: This will cause an ASSERT if the TCG protocol version is incorrect.
  //       It is assumed this would indicate a software misconfiguration.
  return Status;
} // MsvmTpm2InitLibConstructorDxe()
