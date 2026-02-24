/** @file
  Platform specific TPM2 component for configuring the Platform Hierarchy.

  Copyright (c) 2017 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

// MU_CHANGE - START refactor Tcg2PlatformDxe to lock TPM at EndOfDxe or ReadyToBoot depending on boot mode.

#include <PiDxe.h>

#include <Library/DebugLib.h>
// #include <Library/HobLib.h>                                                   // MS_HYP_CHANGE
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/TpmPlatformHierarchyLib.h>
// #include <Protocol/DxeSmmReadyToLock.h>                                       // MS_HYP_CHANGE

/**
   This callback function will run at EndOfDxe or ReadyToBoot based on boot mode.

   Configuration of the TPM's Platform Hierarchy Authorization Value (platformAuth)
   and Platform Hierarchy Authorization Policy (platformPolicy) can be defined through this function.

  @param  Event   Pointer to this event
  @param  Context Event handler private data
 **/
VOID
EFIAPI
TpmReadyToLockEventCallBack (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  DEBUG ((DEBUG_INFO, "[%a] Disabling TPM Platform Hierarchy\n", __func__));
  ConfigureTpmPlatformHierarchy ();

  gBS->CloseEvent (Event);
}

/**
   The driver's entry point. Will register a function for callback during ReadyToBoot event to
   configure the TPM's platform authorization.

   @param[in] ImageHandle  The firmware allocated handle for the EFI image.
   @param[in] SystemTable  A pointer to the EFI System Table.

   @retval EFI_SUCCESS     The entry point is executed successfully.
   @retval other           Some error occurs when executing this entry point.
**/
EFI_STATUS
EFIAPI
Tcg2PlatformDxeEntryPoint (
  IN    EFI_HANDLE        ImageHandle,
  IN    EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS     Status;
  // MS_HYP_CHANGE BEGIN
  // EFI_BOOT_MODE  BootMode;
  // MS_HYP_CHANGE END
  EFI_EVENT      Event;

  // MS_HYP_CHANGE BEGIN
  // BootMode = GetBootModeHob ();

  // In flash update boot path, leave TPM Platform Hierarchy enabled until ReadyToBoot (which should never actually
  // occur, since capsule reset will occur first).
  // if (BootMode == BOOT_ON_FLASH_UPDATE) {

    //
    // Lock the TPM platform hierarchy at ReadyToBoot rather than EndOfDxe.
    // AziHsmDxe needs access to the TPM platform hierarchy during
    // DriverBindingStart, which executes after EndOfDxe. Moving the lock to
    // ReadyToBoot gives AziHsmDxe the window it needs. This cannot be done
    // from AziHsmDxe's DriverEntry because DriverEntry runs unconditionally
    // and cannot check for the presence of an AziHsm device, which would
    // cause TPM operations to execute unnecessarily on non-AziHsm VMs.
    //
    Status = EfiCreateEventReadyToBootEx (TPL_CALLBACK, TpmReadyToLockEventCallBack, NULL, &Event);
  // } else {
  //   // In all other boot paths, disable TPM Platform Hierarchy at EndOfDxe.
  //   Status = gBS->CreateEventEx (
  //                   EVT_NOTIFY_SIGNAL,
  //                   TPL_CALLBACK,
  //                   TpmReadyToLockEventCallBack,
  //                   NULL,
  //                   &gEfiEndOfDxeEventGroupGuid,
  //                   &Event
  //                   );
  // }
  // MS_HYP_CHANGE END

  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

// MU_CHANGE - END refactor Tcg2PlatformDxe to lock TPM at EndOfDxe or ReadyToBoot depending on boot mode.
