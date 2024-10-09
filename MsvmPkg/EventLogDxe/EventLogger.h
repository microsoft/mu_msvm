/** @file
  Defines types, constants, and function prototypes for event channels

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once


EFI_STATUS
EFIAPI
EventLoggerInitialize();


EFI_STATUS
EFIAPI
EventChannelCreate(
    IN              const EFI_GUID         *Channel,
    IN OPTIONAL     EVENT_CHANNEL_INFO     *Attributes,
    OUT OPTIONAL    EFI_HANDLE             *Handle
    );


EFI_STATUS
EFIAPI
EventChannelFlush(
    IN  const EFI_HANDLE            Channel
    );


EFI_STATUS
EFIAPI
EventChannelReset(
    IN  const EFI_HANDLE            Channel
    );


EFI_STATUS
EFIAPI
EventChannelStatistics(
    IN  const EFI_HANDLE            Channel,
    OUT EVENT_CHANNEL_STATISTICS   *Stats
    );


EFI_STATUS
EFIAPI
EventEnumerate(
    IN      const EFI_HANDLE            Channel,
    IN OUT  EFI_HANDLE                 *Enumerator,
    OUT     EFI_EVENT_DESCRIPTOR       *Metadata,
    OUT     VOID                      **Event
    );


EFI_STATUS
EFIAPI
EventLog(
    IN          const EFI_HANDLE            Channel,
    IN          const EFI_EVENT_DESCRIPTOR *Event,
    IN OPTIONAL const VOID                 *Data
    );


EFI_STATUS
EFIAPI
EventPendingGet(
    IN  const EFI_HANDLE            Channel,
    OUT EFI_EVENT_DESCRIPTOR       *Metadata,
    OUT VOID                      **Data
    );


EFI_STATUS
EFIAPI
EventPendingCommit(
    IN  const EFI_HANDLE            Channel
    );
