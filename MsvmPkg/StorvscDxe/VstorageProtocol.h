/** @file

    Header file for public definitions shared between kernel and user mode.
    The definitions are used for communication between StorVSP and StorVSC.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <MsvmBase.h>

#pragma warning(push)
#pragma warning(disable: 4201) // Allow nameless structs

//
//  Public interface to the server
//

//
//  Protocol versions.
//

//
// Major/minor macros.  Minor version is in LSB, meaning that earlier flat
// version numbers will be interpreted as "0.x" (i.e., 1 becomes 0.1).
//

#define VMSTOR_PROTOCOL_MAJOR(VERSION_)         (((VERSION_) >> 8) & 0xff)
#define VMSTOR_PROTOCOL_MINOR(VERSION_)         (((VERSION_)     ) & 0xff)
#define VMSTOR_PROTOCOL_VERSION(MAJOR_, MINOR_) ((((MAJOR_) & 0xff) << 8) | \
                                                 (((MINOR_) & 0xff)     ))

//
// Invalid version.
//
#define VMSTOR_INVALID_PROTOCOL_VERSION  -1

//
// Version history:
// V1 (Win2k8 Server)
//   Beta                    0.1
//   RC < 2008/1/31          1.0
//   RC > 2008/1/31          2.0
//   Servicing               3.0 (reserved)
//
// Win7
//   M3                      3.0 (deprecated)
//   Beta                    4.0
//   Release                 4.2
//
// Win8
//   M3                      5.0
//   Beta                    5.1
//   RC                      5.1 (added multi-channel support flag)
//
// Win Blue
//   MQ                      6.0 (added Asynchronous Notification)
//
//
#define VMSTOR_PROTOCOL_VERSION_WIN6            VMSTOR_PROTOCOL_VERSION(2, 0)
#define VMSTOR_PROTOCOL_VERSION_WIN7            VMSTOR_PROTOCOL_VERSION(4, 2)
#define VMSTOR_PROTOCOL_VERSION_WIN8            VMSTOR_PROTOCOL_VERSION(5, 1)
#define VMSTOR_PROTOCOL_VERSION_BLUE            VMSTOR_PROTOCOL_VERSION(6, 0)
#define VMSTOR_PROTOCOL_VERSION_CURRENT         VMSTOR_PROTOCOL_VERSION_BLUE

//
//  The max transfer length will be published when we offer a vmbus channel.
//  Max transfer bytes - this determines the reserved MDL size and how large
//  requests can be that the clients will forward.
//
#define MAX_TRANSFER_LENGTH (8*1024*1024)

//
// Indicates that the device supports Asynchronous Notifications (AN)
//
#define VMSTOR_PROPERTY_AN_CAPABLE 0x1

//
//  Packet structure describing virtual storage requests.
//
typedef enum
{
    VStorOperationCompleteIo            = 1,
    VStorOperationRemoveDevice          = 2,
    VStorOperationExecuteSRB            = 3,
    VStorOperationResetLun              = 4,
    VStorOperationResetAdapter          = 5,
    VStorOperationResetBus              = 6,
    VStorOperationBeginInitialization   = 7,
    VStorOperationEndInitialization     = 8,
    VStorOperationQueryProtocolVersion  = 9,
    VStorOperationQueryProperties       = 10,
    VStorOperationEnumerateBus          = 11,
    VStorOperationFcHbaData             = 12,
    VStorOperationCreateSubChannels     = 13,
    VStorOperationEventNotification     = 14,
    VStorOperationMaximum               = 14,
} VSTOR_PACKET_OPERATION;


//
//  Platform neutral description of a SCSI request
//
#pragma pack(push,1)

#define CDB16GENERIC_LENGTH 0x10

#define MAX_DATA_BUFFER_LENGTH_WITH_PADDING 0x14

#define VMSCSI_SENSE_BUFFER_SIZE_REVISION_1 0x12
#define VMSCSI_SENSE_BUFFER_SIZE 0x14

typedef struct _VMSCSI_REQUEST
{
    UINT16 Length;
    UINT8 SrbStatus;
    UINT8 ScsiStatus;

    UINT8 Reserved1;
    UINT8 PathId;
    UINT8 TargetId;
    UINT8 Lun;

    UINT8 CdbLength;
    UINT8 SenseInfoExLength;
    UINT8 DataIn;
    UINT8 Properties;

    UINT32 DataTransferLength;

    union
    {
        UINT8 Cdb[CDB16GENERIC_LENGTH];

        UINT8 SenseDataEx[VMSCSI_SENSE_BUFFER_SIZE];

        UINT8 ReservedArray[MAX_DATA_BUFFER_LENGTH_WITH_PADDING];
    };

    //
    // The following were added in Windows 8
    //
    UINT16  Reserve;
    UINT8   QueueTag;
    UINT8   QueueAction;
    UINT32  SrbFlags;
    UINT32  TimeOutValue;
    UINT32  QueueSortKey;
} VMSCSI_REQUEST, *PVMSCSI_REQUEST;

static_assert((sizeof(VMSCSI_REQUEST) % 4) == 0);

#define VMSTORAGE_SIZEOF_VMSCSI_REQUEST_REVISION_1 OFFSET_OF(VMSCSI_REQUEST, Reserve)

static_assert(VMSTORAGE_SIZEOF_VMSCSI_REQUEST_REVISION_1 == 0x24);

#define VMSTORAGE_SIZEOF_VMSCSI_REQUEST_REVISION_2 (SIZEOF_THROUGH_FIELD(VMSCSI_REQUEST, QueueSortKey))

static_assert(VMSTORAGE_SIZEOF_VMSCSI_REQUEST_REVISION_2 == 0x34);


//
// This structure is sent during the intialization phase to get the different
// properties of the channel.
//
// The reserved properties are not guaranteed to be zero before protocol version
// 5.1.
//
typedef struct _VMSTORAGE_CHANNEL_PROPERTIES
{
    UINT32 Reserved;
    UINT16 MaximumSubChannelCount;
    UINT16 Reserved2;
    UINT32 Flags;
    UINT32 MaxTransferBytes;
    UINT64 Reserved3;
} VMSTORAGE_CHANNEL_PROPERTIES, *PVMSTORAGE_CHANNEL_PROPERTIES;

//
// Channel Property Flags
//

#define STORAGE_CHANNEL_SUPPORTS_MULTI_CHANNEL          0x1

static_assert((sizeof(VMSTORAGE_CHANNEL_PROPERTIES) % 4) == 0);

//
// This structure is sent as part of the channel offer. It exists for old
// versions of the VSC that used this to determine the IDE channel that
// matched up with the VMBus channel.
//
// The reserved properties are not guaranteed to be zero.
//
typedef struct _VMSTORAGE_OFFER_PROPERTIES
{
    UINT16 Reserved;
    UINT8 PathId;
    UINT8 TargetId;
    UINT32 Reserved2;
    UINT32 Flags;
    UINT32 Reserved3[3];
} VMSTORAGE_OFFER_PROPERTIES, *PVMSTORAGE_OFFER_PROPERTIES;

#define STORAGE_OFFER_EMULATED_IDE_FLAG               0x2

//
//  This structure is sent during the storage protocol negotiations.
//
typedef struct _VMSTORAGE_PROTOCOL_VERSION
{
    //
    // Major (MSW) and minor (LSW) version numbers.
    //
    UINT16 MajorMinor;

    //
    // Windows build number. Purely informative.
    //
    UINT16 Build;

} VMSTORAGE_PROTOCOL_VERSION, *PVMSTORAGE_PROTOCOL_VERSION;

static_assert((sizeof(VMSTORAGE_PROTOCOL_VERSION) % 4) == 0);

//
//  This structure is for fibre channel Wwn Packets.
//
typedef struct _VMFC_WWN_PACKET
{
    BOOLEAN PrimaryWwnActive;
    INT8    Reserved1;
    UINT16  Reserved2;

    INT8    PrimaryPortWwn[8];
    INT8    PrimaryNodeWwn[8];
    INT8    SecondaryPortWwn[8];
    INT8    SecondaryNodeWwn[8];
} VMFC_WWN_PACKET, *PVMFC_WWN_PACKET;

static_assert((sizeof(VMFC_WWN_PACKET) % 4) == 0);

//
// Used to register or unregister Asynchronous Media Event Notification to the client
//
typedef struct _VSTOR_CLIENT_PROPERTIES
{
    UINT32 AsyncNotifyCapable : 1;
    UINT32 Reserved           : 31;

} VSTOR_CLIENT_PROPERTIES, *PVSTOR_CLIENT_PROPERTIES;

static_assert((sizeof(VSTOR_CLIENT_PROPERTIES) % 4) == 0);

typedef struct _VSTOR_ASYNC_REGISTER_PACKET
{
    UINT8      Lun;
    UINT8      Target;
    UINT8      Path;
    BOOLEAN    Register;
} VSTOR_ASYNC_REGISTER_PACKET, *PVSTOR_ASYNC_REGISTER_PACKET;

static_assert((sizeof(VSTOR_ASYNC_REGISTER_PACKET) % 4) == 0);

//
// Used to send notifications to StorVsc about media change events
//
typedef struct _VSTOR_NOTIFICATION_PACKET
{
    UINT8    Lun;
    UINT8    Target;
    UINT8    Path;
    UINT8    Flags;
} VSTOR_NOTIFICATION_PACKET, *PVSTOR_NOTIFICATION_PACKET;

static_assert((sizeof(VSTOR_NOTIFICATION_PACKET) % 4) == 0);

typedef struct _VSTOR_PACKET
{
    //
    // Requested operation type
    //
    VSTOR_PACKET_OPERATION Operation;

    //
    //  Flags - see below for values
    //
    UINT32     Flags;

    //
    // Status of the request returned from the server side.
    //
    UINT32     Status;

    //
    // Data payload area
    //
    union
    {
        //
        //  Structure used to forward SCSI commands from the client to the server.
        //  0x34 bytes
        VMSCSI_REQUEST      VmSrb;

        //
        // Structure used to query channel properties.
        //
        VMSTORAGE_CHANNEL_PROPERTIES StorageChannelProperties;

        //
        // Used during version negotiations.
        //
        VMSTORAGE_PROTOCOL_VERSION Version;

        //
        // Used for fibre Channel address packet.
        //
        VMFC_WWN_PACKET FcWwnPacket;

        //
        // Number of subchannels to create via VStorOperationCreateSubChannel.
        //
        UINT16 SubChannelCount;

        //
        // Used to perform Asynchronous Event Notifications
        //
        VSTOR_CLIENT_PROPERTIES      ClientProperties;
        VSTOR_NOTIFICATION_PACKET    NotificationPacket;

        //
        // Buffer. The buffer size will be the maximun of union members. It is
        // used to transfer data.
        //
        UINT8  Buffer[0x34];
    };

} VSTOR_PACKET, *PVSTOR_PACKET;

static_assert((sizeof(VSTOR_PACKET) % 8) == 0);

#define VMSTORAGE_SIZEOF_VSTOR_PACKET_REVISION_1 (SIZEOF_THROUGH_FIELD(VSTOR_PACKET, Status) + VMSTORAGE_SIZEOF_VMSCSI_REQUEST_REVISION_1)

static_assert(VMSTORAGE_SIZEOF_VSTOR_PACKET_REVISION_1 == 0x30);

#define VMSTORAGE_SIZEOF_VSTOR_PACKET_REVISION_2 (SIZEOF_THROUGH_FIELD(VSTOR_PACKET, Status) + VMSTORAGE_SIZEOF_VMSCSI_REQUEST_REVISION_2)

static_assert(VMSTORAGE_SIZEOF_VSTOR_PACKET_REVISION_2 == 0x40);


//
//  Packet flags
//

//
//  This flag indicates that the server should send back a completion for this
//  packet.
//
#define REQUEST_COMPLETION_FLAG 0x1

//
//  This is the set of flags that the VSC can set in any packets it sends
//
#define VSC_LEGAL_FLAGS (REQUEST_COMPLETION_FLAG)


#pragma pack(pop)

typedef struct _ADAPTER_ADDRESS
{
    UINT64 PartitionId;

    GUID ChannelInstanceGUID;

    //
    //  SCSI address
    //
    UINT8 Reserved;
    UINT8 PathId;
    UINT8 TargetId;
    UINT8 Lun;

    //
    //  Flags
    //
    UINT32 Flags;

    //
    // World wide names for SynthFc
    //
    BOOLEAN PrimaryWwnActive;
    UINT8   PrimaryPortWwn[8];
    UINT8   PrimaryNodeWwn[8];
    UINT8   SecondaryPortWwn[8];
    UINT8   SecondaryNodeWwn[8];

} ADAPTER_ADDRESS, *PADAPTER_ADDRESS;

//
//  Flags for ADAPTER_ADDRESS
//
#define ADAPTER_ADDRESS_EMULATED_DEVICE            0x1
#define ADAPTER_ADDRESS_SYNTHFC_DEVICE             0x2


//
// Alignment information
//
#define VSTORAGE_ALIGNMENT_MASK 0x01

#pragma warning(pop)

