/** @file
  Platform Console routines for showing the Hyper-V diagnostic console

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/BootEventLogLib.h>
#include <Protocol/SimpleNetwork.h>
#include <Protocol/BlockIo.h>
#include <Protocol/EventLog.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/PrintLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include "PlatformConsole.h"
#include "String.h"

//
// Convenient utility function implemented in BdsConsole.c
//
EFI_STATUS
ConvertBmpToGopBlt (
  IN     VOID      *BmpImage,
  IN     UINTN     BmpImageSize,
  IN OUT VOID      **GopBlt,
  IN OUT UINTN     *GopBltSize,
     OUT UINTN     *PixelHeight,
     OUT UINTN     *PixelWidth
  );

EFI_STATUS
SetStringEntry (
   EFI_STRING_ID IdName,
   CHAR16 *StringValue
   );

//
// Based on the string "Network Adapter (0123456789ABC)"
// Size in bytes, not characters
//
#define DEVICE_STRING_SIZE    (32 * sizeof(CHAR16))

// Vertical and horizontal padding for the logo
// Note: spacing between the bottom of the logo and the line
//       is controlled by HEADER_LINE_V_PADDING
#define HEADER_LOGO_V_PADDING     8
#define HEADER_LOGO_H_PADDING     10

// Vertical and horizontal padding for the horizontal line
#define HEADER_LINE_V_PADDING     4
#define HEADER_LINE_H_PADDING     3

#define HEADER_LINE_WIDTH         3

extern CHAR16                   *gStringBuffer;

//
// Cached logo information and other state.
//
EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *mLogoBlt = NULL;
UINTN                           mImageHeight;
UINTN                           mImageWidth;
INT32                           mSavedConsoleMode;
BOOLEAN                         OutOfSpace = FALSE;
EFI_STRING_ID                   BootSummaryStringIds[] = {                              // Max 4 error entries, plus 1 reserved
    STRING_TOKEN(STR_BOOT_SUMMARY_DEVICE_1), STRING_TOKEN(STR_BOOT_SUMMARY_ERROR_1),
    STRING_TOKEN(STR_BOOT_SUMMARY_DEVICE_2), STRING_TOKEN(STR_BOOT_SUMMARY_ERROR_2),
    STRING_TOKEN(STR_BOOT_SUMMARY_DEVICE_3), STRING_TOKEN(STR_BOOT_SUMMARY_ERROR_3),
    STRING_TOKEN(STR_BOOT_SUMMARY_DEVICE_4), STRING_TOKEN(STR_BOOT_SUMMARY_ERROR_4),
    STRING_TOKEN(STR_BOOT_SUMMARY_DEVICE_5), STRING_TOKEN(STR_BOOT_SUMMARY_ERROR_5),    // Reserved
};
UINT32                          BootSummaryEntries = sizeof(BootSummaryStringIds) / (2*sizeof(EFI_STRING_ID));
UINT32                          MaxAllowedErrorEntries = sizeof(BootSummaryStringIds) / (2*sizeof(EFI_STRING_ID)) - 1;  // Last Boot Summary entry is reserved

CHAR16*
PlatformConsoleDeviceStatusString(
    IN  BOOT_DEVICE_STATUS  Status
    )
/*++

Routine Description:

    Utility function to convert a BOOT_DEVICE_STATUS into a string
    Currently there are two returned strings that are actually format strings
    - BootDeviceReturnedFailure
    - Unknown BOOT_DEVICE_STATUS

Arguments:

    Status      Status value to convert.

Return Value:

    String representation of BOOT_DEVICE_STATUS on success
    NULL on failure.

--*/
{
    switch (Status)
    {
    case BootPending:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_BOOT_PENDING));
    case BootDeviceNoFilesystem:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_BOOT_NO_FILESYSTEM));
    case BootDeviceNoLoader:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_BOOT_NO_LOADER));
    case BootDeviceIncompatibleLoader:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_BOOT_IMAGE_INVALID));
    case BootDeviceReturnedFailure:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_BOOT_LOADER_FAILED_FORMAT));
    case BootDeviceOsNotLoaded:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_BOOT_NO_OS_LOADED));
    case BootDeviceOsLoaded:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_BOOT_OS_LOADED));
    case BootDeviceNoDevices:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_BOOT_NO_DEVICES));
    case BootDeviceLoadError:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_BOOT_IMAGE_LOAD_ERROR));
    case SecureBootFailed:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_IMAGE_VERIFY_FAILED));
    case SecureBootPolicyDenied:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_IMAGE_POLICY_DENIED));
    case SecureBootHashDenied:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_IMAGE_HASH_DENIED));
    case SecureBootCertDenied:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_IMAGE_CERT_DENIED));
    case SecureBootInvalidImage:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_IMAGE_INVALID));
    case SecureBootUnsignedHashNotInDb:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_IMAGE_UNSIGNED_HASH_NOT_FOUND));
    case SecureBootSignedHashNotFound:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_IMAGE_SIGNED_HASH_NOT_FOUND));
    case SecureBootNeitherCertNorHashInDb:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_IMAGE_HASH_CERT_NOT_FOUND));
    case NetworkBootMediaDisconnected:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_NET_MEDIA_DISCONNECT));
    case NetworkBootDhcpFailed:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_NET_DHCP_FAILED));
    case NetworkBootNoResponse:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_NET_NO_RESPONSE));
    case NetworkBootBufferTooSmall:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_NET_BUFFER_TOO_SMALL));
    case NetworkBootDeviceError:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_NET_DEVICE_ERROR));
    case NetworkBootNoResources:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_NET_NO_RESOURCES));
    case NetworkBootServerTimeout:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_NET_SERVER_TIMEOUT));
    case NetworkBootCancelled:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_NET_CANCELLED));
    case NetworkBootIcmpError:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_NET_ICMP_ERROR));
    case NetworkBootTftpError:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_NET_TFTP_ERROR));
    case NetworkBootNoBootFile:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_NET_NO_BOOT_FILE));
    case NetworkBootUnexpectedFailure:
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_NET_UNEXPECTED_FAILURE));
    default:
        //
        // This indicates that a device status was added but this function was not updated.
        //
        ASSERT(FALSE);
        return PlatformStringById(STRING_TOKEN(STR_DEVSTATUS_UNKNOWN_STATUS_FORMAT));
    }
}


