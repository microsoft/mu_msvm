/** @file
  Implementation of the EFI_EVENTLOG_PROTOCOL and management of event channels
  Event channels use a ring buffer as the backing store

  --- Pending Events ---
  Each Channel can have one event pending and can update the event data but not change
  its size.

  Space is reserved in the backing store for pending events and the event descriptor is written
  The data is cached outside of the backing store for easy updating (once in the ring
  it may wrap around)

  The current pending event can be updated any number of times before commiting.
  Pending events can be incomplete if the event channel is reset or another pending event
  is logged before the current one is committed.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/TimerLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/BiosDeviceLib.h>

#include "EventLogDxe.h"
#include "EfiHandleTable.h"
#include "EfiRing.h"
#include "EventLogger.h"
#include "BiosInterface.h"
#include <IsolationTypes.h>

//
// Information on a currently pending event.
//
typedef struct
{
    RING_HANDLE                 Handle;
    EFI_EVENT_DESCRIPTOR        Metadata;
    UINTN                       CacheSize;
    VOID*                       Cache;
} EVENT_PENDING_INFO;

//
// Internal enumeration context
//
typedef struct
{
    RING_HANDLE                 RingEnum;
    UINT32                      BufferSize;
    UINT8                       Buffer[];
} EVENT_ENUM_CONTEXT;

//
// Internal representation of an event channel
//
typedef struct
{
    EFI_GUID                    Id;
    EVENT_CHANNEL_INFO          Attributes;
    //
    // Cached pending event information
    //
    EVENT_PENDING_INFO          Pending;
    EFI_LOCK                    Lock;
    //
    // Channel Stats, note that the Lost field is unused,
    // it is maintained by the ring buffer.
    //
    EVENT_CHANNEL_STATISTICS    Stats;
    //
    // Backing store for event data
    //
    EFI_RING_BUFFER             Ring;
} EVENT_CHANNEL;

//
//  Channel Id GUID must be the first field in the EVENT_CHANNEL
//  as it is used as the object lookup key in the handle table.
//
static_assert(OFFSET_OF(EVENT_CHANNEL,Id) == 0);

//
// Number of bytes to increase the enumeration bounce buffer by
// See EventEnumerate
//
#define EVENT_ENUM_BUFFER_STEP_SIZE 256

//
// Handle table for managing channels.
//
EFI_HANDLE  mEventChannels      = NULL;

EFI_HV_PROTOCOL *mHv;
EFI_HV_IVM_PROTOCOL *mHvIvm;


const EFI_EVENTLOG_PROTOCOL mEfiEventLogProtocol =
{
    EventChannelCreate,
    EventChannelFlush,
    EventChannelReset,
    EventChannelStatistics,
    EventEnumerate,
    EventLog,
    EventPendingGet,
    EventPendingCommit
};


EFI_STATUS
EventPendingCommitInternal(
    IN      EVENT_CHANNEL              *Channel
    );

VOID
EventPendingCleanup(
    IN      EVENT_CHANNEL              *Channel
    );


VOID*
EventAllocate32BitMemory(
    IN      UINT32                      Size
    )
/*++

Routine Description:

    Allocates memory in 32-bit address space (below 4GB).
    Memory must be freed with FreePages

Arguments:

    Size        The number of bytes to allocate.

Return Value:

    Allocated memory or NULL on failure.

--*/
{
    EFI_PHYSICAL_ADDRESS address = (BASE_4GB - 1ULL);

    if (EFI_ERROR(gBS->AllocatePages(AllocateMaxAddress,
                                     EfiBootServicesData,
                                     EFI_SIZE_TO_PAGES(Size),
                                     (EFI_PHYSICAL_ADDRESS*) &address)))
    {
        return NULL;
    }

    return (VOID*)(UINTN)address;
}


__forceinline
VOID
EventChannelLock(
    IN      EVENT_CHANNEL              *Channel
    )
/*++

Routine Description:

    Locks the given event channel

Arguments:

    Channel         channel to lock

Return Value:

    None

--*/
{
    EfiAcquireLock(&Channel->Lock);
}


__forceinline
VOID
EventChannelUnlock(
    IN      EVENT_CHANNEL              *Channel
    )
