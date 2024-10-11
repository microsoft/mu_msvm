/** @file
    EFI Driver for Synthetic Network Controller

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

#include <Uefi.h>

#include <Protocol/Vmbus.h>
#include <Protocol/Emcl.h>
#include <Protocol/SimpleNetwork.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/EmclLib.h>
#include <Library/SerialPortLib.h>
#include <Library/Printlib.h>

#include <NvspProtocol.h>

#define MAXIMUM_ETHERNET_PACKET_SIZE        1514

// TODO: Make the number of packets in the buffer a PCD variable.
#define NVSC_DEFAULT_RECEIVE_BUFFER_SIZE    MAXIMUM_ETHERNET_PACKET_SIZE * 128
#define NVSC_DEFAULT_SEND_BUFFER_SIZE       MAXIMUM_ETHERNET_PACKET_SIZE * 128

#define NETVSC_VERSION 1

typedef struct _ETHERNET_HEADER
{
    UINT8 DestAddr[PXE_HWADDR_LEN_ETHER];
    UINT8 SrcAddr[PXE_HWADDR_LEN_ETHER];
    UINT16 Type;
} ETHERNET_HEADER;

typedef struct _RX_PACKET_INSTANCE
{
    VOID * PacketContext;
    VOID * Buffer;
    UINT32 BufferLength;
    BOOLEAN CompletionNeeded;
} RX_PACKET_INSTANCE;

typedef struct _RX_QUEUE
{
    RX_PACKET_INSTANCE    *Buffer;
    UINT32                Length;
    UINT32                Head;
    UINT32                Tail;
} RX_QUEUE;

typedef struct _TX_QUEUE
{
    VOID      **Buffer;
    UINT32    Length;
    UINT32    Head;
    UINT32    Tail;
} TX_QUEUE;

typedef struct _NIC_DATA_INSTANCE
{
    EFI_EMCL_PROTOCOL         *Emcl;
    EFI_NETWORK_STATISTICS    Statistics;
    UINTN                     SupportedStatisticsSize;
    BOOLEAN                   MediaPresent;
    BOOLEAN                   EmclStarted;

    UINT8                     PermNodeAddress[PXE_MAC_LENGTH];
    UINT8                     CurrentNodeAddress[PXE_MAC_LENGTH];
    UINT8                     BroadcastNodeAddress[PXE_MAC_LENGTH];

    EFI_EVENT                 RxFilterEvt;
    EFI_STATUS                SetRxFilterStatus;
    EFI_EVENT                 StnAddrEvt;
    EFI_STATUS                GetStnAddrStatus;
    EFI_EVENT                 InitRndisEvt;
    EFI_STATUS                InitRndisStatus;

    VOID                      *RxBufferAllocation;
    VOID                      *RxBuffer;
    UINT32                    RxBufferPageCount;
    UINT32                    RxQueueCount;
    EFI_EMCL_GPADL            *RxGpadl;
    BOOLEAN                   RxInterrupt;
    BOOLEAN                   ReceiveStarted;
    UINT8                     RxFilter;

    VOID                      *TxBufferAllocation;
    VOID                      *TxBuffer;
    UINT32                    TxBufferPageCount;
    UINT32                    TxBufCount;
    UINT32                    TxSectionSize;
    EFI_EMCL_GPADL            *TxGpadl;
    BOOLEAN                   TxedInterrupt;

    RX_QUEUE                  RxPacketQueue;
    TX_QUEUE                  FreeTxBuffersQueue;
    TX_QUEUE                  TxedBuffersQueue;
} NIC_DATA_INSTANCE;

typedef struct _NETVSC_ADAPTER_CONTEXT
{
    EFI_HANDLE                  ControllerHandle;
    EFI_HANDLE                  DeviceHandle;
    EFI_DEVICE_PATH_PROTOCOL    *BaseDevPath;
    EFI_DEVICE_PATH_PROTOCOL    *DevPath;
    NIC_DATA_INSTANCE           NicInfo;
} NETVSC_ADAPTER_CONTEXT, *PNETVSC_ADAPTER_CONTEXT;

typedef struct _TX_PACKET_CONTEXT
{
    NIC_DATA_INSTANCE      *AdapterInfo;
    EFI_EXTERNAL_BUFFER    BufferInfo;
    VOID                   *TxBuffer;
} TX_PACKET_CONTEXT;

EFI_STATUS
NetvscInit(
    IN  NIC_DATA_INSTANCE *AdapterInfo
    );

EFI_STATUS
NetvscShutdown(
    IN  NIC_DATA_INSTANCE *AdapterInfo
    );

EFI_STATUS
NetvscSetFilter(
    IN  NIC_DATA_INSTANCE *AdapterInfo,
    IN  UINT32            newFilter
    );

EFI_STATUS
NetvscTransmit(
    IN  NIC_DATA_INSTANCE               *AdapterInfo,
    IN  VOID                            *Buffer,
    IN  UINT32                          BufferSize
    );

VOID
NetvscReceiveCallback(
    IN  VOID                                    *ReceiveContext,
    IN  VOID                                    *PacketContext,
    IN  VOID                                    *Buffer,
    IN  UINT32                                  BufferLength,
    IN  UINT16                                  TransferPageSetId,
    IN  UINT32                                  RangeCount,
    IN  EFI_TRANSFER_RANGE                      *Ranges
    );

EFI_STATUS
NetvscReceive(
    IN      NIC_DATA_INSTANCE               *AdapterInfo,
    OUT     VOID                            *Buffer,
    IN OUT  UINTN                           *BufferSize,
    OUT     UINTN                         *HeaderSize OPTIONAL,
    OUT     EFI_MAC_ADDRESS               *SrcAddr OPTIONAL,
    OUT     EFI_MAC_ADDRESS               *DestAddr OPTIONAL,
    OUT     UINT16                        *Protocol OPTIONAL
    );

VOID
NetvscResetStatistics(
    IN  NIC_DATA_INSTANCE *AdapterInfo
    );

EFI_STATUS
NvspStatusToEfiStatus(
    IN  NVSP_STATUS nvspStatus
);

__forceinline
EFI_STATUS
RxQueueInit(
    IN  RX_QUEUE    *Queue,
    IN  UINT32      Length
    );

__forceinline
VOID
RxQueueDestroy(
    IN  RX_QUEUE *Queue
    );

__forceinline
BOOLEAN
RxQueueIsAlmostFull(
    IN  RX_QUEUE *Queue
    );

__forceinline
BOOLEAN
RxQueueIsEmpty(
    IN  RX_QUEUE *Queue
    );

__forceinline
VOID
RxQueueEnqueue(
    IN  RX_QUEUE                *Queue,
    IN  RX_PACKET_INSTANCE      *PacketInfo
    );

__forceinline
VOID
RxQueueDequeue(
    IN  RX_QUEUE                *Queue,
    OUT RX_PACKET_INSTANCE      *PacketInfo
    );

__forceinline
EFI_STATUS
TxQueueInit(
    IN  TX_QUEUE    *Queue,
    IN  UINT32      Length
    );

__forceinline
VOID
TxQueueDestroy(
    IN  TX_QUEUE *Queue
    );

__forceinline
BOOLEAN
TxQueueIsFull(
    IN  TX_QUEUE *Queue
    );

__forceinline
BOOLEAN
TxQueueIsEmpty(
    IN  TX_QUEUE *Queue
    );

__forceinline
VOID
TxQueueEnqueue(
    IN  TX_QUEUE    *Queue,
    IN  VOID        *TxBuffer
    );

__forceinline
VOID
TxQueueDequeue(
    IN  TX_QUEUE     *Queue,
    OUT VOID        **TxBuffer
    );


