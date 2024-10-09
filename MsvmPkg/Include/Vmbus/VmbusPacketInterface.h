/** @file
  This file contains the structures that define the transport-related
  interfaces exported by the VMBus driver.

  Copyright (c) Microsoft Corporation.
  Licensed under the BSD-2-Clause-Patent license.
--*/

#pragma once

#include <Vmbus/VmbusPacketFormat.h>

#define EFI_RING_CORRUPT_ERROR              ENCODE_ERROR(0x00000102L)

#define EFI_RING_NEWLY_EMPTY                ENCODE_WARNING(0x00000213L)
#define EFI_RING_SIGNAL_OPPOSITE_ENDPOINT   ENCODE_WARNING(0x00000214L)


//
// These structures should all be treated as opaque. Use the accessor methods
// below.
//

//
// The entire ring context is immutable.
//

typedef struct _PACKET_RING_CONTEXT
{
               VMRCB *          Control;
    volatile UINT8 *            Data;
    UINT32                      DataBytesInRing;
} PACKET_RING_CONTEXT, *PPACKET_RING_CONTEXT;

#pragma warning(disable : 4324) // 4324 - structure was padded due to __declspec(align())
typedef struct _PACKET_LIB_CONTEXT
{
    //
    // R/O or near R/O fields. Try to keep these together in a cache line.
    //

    PACKET_RING_CONTEXT     Outgoing;
    PACKET_RING_CONTEXT     Incoming;

    //
    // Incoming loop mutable fields. Keep these on their own cache line.
    //

    __declspec(align(64))
    UINT32                  IncomingInCache;
    UINT32                  IncomingOut;
    UINT32                  EmptyRingBufferCount;
    UINT32                  NonspuriousInterruptCount;

    //
    // Outgoing loop mutable fields. Keep these on their own cache line.
    //

    __declspec(align(64))
    UINT32                  OutgoingIn;
    UINT32                  OutgoingOutCache;
    UINT32                  PendingSendSize;
    UINT32                  FullRingBufferCount;
    UINT64                  StaticInterruptMaskSkips;
    UINT64*                 InterruptMaskSkips;

} PACKET_LIB_CONTEXT, *PPACKET_LIB_CONTEXT;
#pragma warning(default : 4324)

typedef PPACKET_LIB_CONTEXT PACKET_LIB_HANDLE;


EFI_STATUS
PkInitializeDoubleMappedRingBuffer(
    OUT PPACKET_LIB_CONTEXT Context,
    IN  VOID* IncomingControl,
    IN  VOID* IncomingDataPages,
    IN  UINT32 IncomingDataPageCount,
    IN  VOID* OutgoingControl,
    IN  VOID* OutgoingDataPages,
    IN  UINT32 OutgoingDataPageCount
    );

EFI_STATUS
PkInitializeRingBuffer(
    OUT PPACKET_LIB_CONTEXT Context,
    IN  VOID* IncomingControl,
    IN  VOID* IncomingDataPages,
    IN  UINT32 IncomingDataPageCount,
    IN  VOID* OutgoingControl,
    IN  VOID* OutgoingDataPages,
    IN  UINT32 OutgoingDataPageCount
    );

VOID
PkUninitializeRingBuffer(
    IN  PPACKET_LIB_CONTEXT PkLibContext
    );

EFI_STATUS
PkInit(
    IN  VOID*                  RingBufferPages,
    IN  UINT32                 RingBufferPageCount,
    IN  UINT32                 ClientToServerPages,
    IN  ENDPOINT_TYPE          EndpointType,
    IN  UINT32                 IncomingTransactionQuota OPTIONAL,
    OUT PACKET_LIB_HANDLE      *PkLibContext
    );

VOID
PkCleanup(
    IN  PACKET_LIB_HANDLE PkLibContext
    );

static_assert(OFFSET_OF(VMPACKET_DESCRIPTOR, Type) < 8,
    "VMPACKET_DESCRIPTOR->Type is assumed to be within first 8 bytes of the structure.");