/*++

Routine Description:

    Unlocks the given event channel

Arguments:

    Channel         channel to unlock

Return Value:

    None

--*/
{
    EfiReleaseLock(&Channel->Lock);
}


EVENT_CHANNEL *
EventChannelFromGuid(
    IN      const EFI_GUID             *Channel,
    OUT     EFI_HANDLE                 *Handle
    )
/*++

Routine Description:

    Attempts to find the channel with the given GUID identifier.

Arguments:

    Channel     GUID identifier of the channel to find.

    Handle      Returns the channel handle on success.
                INVALID_EVENT_HANDLE if the channel is not found.

Return Value:

    Pointer to the EVENT_CHANNEL structure if found.
    NULL if not found.

--*/
{
    EVENT_CHANNEL *channel = NULL;

    channel = EfiHandleTableLookupByKey(mEventChannels,
                Channel,
                sizeof(*Channel),
                Handle);

    if (channel != NULL)
    {
        return channel;
    }
    else
    {
        *Handle = INVALID_EVENT_HANDLE;
        return NULL;
    }
}


EVENT_CHANNEL *
EventChannelFromHandle(
    IN      const EFI_HANDLE            Channel
    )
/*++

Routine Description:

    Attempts to find the channel with the given handle.

Arguments:

    Channel     Handle to the channel.

Return Value:

    Pointer to the EVENT_CHANNEL structure if found.
    NULL if not found.

--*/
{
    EVENT_CHANNEL *channel = NULL;

    if (Channel == INVALID_EVENT_HANDLE)
    {
        return NULL;
    }

    channel = EfiHandleTableLookupByHandle(mEventChannels, Channel);

    return channel;
}


EFI_STATUS
EventChannelFlushCallback(
    IN      const EFI_HANDLE            TableHandle,
    IN      VOID                       *CallbackContext,
    IN      const EFI_HANDLE            ObjectHandle,
    IN      VOID                       *Object
    )
