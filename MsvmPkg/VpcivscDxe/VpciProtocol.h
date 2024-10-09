/** @file
  This file defines structures and interfaces that are shared with the
  Virtual PCI implementation

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

#include <IndustryStandard/Pci30.h>

#include "wdm.h"

#define ALIGN_UP(x, y) ALIGN_VALUE((x), sizeof(y))

typedef struct _VPCI_PNP_ID
{
    UINT16  VendorID;
    UINT16  DeviceID;
    UINT8   RevisionID;
    UINT8   ProgIf;
    UINT8   SubClass;
    UINT8   BaseClass;
    UINT16  SubVendorID;
    UINT16  SubSystemID;

} VPCI_PNP_ID, *PVPCI_PNP_ID;

typedef enum _DEVICE_POWER_STATE {
    PowerDeviceUnspecified = 0,
    PowerDeviceD0,
    PowerDeviceD1,
    PowerDeviceD2,
    PowerDeviceD3,
    PowerDeviceMaximum
} DEVICE_POWER_STATE, *PDEVICE_POWER_STATE;


// allow nameless unions
#pragma warning(push)
#pragma warning(disable : 4201)

#define VPCI_PROTOCOL_VERSION_RS1             0x00010002
#define VPCI_PROTOCOL_VERSION_CURRENT         VPCI_PROTOCOL_VERSION_RS1

static const UINT32 VscSupportedVersions[] =
{
    VPCI_PROTOCOL_VERSION_RS1
};


//
// Messages between the Virtual PCI driver and its VSP
//
typedef enum _VPCI_MESSAGE
{
    VpciMsgBusRelations = 0x42490000,
    VpciMsgQueryBusRelations,
    VpciMsgInvalidateDevice,
    VpciMsgInvalidateBus,
    VpciMsgDevicePowerStateChange,
    VpciMsgCurrentResourceRequirements,
    VpciMsgGetResources,
    VpciMsgFdoD0Entry,
    VpciMsgFdoD0Exit,
    VpciMsgReadBlock,
    VpciMsgWriteBlock,
    VpciMsgEject,
    VpciMsgQueryStop,
    VpciMsgReEnable,
    VpciMsgQueryStopFailed,
    VpciMsgEjectComplete,
    VpciMsgAssignedResources,
    VpciMsgReleaseResources,
    VpciMsgInvalidateBlock,
    VpciMsgQueryProtocolVersion,
    VpciMsgCreateInterruptMessage,
    VpciMsgDeleteInterruptMessage,
    VpciMsgAssignedResources2,
    VpciMsgCreateInterruptMessage2,
    VpciMsgDeleteInterruptMessage2
} VPCI_MESSAGE, *PVPCI_MESSAGE;


typedef struct _VPCI_PACKET_HEADER
{
    UINT32      MessageType;
} VPCI_PACKET_HEADER, *PVPCI_PACKET_HEADER;


typedef struct _VPCI_REPLY_HEADER
{
    UINT32      Status;
} VPCI_REPLY_HEADER, *PVPCI_REPLY_HEADER;


typedef struct _VPCI_DEVICE_DESCRIPTION
{
    VPCI_PNP_ID     IDs;
    UINT32          Slot;
    UINT32          SerialNumber;

} VPCI_DEVICE_DESCRIPTION, *PVPCI_DEVICE_DESCRIPTION;

typedef struct _VPCI_QUERY_BUS_RELATIONS
{
    VPCI_PACKET_HEADER      Header;
    UINT32                   DeviceCount;

    VPCI_DEVICE_DESCRIPTION Devices[1];

} VPCI_QUERY_BUS_RELATIONS, *PVPCI_QUERY_BUS_RELATIONS;

#define VPCI_MAX_DEVICES_PER_BUS 255

typedef struct _PCI_SLOT_NUMBER
{
    union
    {
        struct
        {
            UINT32   DeviceNumber:5;
            UINT32   FunctionNumber:3;
            UINT32   Reserved:24;
        } bits;
        UINT32   AsULONG;
    } u;
} PCI_SLOT_NUMBER, *PPCI_SLOT_NUMBER;

#define VPCI_MESSAGE_RESOURCE_2_MAX_CPU_COUNT 32

typedef struct _VPCI_MESSAGE_RESOURCE_2
{
    union
    {
        struct
        {
            UINT16    Reserved;
            UINT16    MessageCount;
            UINT32    DataPayload;
            UINT64    Address;
            UINT16    Reserved2[27];
        } Remapped;

        struct
        {
            UINT8     Vector;
            UINT8     DeliveryMode;
            UINT16    VectorCount;
            UINT16    ProcessorCount;
            UINT16    ProcessorArray[VPCI_MESSAGE_RESOURCE_2_MAX_CPU_COUNT];
        } Descriptor;
    };

} VPCI_MESSAGE_RESOURCE_2, *PVPCI_MESSAGE_RESOURCE_2;


typedef struct _VPCI_QUERY_PROTOCOL_VERSION
{
    VPCI_PACKET_HEADER              Header;
    UINT32                          ProtocolVersion;
} VPCI_QUERY_PROTOCOL_VERSION, *PVPCI_QUERY_PROTOCOL_VERSION;

typedef struct _VPCI_PROTOCOL_VERSION_REPLY
{
    VPCI_REPLY_HEADER               Header;
    UINT32                          ProtocolVersion;
} VPCI_PROTOCOL_VERSION_REPLY, *PVPCI_PROTOCOL_VERSION_REPLY;

typedef struct _VPCI_QUERY_RESOURCE_REQUIREMENTS
{
    VPCI_PACKET_HEADER              Header;
    PCI_SLOT_NUMBER                 Slot;
} VPCI_QUERY_RESOURCE_REQUIREMENTS, *PVPCI_QUERY_RESOURCE_REQUIREMENTS;


typedef struct _VPCI_RESOURCE_REQUIREMENTS_REPLY
{
    VPCI_REPLY_HEADER               Header;
    UINT32                          Bars[PCI_MAX_BAR];
} VPCI_RESOURCE_REQUIREMENTS_REPLY, *PVPCI_RESOURCE_REQUIREMENTS_REPLY;


typedef struct _VPCI_DEVICE_POWER_CHANGE
{
    union
    {
        VPCI_PACKET_HEADER         Header;
        VPCI_REPLY_HEADER          ReplyHeader;
    };
    PCI_SLOT_NUMBER                Slot;
    DEVICE_POWER_STATE             TargetState;

} VPCI_DEVICE_POWER_CHANGE, *PVPCI_DEVICE_POWER_CHANGE;

//
// This message indicates which resources the device is "decoding"
// within the child partition at the moment that it is sent.  It is
// valid for the device to be decoding no resources.  Mmio resources
// are configured using Base Address Registers which are limited to 6.
// Unused registers and registers that are used at the high part of
// 64-bit addresses are encoded as CmResourceTypeNull.
//
// The completion packet uses the same structure to return the
// translated MSI resources.
//
typedef struct _VPCI_DEVICE_TRANSLATE_2
{
    union
    {
        VPCI_PACKET_HEADER         Header;
        VPCI_REPLY_HEADER          ReplyHeader;
    };
    PCI_SLOT_NUMBER                Slot;

    CM_PARTIAL_RESOURCE_DESCRIPTOR MmioResources[PCI_MAX_BAR];

    UINT32                         MsiResourceCount;

    VPCI_MESSAGE_RESOURCE_2        MsiResources[1];

} VPCI_DEVICE_TRANSLATE_2, *PVPCI_DEVICE_TRANSLATE_2;

// NOTE: This doesn't exist in the windows header. Normally we'd use the
//       the same packet as above for the response as it gives us the remapped
//       MSI interrupts, but in UEFI we don't care about interrupts. Thus we
//       only care about the status, so this is a nice partial packet for that.
typedef struct _VPCI_DEVICE_TRANSLATE_2_REPLY
{
    VPCI_REPLY_HEADER Header;
    PCI_SLOT_NUMBER Slot;
}  VPCI_DEVICE_TRANSLATE_2_REPLY, *PVPCI_DEVICE_TRANSLATE_2_REPLY;

typedef struct _VPCI_FDO_D0_ENTRY
{
    VPCI_PACKET_HEADER      Header;
    UINT32                  Padding;
    UINT64                  MmioStart;
} VPCI_FDO_D0_ENTRY, *PVPCI_FDO_D0_ENTRY;

// NOTE: This doesn't exist in the corresponding windows header. But it's nicer
// to have this way, as this is what the response is.
typedef struct _VPCI_FDO_D0_ENTRY_REPLY
{
    UINT32 NtStatus;
    UINT32 Pad;
} VPCI_FDO_D0_ENTRY_REPLY, *PVPCI_FDO_D0_ENTRY_REPLY;

#pragma warning(pop)
