/** @file
  Event logging protocol

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#ifndef __EFI_EVENTLOG_PROTOCOL_H__
#define __EFI_EVENTLOG_PROTOCOL_H__

// Include the event log descriptor and other types
#include <BiosEventLogInterface.h>

///
///  The event channel will always contain fixed size records
///  The RecordSize field of EVENT_CHANNEL_INFO defines the size.
///
#define EVENT_CHANNEL_FIXED_RECORDS         0x00000001

///
///  When set and the underlying event record storage becomes full
///  older records will be overwritten to make room for newer ones.
///  When not set, newer records will fail to be logged.
///
#define EVENT_CHANNEL_OVERWRITE_RECORDS     0x00000002

///
///  Special handle value for invalid handles
///  Can also be used to trigger special behavior in some functions.
///
#define INVALID_EVENT_HANDLE (EFI_HANDLE)((UINTN)-1)


/**
  Defines the attributes of an event channel when creating it.
**/
typedef struct
{
    ///
    /// TPL associated with the event channel.
    /// Events can be logged at this TPL or lower.
    ///
    EFI_TPL Tpl;
    ///
    /// Flags defining the characteristics of the channel.
    ///
    UINT32  Flags;
    ///
    /// Defines the size of each event log entry in bytes.
    /// Unused for variable sized records (initialize to 0)
    ///
    UINT32  RecordSize;
    ///
    /// Buffer size in bytes for the channel
    /// This will be rounded to a multiple of RecordSize for fixed sized records.
    ///
    UINT32  BufferSize;
} EVENT_CHANNEL_INFO;

/**
  Counters for various operations on an event channel
**/
typedef struct
{
    UINT32      Written;
    UINT32      Lost;
    UINT32      Reset;
    UINT32      Flush;
} EVENT_CHANNEL_STATISTICS;