/*++

Routine Description:

    Handle table object enumeration callback.
    Flushes a single channel to the BiosEventLog device.
    Any pending events are marked as incomplete and committed.

Arguments:

    TableHandle         Unused -
                        Handle for the EFI handle table that the enumerated object belongs to.

    CallbackContext     Unused -
                        Context provided to the call to EfiHandleTableEnumerateObjects

    ObjectHandle        Unused -
                        Handle representing the object

    Object              EVENT_CHANNEL object.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_HV_PROTECTION_HANDLE protectionHandle;
    BIOS_EVENT_CHANNEL *channelDescriptor;
    EVENT_CHANNEL *channel   = (EVENT_CHANNEL *)Object;
    UINT32         dataSize  = channel->Ring.Size;
    UINT32         allocSize = dataSize + sizeof(*channelDescriptor);
    UINT64 physicalAddress;
    UINT64 virtualAddress;
    BOOLEAN hostEmulatorsPresent = PcdGetBool(PcdHostEmulatorsWhenHardwareIsolated);
    VOID* originalAllocation;

    //
    // Allocate a region below 4GB since the BIOS data port
    // only accepts 32-Bit values.
    //
    channelDescriptor = EventAllocate32BitMemory(allocSize);
    originalAllocation = channelDescriptor;
    if (channelDescriptor == NULL)
    {
        return EFI_OUT_OF_RESOURCES;
    }

    EventChannelLock(channel);

    if (IsHardwareIsolatedNoParavisor() && hostEmulatorsPresent)
    {
        //
        // In a hardware isolated system, making a chunk of guest memory visible to host
        // scrambles that chunk of memory. Memory block needs to be re-populated with data.
        //
        status = mHvIvm->MakeAddressRangeHostVisible(mHvIvm,
                                        (HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE),
                                        (void*)channelDescriptor,
                                        EFI_SIZE_TO_PAGES(allocSize) * EFI_PAGE_SIZE,
                                        FALSE,
                                        &protectionHandle);

        if (EFI_ERROR(status))
        {
            goto Exit;
        }

        //
        // After making memory chunk host visible, guest needs virtual address to access memory.
        //
        physicalAddress = (UINT64)channelDescriptor;
        physicalAddress += PcdGet64(PcdIsolationSharedGpaBoundary);
        virtualAddress = (physicalAddress | PcdGet64(PcdIsolationSharedGpaCanonicalizationBitmask));
        channelDescriptor = (BIOS_EVENT_CHANNEL *)virtualAddress;
    }

    //
    // Forcefully commit any pending event before flushing the ring.
    //
    if (channel->Pending.Handle != INVALID_RING_HANDLE)
    {
        channel->Pending.Metadata.Flags |= EVENT_FLAG_INCOMPLETE;
        EventPendingCommitInternal(channel);
    }

    //
    // Flatten the ring data before flushing to the bios device.
    //
    channelDescriptor->DataSize = channel->Ring.Size;
    RingBufferFlatten(&channel->Ring, &channelDescriptor->DataSize, channelDescriptor->Data);

    channelDescriptor->Channel       = channel->Id;
    channelDescriptor->EventsLost    = channel->Ring.Stats.LostWrites;
    channelDescriptor->EventsWritten = channel->Stats.Written;

    //
    // Flush the log to a persistent storage. If there is a host BIOS device, that
    // works like our persistent storage. If not, currently don't do anything.
    //
    if (IsHardwareIsolatedNoParavisor() && !hostEmulatorsPresent)
    {
        //
        // TODO: Ideally, these would go into some persistent across boot storage like CMOS, Flash or EFI partition.
        //
    }
    else
    {
        WriteBiosDevice(BiosConfigEventLogFlush, (UINT32)(UINTN)channelDescriptor);
    }

    if (IsHardwareIsolatedNoParavisor() && hostEmulatorsPresent)
    {
        mHvIvm->MakeAddressRangeNotHostVisible(mHvIvm, protectionHandle);
    }

    channel->Stats.Flush++;
    status = EFI_SUCCESS;

Exit:
    EventChannelUnlock(channel);
    FreePages(originalAllocation, EFI_SIZE_TO_PAGES(allocSize));

    return status;
}


EFI_STATUS
EventChannelResetCallback(
    IN      const EFI_HANDLE            TableHandle,
    IN      VOID                       *CallbackContext,
    IN      const EFI_HANDLE            ObjectHandle,
    IN      VOID                       *Object
    )
/*++

Routine Description:

    Handle table object enumeration callback that resets a single channel.

Arguments:

    TableHandle         Unused -
                        Handle for EFI handle table that the enumerated object belongs to.

    CallbackContext     Unused -
                        Context provided to the call to EfiHandleTableEnumerateObjects

    ObjectHandle        Unused -
                        Handle representing the object

    Object              EVENT_CHANNEL object.

Return Value:

    EFI_STATUS.

--*/
{
    EVENT_CHANNEL *channel = (EVENT_CHANNEL *)Object;

    EventChannelLock(channel);
    EventPendingCleanup(channel);

    channel->Stats.Reset++;
    channel->Stats.Written = 0;
    RingBufferReset(&channel->Ring);

    EventChannelUnlock(channel);
    return EFI_SUCCESS;
}


EFI_STATUS
EventPendingCommitInternal(
    IN          EVENT_CHANNEL          *Channel
    )
/*++

Routine Description:

    Commits the currently pending event on the given channel.
    The channel should be locked before calling this function.

Arguments:

    Channel          EVENT_CHANNEL to commit the pending event on.

Return Value:

    EFI_SUCCESS      The function completed successfully
    EFI_NOT_FOUND    No event was pending on the channel

--*/
{
    UINT32     ioSize;
    EFI_STATUS status;

    //
    // Flush metadata then data.
    //
    Channel->Pending.Metadata.Flags &= (~EVENT_FLAG_PENDING);
    Channel->Pending.Metadata.CommitTime = GetPerformanceCounter();
    ioSize = sizeof(EFI_EVENT_DESCRIPTOR);

    status = RingBufferIo(&Channel->Ring,
                Channel->Pending.Handle,
                DataWrite,
                0,
                &Channel->Pending.Metadata,
                &ioSize);

    ASSERT_EFI_ERROR(status);

    if ((Channel->Pending.Cache != NULL) &&
        (Channel->Pending.Metadata.DataSize > 0))
    {
        ioSize = Channel->Pending.Metadata.DataSize;

        status = RingBufferIo(&Channel->Ring,
                    Channel->Pending.Handle,
                    DataWrite,
                    sizeof(EFI_EVENT_DESCRIPTOR),
                    Channel->Pending.Cache,
                    &ioSize);

        ASSERT_EFI_ERROR(status);
    }

    EventPendingCleanup(Channel);

    return status;
}


