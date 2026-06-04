/** @file
  DXE driver that registers RAM disks declared in the config blob.

  During PEI, the config blob parser creates GUIDed HOBs for each
  UefiConfigRamDisk entry. This driver enumerates those HOBs and
  registers each RAM disk via EFI_RAM_DISK_PROTOCOL so that the
  volumes become available for BDS boot option processing.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/RamDisk.h>
#include <RamDiskConfigHob.h>

/**
  Entry point for RamDiskConfigDxe.

  Enumerates RAM disk config HOBs and registers each RAM disk.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The driver completed successfully.

**/
EFI_STATUS
EFIAPI
RamDiskConfigDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS            Status;
  EFI_RAM_DISK_PROTOCOL *RamDiskProtocol;
  VOID                  *Hob;
  UINTN                 Count;

  //
  // Find the first RAM disk config HOB. If none exist, nothing to do.
  //
  Hob = GetFirstGuidHob (&gMsvmRamDiskConfigHobGuid);
  if (Hob == NULL) {
    DEBUG ((DEBUG_VERBOSE, "RamDiskConfigDxe: No RAM disk HOBs found\n"));
    return EFI_SUCCESS;
  }

  //
  // Locate the RAM disk protocol.
  //
  Status = gBS->LocateProtocol (&gEfiRamDiskProtocolGuid, NULL, (VOID **)&RamDiskProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "RamDiskConfigDxe: Failed to locate EFI_RAM_DISK_PROTOCOL: %r\n", Status));
    return Status;
  }

  //
  // Iterate over all RAM disk config HOBs and register each one.
  //
  Count = 0;
  while (Hob != NULL) {
    EFI_HOB_GUID_TYPE       *GuidHob = (EFI_HOB_GUID_TYPE *)Hob;
    RAM_DISK_CONFIG_HOB_DATA *Data    = (RAM_DISK_CONFIG_HOB_DATA *)GET_GUID_HOB_DATA (GuidHob);
    EFI_DEVICE_PATH_PROTOCOL *DevicePath = NULL;

    DEBUG ((DEBUG_INFO, "RamDiskConfigDxe: Registering RAM disk #%u at GPA 0x%lx, Size 0x%lx\n",
            Count, Data->RamDiskGpa, Data->RamDiskSize));

    Status = RamDiskProtocol->Register (
                                Data->RamDiskGpa,
                                Data->RamDiskSize,
                                &gEfiVirtualDiskGuid,
                                NULL,
                                &DevicePath
                                );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "RamDiskConfigDxe: Failed to register RAM disk #%u: %r\n", Count, Status));
    } else {
      DEBUG ((DEBUG_INFO, "RamDiskConfigDxe: RAM disk #%u registered successfully\n", Count));
    }

    Count++;
    Hob = GetNextGuidHob (&gMsvmRamDiskConfigHobGuid, GET_NEXT_HOB (Hob));
  }

  DEBUG ((DEBUG_INFO, "RamDiskConfigDxe: Processed %u RAM disk(s)\n", Count));
  return EFI_SUCCESS;
}
