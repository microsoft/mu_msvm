/** @file
  Device Boot Manager  - Device Extensions to BdsDxe.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Uefi.h>

#include <Protocol/Emcl.h>
#include <Protocol/LoadFile.h>
#include <Protocol/SimpleFileSystem.h>

#include <Library/BaseMemoryLib.h>
#include <Library/BiosDeviceLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceBootManagerLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/EmclLib.h>
#include <Library/PcdLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MsBootPolicyLib.h>
#include <Library/MsLogoLib.h>
#include <Library/MsPlatBdsLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <BiosInterface.h>

#include <VirtualDeviceId.h>

//
// Predefined platform default console device path
//
static BDS_CONSOLE_CONNECT_ENTRY   gPlatformConsoles[] = {
  //
  // Place holder for serial console. Any non USB device used for
  // CONIN must be in this table.  Any non display used for CONOUT
  // must also be in this list.
  //
  {
    NULL,
    CONSOLE_IN
  },
  {
    NULL,
    0
  }
};


/**
 * Constructor   - This runs when BdsDxe is loaded, before BdsArch protocol is published
 *
 * @return EFI_STATUS
 */
EFI_STATUS
EFIAPI
DeviceBootManagerConstructor (
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
  ) {

    return EFI_SUCCESS;
}

/**
  OnDemandConInCOnnect
 */
EFI_DEVICE_PATH_PROTOCOL **
EFIAPI
DeviceBootManagerOnDemandConInConnect (
  VOID
  ) {

    return NULL;
}

/**
  Do the device specific action at start of BdsEntry (callback into BdsArch from DXE Dispatcher)
**/
VOID
EFIAPI
DeviceBootManagerBdsEntry (
  VOID
  ) {
   PlatformBdsInit();
}

//Device path filter routines
typedef
BOOLEAN
(EFIAPI *FILTER_ROUTINE)(
                         IN  EFI_DEVICE_PATH_PROTOCOL *DevicePath);

BOOLEAN CheckDeviceNodeEx(
    EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    UINT8  Type,
    UINT8  SubType) {
    while (!IsDevicePathEndType(DevicePath)) {
        if ((DevicePathType(DevicePath) == Type) &&
            (DevicePathSubType(DevicePath) == SubType)) {
            return TRUE;
        }
        DevicePath = NextDevicePathNode(DevicePath);
    }
    return FALSE;
}

BOOLEAN CheckDeviceNode(
    EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    UINT8  Type) {
    while (!IsDevicePathEndType(DevicePath)) {
        if (DevicePathType(DevicePath) == Type) {
            return TRUE;
        }
        DevicePath = NextDevicePathNode(DevicePath);
    }
    return FALSE;
}

BOOLEAN IsDevicePathUSB(EFI_DEVICE_PATH_PROTOCOL *DevicePath) {
    return CheckDeviceNodeEx(DevicePath, MESSAGING_DEVICE_PATH, MSG_USB_DP);
}

BOOLEAN IsDevicePathMedia(EFI_DEVICE_PATH_PROTOCOL *DevicePath) {
    return CheckDeviceNode(DevicePath, MEDIA_DEVICE_PATH);
}

BOOLEAN IsDevicePathIPv4(EFI_DEVICE_PATH_PROTOCOL *DevicePath) {
    return CheckDeviceNodeEx(DevicePath, MESSAGING_DEVICE_PATH, MSG_IPv4_DP);
}

BOOLEAN IsDevicePathIPv6(EFI_DEVICE_PATH_PROTOCOL *DevicePath) {
    return CheckDeviceNodeEx(DevicePath, MESSAGING_DEVICE_PATH, MSG_IPv6_DP);
}

BOOLEAN FilterNoUSB(EFI_DEVICE_PATH_PROTOCOL *DevicePath) {
    return (FALSE == IsDevicePathUSB(DevicePath));
}

BOOLEAN FilterOnlyMedia(EFI_DEVICE_PATH_PROTOCOL *DevicePath) {
    return (TRUE == IsDevicePathMedia(DevicePath));
}

BOOLEAN FilterOnlyIPv4(EFI_DEVICE_PATH_PROTOCOL *DevicePath) {
    return (TRUE == IsDevicePathIPv4(DevicePath));
}

