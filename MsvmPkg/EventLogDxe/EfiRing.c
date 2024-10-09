/** @file
  Ring buffer Implementation for UEFI Event Channels.

  This implementation supports writing variable length data which is stored internally
  as a record header followed by caller data. Space can be reserved in the ring and the
  data written at a later time using a data handle.

  The overflow behavior is controllable between overwriting one or more of the
  oldest records or dropping new data once full.

  Note that if data is overwritten, previously returned handles are invalidated
  and functions utilizing them will fail.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "EventLogDxe.h"
#include "EfiRing.h"

//
// Data and enumeration handles have a key encoded in the upper 32-bits that remains
// valid as long as no record is removed or the ring is not reset.
//
#define RING_HANDLE_KEY_MASK             0xFFFFFFFFULL
#define RING_HANDLE_GET_OFFSET(handle)   ((RING_HANDLE)(handle) & RING_HANDLE_KEY_MASK)
#define RING_HANDLE_GET_KEY(handle)      (((RING_HANDLE)(handle) >> 32))
#define RING_HANDLE_SET_KEY(handle, key) (((RING_HANDLE)(handle)) | ((UINT64)(key)) << 32)

#define RING_REMOVED_SIGNATURE  'R'
#define RING_REMOVED_SIZE       0xFFFFFFFF

//
// Internal record header describes the size of
// the data written into the buffer.
//
typedef struct
{
    UINT32 Size;
} RING_RECORD;


UINT32
RingBufferCurrentKey(
    IN      EFI_RING_BUFFER        *Ring
    )
/*++

Routine Description:

    Returns the current Key used for data and enumeration handles.

Arguments:

    Ring - Ring buffer to get the current key for.

Return Value:

    Current Key.

--*/
{
    //
    // HandleKey of 0 is reserved for RingBufferReset().
    //
    if (Ring->HandleKey == 0)
    {
        Ring->HandleKey = 1;
    }

    return Ring->HandleKey;
}


UINT32
RingBufferBytesFree(
    IN      const EFI_RING_BUFFER  *Ring
    )
/*++

Routine Description:

    Determine the amount of free space in the ring.  If In equals Out, then
    the buffer is empty.  One data byte can't be used, or a full buffer looks
    like an empty buffer.

Arguments:

    Ring - Ring buffer to get the amount of free space for.

Return Value:

    Returns the number of bytes free.

--*/
{
    UINT32 bytesFree;

    bytesFree = Ring->Tail - Ring->Head - 1;
    if (bytesFree < Ring->Size)
    {
        //            1         2         3
        //  0123456789012345678901234567890123456789
        // +----------------------------------------+
        // |ffffgggghhhh--------aaaabbbbccccddddeeee|
        // +----------------------------------------+
        //  ^           ^       ^                   ^
        //  Buffer      Head    Tail                Size
        //
        // In this example, Head < Tail, there are 32 bytes of free
        // space, 20 at the end of the buffer and another 12 at the
        // beginning.
        //

        return bytesFree;
    }
    else
    {
        //            1         2         3
        //  0123456789012345678901234567890123456789
        // +----------------------------------------+
        // |------------aaaabbbb--------------------|
        // +----------------------------------------+
        //  ^           ^       ^                   ^
        //  Buffer      Tail    Head                Size
        //
        // In this example, Head >= Tail, (remember that == means the
        // buffer is empty) there are 8 bytes of free space
        // represented by aaaabbbb.
        //

        return Ring->Size + bytesFree;
    }
}


VOID
RingBufferWrapIfNeeded(
    IN      const EFI_RING_BUFFER      *Ring,
    IN OUT  UINT32                     *Offset,
    IN      UINT32                      BytesRequired
    )
/*++

Routine Description:

    Wraps the given offset to the begining of the ring if the required
    number of bytes are not available between the current offset and the end of
    the ring.

Arguments:

    Ring            - Ring buffer to get the amount of free space for.

    Offset          - Offset to wrap if needed.

    BytesRequired   - Required contigous bytes between the offset and the end of the ring.

Return Value:

    None.

--*/
{
    ASSERT(*Offset < Ring->Size);
    if ((Ring->Size - *Offset) < BytesRequired)
    {
        *Offset = 0;
    }
}


