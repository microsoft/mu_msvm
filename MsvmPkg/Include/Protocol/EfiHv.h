/** @file
  Provides the protocol definition for EFI_HV_PROTOCOL, which provides
  UEFI access to the Hyper-V hypervisor.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

#include <Hv/HvGuestHypercall.h>
#include <Hv/HvGuestSyntheticInterrupts.h>

typedef struct _EFI_HV_PROTOCOL EFI_HV_PROTOCOL;
typedef struct _EFI_HV_IVM_PROTOCOL EFI_HV_IVM_PROTOCOL;

typedef
VOID
(EFIAPI *EFI_HV_INTERRUPT_HANDLER)(
    IN  VOID *Context
    );

typedef
EFI_STATUS
(EFIAPI *EFI_HV_CONNECT_SINT)(
    IN  EFI_HV_PROTOCOL *This,
    IN  HV_SYNIC_SINT_INDEX SintIndex,
    IN  UINT8 Vector,
    IN  BOOLEAN NoProxy,
    IN  EFI_HV_INTERRUPT_HANDLER InterruptHandler,
    IN  VOID *Context
    );

typedef
EFI_STATUS
(EFIAPI *EFI_HV_CONNECT_SINT_TO_EVENT)(
    IN  EFI_HV_PROTOCOL *This,
    IN  HV_SYNIC_SINT_INDEX SintIndex,
    IN  UINT8 Vector,
    IN  EFI_EVENT Event
    );

typedef
VOID
(EFIAPI *EFI_HV_DISCONNECT_SINT)(
    IN  EFI_HV_PROTOCOL *This,
    IN  HV_SYNIC_SINT_INDEX SintIndex
    );

typedef
HV_MESSAGE *
(EFIAPI *EFI_HV_GET_SINT_MESSAGE)(
    IN  EFI_HV_PROTOCOL *This,
    IN  HV_SYNIC_SINT_INDEX SintIndex,
    IN  BOOLEAN Direct
    );

typedef
EFI_STATUS
(EFIAPI *EFI_HV_COMPLETE_SINT_MESSAGE)(
    IN  EFI_HV_PROTOCOL *This,
    IN  HV_SYNIC_SINT_INDEX SintIndex,
    IN  BOOLEAN Direct
    );

typedef
volatile HV_SYNIC_EVENT_FLAGS *
(EFIAPI *EFI_HV_GET_SINT_EVENT_FLAGS)(
    IN  EFI_HV_PROTOCOL *This,
    IN  HV_SYNIC_SINT_INDEX SintIndex,
    IN  BOOLEAN Direct
    );

typedef
UINT64
(EFIAPI *EFI_HV_GET_REFERENCE_TIME)(
    IN  EFI_HV_PROTOCOL *This
    );

typedef
UINT32
(EFIAPI *EFI_HV_GET_CURRENT_VP_INDEX)(
    IN  EFI_HV_PROTOCOL *This
    );

typedef
BOOLEAN
(EFIAPI *EFI_HV_DIRECT_TIMER_SUPPORTED)(
    VOID
    );

typedef
EFI_STATUS
(EFIAPI *EFI_HV_CONFIGURE_TIMER)(
    IN          EFI_HV_PROTOCOL *This,
    IN          UINT32 TimerIndex,
    IN          HV_SYNIC_SINT_INDEX SintIndex,
    IN          BOOLEAN Periodic,
    IN          BOOLEAN DirectMode,
    IN          UINT8 Vector,
    IN OPTIONAL EFI_HV_INTERRUPT_HANDLER InterruptHandler
    );

typedef
VOID
(EFIAPI *EFI_HV_SET_TIMER)(
    IN  EFI_HV_PROTOCOL *This,
    IN  UINT32 TimerIndex,
    IN  UINT64 Expiration
    );

typedef
EFI_STATUS
(EFIAPI *EFI_HV_POST_MESSAGE)(
    IN  EFI_HV_PROTOCOL *This,
    IN  HV_CONNECTION_ID ConnectionId,
    IN  HV_MESSAGE_TYPE MessageType,
    IN  VOID *Payload,
    IN  UINT32 PayloadSize,
    IN  BOOLEAN DirectHypercall
    );

typedef
EFI_STATUS
(EFIAPI *EFI_HV_SIGNAL_EVENT)(
    IN  EFI_HV_PROTOCOL *This,
    IN  HV_CONNECTION_ID ConnectionId,
    IN  UINT16 FlagNumber
    );

typedef
EFI_STATUS
(EFIAPI *EFI_HV_START_AP)(
    IN  EFI_HV_PROTOCOL *This,
    IN  UINT64 VpIndex,
    IN  PHV_INITIAL_VP_CONTEXT VpContext
    );

struct _EFI_HV_PROTOCOL
{
    EFI_HV_CONNECT_SINT ConnectSint;
    EFI_HV_CONNECT_SINT_TO_EVENT ConnectSintToEvent;
    EFI_HV_DISCONNECT_SINT DisconnectSint;

    EFI_HV_GET_SINT_MESSAGE GetSintMessage;
    EFI_HV_COMPLETE_SINT_MESSAGE CompleteSintMessage;
    EFI_HV_GET_SINT_EVENT_FLAGS GetSintEventFlags;

    EFI_HV_GET_REFERENCE_TIME GetReferenceTime;
    EFI_HV_GET_CURRENT_VP_INDEX GetCurrentVpIndex;

    EFI_HV_DIRECT_TIMER_SUPPORTED DirectTimerSupported;
    EFI_HV_CONFIGURE_TIMER ConfigureTimer;
    EFI_HV_SET_TIMER SetTimer;

    EFI_HV_POST_MESSAGE PostMessage;
    EFI_HV_SIGNAL_EVENT SignalEvent;

    EFI_HV_START_AP StartApplicationProcessor;
};

extern GUID gEfiHvProtocolGuid;

typedef struct _EFI_HV_PROTECTION_OBJECT *EFI_HV_PROTECTION_HANDLE;

typedef
EFI_STATUS
(EFIAPI *EFI_HV_MAKE_ADDRESS_RANGE_HOST_VISIBLE)(
    IN              EFI_HV_IVM_PROTOCOL         *This,
    IN              HV_MAP_GPA_FLAGS            MapFlags,
    IN              VOID                        *BaseAddress,
    IN              UINT32                      ByteCount,
    IN              BOOLEAN                     ZeroPages,
    OUT OPTIONAL    EFI_HV_PROTECTION_HANDLE    *ProtectionHandle
    );

typedef
VOID
(EFIAPI *EFI_HV_MAKE_ADDRESS_RANGE_NOT_HOST_VISIBLE)(
    IN  EFI_HV_IVM_PROTOCOL *This,
    IN  EFI_HV_PROTECTION_HANDLE ProtectionHandle
    );

// Interface to Hypervisor for the Isolated VM (IVM) calls
struct _EFI_HV_IVM_PROTOCOL
{
    EFI_HV_MAKE_ADDRESS_RANGE_HOST_VISIBLE MakeAddressRangeHostVisible;
    EFI_HV_MAKE_ADDRESS_RANGE_NOT_HOST_VISIBLE MakeAddressRangeNotHostVisible;
};

extern GUID gEfiHvIvmProtocolGuid;