BOOLEAN FilterOnlyIPv6(EFI_DEVICE_PATH_PROTOCOL *DevicePath) {
    return (TRUE == IsDevicePathIPv6(DevicePath));
}

BOOLEAN FilterOnlyUSB(EFI_DEVICE_PATH_PROTOCOL *DevicePath) {
    return (TRUE == IsDevicePathUSB(DevicePath));
}

VOID FilterHandles(EFI_HANDLE *HandleBuffer, UINTN *HandleCount, FILTER_ROUTINE KeepHandleFilter) {
    UINTN                     Index;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    EFI_STATUS                Status;

    for (Index = 0; Index < *HandleCount;) {

        Status = gBS->HandleProtocol(
            HandleBuffer[Index],
            &gEfiDevicePathProtocolGuid,
            (VOID **)&DevicePath
            );

        if (EFI_ERROR(Status) ||            // Remove handles that don't have device path
            (!KeepHandleFilter(DevicePath))) {  // TRUE keeps handle, FALSE deletes handle
            (*HandleCount)--;
            CopyMem(&HandleBuffer[Index], &HandleBuffer[Index + 1], (*HandleCount-Index) * sizeof(EFI_HANDLE *));
            continue;
        }
        Index++;
    }
    return;
}

BOOLEAN CompareDevicePathAgtB(
    EFI_DEVICE_PATH_PROTOCOL *DevicePathA,
    EFI_DEVICE_PATH_PROTOCOL *DevicePathB) {
    UINTN            LengthA;
    UINTN            LengthB;
    UINTN            CompareLength;
    INTN             Result = 0;
    USB_DEVICE_PATH *UsbPathA;
    USB_DEVICE_PATH *UsbPathB;
    PCI_DEVICE_PATH *PciPathA;
    PCI_DEVICE_PATH *PciPathB;
    NVME_NAMESPACE_DEVICE_PATH *NvmePathA;
    NVME_NAMESPACE_DEVICE_PATH *NvmePathB;

    LengthA = GetDevicePathSize(DevicePathA);
    LengthB = GetDevicePathSize(DevicePathB);

    CompareLength = LengthA;  //CompareLength=MIN(LengthA,LengthB);
    if (LengthB < LengthA) {
        CompareLength = LengthB;
    }

    while (!IsDevicePathEnd(DevicePathA) &&
           !IsDevicePathEnd(DevicePathB)) {
        Result = CompareMem(DevicePathA, DevicePathB, DevicePathNodeLength(DevicePathA));
        if (Result != 0) {
            // Note - Device paths are not sortable in binary.  Node fields are sortable,
            //        but may not in the proper order in memory. Only some node types are
            //        of interest at this time.  All others can use a binary compare for now.
            if ((DevicePathType(DevicePathA) == DevicePathType(DevicePathB)) &&
                (DevicePathSubType(DevicePathA) == DevicePathSubType(DevicePathB))) {
                switch (DevicePathType(DevicePathA)) {

                case HARDWARE_DEVICE_PATH:
                    switch (DevicePathSubType(DevicePathA)) {
                    case HW_PCI_DP:
                        PciPathA = (PCI_DEVICE_PATH *)DevicePathA;
                        PciPathB = (PCI_DEVICE_PATH *)DevicePathB;
                        Result = PciPathA->Device - PciPathB->Device;
                        if (Result == 0) {
                            Result = PciPathA->Function - PciPathB->Function;
                        }
                        break;
                    default:
                        Result = CompareMem(DevicePathA, DevicePathB, DevicePathNodeLength(DevicePathA));
                    }
                    break;

                case  MESSAGING_DEVICE_PATH:
                    switch (DevicePathSubType(DevicePathA)) {
                    case MSG_USB_DP:
                        UsbPathA = (USB_DEVICE_PATH *)DevicePathA;
                        UsbPathB = (USB_DEVICE_PATH *)DevicePathB;
                        Result = UsbPathA->InterfaceNumber - UsbPathB->InterfaceNumber;
                        if (Result == 0) {
                            Result = UsbPathA->ParentPortNumber - UsbPathB->ParentPortNumber;
                        }
                        break;
                    case MSG_NVME_NAMESPACE_DP:
                        NvmePathA = (NVME_NAMESPACE_DEVICE_PATH*)DevicePathA;
                        NvmePathB = (NVME_NAMESPACE_DEVICE_PATH*)DevicePathB;
                        Result = NvmePathA->NamespaceId - NvmePathB->NamespaceId;
                        break;

                    default:
                        Result = CompareMem(DevicePathA, DevicePathB, DevicePathNodeLength(DevicePathA));
                    }
                    break;

                default:
                    Result = CompareMem(DevicePathA, DevicePathB, DevicePathNodeLength(DevicePathA));
                    break;
                }
            } else {
                Result = CompareMem(DevicePathA, DevicePathB, CompareLength);
            }
            if (Result != 0) {
                break;   // Exit while loop.
            }
        }
        DevicePathA = NextDevicePathNode(DevicePathA);
        DevicePathB = NextDevicePathNode(DevicePathB);
        LengthA = GetDevicePathSize(DevicePathA);
        LengthB = GetDevicePathSize(DevicePathB);

        CompareLength = LengthA;  //CompareLength=MIN(LengthA,LengthB);
        if (LengthB < LengthA) {
            CompareLength = LengthB;
        }
    }

    return (Result >= 0);
}

