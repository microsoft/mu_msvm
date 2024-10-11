/** @file
  Internal include file the Event Log Runtime DXE Driver.

  This code is derived from:
    IntelFrameworkModulePkg\Universal\StatusCode\RuntimeDxe\StatusCodeRuntimeDxe.c

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "EventLogDxe.h"
#include "StatusCode.h"
#include "EventLogger.h"

#include <Guid/MemoryStatusCodeRecord.h>
#include <Protocol/StatusCode.h>
#include <Library/HobLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Protocol/Vmbus.h>
#include <Protocol/ReportStatusCodeHandler.h>

EFI_STATUS
EFIAPI
ReportStatusCode(
  IN  EFI_STATUS_CODE_TYPE     CodeType,
  IN  EFI_STATUS_CODE_VALUE    Value,
  IN  UINT32                   Instance,
  IN  EFI_GUID                 *CallerId OPTIONAL,
  IN  EFI_STATUS_CODE_DATA     *Data OPTIONAL
  );


//
// GUID for status code event channel.
//
extern EFI_GUID gStatusCodeEventChannelGuid;

EFI_HANDLE  mEfiStatusCodeEventHandle;

//
// CallerId field is valid.
//
#define EFI_STATUS_EVENT_HAS_CALLER_GUID    0x00000001
//
// Data field is valid.
//
#define EFI_STATUS_EVENT_HAS_DATA           0x00000002

//
// Status code event log entry.
//
typedef struct
{
    UINT32                  Flags;
    EFI_STATUS_CODE_VALUE   Value;
    UINT32                  Instance;
    EFI_GUID                CallerId;
    EFI_STATUS_CODE_DATA    Data;
} EFI_STATUS_CODE_EVENT;


/**
  Check if it's a Device Path pointing to BootManagerMenu.

  @param  DevicePath     Input device path.

  @retval TRUE   The device path is BootManagerMenu File Device Path.
  @retval FALSE  The device path is NOT BootManagerMenu File Device Path.
**/
BOOLEAN
IsBootManagerMenuFilePath (
  EFI_DEVICE_PATH_PROTOCOL     *DevicePath
)
{
    EFI_HANDLE                      FvHandle;
    VOID                            *NameGuid;
    EFI_STATUS                      Status;

    Status = gBS->LocateDevicePath (&gEfiFirmwareVolume2ProtocolGuid, &DevicePath, &FvHandle);
    if (!EFI_ERROR (Status)) {
        NameGuid = EfiGetNameGuidFromFwVolDevicePathNode ((CONST MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *) DevicePath);
        if (NameGuid != NULL) {
            return CompareGuid (NameGuid, PcdGetPtr (PcdBootManagerMenuFile));
        }
    }

    return FALSE;
}


/**
  Check if it's a Device Path pointing to Network Device.

  @param  DevicePath     Input device path.

  @retval TRUE   The device path is Network Device Device Path.
  @retval FALSE  The device path is NOT Network Device Device Path.
**/
BOOLEAN
IsNetworkDeviceFilePath (
  EFI_DEVICE_PATH_PROTOCOL     *DevicePath
)
{
    VMBUS_DEVICE_PATH               *vmbusDevicePath;
    VENDOR_DEVICE_PATH              *vendorDevicePath;

    while (!IsDevicePathEnd(DevicePath))
    {
        if ((DevicePathType(DevicePath) == HARDWARE_DEVICE_PATH) &&
            (DevicePathSubType(DevicePath) == HW_VENDOR_DP))
        {
            vendorDevicePath = (VENDOR_DEVICE_PATH*) DevicePath;

            if (CompareGuid(
                &vendorDevicePath->Guid,
                &gEfiVmbusChannelDevicePathGuid))
            {
                vmbusDevicePath = (VMBUS_DEVICE_PATH*) DevicePath;
                if (CompareGuid(
                    &vmbusDevicePath->InterfaceType,
                    &gSyntheticNetworkClassGuid))
                {
                    return TRUE;
                }
            }
        }
        DevicePath = NextDevicePathNode(DevicePath);
    }

    return FALSE;
}