CHAR16*
PlatformConsoleNetDeviceName(
    IN  EFI_DEVICE_PATH_PROTOCOL    *DevicePath,
    IN  MAC_ADDR_DEVICE_PATH        *MacPath
    )
/*++

Routine Description:

    Converts a network device path into a friendly string

Arguments:

    DevicePath      Full device path to convert

    MacPath         MAC address portion of the above device path


Return Value:

    Friendly string for the device
    NULL on failure.

--*/
{
    EFI_SIMPLE_NETWORK_PROTOCOL *snp          = NULL;
    EFI_MAC_ADDRESS             *macAddress   = NULL;
    CHAR16                      *stringBuffer = NULL;
    EFI_HANDLE handle = NULL;
    EFI_STATUS status = EFI_SUCCESS;

    stringBuffer = AllocateZeroPool(DEVICE_STRING_SIZE);

    if (stringBuffer == NULL)
    {
        goto Exit;
    }

    //
    // Hyper-V NIC device paths do not contain the MAC address, so
    // get the SNP protocol to get the current MAC. Fallback to the
    // device path if any error occurs.
    //
    macAddress   = &MacPath->MacAddress;

    status = gBS->LocateDevicePath(&gEfiSimpleNetworkProtocolGuid, &DevicePath, &handle);

    if (!EFI_ERROR(status))
    {
        status = gBS->OpenProtocol(handle,
                      &gEfiSimpleNetworkProtocolGuid,
                      (VOID **) &snp,
                      NULL,
                      NULL,
                      EFI_OPEN_PROTOCOL_GET_PROTOCOL);

        if (!EFI_ERROR(status))
        {
            macAddress = &snp->Mode->CurrentAddress;
        }
    }

    //
    // IfType of 0 or 1 indicate a 6 byte MAC address
    // (unfortunately this isn't defined in a header file).
    // Hyper-V doesn't support other types of MAC addresses
    // so this should always be the IfType.
    // Either way only six bytes are printed.
    //
    ASSERT(MacPath->IfType < 2);

    PlatformStringPrintSById(stringBuffer, DEVICE_STRING_SIZE, STRING_TOKEN(STR_NET_DEVICE_FORMAT),
        macAddress->Addr[0], macAddress->Addr[1], macAddress->Addr[2],
        macAddress->Addr[3], macAddress->Addr[4], macAddress->Addr[5]);

Exit:
    return stringBuffer;
}


CHAR16*
PlatformConsoleScsiDeviceName(
    IN  EFI_DEVICE_PATH_PROTOCOL    *DevicePath,
    IN  SCSI_DEVICE_PATH            *ScsiPath
    )