VOID DisplayDevicePaths(EFI_HANDLE *HandleBuffer, UINTN HandleCount) {
    UINTN   i;
    UINT16  *Tmp;
    for (i = 0; i < HandleCount; i++) {
        Tmp = ConvertDevicePathToText (DevicePathFromHandle(HandleBuffer[i]),TRUE,TRUE);
        if (Tmp != NULL) {
          DEBUG((DEBUG_INFO, "%3d %s", i, Tmp)); // Output newline in different call as
        } else {
          DEBUG((DEBUG_INFO, "%3d NULL\n", i));
        }
        DEBUG((DEBUG_INFO,"\n"));  //Device Paths can be longer than DEBUG limit.
        if (Tmp != NULL) {
            FreePool(Tmp);
        }
    }
}

VOID SortHandles(EFI_HANDLE *HandleBuffer, UINTN HandleCount) {
    UINTN                     Index;
    EFI_DEVICE_PATH_PROTOCOL *DevicePathA;
    EFI_DEVICE_PATH_PROTOCOL *DevicePathB;
    EFI_HANDLE               *TempHandle;
    BOOLEAN                   Swap;
    UINTN                     SwapCount;

    DEBUG((DEBUG_INFO,"%a\n",__FUNCTION__));
    if (HandleCount < 2) {
        return;
    }
    SwapCount = 0;
    DEBUG((DEBUG_INFO,"SortHandles - Before sorting\n"));
    DisplayDevicePaths(HandleBuffer, HandleCount);
    do {
        Swap = FALSE;
        for (Index = 0; Index < (HandleCount - 1); Index++) {

            DevicePathA = DevicePathFromHandle(HandleBuffer[Index]);
            DevicePathB = DevicePathFromHandle(HandleBuffer[Index + 1]);

            if (CompareDevicePathAgtB(DevicePathA, DevicePathB)) {
                TempHandle = HandleBuffer[Index];
                HandleBuffer[Index] = HandleBuffer[Index + 1];
                HandleBuffer[Index + 1] = TempHandle;
                Swap = TRUE;
            }
        }
        if (Swap) {
            SwapCount++;
        }
    } while ((Swap == TRUE) && (SwapCount < 50));
    DEBUG((DEBUG_INFO,"SortHandles - After sorting\n"));
    DisplayDevicePaths(HandleBuffer, HandleCount);
    DEBUG((DEBUG_INFO,"Exit %a, swapcount = %d\n",__FUNCTION__,SwapCount));
    return;
}

