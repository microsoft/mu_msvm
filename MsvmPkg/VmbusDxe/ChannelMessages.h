/** @file
    This files contains definitions for messages that are sent between
    instances of the Channel Management Library in separate partitions, or
    in some cases, back to itself.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#pragma once

#include <Vmbus/NtStatus.h>

#pragma pack(push,1)

//
// A revision number of vmbus that is used for ensuring both ends on a
// partition are using compatible versions.
//
#define VMBUS_MAKE_VERSION(Major, Minor) (((Major) << 16) | (Minor))

#define VMBUS_VERSION_V1           VMBUS_MAKE_VERSION(0, 13)
#define VMBUS_VERSION_WIN7         VMBUS_MAKE_VERSION(1, 1)
#define VMBUS_VERSION_WIN8         VMBUS_MAKE_VERSION(2, 4)
#define VMBUS_VERSION_WIN8_1       VMBUS_MAKE_VERSION(3, 0)
#define VMBUS_VERSION_WIN10        VMBUS_MAKE_VERSION(4, 0)
#define VMBUS_VERSION_WIN10RS3_0   VMBUS_MAKE_VERSION(4, 1)
#define VMBUS_VERSION_WIN10RS3_1   VMBUS_MAKE_VERSION(5, 0)
#define VMBUS_VERSION_WIN10RS4     VMBUS_MAKE_VERSION(5, 1)
#define VMBUS_VERSION_WIN10RS5     VMBUS_MAKE_VERSION(5, 2)
#define VMBUS_VERSION_IRON         VMBUS_MAKE_VERSION(5, 3)
#define VMBUS_VERSION_COPPER       VMBUS_MAKE_VERSION(6, 0)

#define VMBUS_VERSION_LATEST       VMBUS_VERSION_COPPER
#define VMBUS_VERSION_MULTICLIENT  VMBUS_VERSION_WIN10RS3_1

//
// Feature which allows the guest to specify an event flag and connection ID
// when opening a channel. If not used, the event flag defaults to the channel
// ID and the connection ID is specified by the host in the offer channel
// message.
//

#define VMBUS_FEATURE_FLAG_GUEST_SPECIFIED_SIGNAL_PARAMETERS 0x1

//
// Indicates the REDIRECT_INTERRUPT flag is supported in the OpenChannel flags.
//

#define VMBUS_FEATURE_FLAG_CHANNEL_INTERRUPT_REDIRECTION 0x2

//
// Indicates the ChannelMessageModifyConnection and
// ChannelMessageModifyConnectionResponse messages are supported.
//

#define VMBUS_FEATURE_FLAG_MODIFY_CONNECTION 0x4

//
// Feature which allows the guest to specify a GUID when initiating contact.
// The GUID signifies the type of VMBus client that is contacting the host,
// e.g. Windows, Linux, UEFI, minivmbus, etc.
//

#define VMBUS_FEATURE_FLAG_CLIENT_ID 0x8

//
// Indicates the CONFIDENTIAL_CHANNEL flag is supported in the OfferChannel
// flags.
//
// N.B. This flag is only used by paravisors offering VmBus service and is not
//      supported by the root VmBus driver.
//

#define VMBUS_FEATURE_FLAG_CONFIDENTIAL_CHANNELS 0x10

#define VMBUS_SUPPORTED_FEATURE_FLAGS_COPPER \
    (VMBUS_FEATURE_FLAG_GUEST_SPECIFIED_SIGNAL_PARAMETERS | \
     VMBUS_FEATURE_FLAG_CHANNEL_INTERRUPT_REDIRECTION | \
     VMBUS_FEATURE_FLAG_MODIFY_CONNECTION)

#define VMBUS_SUPPORTED_FEATURE_FLAGS_DILITHIUM \
    (VMBUS_SUPPORTED_FEATURE_FLAGS_COPPER | VMBUS_FEATURE_FLAG_CLIENT_ID)

//
// Version 1 messages
//

typedef enum _VMBUS_CHANNEL_MESSAGE_TYPE
{
    ChannelMessageInvalid                   =  0,
    ChannelMessageOfferChannel              =  1,
    ChannelMessageRescindChannelOffer       =  2,
    ChannelMessageRequestOffers             =  3,
    ChannelMessageAllOffersDelivered        =  4,
    ChannelMessageOpenChannel               =  5,
    ChannelMessageOpenChannelResult         =  6,
    ChannelMessageCloseChannel              =  7,
    ChannelMessageGpadlHeader               =  8,
    ChannelMessageGpadlBody                 =  9,
    ChannelMessageGpadlCreated              = 10,
    ChannelMessageGpadlTeardown             = 11,
    ChannelMessageGpadlTorndown             = 12,
    ChannelMessageRelIdReleased             = 13,
    ChannelMessageInitiateContact           = 14,
    ChannelMessageVersionResponse           = 15,
    ChannelMessageUnload                    = 16,
    ChannelMessageUnloadComplete            = 17,
    ChannelMessageOpenReservedChannel       = 18,
    ChannelMessageCloseReservedChannel      = 19,
    ChannelMessageCloseReservedResponse     = 20,
    ChannelMessageTlConnectRequest          = 21,
    ChannelMessageModifyChannel             = 22,
    ChannelMessageTlConnectRequestResult    = 23,
    ChannelMessageModifyChannelResponse     = 24,
    ChannelMessageModifyConnection          = 25,
    ChannelMessageModifyConnectionResponse  = 26,
    ChannelMessageCount
} VMBUS_CHANNEL_MESSAGE_TYPE, *PVMBUS_CHANNEL_MESSAGE_TYPE;

typedef struct _VMBUS_CHANNEL_MESSAGE_HEADER
{
    VMBUS_CHANNEL_MESSAGE_TYPE  MessageType;
    UINT32                      Padding;
} VMBUS_CHANNEL_MESSAGE_HEADER, *PVMBUS_CHANNEL_MESSAGE_HEADER;

//
// Offer flags. The flags parameter is 8 bits, and any undefined bits are
// available, since bits that were not defined are masked out when using an
// older protocol version.
//

#define VMBUS_OFFER_FLAG_ENUMERATE_DEVICE_INTERFACE     0x1
// This flag indicates that the channel is offered by the paravisor, and may
// use encrypted memory for the channel ring buffer.
#define VMBUS_OFFER_FLAG_CONFIDENTIAL_CHANNEL           0x2
#define VMBUS_OFFER_FLAG_NAMED_PIPE_MODE                0x10
#define VMBUS_OFFER_FLAG_TLNPI_PROVIDER                 0x2000

#define VMBUS_OFFER_FLAGS_WIN6 (VMBUS_OFFER_FLAG_ENUMERATE_DEVICE_INTERFACE | \
                                VMBUS_OFFER_FLAG_NAMED_PIPE_MODE)

#define VMBUS_OFFER_FLAGS_WIN10 (VMBUS_OFFER_FLAGS_WIN6 | \
                                VMBUS_OFFER_FLAG_TLNPI_PROVIDER)

#define VMBUS_VP_INDEX_DISABLE_INTERRUPT ((UINT32)-1)

// Offer Channel parameters

typedef struct _VMBUS_CHANNEL_OFFER_CHANNEL
{
    VMBUS_CHANNEL_MESSAGE_HEADER;

    GUID InterfaceType;
    GUID InterfaceInstance;

    //
    // These reserved fields may be non-zero before Windows 8.
    //

    UINT64 Reserved;
    UINT64 Reserved2;

    UINT16 Flags;
    UINT16 MmioMegabytes;

    UINT8 UserDefined[MAX_USER_DEFINED_BYTES];

    UINT16 SubChannelIndex; // Defined in Win8
    UINT16 MmioMegabytesOptional;  // mmio memory in addition to MmioMegabytes that is optional
    UINT32 ChildRelId;

    UINT8 MonitorId;
    UINT8 MonitorAllocated:1;
    UINT8 Reserved4:7;

    //
    // The following fields are only available in Windows 7 and later.
    //

    union
    {
        struct
        {
            UINT16 IsDedicatedInterrupt:1;
            UINT16 Reserved5:15;

            UINT32 ConnectionId;
        };

        UINT8 Windows6Offset;
    };

} VMBUS_CHANNEL_OFFER_CHANNEL, *PVMBUS_CHANNEL_OFFER_CHANNEL;

static_assert(sizeof(VMBUS_CHANNEL_OFFER_CHANNEL) <= MAXIMUM_SYNIC_MESSAGE_BYTES, "Offer message too large");

#define VMBUS_CHANNEL_OFFER_CHANNEL_SIZE_PRE_WIN7 (UINT32)OFFSET_OF(VMBUS_CHANNEL_OFFER_CHANNEL, Windows6Offset)

// Rescind Offer parameters
typedef struct _VMBUS_CHANNEL_RESCIND_OFFER
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    UINT32          ChildRelId;
} VMBUS_CHANNEL_RESCIND_OFFER, *PVMBUS_CHANNEL_RESCIND_OFFER;

//
// Indicates the host-to-guest interrupt for this channel should be sent to the
// redirected VTL and SINT. This has no effect if the server is not using
// redirection.
//

#define VMBUS_OPEN_FLAG_REDIRECT_INTERRUPT 0x1

// Open Channel parameters
typedef struct _VMBUS_CHANNEL_OPEN_CHANNEL
{
    VMBUS_CHANNEL_MESSAGE_HEADER;

    //
    // Identifies the specific VMBus channel that is being opened.
    //
    UINT32          ChildRelId;

    //
    // ID making a particular open request at a channel offer unique.
    //
    UINT32          OpenId;

    //
    // GPADL for the channel's ring buffer.
    //
    UINT32          RingBufferGpadlHandle;

    //
    // Target VP index for the server-to-client interrupt. (>= Win8 only)
    //
    UINT32          TargetVp;

    //
    // The upstream ring buffer begins at offset zero in the memory described
    // by RingBufferGpadlHandle. The downstream ring buffer follows it at this
    // offset (in pages).
    //
    UINT32          DownstreamRingBufferPageOffset;

    //
    // User-specific data to be passed along to the server endpoint.
    //
    UINT8           UserData[MAX_USER_DEFINED_BYTES];

    //
    // Guest-specified signal parameters; valid only if
    // VMBUS_FEATURE_FLAG_GUEST_SPECIFIED_SIGNAL_PARAMETERS is used.
    //

    UINT32          ConnectionId;
    UINT16          EventFlag;

    //
    // Valid only if VMBUS_FEATURE_FLAG_INTERRUPT_REDIRECTION is used.
    //

    UINT16          Flags;

} VMBUS_CHANNEL_OPEN_CHANNEL, *PVMBUS_CHANNEL_OPEN_CHANNEL;

#define VMBUS_CHANNEL_OPEN_CHANNEL_MIN_SIZE OFFSET_OF(VMBUS_CHANNEL_OPEN_CHANNEL, ConnectionId)

// Open Channel Result parameters
typedef struct _VMBUS_CHANNEL_OPEN_RESULT
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    UINT32      ChildRelId;
    UINT32      OpenId;
    UINT32      Status;
} VMBUS_CHANNEL_OPEN_RESULT, *PVMBUS_CHANNEL_OPEN_RESULT;

// Close channel parameters;
typedef struct _VMBUS_CHANNEL_CLOSE_CHANNEL
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    UINT32      ChildRelId;
} VMBUS_CHANNEL_CLOSE_CHANNEL, *PVMBUS_CHANNEL_CLOSE_CHANNEL;

typedef struct _VMBUS_CHANNEL_MODIFY_CHANNEL
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    UINT32      ChildRelId;

    //
    // Target VP index for the server-to-client interrupt.
    //
    UINT32          TargetVp;

} VMBUS_CHANNEL_MODIFY_CHANNEL, *PVMBUS_CHANNEL_MODIFY_CHANNEL;

typedef struct _VMBUS_CHANNEL_MODIFY_CHANNEL_RESPONSE
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    UINT32      ChildRelId;
    NTSTATUS    Status;
} VMBUS_CHANNEL_MODIFY_CHANNEL_RESPONSE, *PVMBUS_CHANNEL_MODIFY_CHANNEL_RESPONSE;

//
// The number of PFNs in a GPADL message is defined by the number of pages
// that would be spanned by ByteCount and ByteOffset.  If the implied number
// of PFNs won't fit in this packet, there will be a follow-up packet that
// contains more.
//

typedef struct _VMBUS_CHANNEL_GPADL_HEADER
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    UINT32      ChildRelId;
    UINT32      Gpadl;
    UINT16      RangeBufLen;
    UINT16      RangeCount;
    GPA_RANGE   Range[1];
} VMBUS_CHANNEL_GPADL_HEADER, *PVMBUS_CHANNEL_GPADL_HEADER;

//
// This is the followup packet that contains more PFNs.
//

typedef struct _VMBUS_CHANNEL_GPADL_BODY
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    UINT32              MessageNumber;
    UINT32              Gpadl;
    UINT64              Pfn[1];
} VMBUS_CHANNEL_GPADL_BODY, *PVMBUS_CHANNEL_GPADL_BODY;


typedef struct _VMBUS_CHANNEL_GPADL_CREATED
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    UINT32              ChildRelId;
    UINT32              Gpadl;
    UINT32              CreationStatus;
} VMBUS_CHANNEL_GPADL_CREATED, *PVMBUS_CHANNEL_GPADL_CREATED;

typedef struct _VMBUS_CHANNEL_GPADL_TEARDOWN
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    UINT32              ChildRelId;
    UINT32              Gpadl;
} VMBUS_CHANNEL_GPADL_TEARDOWN, *PVMBUS_CHANNEL_GPADL_TEARDOWN;

typedef struct _VMBUS_CHANNEL_GPADL_TORNDOWN
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    UINT32              Gpadl;
} VMBUS_CHANNEL_GPADL_TORNDOWN, *PVMBUS_CHANNEL_GPADL_TORNDOWN;

typedef struct _VMBUS_CHANNEL_RELID_RELEASED
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    UINT32              ChildRelId;
} VMBUS_CHANNEL_RELID_RELEASED, *PVMBUS_CHANNEL_RELID_RELEASED;

typedef struct _VMBUS_CHANNEL_INITIATE_CONTACT
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    UINT32              VMBusVersionRequested;
    UINT32              TargetMessageVp;
    union
    {
        UINT64              InterruptPage;
        struct
        {
            UINT8           TargetSint; // VMBUS_VERSION_WIN10RS3_1
            UINT8           TargetVtl; // VMBUS_VERSION_WIN10RS4
            UINT8           Reserved[2];
            UINT32          FeatureFlags; // VMBUS_VERSION_COPPER
        };
    };

    UINT64              ParentToChildMonitorPageGpa;
    UINT64              ChildToParentMonitorPageGpa;
    GUID                ClientId; // VMBUS_FEATURE_FLAG_CLIENT_ID
} VMBUS_CHANNEL_INITIATE_CONTACT, *PVMBUS_CHANNEL_INITIATE_CONTACT;

#define VMBUS_CHANNEL_INITIATE_CONTACT_MIN_SIZE OFFSET_OF(VMBUS_CHANNEL_INITIATE_CONTACT, ClientId)

typedef struct _VMBUS_CHANNEL_VERSION_RESPONSE
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    BOOLEAN     VersionSupported;
    UINT8       ConnectionState;
    UINT8       Pad[2];
    union
    {
        UINT32      SelectedVersion;
        UINT32      ConnectionId;
    };

    // Supported features is available with the Copper protocol.
    UINT32      SupportedFeatures;

} VMBUS_CHANNEL_VERSION_RESPONSE, * PVMBUS_CHANNEL_VERSION_RESPONSE;

#define VMBUS_CHANNEL_VERSION_RESPONSE_MIN_SIZE OFFSET_OF(VMBUS_CHANNEL_VERSION_RESPONSE, SupportedFeatures)

//
// Status codes for the ConnectionState field of
// VMBUS_CHANNEL_VERSION_RESPONSE.
//
// N.B. If VersionSupported is FALSE, do not consult this value.
// If the requested version is less than VMBUS_VERSION_WIN8, these values
// may be uninitialized memory, cannot be consulted, and the effective value
// must be assumed to be VmbusChannelConnectionSuccessful.
//
// All non-zero values should be taken to mean a failure. The specific values
// are merely used to better provide information to the guest about the cause
// of the failure.
//

enum
{
    VmbusChannelConnectionSuccessful = 0,
    VmbusChannelConnectionFailedLowResources = 1,
    VmbusChannelConnectionFailedUnknownFailure = 2,
};

typedef VMBUS_CHANNEL_MESSAGE_HEADER VMBUS_CHANNEL_UNLOAD, *PVMBUS_CHANNEL_UNLOAD;

typedef VMBUS_CHANNEL_MESSAGE_HEADER VMBUS_CHANNEL_UNLOAD_COMPLETE, *PVMBUS_CHANNEL_UNLOAD_COMPLETE;

typedef struct _VMBUS_CHANNEL_OPEN_RESERVED_CHANNEL
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    UINT32 ChannelId;
    UINT32 TargetVp;
    UINT32 TargetSint;
    UINT32 RingBufferGpadl;
    UINT32 DownstreamPageOffset;
} VMBUS_CHANNEL_OPEN_RESERVED_CHANNEL, *PVMBUS_CHANNEL_OPEN_RESERVED_CHANNEL;

typedef struct _VMBUS_CHANNEL_CLOSE_RESERVED_CHANNEL
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    UINT32 ChannelId;
    UINT32 TargetVp;
    UINT32 TargetSint;
} VMBUS_CHANNEL_CLOSE_RESERVED_CHANNEL, *PVMBUS_CHANNEL_CLOSE_RESERVED_CHANNEL;

typedef struct _VMBUS_CHANNEL_CLOSE_RESERVED_RESPONSE
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    UINT32 ChannelId;
} VMBUS_CHANNEL_CLOSE_RESERVED_RESPONSE, *PVMBUS_CHANNEL_CLOSE_RESERVED_RESPONSE;

typedef struct _VMBUS_CHANNEL_TL_CONNECT_REQUEST
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    GUID EndpointId;
    GUID ServiceId;

    // The SiloId is available with the RS5 vmbus protocol version.
    union
    {
        GUID SiloId;
        UINT8 WindowsRS1Offset;
    };
} VMBUS_CHANNEL_TL_CONNECT_REQUEST, *PVMBUS_CHANNEL_TL_CONNECT_REQUEST;

#define VMBUS_CHANNEL_TL_CONNECT_REQUEST_PRE_RS5_SIZE (UINT32)OFFSET_OF(VMBUS_CHANNEL_TL_CONNECT_REQUEST, WindowsRS1Offset)

typedef struct _VMBUS_CHANNEL_TL_CONNECT_RESULT
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    GUID EndpointId;
    GUID ServiceId;
    NTSTATUS Status;
} VMBUS_CHANNEL_TL_CONNECT_RESULT, *PVMBUS_CHANNEL_TL_CONNECT_RESULT;

typedef struct _VMBUS_CHANNEL_MODIFY_CONNECTION
{
    VMBUS_CHANNEL_MESSAGE_HEADER;
    UINT64 ParentToChildMonitorPageGpa;
    UINT64 ChildToParentMonitorPageGpa;
} VMBUS_CHANNEL_MODIFY_CONNECTION, *PVMBUS_CHANNEL_MODIFY_CONNECTION;

typedef struct _VMBUS_CHANNEL_MODIFY_CONNECTION_RESPONSE
{
    VMBUS_CHANNEL_MESSAGE_HEADER;

    //
    // This accepts the same values as in VMBUS_CHANNEL_VERSION_RESPONSE.
    //

    UINT8 ConnectionState;
} VMBUS_CHANNEL_MODIFY_CONNECTION_RESPONSE, *PVMBUS_CHANNEL_MODIFY_CONNECTION_RESPONSE;

//
// Kind of a table to use the preprocessor to get us the right type for a
// specified message ID. Used with ChAllocateSendMessage()
//
#define ChannelMessageOfferChannel_TYPE             VMBUS_CHANNEL_OFFER_CHANNEL
#define ChannelMessageRescindChannelOffer_TYPE      VMBUS_CHANNEL_RESCIND_OFFER
#define ChannelMessageRequestOffers_TYPE            VMBUS_CHANNEL_MESSAGE_HEADER
#define ChannelMessageAllOffersDelivered_TYPE       VMBUS_CHANNEL_MESSAGE_HEADER
#define ChannelMessageOpenChannel_TYPE              VMBUS_CHANNEL_OPEN_CHANNEL
#define ChannelMessageOpenChannelResult_TYPE        VMBUS_CHANNEL_OPEN_RESULT
#define ChannelMessageCloseChannel_TYPE             VMBUS_CHANNEL_CLOSE_CHANNEL
#define ChannelMessageGpadlHeader_TYPE              VMBUS_CHANNEL_GPADL_HEADER
#define ChannelMessageGpadlBody_TYPE                VMBUS_CHANNEL_GPADL_BODY
#define ChannelMessageGpadlCreated_TYPE             VMBUS_CHANNEL_GPADL_CREATED
#define ChannelMessageGpadlTeardown_TYPE            VMBUS_CHANNEL_GPADL_TEARDOWN
#define ChannelMessageGpadlTorndown_TYPE            VMBUS_CHANNEL_GPADL_TORNDOWN
#define ChannelMessageRelIdReleased_TYPE            VMBUS_CHANNEL_RELID_RELEASED
#define ChannelMessageInitiateContact_TYPE          VMBUS_CHANNEL_INITIATE_CONTACT
#define ChannelMessageVersionResponse_TYPE          VMBUS_CHANNEL_VERSION_RESPONSE
#define ChannelMessageUnload_TYPE                   VMBUS_CHANNEL_UNLOAD
#define ChannelMessageUnloadComplete_TYPE           VMBUS_CHANNEL_UNLOAD_COMPLETE
#define ChannelMessageOpenReservedChannel_TYPE      VMBUS_CHANNEL_OPEN_RESERVED_CHANNEL
#define ChannelMessageCloseReservedChannel_TYPE     VMBUS_CHANNEL_CLOSE_RESERVED_CHANNEL
#define ChannelMessageCloseReservedResponse_TYPE    VMBUS_CHANNEL_CLOSE_RESERVED_RESPONSE
#define ChannelMessageTlConnectRequest_TYPE         VMBUS_CHANNEL_TL_CONNECT_REQUEST
#define ChannelMessageModifyChannel_TYPE            VMBUS_CHANNEL_MODIFY_CHANNEL
#define ChannelMessageTlConnectRequestResult_TYPE   VMBUS_CHANNEL_TL_CONNECT_RESULT
#define ChannelMessageModifyChannelResponse_TYPE    VMBUS_CHANNEL_MODIFY_CHANNEL_RESPONSE
#define ChannelMessageModifyConnection_TYPE         VMBUS_CHANNEL_MODIFY_CONNECTION
#define ChannelMessageModifyConnectionResponse_TYPE VMBUS_CHANNEL_MODIFY_CONNECTION_RESPONSE

#pragma pack(pop)