VOID
EventPendingCleanup(
    IN          EVENT_CHANNEL          *Channel
    )
/*++

Routine Description:

    Invalidates and cleans up any currently pending event on the given channel

Arguments:

    Channel     Event channel to cleanup pending events on.

Return Value:

    None.

--*/
{
    //
    // Reset the pending info.
    // Note that the cache buffer is not freed
    //
    ZeroMem(&Channel->Pending.Metadata, sizeof(EFI_EVENT_DESCRIPTOR));
    Channel->Pending.Handle = INVALID_RING_HANDLE;
}


EFI_STATUS
EFIAPI
EventChannelCreate(
    IN              const EFI_GUID         *Channel,
    IN OPTIONAL     EVENT_CHANNEL_INFO     *Attributes,
    OUT OPTIONAL    EFI_HANDLE             *Handle
    )
/*++

Routine Description:

    Creates or Opens an event channel.

    A NULL pointer for the Attributes parameter indicates that a channel should not be
    created.  If the channel exists it will be opened.

Arguments:

    Channel           GUID identifying the channel to create.

    Attributes        Attributes of the channel.
                      If NULL the channel will be opened if it exists.

    Handle            Returns a handle to the channel.

Return Value:

    EFI_SUCCESS           The function completed successfully
    EFI_NOT_FOUND         Open only behavior was desired, but the channel was not found.
    EFI_UNSUPPORTED       Channel creation was attempted at runtime.

--*/
{
    EVENT_CHANNEL *channel = NULL;
    EFI_HANDLE    handle   = INVALID_EVENT_HANDLE;
    UINTN         allocSize;
    EFI_STATUS    status;

    //
    // Try to find the channel.
    // If found the channel has already been created
    // Just return the handle if the caller wants it.
    //
    channel = EventChannelFromGuid(Channel, &handle);

    if (channel != NULL)
    {
        status = EFI_SUCCESS;
        goto Exit;
    }

    //
    // No channel was found, this is an error for open only requests.
    //
    if (Attributes == NULL)
    {
        status = EFI_NOT_FOUND;
        goto Exit;
    }

    //
    // BufferSize must be power of two
    //
    if ((Attributes->BufferSize & (Attributes->BufferSize - 1)) != 0)
    {
        status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    //
    // Fixed sized channels must have a buffer size that is a multiple of
    // the record size.
    //
    if ((Attributes->Flags & EVENT_CHANNEL_FIXED_RECORDS) &&
        ((Attributes->RecordSize == 0) ||
         ((Attributes->BufferSize % Attributes->RecordSize) != 0)))
    {
        status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    allocSize = sizeof(EVENT_CHANNEL) + Attributes->BufferSize;

    status = EfiHandleTableAllocateObject(mEventChannels, allocSize, &channel, &handle);

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    CopyGuid(&channel->Id, Channel);
    CopyMem(&channel->Attributes, Attributes, sizeof(channel->Attributes));

    EfiInitializeLock(&channel->Lock, Attributes->Tpl);
    channel->Pending.Handle    = INVALID_RING_HANDLE;
    channel->Pending.Cache     = NULL;
    channel->Pending.CacheSize = 0;

    status = RingBufferInitialize(&channel->Ring,
        Attributes->BufferSize,
        ((Attributes->Flags & EVENT_CHANNEL_OVERWRITE_RECORDS) ? RING_BUFFER_OVERWRITE : 0));

    ASSERT_EFI_ERROR(status);

Exit:

    if (Handle != NULL)
    {
        *Handle = handle;
    }

    return status;
}


EFI_STATUS
EFIAPI
EventChannelFlush(
    IN      const EFI_HANDLE            Channel
    )
/*++

Routine Description:

    Flushes an event channel to implementation defined peristant storage.
    Any pending events are committed as is.  After the flush completes all
    existing events are still present.

Arguments:

    Channel           GUID identifying the channel to create.
                      INVALID_EVENT_HANDLE indicates that all channels should be flushed.

Return Value:

   EFI_SUCCESS       The function completed successfully

--*/
{
    EFI_STATUS status;

    if (Channel == INVALID_EVENT_HANDLE)
    {
        //
        //  Enumerate all channels and flush them.
        //
        status = EfiHandleTableEnumerateObjects(mEventChannels, NULL, EventChannelFlushCallback);
    }
    else
    {
        EVENT_CHANNEL *channel = EventChannelFromHandle(Channel);

        if (channel != NULL)
        {
            status = EventChannelFlushCallback(NULL, NULL, Channel, channel);
        }
        else
        {
            status = EFI_NOT_FOUND;
        }
    }

    return status;
}


EFI_STATUS
EFIAPI
EventChannelReset(
    IN      const EFI_HANDLE            Channel
    )
/*++

Routine Description:

    Resets an event channel clearing all events.

Arguments:

    Channel           GUID identifying the channel to create.
                      INVALID_EVENT_HANDLE indicates that all channels should be reset.

Return Value:

   EFI_SUCCESS       The function completed successfully

--*/
{
    EFI_STATUS status;

    if (Channel == INVALID_EVENT_HANDLE)
    {
        //
        //  Enumerate all channels and reset them.
        //
        status = EfiHandleTableEnumerateObjects(mEventChannels, NULL, EventChannelResetCallback);
    }
    else
    {
        EVENT_CHANNEL *channel = EventChannelFromHandle(Channel);

        if (channel != NULL)
        {
            status = EventChannelResetCallback(NULL, NULL, Channel, channel);
        }
        else
        {
            status = EFI_NOT_FOUND;
        }
    }

    return status;
}


EFI_STATUS
EFIAPI
EventChannelStatistics(
    IN      const EFI_HANDLE            Channel,
    OUT     EVENT_CHANNEL_STATISTICS   *Stats
    )
/*++

Routine Description:

    Retrieves statistics on the given event channel

Arguments:

    Channel           Channel to retrieves statistics for

    Stats             Buffer to contain the channel statistics

Return Value:

    EFI_SUCCESS.
    EFI_NOT_FOUND if the channel handle was invalid

--*/
{
    EVENT_CHANNEL *channel = EventChannelFromHandle(Channel);

    ASSERT(Stats != NULL);

    if (channel != NULL)
    {
        CopyMem(Stats, &channel->Stats, sizeof(*Stats));
        Stats->Lost = channel->Ring.Stats.LostWrites;

        return EFI_SUCCESS;
    }
    else
    {
        return EFI_NOT_FOUND;
    }
}


EFI_STATUS
EFIAPI
EventEnumerate(
    IN      const EFI_HANDLE            Channel,
    IN OUT  EFI_HANDLE                 *Enumerator,
    OUT     EFI_EVENT_DESCRIPTOR       *Metadata,
    OUT     VOID                      **Event
    )
/*++

Routine Description:

    Enumerates the event entries present on an event channel.

Arguments:
    Channel           GUID identifying the channel to enumerate on.

    Enumerator        Pointer to a Handle used for enumerating events.
                      Set the handle NULL to start enumerating from the beginning
                      Once enumeration is finished, callers are responsible for
                      freeing the enumerator via FreePool

    Metadata          Contains a copy of the metadata associated with the current event

    Event             Contains a pointer to the current event.  The actual data contents
                      are event channel dependent.

Return Value:

    EFI_SUCCESS       The function completed successfully
    EFI_NOT_FOUND     The GUID was not a valid channel
    EFI_END_OF_FILE   No more events are present in the channel.

--*/
{
    EVENT_ENUM_CONTEXT  *enumContext = NULL;
    VOID                *data = NULL;
    EVENT_CHANNEL       *channel;
    EFI_EVENT_DESCRIPTOR eventMeta;
    RING_HANDLE     curItem;
    UINT32          curItemSize;
    UINT32          readSize;
    UINT32          contextSize;
    EFI_STATUS      status;
    BOOLEAN         channelLocked = FALSE;

    ZeroMem(&eventMeta, sizeof(eventMeta));

    channel = EventChannelFromHandle(Channel);

    if (channel == NULL)
    {
        status = EFI_NOT_FOUND;
        goto Exit;
    }

    ASSERT(Enumerator != NULL);
    ASSERT(Metadata != NULL);
    ASSERT(Event != NULL);

    enumContext = *Enumerator;

    if (enumContext == NULL)
    {
        //
        // Setup up ring enumerator and initial bounce buffer.
        //
        contextSize = EVENT_ENUM_BUFFER_STEP_SIZE;
        enumContext = AllocateZeroPool(sizeof(EVENT_ENUM_CONTEXT) + contextSize);

        if (enumContext == NULL)
        {
            status = EFI_OUT_OF_RESOURCES;
            goto Exit;
        }

        enumContext->RingEnum   = INVALID_RING_HANDLE;
        enumContext->BufferSize = contextSize;
    }

    EventChannelLock(channel);
    channelLocked = TRUE;

    //
    // Enumerate the current item.
    //
    status = RingBufferHandleEnumerate(&channel->Ring,
                        &enumContext->RingEnum,
                        &curItem,
                        &curItemSize);

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    //
    // Read the event metadata for the current item.
    //
    readSize = sizeof(EFI_EVENT_DESCRIPTOR);
    status = RingBufferIo(&channel->Ring, curItem, DataRead, 0, &eventMeta, &readSize);

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    if (eventMeta.Flags & EVENT_FLAG_PENDING)
    {
        ASSERT(channel->Pending.CacheSize >= eventMeta.DataSize);
        data = channel->Pending.Cache;
    }
    else
    {
        //
        // The current item is not pending so we need to read the data from the ring.
        // If the current enum bounce buffer is too small, reallocate the context and
        // buffer.
        //
        if (eventMeta.DataSize > enumContext->BufferSize)
        {
            EVENT_ENUM_CONTEXT *newContext;

            contextSize = ALIGN_VALUE(eventMeta.DataSize, EVENT_ENUM_BUFFER_STEP_SIZE);
            newContext = AllocateZeroPool(sizeof(EVENT_ENUM_CONTEXT) + contextSize);

            if (newContext == NULL)
            {
                status = EFI_OUT_OF_RESOURCES;
                goto Exit;
            }

            newContext->RingEnum   = enumContext->RingEnum;
            newContext->BufferSize = contextSize;

            gBS->FreePool(enumContext);
            enumContext = newContext;
        }

        //
        // Read the event data, skipping past the metadata.
        //
        readSize = enumContext->BufferSize;
        status = RingBufferIo(&channel->Ring,
                    curItem,
                    DataRead,
                    sizeof(EFI_EVENT_DESCRIPTOR),
                    &enumContext->Buffer,
                    &readSize);

        if (EFI_ERROR(status))
        {
            goto Exit;
        }

        ASSERT(readSize == eventMeta.DataSize);
        data = enumContext->Buffer;
    }

Exit:

    if (channelLocked)
    {
        EventChannelUnlock(channel);
    }

    if (!EFI_ERROR(status))
    {
        *Metadata = eventMeta;
    }

    *Enumerator = enumContext;
    *Event      = data;

    return status;
}


EFI_STATUS
EFIAPI
EventLog(
    IN          const EFI_HANDLE            Channel,
    IN          const EFI_EVENT_DESCRIPTOR *Event,
    IN OPTIONAL const VOID                 *Data
    )
/*++

Routine Description:

    Logs a new event to the given event channel.
    The new event can be delayed by specifying the EVENT_FLAG_PENDING flag.
    Events without this flag set will be committed immediately.
    Delayed events can be retrieved via the EventPendingGet function and then committed
    via EventPendingCommit.

Arguments:

    Channel          GUID identifying the channel to create.

    EventDesc        EFI_EVENT_DESCRIPTOR describing the event to be logged.
                     Callers can specify all parameters except for the time stamps.

    Data             Optional data associated with the event.
                     The size of the data is given by the DataSize field in the EFI_EVENT_DESCRIPTOR.

Return Value:

    EFI_SUCCESS      The function completed successfully

--*/
{
    EVENT_CHANNEL      *channel;
    RING_HANDLE         eventHandle;
    EFI_EVENT_DESCRIPTOR eventMeta;
    UINT32              ioSize;
    EFI_STATUS          status;
    BOOLEAN             channelLocked = FALSE;

    channel = EventChannelFromHandle(Channel);

    if (channel == NULL)
    {
        status = EFI_NOT_FOUND;
        goto Exit;
    }

    if (Event == NULL)
    {
        status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    EventChannelLock(channel);
    channelLocked = TRUE;

    //
    // Note the order of operations below is design to ensure that any currently pending event
    // is preserved until it can be assured that a new pending event will succeed i.e. don't
    // lose two events if an error occurs.
    //
    // 1. Attempt to grow the pending data cache if needed.
    //    If this fails, the new event is lost (the previously pending event, if any, is preserved)
    // 2. Reserve space in the ring
    //    If this fails, the new event is lost.
    //    (the pending cache may be larger than needed at this point, but no data has been lost).
    //
    // -- From this point on, no failures should occur ---
    //
    // 3. Write the event descriptor to the ring (potentially marked as pending)
    // 4. If the event is pending,
    //    Commit any previous pending data (marked as incomplete) and cache the current event data.
    //    Non-pending events will have their data written to the ring at this time.
    //

    if ((Event->Flags & EVENT_FLAG_PENDING) &&
        (Data != NULL) &&
        (Event->DataSize != 0) &&
        (Event->DataSize > channel->Pending.CacheSize))
    {
        VOID* cacheBuffer;

        //
        // Pending events can only be allowed at TPL_NOTIFY or lower
        // since memory allocation is not available at higher TPLs
        //
        ASSERT(EfiGetCurrentTpl() <= TPL_NOTIFY);

        //
        // Attempt to resize the current buffer to accomodate the incoming data.
        // Note that if this fails the currently pending data is not lost,
        // only the current log operation.
        //
        cacheBuffer = ReallocatePool(channel->Pending.CacheSize,
                            Event->DataSize,
                            channel->Pending.Cache);

        if (cacheBuffer == NULL)
        {
            status = EFI_OUT_OF_RESOURCES;
            goto Exit;
        }

        channel->Pending.Cache     = cacheBuffer;
        channel->Pending.CacheSize = Event->DataSize;
    }

    //
    // Reserve space in the ring for an event header and the caller's data
    // Write the initial header first then the associated data if not a pending event.
    //
    status = RingBufferReserve(&channel->Ring,
                Event->DataSize+sizeof(EFI_EVENT_DESCRIPTOR),
                &eventHandle);

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    //
    // From this point on, errors should be avoided (and really should not occurr at all)
    // as space has already been reserved in the ring.
    //


    CopyMem(&eventMeta, Event, sizeof(*Event));
    eventMeta.HeaderSize = SIZEOF_EFI_EVENT_DESCRIPTOR_REVISION_1;
    eventMeta.Flags      = (Event->Flags & EVENT_FLAG_PENDING);
    eventMeta.CreateTime = GetPerformanceCounter();
    eventMeta.CommitTime = ((Event->Flags & EVENT_FLAG_PENDING) ? 0LL : eventMeta.CreateTime);

    //
    // Always write the descriptor into the ring.
    // The enumeration code needs to look at the flags.
    //
    ioSize = sizeof(EFI_EVENT_DESCRIPTOR);
    status = RingBufferIo(&channel->Ring,
                eventHandle,
                DataWrite,
                0,
                &eventMeta,
                &ioSize);

    if (EFI_ERROR(status))
    {
        ASSERT(FALSE);
        goto Exit;
    }

    //
    // Pending, write data into the cache
    // Any currently pending event will be marked as incomplete and
    // forcefully committed.
    //
    if (Event->Flags & EVENT_FLAG_PENDING)
    {
        if (channel->Pending.Handle != INVALID_RING_HANDLE)
        {
            channel->Pending.Metadata.Flags |= EVENT_FLAG_INCOMPLETE;
            EventPendingCommitInternal(channel);
        }

        channel->Pending.Handle   = eventHandle;
        CopyMem(&channel->Pending.Metadata, &eventMeta, sizeof(eventMeta));

        ASSERT(Event->DataSize <= channel->Pending.CacheSize);
        CopyMem(channel->Pending.Cache, Data, Event->DataSize);
    }
    //
    // Non-Pending, write the data now.
    //
    else if ((Data != NULL) &&
             (Event->DataSize != 0))
    {
        ioSize = Event->DataSize;
        status = RingBufferIo(&channel->Ring,
                    eventHandle,
                    DataWrite,
                    sizeof(EFI_EVENT_DESCRIPTOR),
                    (VOID*)Data,
                    &ioSize);

        if (EFI_ERROR(status))
        {
            ASSERT(FALSE);
            goto Exit;
        }
    }

    channel->Stats.Written++;

Exit:

    if (channelLocked)
    {
        EventChannelUnlock(channel);
    }

    return status;
}


EFI_STATUS
EFIAPI
EventPendingGet(
    IN      const EFI_HANDLE            Channel,
    OUT     EFI_EVENT_DESCRIPTOR       *Metadata,
    OUT     VOID                      **Data
    )
/*++

Routine Description:

    Retrieves the currently pending event on the given channel.
    The data of the event can be modified before event is committed

Arguments:

    Channel           GUID identifying the channel to create.

    Metadata          Contains a copy of the metadata associated with the current event.

    Event             Contains a pointer to the current event.  The actual data contents
                      are event channel dependent. This data can be modified before being
                      committed.  The size of the data cannot be changed.

Return Value:

   EFI_SUCCESS       The function completed successfully
   EFI_NOT_FOUND     No event was pending on the channel

--*/
{
    EVENT_CHANNEL *channel;
    EFI_STATUS status = EFI_NOT_FOUND;

    channel = EventChannelFromHandle(Channel);

    if (channel != NULL)
    {
        EventChannelLock(channel);

        if (EFI_ERROR(RingBufferHandleIsValid(&channel->Ring, channel->Pending.Handle)))
        {
            // Pending event has been invalidated by another operation on the ring.
            EventPendingCleanup(channel);
            *Data     = NULL;
        }
        else
        {
            *Metadata = channel->Pending.Metadata;
            *Data     = channel->Pending.Cache;
            status    = EFI_SUCCESS;
        }

        EventChannelUnlock(channel);
    }

    return status;
}


EFI_STATUS
EFIAPI
EventPendingCommit(
    IN      const EFI_HANDLE            Channel
    )
/*++

Routine Description:

   Commits the currently pending event on the given channel.

Arguments:

   Channel          GUID identifying the channel to create.

Return Value:

   EFI_SUCCESS      The function completed successfully
   EFI_NOT_FOUND    No event was pending on the channel

--*/
{
    EVENT_CHANNEL *channel;
    EFI_STATUS status = EFI_NOT_FOUND;

    channel = EventChannelFromHandle(Channel);

    if (channel != NULL)
    {
        EventChannelLock(channel);

        if (EFI_ERROR(RingBufferHandleIsValid(&channel->Ring, channel->Pending.Handle)))
        {
            EventPendingCleanup(channel);
        }
        else
        {
            status = EventPendingCommitInternal(channel);
        }

        EventChannelUnlock(channel);
    }

    return status;
}


EFI_STATUS
EFIAPI
EventLoggerInitialize()
/*++

Routine Description:

    Initializes the event logger

Arguments:

    None.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_HANDLE handle = NULL;
    EFI_STATUS status;

    const EFI_HANDLE_TABLE_INFO eventChannels =
    {
        AllocateZeroPool,
        FreePool,
        sizeof(EFI_GUID)
    };

    DEBUG((DEBUG_INIT, "Initializing Event Logger, Maximum %d Channels\n", FixedPcdGet32(PcdEventLogMaxChannels)));

    status = EfiHandleTableInitialize(&eventChannels,
                FixedPcdGet32(PcdEventLogMaxChannels),
                'B',
                &mEventChannels);

    if (EFI_ERROR(status))
    {
        ASSERT(FALSE);
        goto Exit;
    }

    status = gBS->InstallMultipleProtocolInterfaces(
              &handle,
              &gEfiEventLogProtocolGuid,
              &mEfiEventLogProtocol,
              NULL);

    if (EFI_ERROR(status))
    {
        DEBUG((DEBUG_ERROR, "Failed to Register Event Log Protocol. Error %08x\n", status));
        ASSERT(FALSE);
    }

Exit:

    return status;
}