EFI_STATUS
EFIAPI
ReportStatusCode(
    IN  EFI_STATUS_CODE_TYPE     CodeType,
    IN  EFI_STATUS_CODE_VALUE    Value,
    IN  UINT32                   Instance,
    IN  EFI_GUID                *CallerId OPTIONAL,
    IN  EFI_STATUS_CODE_DATA    *Data OPTIONAL
    )
/*++

Routine Description:

    This function implements EFI_STATUS_CODE_PROTOCOL.ReportStatusCode().
    It logs the status code and associated data to the status code event channel.

Arguments:

    CodeType        Indicates the type of status code being reported.

    Value           Describes the current status of a hardware or software entity.
                    This includes information about the class and subclass that is used to
                    classify the entity as well as an operation.

    Instance        The enumeration of a hardware or software entity within
                    the system. Valid instance numbers start with 1.

    CallerId        This optional parameter may be used to identify the caller.
                    This parameter allows the status code driver to apply different rules to
                    different callers.

    Data            This optional parameter may be used to pass additional data.

Return Value:

    EFI_SUCCESS      The function completed successfully
    EFI_DEVICE_ERROR The function should not be completed due to a device error.


--*/
{
    EFI_STATUS_CODE_EVENT localEvent;
    EFI_STATUS_CODE_EVENT *eventData = NULL;
    EFI_EVENT_DESCRIPTOR eventDesc;
    UINT32 size = 0;
    static BOOLEAN EventAlreadyUpdated = FALSE;

    ASSERT(EfiGetCurrentTpl() <= TPL_NOTIFY);

    if ((CodeType & EFI_STATUS_CODE_TYPE_MASK) == EFI_PROGRESS_CODE && Value == PcdGet32 (PcdProgressCodeOsLoaderLoad))
    {
        //
        // Start a boot event for this device.
        // Status for the boot device will be updated as needed in a distributed
        // fashion (e.g. a PXE boot failure status will be update in the PXE code)
        //
        // The boot event will be completed before this function exits or
        // in ExitBootServices.
        //
        // Set the initial boot status to indicate an I/O error.
        // If an I/O error occurs, LoadImage doesn't return a useful
        // status code.
        //
        // Note:
        //   At this point the device path may not contain the Bootx64.efi
        //   file path which may be appended later.  This omission is OK for
        //   boot logging.
        //
        UINTN DevicePathData;
        UINTN OptionNumber;

        if (Data != NULL && Data->Size == (sizeof(UINTN) * 2))
        {
           DevicePathData = *((UINTN *)(Data + 1));

           // Filter out FrontPage/BootManager
           if (!IsBootManagerMenuFilePath((EFI_DEVICE_PATH_PROTOCOL *)DevicePathData))
           {
              OptionNumber = *((UINTN *)(Data + 1) + 1);
              DEBUG((DEBUG_INFO, "[HVBE] Starting new boot event. DP Ptr: 0x%X, OptionNumber: %d\n", DevicePathData, OptionNumber));
              DEBUG((DEBUG_INFO, "[HVBE] DP: %s\n", ConvertDeviceNodeToText((EFI_DEVICE_PATH_PROTOCOL *)DevicePathData, FALSE, FALSE)));
              BootDeviceEventStart((EFI_DEVICE_PATH_PROTOCOL *)DevicePathData, (UINT16)OptionNumber, BootDeviceOsLoaded, EFI_SUCCESS);
           }

           EventAlreadyUpdated = FALSE;
        }
    }
    else if ((CodeType & EFI_STATUS_CODE_TYPE_MASK) == EFI_ERROR_CODE && Value == (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_DXE_BS_EC_BOOT_OPTION_LOAD_ERROR))
    {
        if (!EventAlreadyUpdated && Data != NULL && Data->Size == (sizeof(UINTN) * 2))
        {
            UINTN DevicePathData = *((UINTN *)(Data + 1));
            UINTN StatusCode = *((UINTN *)(Data + 1) + 1);

            if (GET_BOOT_DEVICE_STATUS_GROUP(StatusCode) == DeviceStatusSecureBootGroup)
            {
                BootDeviceEventUpdate(StatusCode, EFI_ACCESS_DENIED);
                DEBUG((DEBUG_INFO, "[HVBE] Updating boot event: 0x%X, %r\n", StatusCode, EFI_ACCESS_DENIED));
            }
            else if (IsNetworkDeviceFilePath((EFI_DEVICE_PATH_PROTOCOL *)DevicePathData))
            {
                BOOT_DEVICE_STATUS DeviceStatus = NetworkBootUnexpectedFailure;
                EFI_STATUS Status = (EFI_STATUS)StatusCode;

                if (Status == EFI_BUFFER_TOO_SMALL) {
                  DeviceStatus = NetworkBootBufferTooSmall;
                } else if (Status == EFI_DEVICE_ERROR) {
                  DeviceStatus = NetworkBootDeviceError;
                } else if (Status == EFI_OUT_OF_RESOURCES) {
                  DeviceStatus = NetworkBootNoResources;
                } else if (Status == EFI_NO_MEDIA) {
                  DeviceStatus = NetworkBootMediaDisconnected;
                } else if (Status == EFI_NO_RESPONSE) {
                  DeviceStatus = NetworkBootNoResponse;
                } else if (Status == EFI_TIMEOUT) {
                  DeviceStatus = NetworkBootServerTimeout;
                } else if (Status == EFI_ABORTED) {
                  DeviceStatus = NetworkBootCancelled;
                } else if (Status == EFI_ICMP_ERROR) {
                  DeviceStatus = NetworkBootIcmpError;
                } else if (Status == EFI_TFTP_ERROR) {
                  DeviceStatus = NetworkBootTftpError;
                } else if (Status == EFI_NOT_FOUND) {
                  DeviceStatus = NetworkBootNoBootFile;
                } else {
                  DeviceStatus = NetworkBootUnexpectedFailure;
                }

                BootDeviceEventUpdate(DeviceStatus, Status);
                DEBUG((DEBUG_INFO, "[HVBE] Updating boot event: 0x%X, %r\n", DeviceStatus, Status));
            }
            else {
               BootDeviceEventUpdate(BootDeviceOsNotLoaded, EFI_LOAD_ERROR);
               DEBUG((DEBUG_INFO, "[HVBE] Updating boot event: BootDeviceOsNotLoaded, EFI_LOAD_ERROR\n"));
            }

            BootDeviceEventComplete();
            DEBUG((DEBUG_INFO, "[HVBE] Completing boot event\n"));
            EventAlreadyUpdated = TRUE;
        }
    }
    else if ((CodeType & EFI_STATUS_CODE_TYPE_MASK) == EFI_ERROR_CODE && Value == (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_DXE_BS_EC_BOOT_OPTION_FAILED))
    {
        BootDeviceEventUpdate(BootDeviceReturnedFailure, EFI_NOT_STARTED);
        DEBUG((DEBUG_INFO, "[HVBE] Updating boot event: BootDeviceReturnedFailure, EFI_NOT_STARTED\n"));
        BootDeviceEventComplete();
        DEBUG((DEBUG_INFO, "[HVBE] Completing boot event\n"));
    }
    else if ((CodeType & EFI_STATUS_CODE_TYPE_MASK) == EFI_PROGRESS_CODE && Value == (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_DXE_BS_PC_BOOT_OPTION_COMPLETE))
    {
        BootDeviceEventComplete();
        DEBUG((DEBUG_INFO, "[HVBE] Completing boot event\n"));
    }

    if ((Data != NULL) &&
        (Data->HeaderSize >= sizeof(EFI_STATUS_CODE_DATA)))
    {
        //
        // Subract out the size of the embedded EFI_STATUS_CODE_DATA
        // to avoid over-allocating. The HeaderSize field will be set to the
        // needed size.
        //
        size = (sizeof(EFI_STATUS_CODE_EVENT) - sizeof(EFI_STATUS_CODE_DATA)) +
               Data->HeaderSize +
               Data->Size;

        eventData = AllocateZeroPool(size);

        if (eventData != NULL)
        {
            CopyMem(&eventData->Data, Data, (Data->HeaderSize + Data->Size));
            eventData->Flags |= EFI_STATUS_EVENT_HAS_DATA;
        }
    }

    if (eventData == NULL)
    {
        //
        // No data was provided or allocating memory failed.
        // Fallback to the local event and drop the data.
        //
        ZeroMem(&localEvent, sizeof(localEvent));
        size  = sizeof(EFI_STATUS_CODE_EVENT);
        eventData = &localEvent;
    }

    if (CallerId != NULL)
    {
        CopyGuid(&eventData->CallerId, CallerId);
        eventData->Flags |= EFI_STATUS_EVENT_HAS_CALLER_GUID;
    }

    eventData->Instance = Instance;
    eventData->Value    = Value;

    ZeroMem(&eventDesc, sizeof(eventDesc));
    eventDesc.EventId  = CodeType;  // Use as EventId
    eventDesc.DataSize = size;

    EventLog(mEfiStatusCodeEventHandle,
        &eventDesc,
        eventData);

    if (eventData != &localEvent)
    {
        gBS->FreePool(eventData);
    }

    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
StatusCodeRuntimeInitialize()
/*++

Routine Description:

    Initializes the implementation of EFI_STATUS_CODE_PROTOCOL and creates an event channel
    for collecting status code events.
    If enabled, status codes saved during the PEI phase will be retrieved and logged.

Arguments:

    None.

Return Value:

    EFI_SUCCESS on success

--*/
{
    EVENT_CHANNEL_INFO                Attributes;
    EFI_PEI_HOB_POINTERS              Hob;
    MEMORY_STATUSCODE_PACKET_HEADER  *PacketHeader;
    MEMORY_STATUSCODE_RECORD         *Record;
    UINTN                             Index;
    UINTN                             MaxRecordNumber;
    EFI_STATUS                        Status;
    EFI_RSC_HANDLER_PROTOCOL          *RscHandlerProtocol = NULL;

    DEBUG((DEBUG_INIT, "Initializing Status Code Event Channel\n"));
    //
    // Create the event channel for logging UEFI status codes.
    //
    Attributes.Flags      = EVENT_CHANNEL_OVERWRITE_RECORDS;
    Attributes.BufferSize = PcdGet32(PcdStatusCodeEventLogSize);
    Attributes.RecordSize = 0;
    Attributes.Tpl        = TPL_NOTIFY;

    Status = EventChannelCreate(&gStatusCodeEventChannelGuid, &Attributes, &mEfiStatusCodeEventHandle);

    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to Create Status Code Event Channel. Error %08x\n", Status));
        ASSERT(FALSE);
        goto Exit;
    }

    //
    // Replay Status code entries which were logged during the PEI phase.
    // They are saved in a GUID HOB.
    //
    if (FeaturePcdGet(PcdStatusCodeReplayIn))
    {
        Hob.Raw = GetFirstGuidHob(&gMemoryStatusCodeRecordGuid);

        if (Hob.Raw != NULL)
        {
            PacketHeader    = (MEMORY_STATUSCODE_PACKET_HEADER *) GET_GUID_HOB_DATA (Hob.Guid);
            MaxRecordNumber = (UINTN) PacketHeader->RecordIndex;
            Record          = (MEMORY_STATUSCODE_RECORD *) (PacketHeader + 1);

            if (PacketHeader->PacketIndex > 0)
            {
                //
                // RecordIndex has wrapped around. The record count is
                // the maximum.
                //
                MaxRecordNumber = (UINTN) PacketHeader->MaxRecordsNumber;
            }

            //
            // FUTURE: If the PEI status code ring buffer overflowed,
            //  the buffer is not processed in order.
            //   start at index RecordIndex,
            //   go up and mask Index by the max size.
            //   stop after processing MaxRecordNumber
            //
            for (Index = 0; Index < MaxRecordNumber; Index++)
            {
                ReportStatusCode(Record[Index].CodeType,
                    Record[Index].Value,
                    Record[Index].Instance,
                    NULL,  // PEI Phase events don't have a caller ID or extra data.
                    NULL);
            }
        }
    }

    //
    // Get Report Status Code Handler Protocol.
    //
    Status = gBS->LocateProtocol (&gEfiRscHandlerProtocolGuid, NULL, (VOID **) &RscHandlerProtocol);
    ASSERT_EFI_ERROR (Status);

    //
    // Register report status code listener for Boot Events.
    //
    Status = RscHandlerProtocol->Register (ReportStatusCode, TPL_HIGH_LEVEL);
    ASSERT_EFI_ERROR (Status);

Exit:

    return Status;
}
