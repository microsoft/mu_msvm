/** @file

    EFI Driver for Synthetic SCSI Controller.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Uefi.h>

#include <Protocol/Vmbus.h>
#include <Protocol/Emcl.h>
#include <Protocol/ScsiPassThruExt.h>
#include <Protocol/InternalEventServices.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/CrashLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/EmclLib.h>

#include <VstorageProtocol.h>

#define STORVSC_VERSION 1
#define STORVSC_ADAPTER_CONTEXT_SIGNATURE SIGNATURE_32 ('S','V','s','c')

#define VMSTOR_MAX_TARGETS 2
#define TPL_STORVSC_CALLBACK (TPL_CALLBACK + 1)
#define TPL_STORVSC_NOTIFY TPL_NOTIFY

#define STORVSC_MAX_LUN_TRANSFER_LENGTH (sizeof(UINT8) * 8 * SCSI_MAXIMUM_LUNS_PER_TARGET)

typedef struct _STORVSC_CHANNEL_CONTEXT
{
    EFI_EMCL_V2_PROTOCOL *Emcl;
    VMSTORAGE_CHANNEL_PROPERTIES Properties;
    UINT16 ProtocolVersion;

    UINT16 MaxPacketSize;
    UINT16 MaxSrbLength;
    UINT8 MaxSrbSenseDataLength;
} STORVSC_CHANNEL_CONTEXT, *PSTORVSC_CHANNEL_CONTEXT;

typedef struct _STORVSC_ADAPTER_CONTEXT
{
    UINTN Signature;
    EFI_HANDLE Handle;

    EFI_EMCL_V2_PROTOCOL *Emcl;
    EFI_EXT_SCSI_PASS_THRU_PROTOCOL ExtScsiPassThru;
    EFI_EXT_SCSI_PASS_THRU_MODE ExtScsiPassThruMode;

    PSTORVSC_CHANNEL_CONTEXT ChannelContext;
    LIST_ENTRY LunList;
} STORVSC_ADAPTER_CONTEXT, *PSTORVSC_ADAPTER_CONTEXT;

typedef struct _STORVSC_CHANNEL_REQUEST
{
    EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET *ScsiRequest;
    EFI_EVENT Event;
} STORVSC_CHANNEL_REQUEST, *PSTORVSC_CHANNEL_REQUEST;

typedef struct _TARGET_LUN
{
    LIST_ENTRY ListEntry;
    UINT8 TargetId;
    UINT8 Lun;
} TARGET_LUN, *PTARGET_LUN;


#define STORVSC_ADAPTER_CONTEXT_FROM_EXT_SCSI_PASS_THRU_THIS(a) \
    CR( \
        a, \
        STORVSC_ADAPTER_CONTEXT, \
        ExtScsiPassThru, \
        STORVSC_ADAPTER_CONTEXT_SIGNATURE \
        )


extern EFI_DRIVER_BINDING_PROTOCOL gStorvscDriverBinding;
extern EFI_COMPONENT_NAME2_PROTOCOL gStorvscComponentName2;
extern EFI_COMPONENT_NAME_PROTOCOL gStorvscComponentName;

EFI_STATUS
EFIAPI
StorvscDriverEntryPoint (
    IN  EFI_HANDLE ImageHandle,
    IN  EFI_SYSTEM_TABLE *SystemTable
    );

EFI_STATUS
EFIAPI
StorvscDriverBindingSupported (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    );

EFI_STATUS
EFIAPI
StorvscDriverBindingStart (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    );

EFI_STATUS
EFIAPI
StorvscDriverBindingStop (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  UINTN NumberOfChildren,
    IN  EFI_HANDLE *ChildHandleBuffer
    );

EFI_STATUS
EFIAPI
StorvscComponentNameGetDriverName (
    IN  EFI_COMPONENT_NAME2_PROTOCOL *This,
    IN  CHAR8 *Language,
    OUT CHAR16 **DriverName
    );

EFI_STATUS
EFIAPI
StorvscComponentNameGetControllerName (
    IN  EFI_COMPONENT_NAME2_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_HANDLE ChildHandle OPTIONAL,
    IN  CHAR8 *Language,
    OUT CHAR16 **ControllerName
    );

EFI_STATUS
EFIAPI
StorvscExtScsiPassThruPassThru (
    IN      EFI_EXT_SCSI_PASS_THRU_PROTOCOL *This,
    IN      UINT8 *Target,
    IN      UINT64 Lun,
    IN OUT  EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET *Packet,
    IN      EFI_EVENT Event OPTIONAL
    );

EFI_STATUS
EFIAPI
StorvscExtScsiPassThruGetNextTargetLun (
    IN      EFI_EXT_SCSI_PASS_THRU_PROTOCOL *This,
    IN OUT  UINT8 **Target,
    IN OUT  UINT64 *Lun
    );

EFI_STATUS
EFIAPI
StorvscExtScsiPassThruBuildDevicePath (
    IN      EFI_EXT_SCSI_PASS_THRU_PROTOCOL *This,
    IN      UINT8 *Target,
    IN      UINT64 Lun,
    IN OUT  EFI_DEVICE_PATH_PROTOCOL **DevicePath
    );

EFI_STATUS
EFIAPI
StorvscExtScsiPassThruGetTargetLun (
    IN  EFI_EXT_SCSI_PASS_THRU_PROTOCOL *This,
    IN  EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    OUT UINT8 **Target,
    OUT UINT64 *Lun
    );

EFI_STATUS
EFIAPI
StorvscExtScsiPassThruResetChannel (
    IN  EFI_EXT_SCSI_PASS_THRU_PROTOCOL *This
    );

EFI_STATUS
EFIAPI
StorvscExtScsiPassThruResetTargetLun (
    IN  EFI_EXT_SCSI_PASS_THRU_PROTOCOL *This,
    IN  UINT8 *Target,
    IN  UINT64 Lun
    );

EFI_STATUS
EFIAPI
StorvscExtScsiPassThruGetNextTarget (
    IN      EFI_EXT_SCSI_PASS_THRU_PROTOCOL *This,
    IN OUT  UINT8 **Target
    );

EFI_STATUS
StorChannelOpen (
    IN  EFI_EMCL_V2_PROTOCOL* Emcl,
    OUT PSTORVSC_CHANNEL_CONTEXT *ChannelContext
    );

VOID
StorChannelClose (
    IN  PSTORVSC_CHANNEL_CONTEXT ChannelContext
    );

EFI_STATUS
StorChannelInitScsiPacket (
    IN  EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET *ScsiRequest,
    IN  UINT8 *Target,
    IN  UINT64 Lun,
    OUT VSTOR_PACKET *Packet,
    OUT EFI_EXTERNAL_BUFFER *ExternalBuffer
    );

VOID
StorChannelCopyPacketDataToRequest (
    IN      PVSTOR_PACKET Packet,
    IN OUT  EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET *ScsiRequest
    );

VOID
StorChannelCompletionRoutine (
    IN  VOID *Context OPTIONAL,
    IN  VOID *Buffer,
    IN  UINT32 BufferLength
    );

EFI_STATUS
StorChannelSendScsiRequest (
    IN      PSTORVSC_CHANNEL_CONTEXT ChannelContext,
    IN OUT  EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET *ScsiRequest,
    IN      UINT8 *Target,
    IN      UINT64 Lun,
    IN      EFI_EVENT Event OPTIONAL
    );

EFI_STATUS
StorChannelSendScsiRequestSync (
    IN      PSTORVSC_CHANNEL_CONTEXT ChannelContext,
    IN OUT  EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET *ScsiRequest,
    IN      UINT8 *Target,
    IN      UINT64 Lun
    );

VOID
StorChannelReceivePacketCallback (
    IN  VOID *ReceiveContext,
    IN  VOID *PacketContext,
    IN  VOID *Buffer OPTIONAL,
    IN  UINT32 BufferLength,
    IN  UINT16 TransferPageSetId,
    IN  UINT32 RangeCount,
    IN  EFI_TRANSFER_RANGE *Ranges
    );

VOID
StorChannelInitSyntheticVstorPacket (
    OUT  PVSTOR_PACKET Packet
    );

EFI_STATUS
StorChannelSendSyntheticVstorPacket (
    IN      PSTORVSC_CHANNEL_CONTEXT ChannelContext,
    IN OUT  PVSTOR_PACKET Packet
    );

EFI_STATUS
StorChannelEstablishCommunications (
    IN  PSTORVSC_CHANNEL_CONTEXT ChannelContext
    );

VOID
StorChannelTeardownReportLunsRequest (
    IN OUT  EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET *Request
    );

EFI_STATUS
StorChannelBuildLunList(
    IN  PSTORVSC_CHANNEL_CONTEXT ChannelContext,
    OUT  LIST_ENTRY *LunList
    );

VOID
StorChannelFreeLunList(
    IN OUT  LIST_ENTRY *LunList
    );

LIST_ENTRY*
StorChannelSearchLunList (
    IN  LIST_ENTRY *LunList,
    IN  UINT8 Target,
    IN  UINT8 Lun
    );

