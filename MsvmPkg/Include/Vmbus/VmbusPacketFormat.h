/** @file
  This file contains the structures that defines the format of the vmbus
  packets.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

// allow nameless unions
#pragma warning(push)
#pragma warning(disable : 4201)

typedef enum _ENDPOINT_TYPE {
    VmbusServerEndpoint = 0,
    VmbusClientEndpoint,
    VmbusEndpointMaximum
} ENDPOINT_TYPE, *PENDPOINT_TYPE;

#pragma pack(push, 1)

//
// The VM ring control block is the control region for one direction of
// an endpoint. It is always page aligned.
//

typedef struct _VMRCB
{
    UINT32 In;        // Offset in bytes from the ring base
    UINT32 Out;       // Offset in bytes from the ring base

    //
    // If the receiving endpoint sets this to some non-zero value, the sending
    // endpoint should not send any interrupts.
    //

    UINT32 InterruptMask;

    //
    // If the sending endpoint sets this to a non-zero value, the receiving
    // endpoint should send an interrupt when the free byte count is greater
    // than this value.
    //

    UINT32 PendingSendSize;

    UINT32 Reserved[12];

    union
    {
        struct {
            UINT32 SupportsPendingSendSize:1;
        };
        UINT32 Value;
    } FeatureBits;

} VMRCB, *PVMRCB;

static_assert(OFFSET_OF(VMRCB, FeatureBits) == 64);

//
// This structure defines a range in guest physical space that can be made
// to look virtually contiguous.
//

typedef struct _GPA_RANGE
{
    UINT32  ByteCount;
    UINT32  ByteOffset;
    UINT64  PfnArray[1];

} GPA_RANGE, *PGPA_RANGE;

#define GPA_RANGE_MAX_PFN_COUNT 0xfffff

typedef struct _VMPACKET_DESCRIPTOR
{
    UINT16 Type;
    UINT16 DataOffset8;
    UINT16 Length8;
    UINT16 Flags;
    UINT64 TransactionId;
} VMPACKET_DESCRIPTOR, *PVMPACKET_DESCRIPTOR;

typedef union _PREVIOUS_PACKET_OFFSET
{
    struct
    {
        UINT32 Reserved;
        UINT32 Offset;
    };

    UINT64 AsUINT64;
} PREVIOUS_PACKET_OFFSET, *PPREVIOUS_PACKET_OFFSET;

typedef struct _VMTRANSFER_PAGE_RANGE
{
    UINT32  ByteCount;
    UINT32  ByteOffset;
} VMTRANSFER_PAGE_RANGE, *PVMTRANSFER_PAGE_RANGE;

typedef struct _VMTRANSFER_PAGE_PACKET_HEADER
{
    VMPACKET_DESCRIPTOR     Descriptor;
    UINT16                  TransferPageSetId;
    BOOLEAN                 SenderOwnsSet;
    UINT8                   Reserved;
    UINT32                  RangeCount;
    VMTRANSFER_PAGE_RANGE   Ranges[1];

} VMTRANSFER_PAGE_PACKET_HEADER, *PVMTRANSFER_PAGE_PACKET_HEADER;

//
// This is the format for a GPA-Direct packet, which contains a set of GPA
// ranges, in addition to commands and/or data.
//

typedef struct _VMDATA_GPA_DIRECT
{
    VMPACKET_DESCRIPTOR Descriptor;
    UINT32              Reserved;
    UINT32              RangeCount;
    GPA_RANGE           Range[1];

} VMDATA_GPA_DIRECT, *PVMDATA_GPA_DIRECT;

typedef enum _VMPIPE_PROTOCOL_MESSAGE_TYPE
{
    VmPipeMessageInvalid = 0,
    VmPipeMessageData = 1,
    VmPipeMessagePartial = 2,
    VmPipeMessageSetupGpaDirect = 3,
    VmPipeMessageTeardownGpaDirect = 4,
    VmPipeMessageIndicationComplete = 5,
} VMPIPE_PROTOCOL_MESSAGE_TYPE, *PVMPIPE_PROTOCOL_MESSAGE_TYPE;

typedef struct _VMPIPE_PROTOCOL_HEADER
{
    VMPIPE_PROTOCOL_MESSAGE_TYPE PacketType;
    union
    {
        UINT32 DataSize;

        struct
        {
            UINT16 DataSize;
            UINT16 Offset;
        } Partial;
    };
} VMPIPE_PROTOCOL_HEADER, *PVMPIPE_PROTOCOL_HEADER;

typedef struct _VMPIPE_SETUP_GPA_DIRECT_BODY
{
    UINT32 Handle;

    BOOLEAN IsWritable;

    UINT32              RangeCount;
    GPA_RANGE           Range[1];

} VMPIPE_SETUP_GPA_DIRECT_BODY, *PVMPIPE_SETUP_GPA_DIRECT_BODY;

typedef struct _VMPIPE_TEARDOWN_GPA_DIRECT_BODY
{
    UINT32 Handle;

} VMPIPE_TEARDOWN_GPA_DIRECT_BODY, *PVMPIPE_TEARDOWN_GPA_DIRECT_BODY;


typedef enum _VMBUS_PACKET_TYPE
{
    VmbusPacketTypeInvalid                      = 0x0,
    //
    // 1 through 5 are reserved.
    //
    VmbusPacketTypeDataInBand                   = 0x6,
    VmbusPacketTypeDataUsingTransferPages       = 0x7,
    //
    // 8 is reserved.
    //
    VmbusPacketTypeDataUsingGpaDirect           = 0x9,
    VmbusPacketTypeCancelRequest                = 0xa,
    VmbusPacketTypeCompletion                   = 0xb,
} VMBUS_PACKET_TYPE, *PVMBUS_PACKET_TYPE;

#define VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED    1

#pragma pack(pop)

typedef struct _VMTRANSFER_PAGE_RANGES
{
    struct _VMTRANSFER_PAGE_RANGES *Next;
    UINT32                          RangeCount;
    VMTRANSFER_PAGE_RANGE           Range[1];

} VMTRANSFER_PAGE_RANGES, *PVMTRANSFER_PAGE_RANGES;

#pragma warning(pop)