EFI_STATUS SelectAndBootDevice(EFI_GUID *ByGuid, FILTER_ROUTINE ByFilter) {
    EFI_STATUS                   Status;
    EFI_HANDLE                  *Handles;
    UINTN                        HandleCount;
    UINTN                        Index;
    UINTN                        FlagSize;
    EFI_BOOT_MANAGER_LOAD_OPTION BootOption;
    CHAR16                      *TmpStr;
    EFI_DEVICE_PATH_PROTOCOL    *DevicePath;

    FlagSize = sizeof(UINTN);
    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        ByGuid,
        NULL,
        &HandleCount,
        &Handles
        );
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR,"Unable to locate any %g handles - code=%r\n",ByGuid,Status));
        return Status;
    }
    DEBUG((DEBUG_INFO,"Found %d handles\n",HandleCount));
    DisplayDevicePaths(Handles, HandleCount);
    FilterHandles(Handles, &HandleCount, ByFilter);
    DEBUG((DEBUG_INFO,"%d handles survived filtering\n",HandleCount));
    if (HandleCount == 0) {
        DEBUG((DEBUG_WARN, "No handles survived filtering!\n"));
        return EFI_NOT_FOUND;
    }

    SortHandles(Handles, HandleCount);

    Status = EFI_DEVICE_ERROR;
    for (Index = 0; Index < HandleCount; Index++) {
        DevicePath = DevicePathFromHandle(Handles[Index]);
        if (DevicePath == NULL) {
          DEBUG((DEBUG_ERROR, "DevicePathFromHandle(%p) FAILED\n", Handles[Index]));
          continue;
        }
        TmpStr = ConvertDevicePathToText (DevicePath,TRUE,TRUE);
        if (TmpStr == NULL) {
          DEBUG((DEBUG_ERROR,"ConvertDevicePathToText(%p) FAILED ",DevicePath));
          continue;
        }
        DEBUG((DEBUG_INFO,"Selecting device %s",TmpStr));
        DEBUG((DEBUG_INFO,"\n"));
        if (MsBootPolicyLibIsDeviceBootable(Handles[Index])) {
            EfiBootManagerInitializeLoadOption(
                &BootOption,
                LoadOptionNumberUnassigned,
                LoadOptionTypeBoot,
                LOAD_OPTION_ACTIVE,
                L"MsTemp",
                DevicePathFromHandle(Handles[Index]),
                NULL,
                0
                );

            EfiBootManagerBoot(&BootOption);
            Status = BootOption.Status;

            EfiBootManagerFreeLoadOption(&BootOption);
            // if EFI_SUCCESS, device was booted, and the return is back to setup
            if (Status == EFI_SUCCESS) {
                break;
            }
        } else {
            DEBUG((DEBUG_WARN,"Device %s\n",TmpStr));
            DEBUG((DEBUG_WARN," was blocked from booting\n"));
        }
        FreePool(TmpStr);
    }
    return Status;
}

/**
  Do the device specific action before the console is connected.

  Such as:
      Initialize the platform boot order
      Supply Console information
**/
EFI_HANDLE
EFIAPI
DeviceBootManagerBeforeConsole (
  EFI_DEVICE_PATH_PROTOCOL    **DevicePath,
  BDS_CONSOLE_CONNECT_ENTRY   **PlatformConsoles
  ) {
    EFI_STATUS               Status;
    EFI_HANDLE              *HandleBuffer;
    UINTN                    HandleCount;
    UINTN                    Index;
    EFI_HANDLE               ConsoleIn = NULL;
    EFI_HANDLE               ConsoleOut = NULL;

    *DevicePath = NULL;
    *PlatformConsoles = NULL;

    Status = gBS->LocateHandleBuffer (
       ByProtocol,
       &gEfiVmbusProtocolGuid,
       NULL,
       &HandleCount,
       &HandleBuffer
       );
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Handles with gEfiVmbusProtocolGuid not found. Status = %r\n", Status));
        goto Exit;
    }

    DEBUG((DEBUG_INFO, "Count of handles with gEfiVmbusProtocolGuid = %d\n", HandleCount));

    for (Index = 0; Index < HandleCount; Index++) {
        if (ConsoleIn == NULL) {
            Status = EmclChannelTypeSupported(HandleBuffer[Index],
                                              &gSyntheticKeyboardClassGuid,
                                              NULL);
            if (!EFI_ERROR(Status)) {
                ConsoleIn = HandleBuffer[Index];
            }
        }

        if (ConsoleOut == NULL) {
            Status = EmclChannelTypeSupported(HandleBuffer[Index],
                                              &gSyntheticVideoClassGuid,
                                              NULL);
            if (!EFI_ERROR(Status)) {
                ConsoleOut = HandleBuffer[Index];
            }
            else {
                Status = EmclChannelTypeSupported(HandleBuffer[Index],
                                                  &gSynthetic3dVideoClassGuid,
                                                  NULL);
                if (!EFI_ERROR(Status)) {
                    ConsoleOut = HandleBuffer[Index];
                }
            }
        }
    }

    if (ConsoleIn != NULL) {
        Status = gBS->HandleProtocol (
                        ConsoleIn,
                        &gEfiDevicePathProtocolGuid,
                        &(gPlatformConsoles[0].DevicePath)     // device path for ConIn
                        );
        if (EFI_ERROR (Status)) {
            DEBUG((DEBUG_ERROR, "Device Path on handle of Hyper-V keyboard device not found.  Status = %r\n", Status));
        }
    }
    else {
        DEBUG((DEBUG_ERROR, "Handle for Hyper-V keyboard device not found\n"));
    }

    if (ConsoleOut != NULL) {
        Status = gBS->HandleProtocol (
                        ConsoleOut,
                        &gEfiDevicePathProtocolGuid,
                        DevicePath                             // device path for ConOut
                        );
        if (EFI_ERROR (Status)) {
            ConsoleOut = NULL;
            DEBUG((DEBUG_ERROR, "Device Path on handle of Hyper-V video device not found.  Status = %r\n", Status));
        }
        else {
            *DevicePath = DuplicateDevicePath(*DevicePath);
            if (*DevicePath == NULL) {
                ConsoleOut = NULL;
            }
        }
    }
    else {
        DEBUG((DEBUG_ERROR, "Handle for Hyper-V video device not found\n"));
    }

    *PlatformConsoles = (BDS_CONSOLE_CONNECT_ENTRY *)&gPlatformConsoles;
    gBS->FreePool(HandleBuffer);

