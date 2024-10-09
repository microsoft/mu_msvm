/** @file
  Library wrapper around EFI_EVENTLOG_PROTOCOL for logging boot events

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

#include <Library/EventLogLib.h>

#include <BiosBootLogInterface.h>

extern EFI_GUID gBootEventChannelGuid;


EFI_STATUS
EFIAPI
BootEventLogLibInit(
    IN  EFI_HANDLE          ImageHandle,
    IN  EFI_SYSTEM_TABLE    *SystemTable
    );


EFI_STATUS
EFIAPI
BootDeviceEventStart(
    IN  const EFI_DEVICE_PATH_PROTOCOL  *DevicePath,
        UINT16                          BootVariableNumber,
        BOOT_DEVICE_STATUS              Status,
        EFI_STATUS                      ExtendedStatus
    );


EFI_STATUS
EFIAPI
BootDeviceEventUpdate(
    BOOT_DEVICE_STATUS  Status,
    EFI_STATUS  ExtendedStatus
    );


EFI_STATUS
EFIAPI
BootDeviceEventPendingStatus(
    OUT BOOT_DEVICE_STATUS  *Status,
    OUT EFI_STATUS          *ExtendedStatus
    );


EFI_STATUS
EFIAPI
BootDeviceEventComplete(
    VOID
    );


EFI_STATUS
EFIAPI
BootDeviceEventResetLog(
    VOID
    );


EFI_STATUS
EFIAPI
BootDeviceEventFlushLog(
    VOID
    );


EFI_STATUS
EFIAPI
BootDeviceEventStatistics(
    OUT EVENT_CHANNEL_STATISTICS    *Stats
    );


EFI_STATUS
EFIAPI
BootDeviceEventEnumerate(
    IN  EFI_EVENTLOG_ENUMERATE_CALLBACK Callback,
    IN  const VOID                      *Context
    );