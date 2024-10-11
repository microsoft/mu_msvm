/** @file
  Library to inform Tcg2Pei not to measure FvMain.
  Some legacy Hyper-V versions require this

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <PiPei.h>
#include <Library/PeiServicesLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Ppi/FirmwareVolumeInfoMeasurementExcluded.h>


EFI_PEI_FIRMWARE_VOLUME_INFO_MEASUREMENT_EXCLUDED_PPI exclude = {
	1, //count
	{
		(EFI_PHYSICAL_ADDRESS) FixedPcdGet64(PcdFvBaseAddress),
		(UINT64) FixedPcdGet32(PcdFvSize)
	}
};

STATIC EFI_PEI_PPI_DESCRIPTOR PpiList =
{
	EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST,
	&gEfiPeiFirmwareVolumeInfoMeasurementExcludedPpiGuid,
	&exclude
};


EFI_STATUS
EFIAPI
ExcludeMainFvFromMeasurementLibConstructor (
  IN       EFI_PEI_FILE_HANDLE       FileHandle,
  IN CONST EFI_PEI_SERVICES          **PeiServices
  )
{
	EFI_STATUS Status;
	Status = EFI_SUCCESS;

	if(PcdGetBool(PcdExcludeFvMainFromMeasurements))
	{
		Status = PeiServicesInstallPpi(&PpiList);
		ASSERT_EFI_ERROR(Status);
	}
	return Status;;
}