/*++

Routine Description:

    Converts a SCSI device path into a friendly string

Arguments:

    DevicePath      Full device path to convert

    ScsiPath        SCSI portion of the above device path

Return Value:

    Friendly string for the device
    NULL on failure.

--*/
{
    EFI_BLOCK_IO_PROTOCOL  *blkIo        = NULL;
    CHAR16                 *stringBuffer = NULL;
    EFI_STRING_ID  formatString = (EFI_STRING_ID)-1;
    EFI_HANDLE     handle = NULL;
    EFI_STATUS     status = EFI_SUCCESS;

    stringBuffer = AllocateZeroPool(DEVICE_STRING_SIZE);

    if (stringBuffer == NULL)
    {
        goto Exit;
    }

    status = gBS->LocateDevicePath(&gEfiBlockIoProtocolGuid, &DevicePath, &handle);

    if (!EFI_ERROR(status))
    {
        status = gBS->OpenProtocol(handle,
                    &gEfiBlockIoProtocolGuid,
                    (VOID **) &blkIo,
                    NULL,
                    NULL,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL);

        if (!EFI_ERROR(status))
        {
            if (blkIo->Media->RemovableMedia)
            {
                formatString = STRING_TOKEN(STR_SCSI_DVD_FORMAT);
            }
            else
            {
                formatString = STRING_TOKEN(STR_SCSI_DISK_FORMAT);
            }
        }
    }

    if (formatString == (EFI_STRING_ID)-1)
    {
        formatString = STRING_TOKEN(STR_SCSI_DEVICE_FORMAT);
    }

    PlatformStringPrintSById(stringBuffer,
        DEVICE_STRING_SIZE,
        formatString,
        ScsiPath->Pun,
        ScsiPath->Lun);

Exit:

    return stringBuffer;
}


CHAR16*
PlatformConsoleUnknownDeviceName(
    IN  const BOOTEVENT_DEVICE_ENTRY   *Event
    )
/*++

Routine Description:

    No friendly string could be created so use the option name stored in NVRAM
    This is relatively slow but should be an uncommon occurrence

Arguments:

    Event       Event to try and get the NVRAM description for

Return Value:

    NVRAM description string on success.
    NULL on failure.

--*/
{
    //BDS_COMMON_OPTION   *bootOption   = NULL;
    CHAR16              *friendlyName = NULL;
    //CHAR16     entryName[sizeof("Boot####")];
    //LIST_ENTRY list;

    friendlyName = PlatformStringById(STRING_TOKEN(STR_UNKNOWN_DEVICE));

    /*
    InitializeListHead(&list);
    UnicodeSPrint (entryName, sizeof (entryName), L"Boot%04x", Event->BootVariableNumber);
    bootOption = BdsLibVariableToOption(&list, entryName);

    if ((bootOption != NULL) &&
        (bootOption->Description != NULL))
    {
        friendlyName = bootOption->Description;
    }
    else
    {
        friendlyName = PlatformStringById(STRING_TOKEN(STR_UNKNOWN_DEVICE));
    }

    if (bootOption != NULL)
    {
        //
        // N.B.
        //   bootOption->Description is explicitly not freed as
        //   the allocated string will be returned to the caller.
        //
        gBS->FreePool(bootOption->DevicePath);
        gBS->FreePool(bootOption->LoadOptions);
        gBS->FreePool(bootOption);
    }
    */

    return friendlyName;
}


CHAR16*
PlatformConsoleDeviceNameString(
    IN  const BOOTEVENT_DEVICE_ENTRY   *Event
    )
/*++

Routine Description:

    Retrieves the friendly device string for the given event

Arguments:

    Event

Return Value:

    Device string on success.
    NULL on failure.

--*/
{
    EFI_DEVICE_PATH_PROTOCOL *devicePath;
    EFI_DEVICE_PATH_PROTOCOL *curPathNode;
    CHAR16                   *finishedString = NULL;
    //
    //   Map the device status to a detailed string.
    //   The name in the NVRAM boot entry is not used.
    //
    //     SCSI       "SCSI Disk (x,y)"
    //     DVD        "SCSI DVD  (x,y)"
    //                "SCSI Device (x,y)"
    //     Network    "Network Adapter (0123456789ABC)"
    //     File Path  <Same as SCSI device>
    //     Other      "Unknown Device"
    //
    // Typical Hyper-V device paths look like:
    //   ACPI()\VMBUS (disk)\SCSI (Disk & DVD)
    //   ACPI()\VMBUS (NIC)\MAC
    //
    devicePath = (EFI_DEVICE_PATH_PROTOCOL*)Event->DevicePath;

    if ((devicePath != NULL) &&
        (Event->DevicePathSize > sizeof(EFI_DEVICE_PATH_PROTOCOL)))
    {
        //
        // Traverse the device path to find the nodes that can be formatted
        //
        for (curPathNode = devicePath;
             !IsDevicePathEnd(curPathNode);
             (curPathNode = NextDevicePathNode(curPathNode)))
        {
            UINT8 subType = DevicePathSubType(curPathNode);
            if (DevicePathType(curPathNode) != MESSAGING_DEVICE_PATH)
            {
                continue;
            }

            if (subType == MSG_SCSI_DP)
            {
                finishedString = PlatformConsoleScsiDeviceName(devicePath, (SCSI_DEVICE_PATH*)curPathNode);
                break;
            }
            else if (subType == MSG_MAC_ADDR_DP)
            {
                finishedString = PlatformConsoleNetDeviceName(devicePath, (MAC_ADDR_DEVICE_PATH*)curPathNode);
                break;
            }
        }
    }

    //
    // If no friendly string was created try to fallback.
    //
    if (finishedString == NULL)
    {
        finishedString = PlatformConsoleUnknownDeviceName(Event);
    }

    return finishedString;
}


