/** @file
  Provides the protocol definition for EFI_EMCL_PROTOCOL, which provides
  ring buffer management and packet transport for VMBus channels.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

#define TPL_EMCL (TPL_HIGH_LEVEL - 1)

typedef struct _EFI_EMCL_PROTOCOL EFI_EMCL_PROTOCOL;
typedef struct _EFI_EMCL_V2_PROTOCOL EFI_EMCL_V2_PROTOCOL;

typedef VOID EFI_EMCL_GPADL;
typedef UINT32 HV_MAP_GPA_FLAGS, *PHV_MAP_GPA_FLAGS;

typedef struct _EFI_TRANSFER_RANGE
{
    UINT32 ByteCount;
    UINT32 ByteOffset;

} EFI_TRANSFER_RANGE;

typedef struct _EFI_EXTERNAL_BUFFER
{
    VOID *Buffer;
    UINT32 BufferSize;

} EFI_EXTERNAL_BUFFER;

typedef
VOID
(*EFI_EMCL_COMPLETION_ROUTINE)(
    IN OPTIONAL VOID    *Context,
    IN          VOID    *Buffer,
                UINT32  BufferLength
    );

typedef
VOID
(*EFI_EMCL_RECEIVE_PACKET)(
    IN          VOID                *ReceiveContext,
    IN          VOID                *PacketContext,
    IN OPTIONAL VOID                *Buffer,
                UINT32              BufferLength,
                UINT16              TransferPageSetId,
                UINT32              RangeCount,
    IN          EFI_TRANSFER_RANGE  *Ranges
    );

typedef
EFI_STATUS
(EFIAPI *EFI_EMCL_START_CHANNEL)(
    IN  EFI_EMCL_PROTOCOL   *This,
        UINT32              IncomingRingBufferPageCount,
        UINT32              OutgoingRingBufferPageCount
    );

typedef
VOID
(EFIAPI *EFI_EMCL_STOP_CHANNEL)(
    IN  EFI_EMCL_PROTOCOL *This
    );

typedef
EFI_STATUS
(EFIAPI *EFI_EMCL_SEND_PACKET)(
    IN          EFI_EMCL_PROTOCOL           *This,
    IN          VOID                        *InlineBuffer,
                UINT32                      InlineBufferLength,
    IN          EFI_EXTERNAL_BUFFER         *ExternalBuffers,
                UINT32                      ExternalBufferCount,
    IN OPTIONAL EFI_EMCL_COMPLETION_ROUTINE CompletionRoutine,
    IN OPTIONAL VOID                        *CompletionContext
    );

typedef
EFI_STATUS
(EFIAPI *EFI_EMCL_COMPLETE_PACKET)(
    IN  EFI_EMCL_PROTOCOL   *This,
    IN  VOID                *PacketContext,
    IN  VOID                *Buffer,
        UINT32              BufferLength
    );

typedef
EFI_STATUS
(EFIAPI *EFI_EMCL_SET_RECEIVE_CALLBACK)(
    IN          EFI_EMCL_PROTOCOL       *This,
    IN OPTIONAL EFI_EMCL_RECEIVE_PACKET ReceiveCallback,
    IN OPTIONAL VOID                    *ReceiveContext,
    IN          EFI_TPL                 Tpl
    );

typedef
EFI_STATUS
(EFIAPI *EFI_EMCL_CREATE_GPADL)(
    IN  EFI_EMCL_PROTOCOL   *This,
    IN  VOID                *Buffer,
        UINT32              BufferLength,
    IN  HV_MAP_GPA_FLAGS    MapFlags,
    OUT EFI_EMCL_GPADL      **Gpadl
    );

typedef
UINT32
(EFIAPI *EFI_EMCL_GET_GPADL_HANDLE)(
    IN EFI_EMCL_PROTOCOL    *This,
    IN EFI_EMCL_GPADL       *Gpadl
    );

typedef
VOID*
(EFIAPI *EFI_EMCL_GET_GPADL_BUFFER)(
    IN EFI_EMCL_PROTOCOL    *This,
    IN EFI_EMCL_GPADL       *Gpadl
    );

typedef
EFI_STATUS
(EFIAPI *EFI_EMCL_DESTROY_GPADL)(
    IN EFI_EMCL_PROTOCOL    *This,
    IN EFI_EMCL_GPADL       *Gpadl
    );

typedef
EFI_STATUS
(EFIAPI *EFI_EMCL_CREATE_GPA_RANGE)(
    IN  EFI_EMCL_PROTOCOL   *This,
        UINT32              Handle,
    IN  EFI_EXTERNAL_BUFFER *ExternalBuffers,
        UINT32              ExternalBufferCount,
        BOOLEAN             Writable
    );

typedef
EFI_STATUS
(EFIAPI *EFI_EMCL_DESTROY_GPA_RANGE)(
    IN  EFI_EMCL_PROTOCOL   *This,
        UINT32              Handle
    );

struct _EFI_EMCL_PROTOCOL
{
    EFI_EMCL_START_CHANNEL StartChannel;
    EFI_EMCL_STOP_CHANNEL StopChannel;

    EFI_EMCL_SEND_PACKET SendPacket;
    EFI_EMCL_COMPLETE_PACKET CompletePacket;
    EFI_EMCL_SET_RECEIVE_CALLBACK SetReceiveCallback;

    EFI_EMCL_CREATE_GPADL CreateGpadl;
    EFI_EMCL_DESTROY_GPADL DestroyGpadl;
    EFI_EMCL_GET_GPADL_HANDLE GetGpadlHandle;
    EFI_EMCL_GET_GPADL_BUFFER GetGpadlBuffer;

    EFI_EMCL_CREATE_GPA_RANGE CreateGpaRange;
    EFI_EMCL_DESTROY_GPA_RANGE DestroyGpaRange;
};

extern EFI_GUID gEfiEmclProtocolGuid;


//
// VERSION 2 of EMCL interface
//

// Modify the handling of ExternalBuffers in scenarios
// where the data is transfered in a bounce buffer.
// Data In Only and Data Out Only are mutually exclusive
#define EMCL_SEND_FLAG_DATA_IN_ONLY 0x1
#define EMCL_SEND_FLAG_DATA_OUT_ONLY 0x2

typedef
EFI_STATUS
(EFIAPI *EFI_EMCL_SEND_PACKET_EX)(
    IN          EFI_EMCL_PROTOCOL           *This,
    IN          VOID                        *InlineBuffer,
                UINT32                      InlineBufferLength,
    IN          EFI_EXTERNAL_BUFFER         *ExternalBuffers,
                UINT32                      ExternalBufferCount,
                UINT32                      SendPacketFlags,
    IN OPTIONAL EFI_EMCL_COMPLETION_ROUTINE CompletionRoutine,
    IN OPTIONAL VOID                        *CompletionContext
    );

#pragma warning(disable : 4201)
struct _EFI_EMCL_V2_PROTOCOL {
    EFI_EMCL_PROTOCOL;

    EFI_EMCL_SEND_PACKET_EX SendPacketEx;
};
#pragma warning(default : 4201)

extern EFI_GUID gEfiEmclV2ProtocolGuid;