/**
  Creates or Opens an event channel.

  A NULL pointer for the Attributes parameter indicates that only an
  existing channel should be opened.  A channel will no be created in this case.

  @param  Channel           GUID identifying the channel to create.

  @param  Attributes        Attributes of the channel.
                            If NULL the channel will be opened if it exists.

  @param  Handle            Returns a handle to the channel.

  @retval EFI_SUCCESS       The function completed successfully
  @retval EFI_NOT_FOUND     Open only behavior was desired, but the channel was not found.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EVENTLOG_CHANNEL_CREATE)(
    IN CONST EFI_GUID                *Channel,
    IN       EVENT_CHANNEL_INFO      *Attributes OPTIONAL,
    OUT      EFI_HANDLE              *Handle OPTIONAL
    );


/**
  Flushes an event channel to implementation defined peristant storage.
  Any pending events are committed as is.  After the flush completes all
  existing events are still present.

  @param  Channel           Handle identifying the channel to flush.
                            INVALID_EVENT_HANDLE indicates that all channels should be flushed.

  @retval EFI_SUCCESS       The function completed successfully

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EVENTLOG_CHANNEL_FLUSH)(
    IN CONST EFI_HANDLE               Channel
    );


/**
  Resets an event channel clearing all events.
  There is no prescribed behavior for event saved in persistent storage.

  @param  Channel           Handle identifying the channel to reset.
                            INVALID_EVENT_HANDLE indicates that all channels should be reset.

  @retval EFI_SUCCESS       The function completed successfully

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EVENTLOG_CHANNEL_RESET)(
    IN CONST EFI_HANDLE               Channel
    );


/**
  Retrieves statistics for an event channel

  @param  Channel           Handle identifying the channel retrieve statistics for.

  @retval EFI_SUCCESS       The function completed successfully

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EVENTLOG_CHANNEL_STATISTICS)(
    IN CONST EFI_HANDLE               Channel,
    OUT      EVENT_CHANNEL_STATISTICS *Stats
    );


/**
  Enumerates the event entries present on an event channel.

  @param  Channel           GUID identifying the channel to enumerate on.

  @param  Enumerator        Pointer to a Handle used for enumerating events.
                            Set the handle NULL to start enumerating from the beginning
                            Once enumeration is finished, callers are responsible for
                            freeing the enumerator via FreePool

  @param  Metadata          Contains a copy of the metadata associated with the current event

  @param  Event             Contains a pointer to the current event.  The actual data contents
                            are event channel dependent.

  @retval EFI_SUCCESS       The function completed successfully
  @retval EFI_NOT_FOUND     The GUID was not a valid channel
  @retval EFI_END_OF_FILE   No more events are present in the channel.
  @retval EFI_INVALID_PARAMETER   NULL GUID, Invalid Enumerator, Metadata, or Event pointer.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_EVENTLOG_EVENT_ENUMERATE)(
    IN CONST EFI_HANDLE               Channel,
    IN OUT   EFI_HANDLE              *Enumerator,
    OUT      EFI_EVENT_DESCRIPTOR    *Metadata,
    OUT      VOID                   **Event
    );

/**
  Logs a new event to the given event channel.
  The new event can be delayed by specifying the EVENT_FLAG_PENDING flag.
  Events without this flag set will be committed immediately.
  Delayed events can be retrieved via the EventPendingGet function and then committed
  via EventPendingCommit.


  @param  Channel           GUID identifying the channel to create.

  @param  EventDesc         EFI_EVENT_DESCRIPTOR describing the event to be logged.
                            Callers can specify all parameters except for the time stamps.

  @param  Data              Optional data associated with the event.

  @retval EFI_SUCCESS       The function completed successfully

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EVENTLOG_EVENT_LOG)(
    IN CONST EFI_HANDLE               Channel,
    IN CONST EFI_EVENT_DESCRIPTOR    *EventDesc,
    IN CONST VOID                    *Data OPTIONAL
    );


/**
  Retrieves the currently pending event on the given channel.
  The data of the event can be modified before event is committed

  @param  Channel           GUID identifying the channel to create.
  @param  Metadata          Contains a copy of the metadata associated with the current event.
  @param  Event             Contains a pointer to the current event.  The actual data contents
                            are event channel dependent. This data can be modified before being
                            committed.  The size of the data cannot be changed.

  @retval EFI_SUCCESS       The function completed successfully
  @retval EFI_NOT_FOUND     No event was pending on the channel

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EVENTLOG_EVENT_PENDING_GET)(
    IN  CONST EFI_HANDLE              Channel,
    OUT       EFI_EVENT_DESCRIPTOR   *Metadata,
    OUT       VOID                  **Data
    );


/**
  Commits the currently pending event on the given channel.

  @param  Channel          GUID identifying the channel to create.

  @retval EFI_SUCCESS      The function completed successfully
  @retval EFI_NOT_FOUND    No event was pending on the channel

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EVENTLOG_EVENT_PENDING_COMMIT)(
    IN CONST EFI_HANDLE            Channel
    );


///
/// Provides flexible event logging services to the platform firmware.
///
typedef struct _EFI_EVENTLOG_PROTOCOL
{
    EFI_EVENTLOG_CHANNEL_CREATE         ChannelCreate;
    EFI_EVENTLOG_CHANNEL_FLUSH          ChannelFlush;
    EFI_EVENTLOG_CHANNEL_RESET          ChannelReset;
    EFI_EVENTLOG_CHANNEL_STATISTICS     ChannelStatistics;
    EFI_EVENTLOG_EVENT_ENUMERATE        EventEnumerate;
    EFI_EVENTLOG_EVENT_LOG              EventLog;
    EFI_EVENTLOG_EVENT_PENDING_GET      EventPendingGet;
    EFI_EVENTLOG_EVENT_PENDING_COMMIT   EventPendingCommit;
} EFI_EVENTLOG_PROTOCOL;

#define EFI_EVENTLOG_PROTOCOL_GUID \
{ 0xe916bdda, 0x6c85, 0x45a0, {0x91, 0x79, 0xb4, 0x18, 0xd0, 0x3d, 0x71, 0x45}}

extern EFI_GUID gEfiEventLogProtocolGuid;

#endif  // __EFI_EVENTLOG_PROTOCOL_H__
