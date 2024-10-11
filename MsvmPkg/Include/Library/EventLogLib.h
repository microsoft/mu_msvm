/** @file
  Library wrapper around EFI_EVENTLOG_PROTOCOL

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

#include <Protocol/EventLog.h>

EFI_STATUS
EFIAPI
EventLogChannelCreate(
    IN              const EFI_GUID      *Channel,
    IN OPTIONAL     EVENT_CHANNEL_INFO  *Attributes,
    OUT OPTIONAL    EFI_HANDLE          *Handle
    );


EFI_STATUS
EFIAPI
EventLogChannelOpen(
    IN              const EFI_GUID  *Channel,
    OUT OPTIONAL    EFI_HANDLE      *Handle
    );


EFI_STATUS
EFIAPI
EventLogLib(
    IN          const EFI_HANDLE    Channel,
                UINT32              Flags,
                const UINT32        EventId,
                const UINT32        DataSize,
    IN OPTIONAL const VOID          *Data
    );


EFI_STATUS
EFIAPI
EventLogPendingGet(
    IN  const EFI_HANDLE        Channel,
    OUT EFI_EVENT_DESCRIPTOR    *Metadata,
    OUT VOID                    **Data
    );


EFI_STATUS
EFIAPI
EventLogPendingCommit(
    IN  const EFI_HANDLE    Channel
    );


EFI_STATUS
EFIAPI
EventLogFlush(
    IN  const EFI_HANDLE    Channel
    );


EFI_STATUS
EFIAPI
EventLogReset(
    IN  const EFI_HANDLE    Channel
    );


EFI_STATUS
EFIAPI
EventLogStatistics(
    IN  const EFI_HANDLE            Channel,
    OUT EVENT_CHANNEL_STATISTICS    *Stats
    );


typedef
BOOLEAN
(EFIAPI *EFI_EVENTLOG_ENUMERATE_CALLBACK)(
    IN  VOID                        *Context,
    IN  const EFI_EVENT_DESCRIPTOR  *Metadata,
    IN  const VOID                  *Event
    );


EFI_STATUS
EFIAPI
EventLogEnumerate(
    IN  const EFI_HANDLE                Channel,
    IN  EFI_EVENTLOG_ENUMERATE_CALLBACK Callback,
    IN  const VOID                      *Context
    );