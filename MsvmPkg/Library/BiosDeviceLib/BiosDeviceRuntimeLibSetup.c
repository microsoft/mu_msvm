/** @file
  Library setup for the runtime version of BiosDeviceLib

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <PiDxe.h>

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>

// Use MMIO access on ARM64 otherwise use IO access
#if defined(MDE_CPU_AARCH64)
#define _USING_BIOS_MMIO_ 1
#elif defined(MDE_CPU_X64)
#define _USING_BIOS_MMIO_ 0
#else
#error Unsupported Architecture
#endif

extern UINTN mBiosBaseAddress;
extern void SetupBaseAddress();

#if _USING_BIOS_MMIO_

// Event handle for virtual address change event
EFI_EVENT   mVirtualAddressChangeEvent = NULL;

VOID
EFIAPI
BiosDeviceLibAddressChangeHandler(
    IN EFI_EVENT Event,
    IN void*     Context
    )
{
    // Convert the virtual base address
    EFI_STATUS status = gST->RuntimeServices->ConvertPointer(0, (void**)&mBiosBaseAddress);
    ASSERT_EFI_ERROR(status);
}

#endif

EFI_STATUS
EFIAPI
BiosDeviceRuntimeLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
    EFI_STATUS status = EFI_SUCCESS;

    SetupBaseAddress();

#if _USING_BIOS_MMIO_

    status = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,
                              TPL_NOTIFY,
                              BiosDeviceLibAddressChangeHandler,
                              NULL,
                              &gEfiEventVirtualAddressChangeGuid,
                              &mVirtualAddressChangeEvent);
    ASSERT_EFI_ERROR(status);

#endif

    return status;
}

EFI_STATUS
EFIAPI
BiosDeviceRuntimeLibDestructor(
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{

#if _USING_BIOS_MMIO_

  if (mVirtualAddressChangeEvent != NULL) {
    gBS->CloseEvent (mVirtualAddressChangeEvent);
  }
#endif

  return EFI_SUCCESS;
}
