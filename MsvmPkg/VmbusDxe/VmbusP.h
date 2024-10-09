/** @file
    Private includes for Vmbus EFI driver.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#pragma once

#include <Protocol/DevicePath.h>
#include <Protocol/EfiHv.h>
#include <Protocol/Vmbus.h>
#include <Library/CrashLib.h>

//
// Disable warnings for nameless unions/structs.
//
#pragma warning(push)
#pragma warning(disable : 4201)

//
// Definitions needed for ChannelMessages.h
//
#define MAXIMUM_SYNIC_MESSAGE_BYTES 240
#define MAX_USER_DEFINED_BYTES 120

typedef struct _GPA_RANGE
{
    UINT32  ByteCount;
    UINT32  ByteOffset;
    UINT64  PfnArray[1];

} GPA_RANGE;

#include <ChannelMessages.h>

#define VMBUS_MESSAGE_CONNECTION_ID 1
#define VMBUS_MESSAGE_TYPE 1

#define VMBUS_ROOT_NODE_HID_STR "VMBus"

#define EFI_VMBUS_CHANNEL_DEVICE_PATH_GUID \
    {0x9b17e5a2, 0x891, 0x42dd, {0xb6, 0x53, 0x80, 0xb5, 0xc2, 0x28, 0x9, 0xba}}

EFI_HV_PROTOCOL *mHv;
EFI_HV_IVM_PROTOCOL *mHvIvm;
UINTN mSharedGpaBoundary;
UINT64 mCanonicalizationMask;

extern EFI_GUID gEfiVmbusChannelDevicePathGuid;

//
// A tag GUID for the VMBus bus controller. The UEFI driver model requires
// bus children to consume a protocol from the bus controller for child tracking
// purposes, so we give the VMBus channels a dummy tag protocol to consume.
//
extern EFI_GUID gEfiVmbusRootProtocolGuid;

//
// Keep track of the GUIDs of channels that are created during the UEFI boot phase.
//
// The following VmBus channels are supported during UEFI:
//
//      Storage channel (StorvscDxe)
//      Networking channel (NetvscDxe)
//      Video channel (VideoDxe)
//      Virtual SMB channel (VmbfsDxe)
//      Keyboard channel (SynthKeyDxe)
//      Virtual PCI channel (VpcivscDxe)
//
// For isolated guests, only allow the channels for drivers that have been triaged for security
// and guest hardening.
//
// The following channels have gone through a security review and are allowed during UEFI:
//
//      Storage channel (StorvscDxe)
//      Networking channel (NetvscDxe)
//      Virtual PCI channel (VpcivscDxe)
//
typedef struct
{
    BOOLEAN IsAllowedWhenIsolated;
    EFI_GUID AllowedGuid;

} VMBUS_ROOT_ALLOWED_GUIDS;

VMBUS_ROOT_ALLOWED_GUIDS gAllowedGuids[] =
{
    {TRUE, { 0xba6163d9, 0x04a1, 0x4d29, {0xb6, 0x05, 0x72, 0xe2, 0xff, 0xb1, 0xdc, 0x7f} }},   // StorvscDxe
    {TRUE, { 0xf8615163, 0xdf3e, 0x46c5, {0x91, 0x3f, 0xf2, 0xd2, 0xf9, 0x65, 0xed, 0xe} }},    // NetvscDxe
    {TRUE, { 0x44c4f61d, 0x4444, 0x4400, {0x9d, 0x52, 0x80, 0x2e, 0x27, 0xed, 0xe1, 0x9f} }},   // VpcivscDxe
    {FALSE, { 0xda0a7802, 0xe377, 0x4aac, {0x8e, 0x77, 0x05, 0x58, 0xeb, 0x10, 0x73, 0xf8} }},  // VideoDxe
    {FALSE, { 0xc376c1c3, 0xd276, 0x48d2, {0x90, 0xa9, 0xc0, 0x47, 0x48, 0x07, 0x2c, 0x60} }},  // VmbfsDxe
    {FALSE, { 0xf912ad6d, 0x2b17, 0x48ea, {0xbd, 0x65, 0xf9, 0x27, 0xa6, 0x1c, 0x76, 0x84} }}   // SynthKeyDxe
};

//
// IMC is a special channel for now and is controlled using the UEFI flag. Having an IMC channel lets us remove
// the extra reboot after provisioning for setting the computer name.
//
EFI_GUID gVmbfsChannelGuid =  {0xc376c1c3, 0xd276, 0x48d2, {0x90, 0xa9, 0xc0, 0x47, 0x48, 0x07, 0x2c, 0x60}};

typedef struct
{
    ACPI_EXTENDED_HID_DEVICE_PATH AcpiExtendedNode;
    CHAR8 HidStr[sizeof(VMBUS_ROOT_NODE_HID_STR)];
    CHAR8 UidStr[1];
    CHAR8 CidStr[1];

} VMBUS_ROOT_NODE;

typedef struct
{
    VMBUS_ROOT_NODE VmbusRootNode;
    EFI_DEVICE_PATH_PROTOCOL End;

} VMBUS_ROOT_DEVICE_PATH;

typedef struct
{
    VMBUS_ROOT_NODE VmbusRootNode;
    VMBUS_DEVICE_PATH VmbusChannelNode;
    EFI_DEVICE_PATH_PROTOCOL End;

} VMBUS_CHANNEL_DEVICE_PATH;

VMBUS_ROOT_NODE gVmbusRootNode;
VMBUS_DEVICE_PATH gVmbusChannelNode;
EFI_DEVICE_PATH_PROTOCOL gEfiEndNode;

#define TPL_VMBUS (TPL_HIGH_LEVEL - 1)
#define VMBUS_MAX_GPADLS 256
#define VMBUS_MAX_CHANNELS HV_EVENT_FLAGS_COUNT

typedef struct _VMBUS_MESSAGE
{
    UINT32 Size;

    union
    {
        UINT8 Data[MAXIMUM_SYNIC_MESSAGE_BYTES];
        VMBUS_CHANNEL_MESSAGE_HEADER Header;
        VMBUS_CHANNEL_OFFER_CHANNEL OfferChannel;
        VMBUS_CHANNEL_RESCIND_OFFER RescindOffer;
        VMBUS_CHANNEL_OPEN_CHANNEL OpenChannel;
        VMBUS_CHANNEL_OPEN_RESULT OpenResult;
        VMBUS_CHANNEL_CLOSE_CHANNEL CloseChannel;
        VMBUS_CHANNEL_GPADL_HEADER GpadlHeader;
        VMBUS_CHANNEL_GPADL_BODY GpadlBody;
        VMBUS_CHANNEL_GPADL_CREATED GpadlCreated;
        VMBUS_CHANNEL_GPADL_TEARDOWN GpadlTeardown;
        VMBUS_CHANNEL_GPADL_TORNDOWN GpadlTorndown;
        VMBUS_CHANNEL_RELID_RELEASED RelIdReleased;
        VMBUS_CHANNEL_INITIATE_CONTACT InitiateContact;
        VMBUS_CHANNEL_VERSION_RESPONSE VersionResponse;
    };

} VMBUS_MESSAGE;

typedef struct _VMBUS_MESSAGE_RESPONSE
{
    EFI_EVENT Event;
    VMBUS_MESSAGE Message;

} VMBUS_MESSAGE_RESPONSE;

#define VMBUS_DRIVER_VERSION 0x10
#define VMBUS_ROOT_CONTEXT_SIGNATURE         SIGNATURE_32('v','m','b','r')

typedef struct _VMBUS_ROOT_CONTEXT VMBUS_ROOT_CONTEXT;

#define VMBUS_CHANNEL_CONTEXT_SIGNATURE         SIGNATURE_32('v','m','b','c')

typedef struct _VMBUS_CHANNEL_CONTEXT
{
    UINT32 Signature;

    EFI_HANDLE Handle;
    EFI_VMBUS_LEGACY_PROTOCOL LegacyVmbusProtocol;
    EFI_VMBUS_PROTOCOL VmbusProtocol;
    VMBUS_CHANNEL_DEVICE_PATH DevicePath;
    LIST_ENTRY Link;
    UINT32 ChannelId;
    HV_CONNECTION_ID ConnectionId;
    VMBUS_ROOT_CONTEXT *RootContext;
    VMBUS_MESSAGE_RESPONSE Response;

    //
    // Interrupt events are managed by the root device.
    //
    EFI_EVENT Interrupt;

    //
    // A confidential channel is a channel offered by the paravisor on a
    // hardware-isolated VM, which means it can use encrypted memory for the
    // ring buffer.
    //
    BOOLEAN Confidential;

} VMBUS_CHANNEL_CONTEXT;

struct _EFI_VMBUS_GPADL
{
    VOID* AllocatedBuffer;
    UINT64 VisibleBufferPA;
    UINT32 BufferLength;
    UINT32 NumberOfPages;
    UINT32 GpadlHandle;
    EFI_HV_PROTECTION_HANDLE ProtectionHandle;
    BOOLEAN Legacy;
};

VMBUS_MESSAGE*
VmbusRootWaitForChannelResponse(
    IN  VMBUS_CHANNEL_CONTEXT *ChannelContext
    );

EFI_STATUS
VmbusRootWaitForGpadlResponse(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  UINT32 GpadlHandle,
    OUT VMBUS_MESSAGE **Message
    );

VOID
VmbusRootInitializeMessage(
    IN OUT  VMBUS_MESSAGE *Message,
    IN      VMBUS_CHANNEL_MESSAGE_TYPE Type,
    IN      UINT32 Size
    );

EFI_STATUS
VmbusRootSendMessage(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  VMBUS_MESSAGE *Message
    );

EFI_STATUS
VmbusRootGetFreeGpadl(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    OUT UINT32 *GpadlHandle
    );

VOID
VmbusRootReclaimGpadl(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  UINT32 GpadlHandle
    );

VOID
VmbusRootSetGpadlPageRange(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  UINT32 GpadlHandle,
    IN  UINT64 GpaPageBase,
    IN  UINT32 PageCount
    );

BOOLEAN
VmbusRootValidateGpadl(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  UINT32 GpadlHandle
    );

VOID
VmbusRootSetInterruptEntry(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  UINT32 ChannelId,
    IN  EFI_EVENT Event
    );

VOID
VmbusRootClearInterruptEntry(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  UINT32 ChannelId
    );

VOID
VmbusChannelInitializeContext(
    IN OUT  VMBUS_CHANNEL_CONTEXT *ChannelContext,
    IN      VMBUS_CHANNEL_OFFER_CHANNEL *Offer,
    IN      VMBUS_ROOT_CONTEXT *RootContext
    );

VOID
VmbusChannelDestroyContext(
    IN  VMBUS_CHANNEL_CONTEXT *ChannelContext
    );

BOOLEAN
VmbusRootSupportsFeatureFlag(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  UINT32 FeatureFlag
    );

#pragma warning(pop)