Exit:
    return ConsoleOut;
}

/**
  Do the device specific action after the console is connected.

  Such as:
**/
EFI_DEVICE_PATH_PROTOCOL **
EFIAPI
DeviceBootManagerAfterConsole (
  VOID
  ) {

    EnableQuietBoot(PcdGetPtr(PcdLogoFile));

    EFI_HANDLE  *HandleBuffer = NULL;
    UINTN        HandleCount;
    UINTN        Index;

    HandleBuffer = NULL;
    HandleCount = 0;
    //
    // Find all instances of VmbusRoot protocol.
    //
    gBS->LocateHandleBuffer( ByProtocol,
                            &gEfiVmbusRootProtocolGuid,
                             NULL,
                            &HandleCount,
                            &HandleBuffer );

    for (Index = 0; Index < HandleCount; Index++) {
        DEBUG((DEBUG_ERROR, "Connecting controller for handle %d \n", Index));
        gBS->ConnectController(HandleBuffer[Index], NULL, NULL, TRUE);
    }

    if (HandleBuffer != NULL) {
        gBS->FreePool (HandleBuffer);
    }

    return NULL;
}

/**
ProcessBootCompletion
*/
VOID
EFIAPI
DeviceBootManagerProcessBootCompletion (
  IN EFI_BOOT_MANAGER_LOAD_OPTION *BootOption
) {

    return;
}

/**
 * Check for HardKeys during boot.  If the hard keys are pressed, builds
 * a boot option for the specific hard key setting.
 *
 *
 * @param BootOption   - Boot Option filled in based on which hard key is pressed
 *
 * @return EFI_STATUS  - EFI_NOT_FOUND - no hard key pressed, no BootOption
 *                       EFI_SUCCESS   - BootOption is valid
 *                       other error   - Unable to build BootOption
 */
EFI_STATUS
EFIAPI
DeviceBootManagerPriorityBoot (
    EFI_BOOT_MANAGER_LOAD_OPTION   *BootOption
    ) {

    return EFI_NOT_FOUND;
}


