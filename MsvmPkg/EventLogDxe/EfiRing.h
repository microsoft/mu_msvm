/** @file
  Ring buffer types, definitions and public functions.
  See EfiRing.c for implementation details.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

typedef UINT64   RING_HANDLE;
typedef UINT32   RING_HANDLE_KEY;

#define INVALID_RING_HANDLE     ((UINT64)(-1))


//
// Overwrite older events if the buffer gets full
// if this is not specified, new events are dropped
// once the buffer is full.
//
#define RING_BUFFER_OVERWRITE   0x00000001


typedef struct
{
    UINT32  LostWrites;
    UINT32  Reserve;
    UINT32  Remove;
} RING_BUFFER_STATS;


typedef struct
{
    //
    // Size of the buffer, must be a power of 2
    //
    UINT32              Size;
    //
    // Mask for offset wrapping
    //
    UINT32              Mask;
    //
    // Offset within the buffer where the next record will be written
    //
    UINT32              Head;
    //
    // Offset within the buffer of the oldest record.
    //
    UINT32              Tail;
    //
    // Behavior flags, see RING_BUFFER_nnnnn
    //
    UINT32              Flags;

    RING_BUFFER_STATS   Stats;
    //
    // Used to invalidate data handles for destructive ring operation.
    //
    RING_HANDLE_KEY     HandleKey;

    UINT8               Buffer[];
} EFI_RING_BUFFER;

//
// IO operations that can be performed
// See RingBufferIo
//
typedef enum
{
    DataRead,
    DataWrite
} RING_IO_OPERATION;


EFI_STATUS
RingBufferReserve(
    IN              EFI_RING_BUFFER        *Ring,
    IN              const UINT32            DataSize,
    OUT OPTIONAL    RING_HANDLE            *DataHandle
    );


EFI_STATUS
RingBufferAdd(
    IN              EFI_RING_BUFFER        *Ring,
    IN              const VOID             *Data,
    IN              const UINT32            DataSize,
    OUT OPTIONAL    RING_HANDLE            *DataHandle
    );


EFI_STATUS
RingBufferRemove(
    IN              EFI_RING_BUFFER        *Ring,
    OUT OPTIONAL    VOID                   *Data,
    IN OUT OPTIONAL UINT32                 *DataSize
    );


EFI_STATUS
RingBufferIo(
    IN      EFI_RING_BUFFER        *Ring,
    IN      const RING_HANDLE       DataHandle,
    IN      const RING_IO_OPERATION Op,
    IN      const UINT32            Offset,
//  _When_(Op == DataRead, _Out_writes_bytes_to_(*DataSize, *DataSize))
//  _When_(Op == DataWrite, _In_bytecount_(*DataSize))
    IN OUT  VOID                   *Data,
    IN OUT  UINT32                 *DataSize
    );


EFI_STATUS
RingBufferHandleEnumerate(
    IN              const EFI_RING_BUFFER  *Ring,
    IN OUT          RING_HANDLE            *Enumerator,
    OUT             RING_HANDLE            *Item,
    OUT OPTIONAL    UINT32                 *ItemSize
    );


EFI_STATUS
RingBufferHandleIsValid(
    IN          EFI_RING_BUFFER        *Ring,
    IN          const RING_HANDLE       DataHandle
    );


VOID
RingBufferReset(
    IN          EFI_RING_BUFFER        *Ring
    );


EFI_STATUS
RingBufferFlatten(
    IN          const EFI_RING_BUFFER  *Ring,
    IN OUT      UINT32                 *BufferSize,
    OUT         VOID                   *Buffer
    );

EFI_STATUS
RingBufferInitialize(
    IN          EFI_RING_BUFFER        *Ring,
    IN          const UINT32            Capacity,
    IN          const UINT32            Flags
    );
