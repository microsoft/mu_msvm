/** @file
  Library for accessing system settings for MsBootPolicy.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Uefi.h>                                     // UEFI base types
#include <Library/DebugLib.h>                         // DEBUG tracing
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MsBootPolicyLib.h>

/**
  Print the device path
*/
static VOID PrintDevicePath(IN EFI_DEVICE_PATH_PROTOCOL *DevicePath) {
    CHAR16                                  *ToText = NULL;

    if (DevicePath != NULL) {
        ToText = ConvertDevicePathToText (DevicePath, TRUE, TRUE);
    }
    DEBUG((DEBUG_INFO,"%s",ToText)); // Output NewLine separately in case string is too long
    DEBUG((DEBUG_INFO,"\n"));

    if (NULL != ToText) {
        FreePool(ToText);
    }
    return;
}

/**

 *Ask if the platform allows booting this controller

 *@retval TRUE     Device is not excluded from booting
 *@retval FALSE    Device is excluded from booting.
**/
BOOLEAN
EFIAPI
MsBootPolicyLibIsDevicePathBootable(
    EFI_DEVICE_PATH_PROTOCOL *DevicePath
    ) {
    BOOLEAN                             rc = TRUE;
    EFI_DEVICE_PATH_PROTOCOL           *Node;


    DEBUG((DEBUG_INFO,__FUNCTION__ "  Checking if the following device path is permitted to boot:\n"));

    if (NULL == DevicePath) {
        DEBUG((DEBUG_ERROR,"NULL device path\n"));
        return TRUE;        // Don't know where this device is, so it is not "excluded"
    }

#ifdef EFI_DEBUG
#define MAX_DEVICE_PATH_SIZE 0x100000  // Arbitrary 1 MB max device path size.
#else
#define MAX_DEVICE_PATH_SIZE 0         // Don't check length on retail builds.
#endif

    PrintDevicePath(DevicePath);
    if (!IsDevicePathValid(DevicePath,MAX_DEVICE_PATH_SIZE)) {
        DEBUG((DEBUG_ERROR,"Invalid device path\n"));
        return FALSE;
    }


    //
    // 1. Check for USB devices (USB devices are also External Devices)
    //
    Node = DevicePath;
    while (!IsDevicePathEnd(Node)) {
        if (MESSAGING_DEVICE_PATH == Node->Type) {       // If any type of USB device
            if ((MSG_USB_DP       == Node->SubType) ||   // don't allow booting
                (MSG_USB_WWID_DP  == Node->SubType) ||
                (MSG_USB_CLASS_DP == Node->SubType)) {
                rc = FALSE;
                break;
            }
        }
        Node = NextDevicePathNode (Node);
    }


    if (rc) {
        DEBUG((DEBUG_INFO,"Boot from this device is enabled\n"));
    } else {
        DEBUG((DEBUG_ERROR,"Boot from this device has been prevented\n"));
    }
    return rc;
}

/**

 *Ask if the platform allows booting this controller

 *@retval TRUE     System is requesting Alternate Boot
 *@retval FALSE    System is not requesting AltBoot.
**/
BOOLEAN
EFIAPI
MsBootPolicyLibIsDeviceBootable (
    EFI_HANDLE   ControllerHandle
    ) {

    return MsBootPolicyLibIsDevicePathBootable (DevicePathFromHandle (ControllerHandle));
}


/**
 *Ask if the platform is requesting Settings Change

 *@retval TRUE     System is requesting Settings Change
 *@retval FALSE    System is not requesting Changes.
**/
BOOLEAN
EFIAPI
MsBootPolicyLibIsSettingsBoot (
  VOID
)
{
  return FALSE;
}

/**
 *Ask if the platform is requesting an Alternate Boot

 *@retval TRUE     System is requesting Alternate Boot
 *@retval FALSE    System is not requesting AltBoot.
**/
BOOLEAN
EFIAPI
MsBootPolicyLibIsAltBoot (
  VOID
)
{
  return FALSE;
}

/**
 *Ask if the platform is requesting an Alternate Boot

 *@retval EFI_SUCCESS  BootSequence pointer returned
 *@retval Other        Error getting boot sequence

 BootSequence is assumed to be a pointer to constant data, and
 is not freed by the caller.

**/
EFI_STATUS
EFIAPI
MsBootPolicyLibGetBootSequence (
  BOOT_SEQUENCE **BootSequence,
  BOOLEAN         AltBootRequest
)
{
  return EFI_UNSUPPORTED;
}

/**
 *Clears the boot requests for settings or Alt boot

 *@retval EFI_SUCCESS  Boot requests cleared
 *@retval Other        Error clearing requests

**/
EFI_STATUS
EFIAPI
MsBootPolicyLibClearBootRequests(
   VOID
)
{
  return EFI_SUCCESS;
}
