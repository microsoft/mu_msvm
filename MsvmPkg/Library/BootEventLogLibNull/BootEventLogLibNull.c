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
   return EFI_SUCCESS;
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
    return EFI_SUCCESS;
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
    return EFI_SUCCESS;
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
    return EFI_SUCCESS;
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
    return EFI_SUCCESS;
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
    return EFI_SUCCESS;
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
    return EFI_SUCCESS;
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
    return EFI_SUCCESS;
}
