/** @file
  Tpm2 intialization hooks specific to the MSFT0101 virtual TPM device.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/Tpm2DeviceLib.h>

#include <TpmInterface.h>           // Definitions specific to the MSFT0101 virtual TPM device.


VOID
WriteTpmPort(
    IN UINT32 AddressRegisterValue,
    IN UINT32 DataRegisterValue
);

UINT32
ReadTpmPort(
    IN UINT32 AddressRegisterValue
);

/**
  Performs basic, one-time initialization for the MSFT0101 virtual TPM device.
  Will allocate a CRB buffer and configure that buffer with the device.

  @retval     EFI_SUCCESS   Everything is fine. Continue with init.
  @retval     Others        Something has gone wrong. Do not initialize TPM any further.

**/
EFI_STATUS
EFIAPI
MsvmTpmDeviceInitEarlyBoot(
	VOID
  )
{
    EFI_STATUS            Status = EFI_SUCCESS;
    EFI_PHYSICAL_ADDRESS  CrBuffer = 0;
    UINT32                TpmIoEstablishedResponse;
    UINT64                TpmBaseAddress;

    Status = PeiServicesAllocatePages(EfiRuntimeServicesData, 2, &CrBuffer);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, __FUNCTION__" - Failed to allocate CRB for TPM device!\n"));
        return Status;
    }

    if (CrBuffer > 0xFFFFFFFFULL) {
        // PEI memory was published as - Base at 1MB, size max 64MB.
        // It is guaranteed that physical address is below 4 GB.
        DEBUG((DEBUG_ERROR, __FUNCTION__" - CRB allocation for TPM device is incorrect!\n"));
        ASSERT(FALSE);
        return EFI_DEVICE_ERROR;
    }

    DEBUG((DEBUG_VERBOSE, __FUNCTION__" - CrBuffer == 0x%016lX\n", CrBuffer));

    ZeroMem((UINT8*)CrBuffer, 2 * EFI_PAGE_SIZE);

    TpmBaseAddress = FixedPcdGet64(PcdTpmBaseAddress);
    TpmBaseAddress += PcdGetBool(PcdTpmLocalityRegsEnabled) ? 0x40 : 0;

    DEBUG((DEBUG_VERBOSE, __FUNCTION__" - TpmBaseAddress == 0x%016lX\n", TpmBaseAddress));

    //
    // Send the request to the TPM device.
    // Cast of command buffer GPA is safe as it was allocated below 4GB.
    //
    WriteTpmPort(TpmIoMapSharedMemory, (UINT32)CrBuffer);

    //
    // Query the mapping result
    //
    TpmIoEstablishedResponse = ReadTpmPort(TpmIoEstablished);
    if (TpmIoEstablishedResponse == 0) {
        //
        // Couldn't establish memory mapping with device.
        //
        DEBUG((DEBUG_ERROR, __FUNCTION__" - Couldn't establish memory mapping with device!\n"));
        return EFI_NO_MAPPING;
    }

    DEBUG((DEBUG_VERBOSE, __FUNCTION__" - TpmIoEstablishedResponse == 0x%08X\n", TpmIoEstablishedResponse));

    Tpm2RegisterTpm2DeviceLib((TPM2_DEVICE_INTERFACE*)TpmBaseAddress);

    return Status;
} // MsvmTpmDeviceInitEarlyBoot()


/**
  Constructor for the lib.
  Important that this runs prior to Tcg2Pei because it may disable some of
  the intended functionality.

  IMPORTANT NOTE: Because Tcg2Pei requests to be shadowed, this constructor
                  will be invoked twice. We need to make sure that we don't perform
                  some of these behaviors twice.

  @retval EFI_SUCCESS     The entry point executed successfully.
  @retval other           Some error occured when executing this entry point.

**/
EFI_STATUS
EFIAPI
MsvmTpm2InitLibConstructorPei (
  IN       EFI_PEI_FILE_HANDLE       FileHandle,
  IN CONST EFI_PEI_SERVICES          **PeiServices
  )
{
  EFI_STATUS        Status = EFI_SUCCESS;
  BOOLEAN           TpmEnabled = FALSE;
  UINTN             GuidSize = sizeof( EFI_GUID );
  static BOOLEAN    EarlyInitComplete = FALSE;

  DEBUG(( DEBUG_INFO, __FUNCTION__"()\n" ));

  //
  // If the TPM is disabled in the Hyper-V UI, don't perform
  // any more TPM init.
  // NOTE: Should occur after the PlatformPei init module from Hyper-V.
  //       This is because of the depex on gEfiPeiMasterBootModePpiGuid.
  TpmEnabled = PcdGetBool( PcdTpmEnabled );
  if (!TpmEnabled) {
    DEBUG(( DEBUG_INFO, __FUNCTION__" - Detected a disabled TPM. Bypassing init.\n" ));
    Status = PcdSetPtrS( PcdTpmInstanceGuid, &GuidSize, &gEfiTpmDeviceInstanceNoneGuid );
    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, __FUNCTION__" - Failed to set the PCD PcdTpmInstanceGuid::0x%x \n", Status));
        ASSERT_EFI_ERROR( Status );
    }
  }

  //
  // If we're still good to continue init, perform the required Hyper-V init.
  if (TpmEnabled && !EarlyInitComplete) {
    Status = MsvmTpmDeviceInitEarlyBoot();
    if (EFI_ERROR( Status )) {
      DEBUG(( DEBUG_ERROR, __FUNCTION__" - MsvmTpmDeviceInitEarlyBoot() returned %r!\n", Status ));
      ASSERT_EFI_ERROR( Status );
    }
    EarlyInitComplete = TRUE;
  }

  // return Status;
  return EFI_SUCCESS;     // Library constructors ASSERT if anything other than EFI_SUCCESS is returned.
} // HyperVTpm2InitLibConstructor()