static_assert(OFFSET_OF(VMPACKET_DESCRIPTOR, DataOffset8) < 8,
    "VMPACKET_DESCRIPTOR->DataOffset8 is assumed to be within first 8 bytes of the structure.");
static_assert(OFFSET_OF(VMPACKET_DESCRIPTOR, Length8) < 8,
    "VMPACKET_DESCRIPTOR->Length8 is assumed to be within first 8 bytes of the structure.");
static_assert(OFFSET_OF(VMPACKET_DESCRIPTOR, Flags) < 8,
    "VMPACKET_DESCRIPTOR->Flags is assumed to be within first 8 bytes of the structure.");

#define PkWriteRingBuffer(_LibContext_,_Dest_,_Src_,_Length_) \
    PkWritePacketSingleMapped((_LibContext_), \
        (_Src_), \
        (_Length_), \
        (UINT32)((UINT8*)(_Dest_) - (UINT8*)((_LibContext_)->Outgoing.Data))); \

#define PkReadRingBuffer(_LibContext_,_Dest_,_Src_,_Length_) \
    PkReadPacketSingleMapped((_LibContext_), \
        (_Dest_), \
        (_Length_), \
        (UINT32)((UINT8*)(_Src_) - (UINT8*)((_LibContext_)->Incoming.Data))); \

#define PkWriteRingBufferField(singledest, singlesrc) \
    { \
        UINT64 _local_value_ = (singlesrc); \
        static_assert(sizeof((singledest)) <= 8, "PkWriteRingBufferField requires the field to be <= size 8"); \
        PkWriteRingBuffer(PkLibContext, &(singledest), &_local_value_, sizeof((singlesrc))); \
    }

VOID
PkWritePacketSingleMapped(
    IN  PPACKET_LIB_CONTEXT PkLibContext,
    IN  VOID*               PacketBuf,
    IN  UINT32              PacketBufSize,
    IN  UINT32              Offset
    );

VOID
PkReadPacketSingleMapped(
    IN  PPACKET_LIB_CONTEXT     PkLibContext,
    OUT VOID*                   PacketBuf,
    IN  UINT32                  PacketBufSize,
    IN  UINT32                  Out
    );

EFI_STATUS
PkInitializeSingleMappedRingBuffer(
    OUT PPACKET_LIB_CONTEXT Context,
    IN  VOID* IncomingControl,
    IN  VOID* IncomingDataPages,
    IN  UINT32 IncomingDataPageCount,
    IN  VOID* OutgoingControl,
    IN  VOID* OutgoingDataPages,
    IN  UINT32 OutgoingDataPageCount
    );

#define PkSendPacketSingleMapped PkSendPacketRaw

EFI_STATUS
PkSendPacketRaw(
    IN  PACKET_LIB_HANDLE  PkLibContext,
    IN  VOID*              PacketBuf,
    IN  UINT32             PacketBufSize
    );

EFI_STATUS
PkGetReceiveBuffer(
    IN      PACKET_LIB_HANDLE PkLibContext,
    IN OUT  UINT32* Offset,
    OUT     VOID* *Buffer,
    OUT     UINT32* Length
    );

EFI_STATUS
PkGetSendBuffer(
    IN      PACKET_LIB_HANDLE PkLibContext,
    IN OUT  UINT32* Offset,
    IN      UINT32 PacketSize,
    OUT     VOID* *Buffer
    );

UINT32
PkGetOutgoingRingSize(
    IN  PACKET_LIB_HANDLE PkLibContext
    );

UINT32
PkGetOutgoingRingFreeBytes(
    IN  PACKET_LIB_HANDLE PkLibContext
    );

UINT32
PkGetIncomingRingOffset(
    IN  PACKET_LIB_HANDLE PkLibContext
    );

UINT32
PkGetOutgoingRingOffset(
    IN  PACKET_LIB_HANDLE PkLibContext
    );

EFI_STATUS
PkCompleteRemoval(
    IN   PACKET_LIB_HANDLE   PkLibContext,
    IN   UINT32                NewOut
    );

EFI_STATUS
PkCompleteInsertion(
    IN   PACKET_LIB_HANDLE PkLibContext,
    IN   UINT32            NewIn
    );

