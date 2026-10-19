/** @file
  Library to inform Tcg2Pei not to measure the firmware volume.
  Some legacy Hyper-V versions require this

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <PiPei.h>
#include <Library/PeiServicesLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Ppi/FirmwareVolumeInfoMeasurementExcluded.h>

STATIC
EFI_PEI_FIRMWARE_VOLUME_INFO_MEASUREMENT_EXCLUDED_PPI  mExcludedFvs = {
  1, // Count
  {
    {
      (EFI_PHYSICAL_ADDRESS)FixedPcdGet64 (PcdFvBaseAddress),
      (UINT64)FixedPcdGet32 (PcdFvSize)
    }
  }
};

STATIC EFI_PEI_PPI_DESCRIPTOR  PpiList =
{
  EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST,
  &gEfiPeiFirmwareVolumeInfoMeasurementExcludedPpiGuid,
  (VOID *)&mExcludedFvs
};

EFI_STATUS
EFIAPI
ExcludeFvsFromMeasurementLibConstructor (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  // If this PCD is enabled, we install the PPI, which will
  // inform Tcg2Pei to exclude the firmware volume from measurements.
  // This means that the FV will not be measured into PCR0,
  // which is required for some legacy Hyper-V versions to boot.
  if (PcdGetBool (PcdExcludeFvsFromMeasurements)) {
    Status = PeiServicesInstallPpi (&PpiList);
    ASSERT_EFI_ERROR (Status);
  }

  return Status;
}
