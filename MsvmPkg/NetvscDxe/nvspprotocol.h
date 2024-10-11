/** @file
    This file contains the protocol used by the network VSP/VSC. This protocol
    defines the messages that are sent through the VMBus ring buffer established
    during the channel offer from the VSP to the VSC. The small size of this
    protocol is possible because most of the work for facilitating a network
    connection is handled by the RNDIS protocol.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

#define NVSP_INVALID_PROTOCOL_VERSION           ((UINT32)0xFFFFFFFF)

#define NVSP_PROTOCOL_MAJOR(VERSION_)           (((VERSION_) >> 16) & 0xFFFF)
#define NVSP_PROTOCOL_MINOR(VERSION_)           (((VERSION_)      ) & 0xFFFF)

#define NVSP_PROTOCOL_VERSION(MAJOR_, MINOR_)   ((((MAJOR_) & 0xFFFF) << 16) | (((MINOR_) & 0xFFFF)))

#define NVSP_PROTOCOL_VERSION_1                 NVSP_PROTOCOL_VERSION(0,2)
#define NVSP_PROTOCOL_VERSION_2                 NVSP_PROTOCOL_VERSION(3,2)
#define NVSP_PROTOCOL_VERSION_4                 NVSP_PROTOCOL_VERSION(4,0)
#define NVSP_PROTOCOL_VERSION_5                 NVSP_PROTOCOL_VERSION(5,0)
#define NVSP_PROTOCOL_VERSION_CURRENT           NVSP_PROTOCOL_VERSION_5

#define NVSP_PROTOCOL_VERSION_IS_VALID(_Version_) \
    ((_Version_) == NVSP_PROTOCOL_VERSION_5 || (_Version_) == NVSP_PROTOCOL_VERSION_4 || \
     (_Version_) == NVSP_PROTOCOL_VERSION_2 || (_Version_) == NVSP_PROTOCOL_VERSION_1)

#define NVSP_OPERATIONAL_STATUS_OK              ((UINT32)0x00000000)
#define NVSP_OPERATIONAL_STATUS_DEGRADED        ((UINT32)0x00000001)
#define NVSP_OPERATIONAL_STATUS_NONRECOVERABLE  ((UINT32)0x00000002)
#define NVSP_OPERATIONAL_STATUS_NO_CONTACT      ((UINT32)0x00000003)
#define NVSP_OPERATIONAL_STATUS_LOST_COMMUNICATION ((UINT32)0x00000004)

//
// The maximun number of transfer pages (packets) the VSP will use on on a receive
//
#define NVSP_MAX_PACKETS_PER_RECEIVE            375

//
// Defines the maximum number of processors that can be used by a single VMQ's
// traffic. We are storing this value here because both the VM and host needs
// it to manage the vRSS indirection table (VM needs it for send and host
// needs it for receive).
//
#define VMS_SWITCH_RSS_MAX_RSS_PROC_COUNT       16

typedef enum _NVSP_MESSAGE_TYPE
{
    NvspMessageTypeNone = 0,

    //
    // Init Messages
    //
    NvspMessageTypeInit                         = 1,
    NvspMessageTypeInitComplete                 = 2,

    NvspVersionMessageStart                     = 100,

    //
    // Version 1 Messages
    //
    NvspMessage1TypeSendNdisVersion             = NvspVersionMessageStart,

    NvspMessage1TypeSendReceiveBuffer,
    NvspMessage1TypeSendReceiveBufferComplete,
    NvspMessage1TypeRevokeReceiveBuffer,

    NvspMessage1TypeSendSendBuffer,
    NvspMessage1TypeSendSendBufferComplete,
    NvspMessage1TypeRevokeSendBuffer,

    NvspMessage1TypeSendRNDISPacket,
    NvspMessage1TypeSendRNDISPacketComplete,

    //
    // The maximum allowed message ID for the v1 protocol.
    //
    NvspMessage1Max = NvspMessage1TypeSendRNDISPacketComplete,

    //
    // Version 2 messages
    //
    NvspMessage2TypeSendChimneyDelegatedBuffer,
    NvspMessage2TypeSendChimneyDelegatedBufferComplete,
    NvspMessage2TypeRevokeChimneyDelegatedBuffer,

    NvspMessage2TypeResumeChimneyRXIndication,

    NvspMessage2TypeTerminateChimney,
    NvspMessage2TypeTerminateChimneyComplete,

    NvspMessage2TypeIndicateChimneyEvent,

    NvspMessage2TypeSendChimneyPacket,
    NvspMessage2TypeSendChimneyPacketComplete,

    NvspMessage2TypePostChimneyRecvRequest,
    NvspMessage2TypePostChimneyRecvRequestComplete,

    NvspMessage2TypeAllocateReceiveBufferDeprecated,
    NvspMessage2TypeAllocateReceiveBufferCompleteDeprecated,

    NvspMessage2TypeFreeReceiveBufferDeprecated,

    NvspMessage2SendVmqRndisPacketDeprecated,
    NvspMessage2SendVmqRndisPacketCompleteDeprecated,

    NvspMessage2TypeSendNdisConfig,

    NvspMessage2TypeAllocateChimneyHandle,
    NvspMessage2TypeAllocateChimneyHandleComplete,

    //
    // The maximum allowed message ID for the v2 protocol.
    //
    NvspMessage2Max = NvspMessage2TypeAllocateChimneyHandleComplete,

    //
    // Version 4 messages
    //
    NvspMessage4TypeSendVFAssociation,
    NvspMessage4TypeSwitchDataPath,

    //
    // Needed so that Win8 RC+ VMs don't AV when running on a Win8 Beta Host
    //
    NvspMessage4TypeUplinkConnectStateDeprecated,

    //
    // The maximum allowed message ID for the v4 protocol.
    //
    NvspMessage4Max = NvspMessage4TypeUplinkConnectStateDeprecated,

    //
    // Version 5 messages
    //
    NvspMessage5TypeOidQueryEx,
    NvspMessage5TypeOidQueryExComplete,
    NvspMessage5TypeSubChannel,
    NvspMessage5TypeSendIndirectionTable,

    //
    // The maximum allowed message ID for the v5 protocol.
    //
    NvspMessage5Max = NvspMessage5TypeSendIndirectionTable
} NVSP_MESSAGE_TYPE, *PNVSP_MESSAGE_TYPE;

#define NVSP_PROTOCOL_VERSION_1_HANDLER_COUNT \
    ((NvspMessage1Max  - NvspVersionMessageStart) + 1)

#define NVSP_PROTOCOL_VERSION_2_HANDLER_COUNT \
    ((NvspMessage2Max  - NvspVersionMessageStart) + 1)

#define NVSP_PROTOCOL_VERSION_4_HANDLER_COUNT \
    ((NvspMessage4Max  - NvspVersionMessageStart) + 1)

#define NVSP_PROTOCOL_VERSION_5_HANDLER_COUNT \
    ((NvspMessage5Max  - NvspVersionMessageStart) + 1)

typedef enum _NVSP_STATUS
{
    NvspStatusNone = 0,
    NvspStatusSuccess,
    NvspStatusFailure,
    NvspStatusDeprecated1,  // was NvspStatusProtocolVersionRangeTooNew,
    NvspStatusDeprecated2,  // was NvspStatusProtocolVersionRangeTooOld,
    NvspStatusInvalidRndisPacket,
    NvspStatusBusy,
    NvspStatusProtocolVersionUnsupported,
    NvspStatusMax,
} NVSP_STATUS, *PNVSP_STATUS;

#pragma pack(push, 1)

typedef struct _NVSP_MESSAGE_HEADER
{
    UINT32                                  MessageType;
} NVSP_MESSAGE_HEADER, *PNVSP_MESSAGE_HEADER;


//
// The following base NDIS type is referenced by nvspprotocol.h.  See
// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/objectheader/ns-objectheader-ndis_object_header
//
typedef struct _NDIS_OBJECT_HEADER
{
    UINT8   Type;
    UINT8   Revision;
    UINT16  Size;
} NDIS_OBJECT_HEADER, *PNDIS_OBJECT_HEADER;

typedef UINT32 GPADL_HANDLE;

//
// Init Messages
//

//
// This message is used by the VSC to initialize the channel
// after the channels has been opened. This message should
// never include anything other then versioning (i.e. this
// message will be the same for ever).
//
// For ever is a long time.  The values have been redefined
// in Win7 to indicate major and minor protocol version
// number.
//
#pragma warning(disable : 4201)
typedef struct _NVSP_MESSAGE_INIT
{
    union
    {
        struct
        {
            UINT16                          MinorProtocolVersion;
            UINT16                          MajorProtocolVersion;
        };
        UINT32                              ProtocolVersion;            // was MinProtocolVersion
    };
    UINT32                                  ProtocolVersion2;           // was MaxProtocolVersion
} NVSP_MESSAGE_INIT, *PNVSP_MESSAGE_INIT;
#pragma warning(default : 4201)

//
// This message is used by the VSP to complete the initialization
// of the channel. This message should never include anything other
// then versioning (i.e. this message will be the same for ever).
//
typedef struct _NVSP_MESSAGE_INIT_COMPLETE
{
    UINT32                                  Deprecated;  // was NegotiatedProtocolVersion (2) in Win6
    UINT32                                  MaximumMdlChainLength;
    UINT32                                  Status;
} NVSP_MESSAGE_INIT_COMPLETE, *PNVSP_MESSAGE_INIT_COMPLETE;

typedef union _NVSP_MESSAGE_INIT_UBER
{
    NVSP_MESSAGE_INIT                       Init;
    NVSP_MESSAGE_INIT_COMPLETE              InitComplete;
} NVSP_MESSAGE_INIT_UBER;

//
// Version 1 Messages
//

//
// This message is used by the VSC to send the NDIS version
// to the VSP. The VSP can use this information when handling
// OIDs sent by the VSC.
//
typedef struct _NVSP_1_MESSAGE_SEND_NDIS_VERSION
{
    UINT32                                  NdisMajorVersion;
    UINT32                                  NdisMinorVersion;
} NVSP_1_MESSAGE_SEND_NDIS_VERSION, *PNVSP_1_MESSAGE_SEND_NDIS_VERSION;

//
// This message is used by the VSC to send a receive buffer
// to the VSP. The VSP can then use the receive buffer to
// send data to the VSC.
//
typedef struct _NVSP_1_MESSAGE_SEND_RECEIVE_BUFFER
{
    GPADL_HANDLE                            GpadlHandle;
    UINT16                                  Id;
} NVSP_1_MESSAGE_SEND_RECEIVE_BUFFER, *PNVSP_1_MESSAGE_SEND_RECEIVE_BUFFER;

typedef struct _NVSP_1_RECEIVE_BUFFER_SECTION
{
    UINT32                                  Offset;
    UINT32                                  SubAllocationSize;
    UINT32                                  NumSubAllocations;
    UINT32                                  EndOffset;
} NVSP_1_RECEIVE_BUFFER_SECTION, *PNVSP_1_RECEIVE_BUFFER_SECTION;

//
// This message is used by the VSP to acknowledge a receive
// buffer send by the VSC. This message must be sent by the
// VSP before the VSP uses the receive buffer.
//
typedef struct _NVSP_1_MESSAGE_SEND_RECEIVE_BUFFER_COMPLETE
{
    UINT32                                  Status;
    UINT32                                  NumSections;

    //
    // The receive buffer is split into two parts, a large
    // suballocation section and a small suballocation
    // section. These sections are then suballocated by a
    // certain size.
    //
    // For example, the following break up of the receive
    // buffer has 6 large suballocations and 10 small
    // suballocations.
    //
    // |            Large Section          |  |   Small Section   |
    // ------------------------------------------------------------
    // |     |     |     |     |     |     |  | | | | | | | | | | |
    // |                                      |
    // LargeOffset                            SmallOffset
    //
    NVSP_1_RECEIVE_BUFFER_SECTION           Sections[1];

} NVSP_1_MESSAGE_SEND_RECEIVE_BUFFER_COMPLETE, *PNVSP_1_MESSAGE_SEND_RECEIVE_BUFFER_COMPLETE;

//
// This message is sent by the VSC to revoke the receive buffer.
// After the VSP completes this transaction, the vsp should never
// use the receive buffer again.
//
typedef struct _NVSP_1_MESSAGE_REVOKE_RECEIVE_BUFFER
{
    UINT16                                  Id;
} NVSP_1_MESSAGE_REVOKE_RECEIVE_BUFFER, *PNVSP_1_MESSAGE_REVOKE_RECEIVE_BUFFER;

//
// This message is used by the VSC to send a send buffer
// to the VSP. The VSC can then use the send buffer to
// send data to the VSP.
//
typedef struct _NVSP_1_MESSAGE_SEND_SEND_BUFFER
{
    GPADL_HANDLE                            GpadlHandle;
    UINT16                                  Id;
} NVSP_1_MESSAGE_SEND_SEND_BUFFER, *PNVSP_1_MESSAGE_SEND_SEND_BUFFER;

//
// This message is used by the VSP to acknowledge a send
// buffer sent by the VSC. This message must be sent by the
// VSP before the VSP uses the sent buffer.
//
typedef struct _NVSP_1_MESSAGE_SEND_SEND_BUFFER_COMPLETE
{
    UINT32                                  Status;

    //
    // The VSC gets to choose the size of the send buffer and
    // the VSP gets to choose the sections size of the buffer.
    // This was done to enable dynamic reconfigurations when
    // the cost of GPA-direct buffers decreases.
    //
    UINT32                                  SectionSize;
} NVSP_1_MESSAGE_SEND_SEND_BUFFER_COMPLETE, *PNVSP_1_MESSAGE_SEND_SEND_BUFFER_COMPLETE;

//
// This message is sent by the VSC to revoke the send buffer.
// After the VSP completes this transaction, the vsp should never
// use the send buffer again.
//
typedef struct _NVSP_1_MESSAGE_REVOKE_SEND_BUFFER
{
    UINT16                                  Id;
} NVSP_1_MESSAGE_REVOKE_SEND_BUFFER, *PNVSP_1_MESSAGE_REVOKE_SEND_BUFFER;

//
// This message is used by both the VSP and the VSC to send
// a RNDIS message to the opposite channel endpoint.
//
typedef struct _NVSP_1_MESSAGE_SEND_RNDIS_PACKET
{
    //
    // This field is specified by RNIDS. They assume there's
    // two different channels of communication. However,
    // the Network VSP only has one. Therefore, the channel
    // travels with the RNDIS packet.
    //
    UINT32                                  ChannelType;

    //
    // This field is used to send part or all of the data
    // through a send buffer. This values specifies an
    // index into the send buffer. If the index is
    // 0xFFFFFFFF, then the send buffer is not being used
    // and all of the data was sent through other VMBus
    // mechanisms.
    //
    UINT32                                  SendBufferSectionIndex;
    UINT32                                  SendBufferSectionSize;
} NVSP_1_MESSAGE_SEND_RNDIS_PACKET, *PNVSP_1_MESSAGE_SEND_RNDIS_PACKET;

//
// This message is used by both the VSP and the VSC to complete
// a RNDIS message to the opposite channel endpoint. At this
// point, the initiator of this message cannot use any resources
// associated with the original RNDIS packet.
//
typedef struct _NVSP_1_MESSAGE_SEND_RNDIS_PACKET_COMPLETE
{
    UINT32                                  Status;
} NVSP_1_MESSAGE_SEND_RNDIS_PACKET_COMPLETE, *PNVSP_1_MESSAGE_SEND_RNDIS_PACKET_COMPLETE;

//
// This message is used by the VSC to send the NDIS version
// to the VSP. The VSP can use this information when handling
// OIDs sent by the VSC.
//
#pragma warning(disable : 4201)
typedef struct _NVSP_2_NETVSC_CAPABILITIES
{
    union
    {
        UINT64                              AsUINT64;
        struct
        {
            UINT64                          VMQ : 1;
            UINT64                          Chimney : 1;
            UINT64                          SRIOV : 1;
            UINT64                          Ieee8021q : 1;
            UINT64                          CorrelationId : 1;
            UINT64                          Teaming : 1;
            UINT64                          VirtualSubnetId : 1;
        };
    };
} NVSP_2_NETVSC_CAPABILITIES, *PNVSP_2_NETVSC_CAPABILITIES;
#pragma warning(default : 4201)

typedef struct _NVSP_2_MESSAGE_SEND_NDIS_CONFIG
{
    UINT32                                  MTU;
    UINT32                                  Reserved;
    NVSP_2_NETVSC_CAPABILITIES              Capabilities;
} NVSP_2_MESSAGE_SEND_NDIS_CONFIG, *PNVSP_2_MESSAGE_SEND_NDIS_CONFIG;

//
// This structure is used in defining the buffers in
// NVSP_2_MESSAGE_SEND_VMQ_RNDIS_PACKET structure
//
typedef struct _NVSP_TRANSFER_PAGE_RANGE
{
    //
    // Specifies the ID of the receive buffer that has the buffer. This
    // ID can be the general receive buffer ID specified in
    // NvspMessage1TypeSendReceiveBuffer or it can be the shared memory receive
    // buffer ID allocated by the VSC and specified in
    // NvspMessage2TypeAllocateReceiveBufferComplete message
    //
    UINT64				TransferPageSetId;

    //
    // Number of bytes
    //
    UINT32				ByteCount;

    //
    // Offset in bytes from the beginning of the buffer
    //
    UINT32				ByteOffset;
} NVSP_TRANSFER_PAGE_RANGE, *PNVSP_TRANSFER_PAGE_RANGE;

//
// NvspMessage4TypeSendVFAssociation
//
typedef struct _NVSP_4_MESSAGE_SEND_VF_ASSOCIATION
{
    //
    // Specifies whether VF is allocated for this channel
    // If 1, SerialNumber of the VF is specified.
    // If 0, ignore SerialNumber
    //
    UINT32                              VFAllocated;

    //
    // Serial number of the VF to team with
    //
    UINT32                              SerialNumber;
} NVSP_4_MESSAGE_SEND_VF_ASSOCIATION,
  *PNVSP_4_MESSAGE_SEND_VF_ASSOCIATION;

//
// This enum is used in specifying the active data path
// in NVSP_4_MESSAGE_SWITCH_DATA_PATH structure
//
typedef enum _NVSP_VM_DATA_PATH
{
    NvspDataPathSynthetic = 0,
    NvspDataPathVF,
    NvspDataPathMax
} NVSP_VM_DATA_PATH, *PNVSP_VM_DATA_PATH;

//
// NvspMessage4TypeSwitchDataPath
//
typedef struct _NVSP_4_MESSAGE_SWITCH_DATA_PATH
{
    //
    // Specifies the current data path that is active in the VM
    //
    NVSP_VM_DATA_PATH                           ActiveDataPath;
} NVSP_4_MESSAGE_SWITCH_DATA_PATH, *PNVSP_4_MESSAGE_SWITCH_DATA_PATH;

//
// NvspMessage5TypeOidQueryEx
//
typedef UINT32 NDIS_OID, *PNDIS_OID;

typedef struct _NVSP_5_MESSAGE_OID_QUERY_EX
{
    //
    // Header information for the Query OID
    //
    NDIS_OBJECT_HEADER Header;
    //
    // OID being queried
    //
    NDIS_OID Oid;
} NVSP_5_MESSAGE_OID_QUERY_EX, *PNVSP_5_MESSAGE_OID_QUERY_EX;

//
// NvspMessage5TypeOidQueryExComplete
//
typedef int NDIS_STATUS, *PNDIS_STATUS;

#pragma warning(disable : 4201)
typedef struct _NVSP_5_MESSAGE_OID_QUERY_EX_COMPLETE
{
    //
    // Result of the query.
    //
    NDIS_STATUS Status;
    union
    {
        //
        // Bytes written to the buffer if query is successful
        //
        UINT32 BytesWritten;
        //
        // Bytes needed if Status if NDIS_STATUS_BUFFER_TOO_SHORT;
        //
        UINT32 BytesNeeded;
    };
} NVSP_5_MESSAGE_OID_QUERY_EX_COMPLETE, *PNVSP_5_MESSAGE_OID_QUERY_EX_COMPLETE;
#pragma warning(default : 4201)

//
// This defines the subchannel requests we can send to the host. We don't need
// the deallocate operation here as when the primary channel closes, the
// subchannels will be closed and we are cleaning up them based on their
// primary channel's channel close callback.
//
typedef enum _NVSP_SUBCHANNEL_OPERATION
{
    NvspSubchannelNone = 0,
    NvspSubchannelAllocate,
    NvspSubchannelMax,
} NVSP_SUBCHANNEL_OPERATION, *PNVSP_SUBCHANNEL_OPERATION;

//
// NvspMessage5TypeSubChannel
//
typedef struct _NVSP_5_MESSAGE_SUBCHANNEL_REQUEST
{
    //
    // The subchannel operation
    //
    NVSP_SUBCHANNEL_OPERATION                   Operation;

    //
    // The number of subchannels to create, if it is a NvspSubchannelAllocate
    // operation.
    //
    UINT32                                      NumSubChannels;
} NVSP_5_MESSAGE_SUBCHANNEL_REQUEST, *PNVSP_5_MESSAGE_SUBCHANNEL_REQUEST;

typedef struct _NVSP_5_MESSAGE_SUBCHANNEL_COMPLETE
{
    //
    // The status of the subchannel operation in NT STATUS code
    //
    UINT32                                      Status;

    //
    // The actual number of subchannels allocated.
    //
    UINT32                                      NumSubChannels;
} NVSP_5_MESSAGE_SUBCHANNEL_COMPLETE, *PNVSP_5_MESSAGE_SUBCHANNEL_COMPLETE;

//
// NvspMessage5TypeSendIndirectionTable
//
typedef struct _NVSP_5_MESSAGE_SEND_INDIRECTION_TABLE
{
    //
    // The number of entries in the send indirection table.
    //
    UINT32 TableEntryCount;

    //
    // The offset of the send indireciton table.
    // The send indirection table tells which channel to put the send traffic
    // on. Each entry is a channel number.
    //
    UINT32 TableOffset;
} NVSP_5_MESSAGE_SEND_INDIRECTION_TABLE, *PNVSP_5_MESSAGE_SEND_INDIRECTION_TABLE;

//
// NVSP Messages
//
typedef union _NVSP_MESSAGE_1_UBER
{
    NVSP_1_MESSAGE_SEND_NDIS_VERSION            SendNdisVersion;

    NVSP_1_MESSAGE_SEND_RECEIVE_BUFFER          SendReceiveBuffer;
    NVSP_1_MESSAGE_SEND_RECEIVE_BUFFER_COMPLETE SendReceiveBufferComplete;
    NVSP_1_MESSAGE_REVOKE_RECEIVE_BUFFER        RevokeReceiveBuffer;

    NVSP_1_MESSAGE_SEND_SEND_BUFFER             SendSendBuffer;
    NVSP_1_MESSAGE_SEND_SEND_BUFFER_COMPLETE    SendSendBufferComplete;
    NVSP_1_MESSAGE_REVOKE_SEND_BUFFER           RevokeSendBuffer;

    NVSP_1_MESSAGE_SEND_RNDIS_PACKET            SendRNDISPacket;
    NVSP_1_MESSAGE_SEND_RNDIS_PACKET_COMPLETE   SendRNDISPacketComplete;

} NVSP_1_MESSAGE_UBER;

typedef union _NVSP_MESSAGE_2_UBER
{
    NVSP_2_MESSAGE_SEND_NDIS_CONFIG SendNdisConfig;

} NVSP_2_MESSAGE_UBER;

typedef union _NVSP_MESSAGE_4_UBER
{
    NVSP_4_MESSAGE_SEND_VF_ASSOCIATION          VFAssociation;
    NVSP_4_MESSAGE_SWITCH_DATA_PATH             SwitchDataPath;
} NVSP_4_MESSAGE_UBER;

typedef union _NVSP_MESSAGE_5_UBER
{
    NVSP_5_MESSAGE_OID_QUERY_EX                 OidQueryEx;
    NVSP_5_MESSAGE_OID_QUERY_EX_COMPLETE        OidQueryExComplete;
    NVSP_5_MESSAGE_SUBCHANNEL_REQUEST           SubChannelRequest;
    NVSP_5_MESSAGE_SUBCHANNEL_COMPLETE          SubChannelRequestComplete;
    NVSP_5_MESSAGE_SEND_INDIRECTION_TABLE       SendTable;
} NVSP_5_MESSAGE_UBER;


typedef union _NVSP_ALL_MESSAGES
{
    NVSP_MESSAGE_INIT_UBER                  InitMessages;
    NVSP_1_MESSAGE_UBER                     Version1Messages;
    NVSP_2_MESSAGE_UBER                     Version2Messages;
    NVSP_4_MESSAGE_UBER                     Version4Messages;
    NVSP_5_MESSAGE_UBER                     Version5Messages;
} NVSP_ALL_MESSAGES;

//
// ALL Messages
//
typedef struct _NVSP_MESSAGE
{
    NVSP_MESSAGE_HEADER                     Header;
    NVSP_ALL_MESSAGES                       Messages;
    UINT32                                  Padding;
} NVSP_MESSAGE, *PNVSP_MESSAGE;

static_assert(sizeof(NVSP_MESSAGE) % 8 == 0);

#pragma pack(pop)