EFI_STATUS
RingBufferIsValidOffset(
    IN          const EFI_RING_BUFFER  *Ring,
    IN          UINT32                  Offset
    )
/*++

Routine Description:

    Determines if the given offset is valid within the given ring buffer.

Arguments:

    Ring    - Ring buffer to get the amount of free space for.

    Offset  - Offset to validate.

Return Value:

    EFI_SUCCESS offset is valid
    EFI_INVALID_PARAMETER offset is invalid

--*/
{
    if ((Offset > Ring->Size) ||        // out of bounds
        ((Offset >= Ring->Head) &&      // unallocated space
         (Offset <  Ring->Tail)))
    {
        return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
}


EFI_STATUS
RingBufferRecordAt(
    IN          const EFI_RING_BUFFER  *Ring,
    IN OUT      UINT32                 *Offset,
    OUT         RING_RECORD           **Header
    )
/*++

Routine Description:

    Returns the record header closest to the given offset in the ring buffer.
    Record headers must be contiguous so the actual header may be at the start
    of the ring buffer if the offset is very close to the end.

Arguments:

    Ring    - Ring buffer to get the amount of free space for.

    Offset  - Offset to retrieve the record header at.
              On return this may be adjusted to account for the
              actual record header location.

    Header  - Returns the record header found

Return Value:

    EFI_SUCCESS
    EFI_NOT_FOUND if the ring is empty
    EFI_INVALID_PARAMETER if the ring record is corrupt

--*/
{
    RING_RECORD *header = NULL;
    UINT32 newOffset;
    EFI_STATUS status = EFI_SUCCESS;

    status = RingBufferIsValidOffset(Ring, *Offset);

    if (EFI_ERROR(status))
    {
        ASSERT(FALSE);
        goto Exit;
    }

    if (Ring->Head == Ring->Tail)
    {
        // empty ring so no header.
        status = EFI_NOT_FOUND;
        goto Exit;
    }

    RingBufferWrapIfNeeded(Ring, Offset, sizeof(RING_RECORD));
    header = (RING_RECORD*)&Ring->Buffer[*Offset];

    if ((header->Size > (Ring->Size - 1)) ||
        (header->Size < sizeof(RING_RECORD)))
    {
        ASSERT(FALSE);
        header = NULL;
        status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    //
    // Detect bad sizes that would place the next record
    // in an invalid location.
    //
    newOffset = (*Offset + header->Size) & Ring->Mask;

    if ((newOffset <= *Offset) &&
        (newOffset > Ring->Head))
    {
        ASSERT(FALSE);
        header = NULL;
        status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

Exit:
    *Header = header;
    return status;
}


UINT32
RingBufferFillDataAt(
    IN          EFI_RING_BUFFER        *Ring,
    IN          const UINT32            Offset,
                const UINT8             Value,
    IN          const UINT32            DataSize
    )
/*++

Routine Description:

    Fills the ring with a value starting at the given offset.
    The data stream will be wrapped if there is not enough space
    between the given offset and the end of the ring.

Arguments:

    Ring     - Ring buffer to write to.

    Offset   - Offset to set the data at.

    Value    - Value to fill the ring region with.

    DataSize - Size of the data.

Return Value:

    Offset to the next byte after the written data.

--*/
{
    UINT32 bytesToCopy = DataSize;
    UINT32 chunkSize;

    ASSERT(Offset < Ring->Size);
    ASSERT(DataSize < Ring->Size);

    //
    // Set the first chunk from the current offset to the end of the ring.
    //
    chunkSize = MIN((Ring->Size - Offset), bytesToCopy);

    if (chunkSize > 0)
    {
        SetMem(&Ring->Buffer[Offset], chunkSize, Value);
        bytesToCopy -= chunkSize;
    }

    //
    // Set the second chunk (if any) after wrapping to the start of the ring.
    //
    if (bytesToCopy > 0)
    {
        chunkSize = MIN(Ring->Size, bytesToCopy);
        SetMem(&Ring->Buffer[0], chunkSize, Value);
    }

    return (Offset + DataSize) & Ring->Mask;
}


UINT32
RingBufferWriteDataAt(
    IN          EFI_RING_BUFFER        *Ring,
    IN          const UINT32            Offset,
    IN          const VOID             *Data,
    IN          const UINT32            DataSize
    )
/*++

Routine Description:

    Writes a stream of bytes to the ring starting at the given offset.
    The data stream will be wrapped if there is not enough space
    between the given offset and the end of the ring.

Arguments:

    Ring     - Ring buffer to write to.

    Offset   - Offset to write the data to.

    Data     - Associated data.

    DataSize - Size of the data.

Return Value:

    Offset to the next byte after the written data.

--*/
{
    UINT8 *src = (UINT8*)Data;
    UINT32 bytesToCopy = DataSize;
    UINT32 chunkSize;

    ASSERT(Offset < Ring->Size);
    ASSERT(DataSize < Ring->Size);

    //
    // Copy in the first chunk from the current offset to the end of the ring.
    //
    chunkSize = MIN((Ring->Size - Offset), bytesToCopy);

    if (chunkSize > 0)
    {
        CopyMem(&Ring->Buffer[Offset], src, chunkSize);
        bytesToCopy -= chunkSize;
        src         += chunkSize;
    }

    //
    // Copy in the second chunk (if any) after wrapping to the start of the ring.
    //
    if (bytesToCopy > 0)
    {
        chunkSize = MIN(Ring->Size, bytesToCopy);
        CopyMem(&Ring->Buffer[0], src, chunkSize);
    }

    return (Offset + DataSize) & Ring->Mask;
}


UINT32
RingBufferReadDataAt(
    IN          const EFI_RING_BUFFER  *Ring,
    IN          UINT32                  Offset,
    OUT         VOID                   *Data,
    IN          UINT32                  DataSize
    )
/*++

Routine Description:

    Reads a record and associated data from the ring buffer at the given offset.

Arguments:

    Ring     - Ring buffer to read from.

    Offset   - Offset to read the data from. On return contains the actual
               offset that data was read from.

    Header   - Returns the record header.

    Data     - Associated data buffer. Contains the read data upon successfull return.

    DataSize - Size of the data buffer. On return contains the actual data size.

Return Value:

    Offset of the next byte in the ring after the read record.

--*/
{
    UINT8 *dst = (UINT8*)Data;
    UINT32 curOffset = Offset;
    UINT32 bytesToCopy = DataSize;
    UINT32 chunkSize;

    ASSERT(DataSize < Ring->Size);
    //
    // Copy the first chunk from the current offset to the end of the ring.
    //
    chunkSize = MIN((Ring->Size - curOffset), bytesToCopy);

    if (chunkSize > 0)
    {
        CopyMem(dst, &Ring->Buffer[curOffset], chunkSize);
        bytesToCopy -= chunkSize;
        dst         += chunkSize;
    }

    //
    // Copy in the second chunk (if any) after wrapping to the start of the ring.
    //
    if (bytesToCopy > 0)
    {
        ASSERT(bytesToCopy < Ring->Size);
        chunkSize = MIN(Ring->Size, bytesToCopy);
        CopyMem(dst, &Ring->Buffer[0], chunkSize);
    }

    return (Offset + DataSize) & Ring->Mask;
}


EFI_STATUS
RingBufferReadRecord(
    IN              const EFI_RING_BUFFER  *Ring,
    IN OUT          UINT32                 *Offset,
    OUT             RING_RECORD           **Header,
    OUT             VOID                   *Data,
    IN OUT OPTIONAL UINT32                 *DataSize
    )
/*++

Routine Description:

    Utility function to return the data of a record at a given offset.
    It returns the following information on the given record:
        Actual offset of the header.
        Pointer to the record header
        Size of the record and copy of the data (Both Optional)

Arguments:

    Ring     - Ring buffer to read from.

    Offset   - Offset to read at. On successful return contains the actual offset of the record.

    Header   - Returns a pointer to the record header.

    Data     - Data buffer. If provided, contains the read data upon successfull return.

    DataSize - Size of the data buffer. If provided, contains the actual data size on return.
               This is required if a Data bufffer is provided.

Return Value:

    EFI_SUCCESS
    EFI_NOT_FOUND if the ring is empty.
    EFI_BUFFER_TOO_SMALL if the provided data buffer is too small. DataSize will contain the needed size.

--*/
{
    RING_RECORD *record   = NULL;
    UINT32       offset   = *Offset;
    UINT32       dataOffset;
    UINT32       dataSize = 0;
    EFI_STATUS   status   = EFI_SUCCESS;

    //
    // Get and verify the record header
    // then read the data.
    //
    status = RingBufferRecordAt(Ring, &offset, &record);

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    dataSize = record->Size - sizeof(RING_RECORD);

    if (Data != NULL)
    {
        ASSERT(DataSize != NULL);

        if (dataSize > *DataSize)
        {
            status = EFI_BUFFER_TOO_SMALL;
            goto Exit;
        }

        // skip the header then copy the data into the caller's buffer
        dataOffset = (offset + sizeof(RING_RECORD)) & Ring->Mask;
        RingBufferReadDataAt(Ring, dataOffset, Data, dataSize);
    }

    *Offset = offset;

Exit:

    *Header = record;

    if (DataSize != NULL)
    {
        *DataSize = dataSize;
    }

    return status;
}


EFI_STATUS
RingBufferIo(
    IN          EFI_RING_BUFFER        *Ring,
    IN          const RING_HANDLE       DataHandle,
    IN          const RING_IO_OPERATION Op,
    IN          const UINT32            Offset,
    IN OUT      VOID                   *Data,
    IN OUT      UINT32                 *DataSize
    )
/*++

Routine Description:

    Reads or writes data from previously allocated ring region using the supplied
    data handle.

Arguments:

    Ring        - Ring buffer to perform IO on.

    DataHandle  - Handle to perform IO on.

    Op          - Operation to perform.

    Offset      - Offset within the region to IO at.

    Data        - Data buffer to use for IO

    DataSize    - Size of the data buffer.
                  For read operations, will contain the actual
                  size read on return.

Return Value:

    EFI_SUCCESS
    EFI_INVALID_DATA the data handle is no longer valid.

--*/
{
    RING_RECORD *record;
    UINT32 ringOffset = RING_HANDLE_GET_OFFSET(DataHandle);
    UINT32 recordDataSize;
    UINT32 byteCount = *DataSize;
    EFI_STATUS status;

    if (RING_HANDLE_GET_KEY(DataHandle) != Ring->HandleKey)
    {
        // handle invalidated.
        status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    //
    // Try and get the record header
    // RingBufferRecordAt will validate the offset
    //
    status = RingBufferRecordAt(Ring, &ringOffset, &record);

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    recordDataSize = record->Size - sizeof(RING_RECORD);

    if (Offset > recordDataSize)
    {
        status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    //
    // validate the caller's start offset and data size with
    // respect to the record.
    //
    ringOffset = (ringOffset + sizeof(RING_RECORD) + Offset) & Ring->Mask;

    if (Op == DataWrite)
    {
        if ((Offset + byteCount) > recordDataSize)
        {
            status = EFI_BAD_BUFFER_SIZE;
            goto Exit;
        }

        RingBufferWriteDataAt(Ring, ringOffset, Data, byteCount);
    }
    else
    {
        //
        // Cap actual read size to the record bounds
        //
        if ((Offset + byteCount) > recordDataSize)
        {
            byteCount = recordDataSize - Offset;
        }

        RingBufferReadDataAt(Ring, ringOffset, Data, byteCount);
        *DataSize = byteCount;
    }

    status = EFI_SUCCESS;

Exit:
    return status;
}


EFI_STATUS
RingBufferReserve(
    IN              EFI_RING_BUFFER        *Ring,
    IN              const UINT32            DataSize,
    OUT OPTIONAL    RING_HANDLE            *DataHandle
    )
/*++

Routine Description:

    Reserves space in a the ring buffer without writing any data.
    The region can later be written via the returned data handle and RingBufferIo.

    If not enough space is available, this function will trigger overflow behavior
    as described at the beginning of this file.

Arguments:

    Ring       - Ring buffer to reserve space in.

    DataSize   - Size to reserve

    DataHandle - Optionally return a data handle that can be used to read and write the
                 data record.

Return Value:

    EFI_SUCCESS
    EFI_BAD_BUFFER_SIZE the data is too large for the ring buffer.
    EFI_OUT_OF_RESOURCES the ring buffer is full and overwrite mode was not specified.

--*/
{
    UINT32 totalSize = DataSize + sizeof(RING_RECORD);
    UINT32 headerOffset;
    RING_RECORD header;
    RING_HANDLE handle = INVALID_RING_HANDLE;
    EFI_STATUS status;

    if (totalSize > (Ring->Size - 1))
    {
        Ring->Stats.LostWrites++;
        status = EFI_BAD_BUFFER_SIZE;
        goto Exit;
    }

    if (RingBufferBytesFree(Ring) < totalSize)
    {
        if (Ring->Flags & RING_BUFFER_OVERWRITE)
        {
            //
            // If there is not enough space between the head and the tail
            // records at the tail will need to be deleted.
            // keep deleting until there is enough space.
            //
            while (RingBufferBytesFree(Ring) < totalSize)
            {
                if (EFI_ERROR(RingBufferRemove(Ring, NULL, NULL)))
                {
                    break;
                }

                Ring->Stats.LostWrites++;
            }
        }
        else
        {
            // dropping new data
            Ring->Stats.LostWrites++;
            status = EFI_OUT_OF_RESOURCES;
            goto Exit;
        }
    }

    header.Size  = totalSize;
    headerOffset = Ring->Head;
    RingBufferWrapIfNeeded(Ring, &headerOffset, sizeof(header));

    DEBUG_CODE
    (
        RingBufferFillDataAt(Ring, headerOffset, (UINT8)Ring->HandleKey, header.Size);
    );

    CopyMem(&Ring->Buffer[headerOffset], &header, sizeof(header));

    Ring->Stats.Reserve++;
    Ring->Head = (headerOffset + header.Size) & Ring->Mask;

    handle = RING_HANDLE_SET_KEY(headerOffset, RingBufferCurrentKey(Ring));
    status = EFI_SUCCESS;

Exit:

    if (DataHandle != NULL)
    {
        *DataHandle = handle;
    }

    return status;
}


EFI_STATUS
RingBufferAdd(
    IN              EFI_RING_BUFFER        *Ring,
    IN              const VOID             *Data,
    IN              const UINT32            DataSize,
    OUT OPTIONAL    RING_HANDLE            *DataHandle
    )
/*++

Routine Description:

    Writes data to the ring buffer.

    If not enough space is available, this function will trigger overflow behavior
    as described at the beginning of this file.

Arguments:

    Ring       - Ring buffer to write to.

    Data       - Data buffer to write.

    DataSize   - Size of the data buffer.

    DataHandle - Optionally return a data handle that can be used to read and write the
                 data record.

Return Value:

    EFI_SUCCESS
    EFI_BUFFER_TOO_SMALL the data is too large for the ring buffer.
    EFI_OUT_OF_RESOURCES the ring buffer is full and overwrite mode was not specified.

--*/
{
    RING_HANDLE handle;
    UINT32 dataOffset;
    EFI_STATUS status;

    //
    // Reserve a region then write the data into it.
    //
    status = RingBufferReserve(Ring, DataSize, &handle);

    if (!EFI_ERROR(status))
    {
        dataOffset = (RING_HANDLE_GET_OFFSET(handle) + sizeof(RING_RECORD)) & Ring->Mask;
        RingBufferWriteDataAt(Ring, dataOffset, Data, DataSize);

        //
        // return a record handle if the caller requested one.
        //
        if (DataHandle != NULL)
        {
            *DataHandle = handle;
        }

        status = EFI_SUCCESS;
    }

    return status;
}


EFI_STATUS
RingBufferRemove(
    IN              EFI_RING_BUFFER        *Ring,
    OUT OPTIONAL    VOID                   *Data,
    IN OUT OPTIONAL UINT32                 *DataSize
    )
/*++

Routine Description:

    Reads the oldest record and removes it from the ring buffer.
    Previously returned data handles are invalidated.

Arguments:

    Ring     - Ring buffer to read from.

    Data     - Associated data buffer. Contains the read data upon successfull return.

    DataSize - Size of the data buffer. On return contains the actual data size;

Return Value:

    EFI_SUCCESS
    EFI_NOT_FOUND if the ring is empty.
    EFI_BUFFER_TOO_SMALL if the provided data buffer is too small.

--*/
{
    RING_RECORD *record = NULL;
    UINT32       offset = Ring->Tail;
    UINT32       newTail;
    EFI_STATUS   status = EFI_SUCCESS;

    status = RingBufferReadRecord(Ring, &offset, &record, Data, DataSize);

    if (!EFI_ERROR(status))
    {
        //
        // Invalidate handles key, 0 is reserved.
        //
        Ring->HandleKey++;
        if (Ring->HandleKey == 0)
        {
            Ring->HandleKey = 1;
        }

        newTail = (offset + record->Size) & Ring->Mask;

        DEBUG_CODE
        (
            RingBufferFillDataAt(Ring, offset, RING_REMOVED_SIGNATURE, record->Size);
        );

        record->Size = RING_REMOVED_SIZE;
        Ring->Tail = newTail;
    }

    return status;
}


EFI_STATUS
RingBufferHandleEnumerate(
    IN              const EFI_RING_BUFFER  *Ring,
    IN OUT          RING_HANDLE            *Enumerator,
    OUT             RING_HANDLE            *Item,
    OUT OPTIONAL    UINT32                 *ItemSize
    )
/*++

Routine Description:

    Enumerates data from the ring buffer starting with the oldest data written.
    Enumerated items are returned as RING_HANDLEs and the actual data can be accessed
    via RingBufferIo.

Arguments:

    Ring        - Ring buffer to write to.

    Enumerator  - Used to enumerator through the records.
                  Set to INVALID_RING_HANDLE to start enumerating.

    Item        - Handle representing the current item.

    ItemSize    - If supplied, returns the size of the current item on success.

Return Value:

    EFI_SUCCESS
    EFI_INVALID_PARAMETER   the enumeration handle is no longer valid
    EFI_NOT_FOUND           no more data is present or the ring is empty.

--*/
{
    RING_HANDLE  enumHandle = INVALID_RING_HANDLE;
    RING_HANDLE  itemHandle = INVALID_RING_HANDLE;
    RING_RECORD *record = NULL;
    UINT32 enumOffset;
    UINT32 currentKey;
    UINT32 itemSize = 0;
    EFI_STATUS status = EFI_NOT_FOUND;

    if (*Enumerator == INVALID_RING_HANDLE)
    {
        enumOffset = Ring->Tail;
    }
    else if (RING_HANDLE_GET_KEY(*Enumerator) != Ring->HandleKey)
    {
        // handle invalidated.
        status = EFI_INVALID_PARAMETER;
        goto Exit;
    }
    else
    {
        enumOffset = RING_HANDLE_GET_OFFSET(*Enumerator);
    }

    // empty or last record.
    if (enumOffset == Ring->Head)
    {
        status = EFI_NOT_FOUND;
        goto Exit;
    }

    currentKey = RingBufferCurrentKey((EFI_RING_BUFFER *)Ring);
    itemHandle = RING_HANDLE_SET_KEY(enumOffset, currentKey);
    status = RingBufferRecordAt(Ring, &enumOffset, &record);

    if (EFI_ERROR(status))
    {
        itemHandle = INVALID_RING_HANDLE;
        goto Exit;
    }

    // advance the enumerator
    itemSize   = record->Size;
    enumOffset = (enumOffset + record->Size) & Ring->Mask;
    enumHandle = RING_HANDLE_SET_KEY(enumOffset, currentKey);

Exit:

    *Enumerator = enumHandle;
    *Item       = itemHandle;

    if (ItemSize != NULL)
    {
        *ItemSize = itemSize;
    }

    return status;
}


EFI_STATUS
RingBufferHandleIsValid(
    IN          EFI_RING_BUFFER        *Ring,
    IN          const RING_HANDLE       DataHandle
    )
/*++

Routine Description:

    Determines if a given handle is valid or not.

Arguments:

    Ring        - Ring buffer to write to.

    DataHandle  - Handle to verify

Return Value:

    EFI_SUCCESS if the handle is valid.

--*/
{
    RING_RECORD *record;
    UINT32 ringOffset = RING_HANDLE_GET_OFFSET(DataHandle);
    EFI_STATUS status;

    if (RING_HANDLE_GET_KEY(DataHandle) != Ring->HandleKey)
    {
        // handle invalidated.
        status = EFI_INVALID_PARAMETER;
    }
    else
    {
        //
        // Try and get the record header
        // RingBufferRecordAt will validate the offset
        //
        status = RingBufferRecordAt(Ring, &ringOffset, &record);
    }

    return status;
}


EFI_STATUS
RingBufferFlatten(
    IN          const EFI_RING_BUFFER  *Ring,
    IN OUT      UINT32                 *BufferSize,
    OUT         VOID                   *Buffer
    )
/*++

Routine Description:

    Flattens the ring buffer by copying data such that
    all records are in-order and contiguous.

Arguments:

    Ring        - Ring buffer to write to.

    BufferSize  - Size of the provided buffer. On return the size of
                  the needed buffer or the actual number of bytes flushed.

    Buffer      - Buffer to flush ring data into.

Return Value:

    EFI_SUCCESS if the handle is valid.
    EFI_BUFFER_TOO_SMALL if the provided buffer is too small.

--*/
{
    RING_RECORD *record = NULL;
    UINT8  *curOutput   = Buffer;
    UINT32 enumOffset   = Ring->Tail;
    UINT32 byteCount;
    UINT32 curDataSize;
    EFI_STATUS status = EFI_SUCCESS;

    //
    // Only flush the part of the ring that is actually in use.
    // Note that this needed size is not truly accurate as it includes
    // the internal RING_RECORD header which is not part of the flattened
    // data.
    //
    byteCount = (Ring->Size - RingBufferBytesFree(Ring));

    if (*BufferSize < byteCount)
    {
        status = EFI_BUFFER_TOO_SMALL;
        goto Exit;
    }

    //
    // Enumerate and write all records. The internal record header is removed.
    //
    byteCount  = 0;
    enumOffset = Ring->Tail;

    while (enumOffset != Ring->Head)
    {
        status = RingBufferRecordAt(Ring, &enumOffset, &record);

        if (EFI_ERROR(status))
        {
            byteCount = 0;
            break;
        }

        //
        // read the current record from the ring skipping the header
        // The ASSERT detects RING_RECORD corruption.
        //
        curDataSize = record->Size - sizeof(RING_RECORD);
        ASSERT(curDataSize <= ((UINTN)(*BufferSize) - ((UINTN)curOutput - (UINTN)Buffer)));

        enumOffset = (enumOffset + sizeof(RING_RECORD)) & Ring->Mask;
        enumOffset = RingBufferReadDataAt(Ring, enumOffset, curOutput, curDataSize);

        byteCount += curDataSize;
        curOutput += curDataSize;
    }

Exit:
    *BufferSize = byteCount;
    return status;
}


VOID
RingBufferReset(
    IN          EFI_RING_BUFFER        *Ring
    )
/*++

Routine Description:

    Resets the ring buffer removing any data present.

Arguments:

    Ring        - Ring buffer to reset.

Return Value:

    None.

--*/
{
    Ring->Head = 0;
    Ring->Tail = 0;
    Ring->HandleKey = 0;
    ZeroMem(&Ring->Stats, sizeof(Ring->Stats));
}


EFI_STATUS
RingBufferInitialize(
    IN          EFI_RING_BUFFER        *Ring,
    IN          const UINT32            Capacity,
    IN          const UINT32            Flags
    )
/*++

Routine Description:

    Initializes a pre-allocated memory region for use as a ring buffer.

Arguments:

    Ring        - Ring buffer to initialize.  Memory for the structure and buffer should
                  already be allocated.

    Capacity    - Size of the buffer in bytes.  This must be a power of 2.

    Flags       - Flags specifying the behavior of the ring buffer
                  RING_BUFFER_OVERWRITE - overwrite old records when the buffer is full.

Return Value:

    EFI_SUCCESS
    EFI_INVALID_PARAMETER the capacity is invalid.

--*/
{
    //
    // verify the capacity is a power of 2 and actually usable for something
    //
    if ((Capacity < sizeof(RING_RECORD) + 1) ||
        ((Capacity & (Capacity - 1)) != 0))
    {
        return EFI_INVALID_PARAMETER;
    }

    Ring->Size  = Capacity;
    Ring->Mask  = Capacity - 1;
    Ring->Flags = Flags;
    RingBufferReset(Ring);

    return EFI_SUCCESS;
}