BOOLEAN
PlatformConsoleEventCallback(
    IN  VOID                            *Context,
    IN  const EFI_EVENT_DESCRIPTOR      *Metadata,
    IN  const BOOTEVENT_DEVICE_ENTRY    *Event
    )
/*++

Routine Description:

    Event enumeration callback.  Parses the boot device event and
    displays a friendly string on the console.

Arguments:

    Context         Event entry number.  This is incremented by this function
                    for each BOOT_DEVICE_EVENT_ID event that is processed.

    Metadata        Metadata for the current event

    Event           Event data

Return Value:

    TRUE

--*/
{
    CHAR16  *friendlyName = NULL;
    CHAR16  *statusString = NULL;
    UINT32  *entryNumber = ((UINT32 *)Context);     // *entryNumber is a 1-based counter

    if (*entryNumber > MaxAllowedErrorEntries)
    {
        OutOfSpace = TRUE;
        return FALSE;
    }

    if (Metadata->EventId != BOOT_DEVICE_EVENT_ID)
    {
        return TRUE;
    }

    friendlyName = PlatformConsoleDeviceNameString(Event);

    statusString = PlatformConsoleDeviceStatusString(Event->Status);

    //
    // Skip the friendly name and number for BootDeviceNoDevices
    // This makes the output look nice.
    //
    if (Event->Status == BootDeviceNoDevices)
    {
        //PlatformStringPrint(L"%s\n", statusString);

        if (PlatformStringPrint(L"%s", statusString) && gStringBuffer != NULL)
        {
            SetStringEntry(BootSummaryStringIds[(*entryNumber - 1)*2], gStringBuffer);
        }
    }
    else
    {
        //PlatformStringPrint(L"%2d. %-32s ", *entryNumber, friendlyName);
        //PlatformStringPrint(statusString, Event->ExtendedStatus);
        //PlatformStringPrint(L"\n");

        if (PlatformStringPrint(L"%2d. %s", *entryNumber, friendlyName) && gStringBuffer != NULL)
        {
            SetStringEntry(BootSummaryStringIds[(*entryNumber - 1)*2], gStringBuffer);
        }

        if (PlatformStringPrint(L"        %s\n", statusString, Event->ExtendedStatus) && gStringBuffer != NULL)
        {
            SetStringEntry(BootSummaryStringIds[(*entryNumber - 1)*2 + 1], gStringBuffer);
        }

    }

    *entryNumber = *entryNumber + 1;

    gBS->FreePool(friendlyName);
    gBS->FreePool(statusString);
    return TRUE;
}


VOID
PlatformConsoleBootSummary(
    IN  EFI_STRING_ID   PromptId
    )
/*++

Routine Description:

    Displays the boot summary followed by an optional prompt string
    then waits for a keypress before returning.

Arguments:

    Id      STRING_TOKEN for the prompt
            -1 if not needed.

Return Value:

    None.

--*/
{
    EVENT_CHANNEL_STATISTICS stats;
    UINT32         eventCount = 1;

    ZeroMem(&stats, sizeof(stats));
    BootDeviceEventStatistics(&stats);

    //
    // Enumerate and display the current boot entries.
    //
    BootDeviceEventEnumerate(PlatformConsoleEventCallback, &eventCount);

    //
    // Notify the user if any boot event entries were lost because the
    // event log was full.
    //
    if (eventCount <= BootSummaryEntries && (stats.Lost > 0 || OutOfSpace))
    {
        SetStringEntry(BootSummaryStringIds[(eventCount - 1)*2], GetStringById(STRING_TOKEN(STR_BOOT_LOST_EVENT_FORMAT)));
        eventCount++;
    }
}


EFI_STATUS
PlatformConsoleInitialize()
/*++

Routine Description:

    Initializes the plaform console for use.

Arguments:

    None.

Return Value:

    EFI_SUCCESS

--*/
{
    return PlatformStringInitialize();
}