/**
 This is called from BDS right before going into front page
 when no bootable devices/options found
*/
VOID
EFIAPI
DeviceBootManagerUnableToBoot (
  VOID
  ) {
    EFI_BOOT_MANAGER_LOAD_OPTION    BootManagerMenu;
    EFI_STATUS                      Status = EFI_DEVICE_ERROR;
    UINT16                          *BootOrder = NULL;
    UINTN                           BootOrderSize = 0;
    BOOLEAN                         AttemptDefaultBoot = FALSE;
    UINTN                           Index;
    EFI_BOOT_MANAGER_LOAD_OPTION    *BootOptions;
    UINTN                           BootOptionCount;
    CHAR16                          *DevicePathString;

    // Default boot has two triggers, either:
    //      No BootOrder variable exists
    //      PCD override that says to always attempt it, set in PEI

    if (PcdGetBool(PcdDefaultBootAlwaysAttempt)) {
        AttemptDefaultBoot = TRUE;
    }
    else {
        Status = GetEfiGlobalVariable2 (L"BootOrder", (VOID **) &BootOrder, &BootOrderSize);
        AttemptDefaultBoot = (Status == EFI_NOT_FOUND);
    }

    // Log the boot order if there is any
    if (BootOrderSize == 0) {
        DEBUG((DEBUG_INFO, "Boot order is empty\n"));
    } else {
        BootOptions = EfiBootManagerGetLoadOptions (&BootOptionCount, LoadOptionTypeBoot);
        DEBUG((DEBUG_INFO, "Boot order : \n"));
        for (Index = 0; Index < BootOrderSize / sizeof (UINT16); Index++) {
            DevicePathString = ConvertDevicePathToText (
                        BootOptions[Index].FilePath,
                        FALSE,
                        FALSE
                        );
            DEBUG((DEBUG_INFO, "Boot%04x Description: %s. Filepath: %s \n", BootOptions[Index].OptionNumber, BootOptions[Index].Description, DevicePathString));
            if (DevicePathString != NULL) {
                FreePool (DevicePathString);
            }
        }
    }

    if (AttemptDefaultBoot) {
        EfiBootManagerConnectAll();

        //Attempt HDD
        if (PcdGetBool(PcdIsVmbfsBoot)) {
            Status = SelectAndBootDevice(&gEfiSimpleFileSystemProtocolGuid, FilterNoUSB);
            DEBUG((DEBUG_INFO, "Attempted to boot from HDD with FilterNoUSB, SelectAndBootDevice returned %r\n", Status));
        }
        else {
            Status = SelectAndBootDevice(&gEfiSimpleFileSystemProtocolGuid, FilterOnlyMedia);
            DEBUG((DEBUG_INFO, "Attempted to boot from HDD with FilterOnlyMedia, SelectAndBootDevice returned %r\n", Status));
        }

        if(PcdGetBool(PcdDefaultBootAttemptPxe)) {
            // Set to low resolution VGA mode
            // TODO: This fails on Hyper-V, so don't do it here. Probably
            // because synth video doesn't support it?
            // Status = MsLogoLibSetConsoleMode(TRUE, FALSE);
            // if (EFI_ERROR(Status) != FALSE) {
            //     DEBUG((DEBUG_ERROR, "%a Unable to set console mode - %r\n", __FUNCTION__, Status));
            // }

            // Attempt PXE based on configured IP version
            if (PcdGetBool(PcdPxeIpV6))
            {
                //IPv6
                Status = SelectAndBootDevice(&gEfiLoadFileProtocolGuid, FilterOnlyIPv6);
                DEBUG((DEBUG_INFO, "Attempted to PXE boot from IPv6, SelectAndBootDevice returned %r\n", Status));
            }
            else {
                //IPv4
                Status = SelectAndBootDevice(&gEfiLoadFileProtocolGuid, FilterOnlyIPv4);
                DEBUG((DEBUG_INFO, "Attempted to PXE boot from IPv4, SelectAndBootDevice returned %r\n", Status));
            }

            //Reset to native resolution
            // Status = MsLogoLibSetConsoleMode(FALSE, FALSE);
            // if (EFI_ERROR(Status) != FALSE) {
            //     DEBUG((DEBUG_ERROR, "%a Unable to set console mode - %r\n", __FUNCTION__, Status));
            // }
        }
    }

    //
    // Tell the host to collect EFI diagnostics.
    //
    DEBUG((EFI_D_INFO, "Signaling BIOS device to collect EFI diagnostics...\n"));
    WriteBiosDevice(BiosConfigProcessEfiDiagnostics, TRUE);

    //
    // BootManagerMenu doesn't contain the correct information when return status is EFI_NOT_FOUND.
    //
    Status = EfiBootManagerGetBootManagerMenu (&BootManagerMenu);

    if(Status != EFI_NOT_FOUND) {
        for (;;) {
        EfiBootManagerBoot (&BootManagerMenu);
        }
    }
}
