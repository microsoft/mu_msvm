/** @file
  Library wrapper around EFI_EVENTLOG_PROTOCOL for logging boot events

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BootEventLogLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/EventLogLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>

EFI_HANDLE mBootEvent = INVALID_EVENT_HANDLE;


EFI_STATUS
EFIAPI
BootEventLogConstructor(
    IN  EFI_HANDLE          ImageHandle,
    IN  EFI_SYSTEM_TABLE    *SystemTable
    )
/*++

Routine Description:

    Initializes the boot event library by creating/opening the boot event channel.

Arguments:

    ImageHandle     Unused

    SystemTable     Unused

Return Value:

    EFI_SUCCESS     The function completed successfully

--*/
{
    EVENT_CHANNEL_INFO attributes;
    EFI_STATUS status = EFI_SUCCESS;

    if (mBootEvent == INVALID_EVENT_HANDLE)
    {
        attributes.Flags      = 0;
        attributes.RecordSize = 0;
        attributes.BufferSize = PcdGet32(PcdBootEventLogSize);
        attributes.Tpl        = TPL_NOTIFY;
        status = EventLogChannelCreate(&gBootEventChannelGuid, &attributes, &mBootEvent);
    }

    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
BootEventLogLibInit(
    IN  EFI_HANDLE          ImageHandle,
    IN  EFI_SYSTEM_TABLE    *SystemTable
    )
/*++

Routine Description:

    Initializes the boot event library by calling its constructor.

Arguments:

    ImageHandle     Unused

    SystemTable     Unused

Return Value:

    EFI_SUCCESS     The function completed successfully

--*/
{
    BootEventLogConstructor(ImageHandle, SystemTable);
    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
BootDeviceEventStart(
    IN  const EFI_DEVICE_PATH_PROTOCOL  *DevicePath,
        UINT16                          BootVariableNumber,
    IN  BOOT_DEVICE_STATUS              InitialStatus,
        EFI_STATUS                      ExtendedStatus
    )
/*++

Routine Description:

    Creates a pending device boot event (EventId of BOOT_DEVICE_EVENT_ID)

Arguments:

    DevicePath              Device path that boot is being attempted from.

    BootVariableNumber      Then number portion of the boot variable (Boot####) for this device.

    InitialStatus           Initial status for the device

    ExtendedStatus          Initial extended status

Return Value:

    EFI_SUCCESS     The function completed successfully

--*/
{
    BOOTEVENT_DEVICE_ENTRY *bootEvent = NULL;
    UINTN devPathSize;
    EFI_STATUS status;

    if (mBootEvent == INVALID_EVENT_HANDLE)
    {
        status = EFI_NOT_READY;
        goto Exit;
    }

    devPathSize = GetDevicePathSize(DevicePath);

    //
    // In practice, device paths shouldn't be very long
    // ASSERT to catch any, but then try to log anyway.
    // The log write will fail but adjust the LostWrites statistic.
    //
    ASSERT(devPathSize < PcdGet32(PcdBootEventLogSize));

    bootEvent = AllocateZeroPool(devPathSize + sizeof(BOOTEVENT_DEVICE_ENTRY));

    if (bootEvent == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    bootEvent->Status             = InitialStatus;
    bootEvent->ExtendedStatus     = ExtendedStatus;
    bootEvent->DevicePathSize     = (UINT32)devPathSize;
    bootEvent->BootVariableNumber = BootVariableNumber;
    CopyMem(bootEvent->DevicePath, DevicePath, devPathSize);

    status = EventLogLib(mBootEvent,
                EVENT_FLAG_PENDING,
                BOOT_DEVICE_EVENT_ID,
                ((UINT32)devPathSize + sizeof(BOOTEVENT_DEVICE_ENTRY)),
                bootEvent);

Exit:

    gBS->FreePool(bootEvent);
    return status;
}


EFI_STATUS
EFIAPI
BootDeviceEventUpdate(
    IN  BOOT_DEVICE_STATUS  Status,
        EFI_STATUS          ExtendedStatus
    )
/*++

Routine Description:

    Updates a previously started device boot event.

Arguments:

    Status              Current status for the previously pended

    ExtendedStatus      EFI_STATUS providing extended status, e.g. an error code

Return Value:

    EFI_SUCCESS     The function completed successfully

--*/
{
    BOOTEVENT_DEVICE_ENTRY *bootEvent = NULL;
    EFI_EVENT_DESCRIPTOR   eventDesc;
    EFI_STATUS status;

    if (mBootEvent == INVALID_EVENT_HANDLE)
    {
        status = EFI_NOT_READY;
        goto Exit;
    }

    status = EventLogPendingGet(mBootEvent, &eventDesc, &bootEvent);

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    if (eventDesc.EventId != BOOT_DEVICE_EVENT_ID)
    {
        status = EFI_NOT_FOUND;
        goto Exit;
    }

    if (eventDesc.DataSize < sizeof(BOOTEVENT_DEVICE_ENTRY))
    {
        ASSERT(FALSE);
        status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    bootEvent->Status         = Status;
    bootEvent->ExtendedStatus = ExtendedStatus;
    status = EFI_SUCCESS;

Exit:

    return status;
}


EFI_STATUS
EFIAPI
BootDeviceEventPendingStatus(
    OUT BOOT_DEVICE_STATUS  *Status,
    OUT EFI_STATUS          *ExtendedStatus
    )
/*++

Routine Description:

    Returns the current status of the currently pending boot device event

Arguments:

    Status              Returns the current BOOT_DEVICE_STATUS.

    ExtendedStatus      Returns the current extended status.

Return Value:

    EFI_SUCCESS on success
    EFI_NOT_FOUND if no boot event was pending.

--*/
{
    BOOTEVENT_DEVICE_ENTRY *bootEvent = NULL;
    EFI_EVENT_DESCRIPTOR   eventDesc;
    EFI_STATUS status;

    if (mBootEvent == INVALID_EVENT_HANDLE)
    {
        status = EFI_NOT_READY;
        goto Exit;
    }

    status = EventLogPendingGet(mBootEvent, &eventDesc, &bootEvent);

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    if (eventDesc.EventId != BOOT_DEVICE_EVENT_ID)
    {
        status = EFI_NOT_FOUND;
        goto Exit;
    }

    if (eventDesc.DataSize < sizeof(BOOTEVENT_DEVICE_ENTRY))
    {
        ASSERT(FALSE);
        status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    *Status         = bootEvent->Status;
    *ExtendedStatus = bootEvent->ExtendedStatus;
    status = EFI_SUCCESS;

Exit:

    return status;
}


EFI_STATUS
EFIAPI
BootDeviceEventComplete(
    VOID
    )
/*++

Routine Description:

    Completes a previously started device boot event.
    The event is committed to teh log and cannot be updated any more.

Arguments:

    None.

Return Value:

    EFI_SUCCESS     The function completed successfully

--*/
{
    EFI_STATUS status;

    if (mBootEvent == INVALID_EVENT_HANDLE)
    {
        status = EFI_NOT_READY;
        goto Exit;
    }

    status = EventLogPendingCommit(mBootEvent);

Exit:

    return status;
}



EFI_STATUS
EFIAPI
BootDeviceEventResetLog(
    VOID
    )
/*++

Routine Description:

    Resets the boot event log clearing all events.

Arguments:

    None

Return Value:

    EFI_SUCCESS     The function completed successfully

--*/
{
    EFI_STATUS status;

    if (mBootEvent == INVALID_EVENT_HANDLE)
    {
        status = EFI_NOT_READY;
        goto Exit;
    }

    status = EventLogReset(mBootEvent);

Exit:

    return status;
}


EFI_STATUS
EFIAPI
BootDeviceEventFlushLog(
    VOID
    )
/*++

Routine Description:

    Flushes the boot event log to peristent storage

Arguments:

    None

Return Value:

    EFI_SUCCESS     The function completed successfully

--*/
{
    EFI_STATUS status;

    if (mBootEvent == INVALID_EVENT_HANDLE)
    {
        status = EFI_NOT_READY;
        goto Exit;
    }

    status = EventLogFlush(mBootEvent);

Exit:

    return status;
}


EFI_STATUS
EFIAPI
BootDeviceEventStatistics(
    OUT EVENT_CHANNEL_STATISTICS    *Stats
    )
/*++

Routine Description:

    Returns statistics for the boot event log.

Arguments:

    Stats           Contains the boot event channel statistic upon successfull return.

Return Value:

    EFI_SUCCESS     The function completed successfully

--*/
{
    EFI_STATUS status;

    if (mBootEvent == INVALID_EVENT_HANDLE)
    {
        status = EFI_NOT_READY;
        goto Exit;
    }

    status = EventLogStatistics(mBootEvent, Stats);

Exit:

    return status;
}


EFI_STATUS
EFIAPI
BootDeviceEventEnumerate(
    IN  EFI_EVENTLOG_ENUMERATE_CALLBACK Callback,
    IN  const VOID                      *Context
    )
/*++

Routine Description:

    Enumerates events in the boot event channel.
    The callback will be called for each event.

Arguments:

    Callback        Callback

    Context         Opaque context provided to the callback.

Return Value:

    EFI_SUCCESS     The function completed successfully

--*/
{
    EFI_STATUS status;

    if (mBootEvent == INVALID_EVENT_HANDLE)
    {
        status = EFI_NOT_READY;
        goto Exit;
    }

    status = EventLogEnumerate(mBootEvent, Callback, Context);

Exit:

    return status;
}