/** @file
  Provides the protocol definition for EFI_VMBUS_PROTOCOL, which manages
  VMBus channels.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

#define EFI_VMBUS_PROTOCOL_FLAGS_PIPE_MODE  0x1

//
// Zero all memory in the buffer used for the GPADL.
//

#define EFI_VMBUS_PREPARE_GPADL_FLAG_ZERO_PAGES      0x1

//
// Indicates that the GPADL buffer may be in encrypted memory on a hardware
// isolated VM, if the channel is confidential. If the channel is not
// confidential, or hardware isolation is not in use, the flag has no effect.
//

#define EFI_VMBUS_PREPARE_GPADL_FLAG_ALLOW_ENCRYPTED 0x2
#define EFI_VMBUS_PREPARE_GPADL_FLAGS \
    (EFI_VMBUS_PREPARE_GPADL_FLAG_ZERO_PAGES | \
     EFI_VMBUS_PREPARE_GPADL_FLAG_ALLOW_ENCRYPTED)

typedef struct _EFI_VMBUS_PROTOCOL EFI_VMBUS_PROTOCOL;
typedef struct _EFI_VMBUS_LEGACY_PROTOCOL EFI_VMBUS_LEGACY_PROTOCOL;

typedef struct _EFI_VMBUS_GPADL EFI_VMBUS_GPADL;
typedef UINT32 HV_MAP_GPA_FLAGS, *PHV_MAP_GPA_FLAGS;

typedef
EFI_STATUS
(EFIAPI *EFI_VMBUS_CREATE_GPADL_LEGACY)(
    IN  EFI_VMBUS_LEGACY_PROTOCOL *This,
    IN  VOID *Buffer,
    IN  UINT32 BufferLength,
    OUT UINT32 *GpadlHandle
    );

typedef
EFI_STATUS
(EFIAPI *EFI_VMBUS_DESTROY_GPADL_LEGACY)(
    IN  EFI_VMBUS_LEGACY_PROTOCOL *This,
    IN  UINT32 GpadlHandle
    );

typedef
EFI_STATUS
(EFIAPI *EFI_VMBUS_OPEN_CHANNEL_LEGACY)(
    IN  EFI_VMBUS_LEGACY_PROTOCOL *This,
    IN  UINT32 RingBufferGpadlHandle,
    IN  UINT32 RingBufferPageOffset
    );

typedef
EFI_STATUS
(EFIAPI *EFI_VMBUS_CLOSE_CHANNEL_LEGACY)(
    IN  EFI_VMBUS_LEGACY_PROTOCOL *This
    );

typedef
EFI_STATUS
(EFIAPI *EFI_VMBUS_REGISTER_ISR_LEGACY)(
    IN  EFI_VMBUS_LEGACY_PROTOCOL *This,
    IN  EFI_EVENT Event OPTIONAL
    );

typedef
EFI_STATUS
(EFIAPI *EFI_VMBUS_SEND_INTERRUPT_LEGACY)(
    IN  EFI_VMBUS_LEGACY_PROTOCOL *This
    );

typedef
EFI_STATUS
(EFIAPI *EFI_VMBUS_PREPARE_GPADL)(
    IN  EFI_VMBUS_PROTOCOL *This,
    IN  VOID *Buffer,
    IN  UINT32 BufferLength,
    IN  UINT32 Flags,
    IN  HV_MAP_GPA_FLAGS MapFlags,
    OUT  EFI_VMBUS_GPADL **Gpadl
    );

typedef
EFI_STATUS
(EFIAPI *EFI_VMBUS_CREATE_GPADL)(
    IN  EFI_VMBUS_PROTOCOL *This,
    IN  EFI_VMBUS_GPADL *Gpadl
    );

typedef
UINT32
(EFIAPI *EFI_VMBUS_GET_GPADL_HANDLE)(
    IN  EFI_VMBUS_PROTOCOL *This,
    IN  EFI_VMBUS_GPADL *Gpadl
    );

typedef
VOID*
(EFIAPI *EFI_VMBUS_GET_GPADL_BUFFER)(
    IN  EFI_VMBUS_PROTOCOL *This,
    IN  EFI_VMBUS_GPADL *Gpadl
    );

typedef
EFI_STATUS
(EFIAPI *EFI_VMBUS_DESTROY_GPADL)(
    IN  EFI_VMBUS_PROTOCOL *This,
    IN  EFI_VMBUS_GPADL *Gpadl
    );

typedef
EFI_STATUS
(EFIAPI *EFI_VMBUS_OPEN_CHANNEL)(
    IN  EFI_VMBUS_PROTOCOL *This,
    IN  EFI_VMBUS_GPADL *RingBufferGpadl,
    IN  UINT32 RingBufferPageOffset
    );

typedef
EFI_STATUS
(EFIAPI *EFI_VMBUS_CLOSE_CHANNEL)(
    IN  EFI_VMBUS_PROTOCOL *This
    );

typedef
EFI_STATUS
(EFIAPI *EFI_VMBUS_REGISTER_ISR)(
    IN  EFI_VMBUS_PROTOCOL *This,
    IN  EFI_EVENT Event OPTIONAL
    );

typedef
EFI_STATUS
(EFIAPI *EFI_VMBUS_SEND_INTERRUPT)(
    IN  EFI_VMBUS_PROTOCOL *This
    );

struct _EFI_VMBUS_LEGACY_PROTOCOL
{
    EFI_VMBUS_CREATE_GPADL_LEGACY CreateGpadl;
    EFI_VMBUS_DESTROY_GPADL_LEGACY DestroyGpadl;

    EFI_VMBUS_OPEN_CHANNEL_LEGACY OpenChannel;
    EFI_VMBUS_CLOSE_CHANNEL_LEGACY CloseChannel;

    EFI_VMBUS_REGISTER_ISR_LEGACY RegisterIsr;
    EFI_VMBUS_SEND_INTERRUPT_LEGACY SendInterrupt;

    UINT32 Flags;
};

struct _EFI_VMBUS_PROTOCOL
{
    EFI_VMBUS_PREPARE_GPADL PrepareGpadl;
    EFI_VMBUS_CREATE_GPADL CreateGpadl;
    EFI_VMBUS_DESTROY_GPADL DestroyGpadl;
    EFI_VMBUS_GET_GPADL_BUFFER GetGpadlBuffer;
    EFI_VMBUS_GET_GPADL_HANDLE GetGpadlHandle;

    EFI_VMBUS_OPEN_CHANNEL OpenChannel;
    EFI_VMBUS_CLOSE_CHANNEL CloseChannel;

    EFI_VMBUS_REGISTER_ISR RegisterIsr;
    EFI_VMBUS_SEND_INTERRUPT SendInterrupt;

    UINT32 Flags;
};

typedef struct _VMBUS_DEVICE_PATH
{
    VENDOR_DEVICE_PATH VendorDevicePath;

    GUID InterfaceType;
    GUID InterfaceInstance;

} VMBUS_DEVICE_PATH;

extern EFI_GUID gEfiVmbusProtocolGuid;
extern EFI_GUID gEfiVmbusLegacyProtocolGuid;
