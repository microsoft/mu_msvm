/** @file
  EFI Driver for Synthetic Network Controller
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <IsolationTypes.h>

#include <Protocol/EfiHv.h>
#include <Protocol/Emcl.h>
#include <Protocol/InternalEventServices.h>

#include <Library/DebugLib.h>
#include <Library/EmclLib.h>

#include "NetvscDxe.h"
#include "NvspProtocol.h"
#include "rndis.h"

//
// This number is just a random 16 bit number which is used to
// identify the single receive buffer.
//
#define RECEIVE_BUFFER_ID                               0x1981
#define SEND_BUFFER_ID                                  0xBEEF

//
// Request IDs for various RNDIS queries
//
#define PERM_NODE_ADDR_REQUEST_ID    0xFAAD
#define CURR_NODE_ADDR_REQUEST_ID    0xFCAD
#define SET_FILTER_REQUEST_ID        0x5CF1
#define SET_STAT_ADDR_REQUEST_ID     0x5CAD

//
// NDIS Receive Filter masks
//
#define  NDIS_PACKET_TYPE_DIRECTED        0x00000001
#define  NDIS_PACKET_TYPE_MULTICAST       0x00000002
#define  NDIS_PACKET_TYPE_ALL_MULTICAST   0x00000004
#define  NDIS_PACKET_TYPE_BROADCAST       0x00000008
#define  NDIS_PACKET_TYPE_PROMISCUOUS     0x00000020

//
// NDIS Status values for REMOTE_NDIS_INDICATE_STATUS_MSG messages
//
#define NDIS_STATUS_NETWORK_CHANGE              ((NDIS_STATUS)0x40010018L)

#define TPL_NETVSC_CALLBACK                (TPL_CALLBACK + 2)

INTERNAL_EVENT_SERVICES_PROTOCOL *mInternalEventServices = NULL;


EFI_STATUS
NetvscInit(
    IN  NIC_DATA_INSTANCE *AdapterInfo
    )
/*++

Routine Description:

    This routine fills up the NIC_DATA_INSTANCE structure.

Arguments:

    AdapterInfo - Pointer to the NIC data structure information

Return Value:

    EFI_SUCCESS - Successful

    Other values - Failure initializing vNIC

--*/
{
    EFI_STATUS status;
    UINT64 txBuffer;
    PRNDIS_MESSAGE pRndisMessage;
    PRNDIS_INITIALIZE_REQUEST pInitRequest;
    PRNDIS_QUERY_REQUEST pQueryRequest;
    UINT32 rndisMsgSize;
    UINT32 rndisBufferIndex, index;
    NVSP_MESSAGE nvspMessage;
    UINTN eventIndex;
    ASSERT(AdapterInfo != NULL);

    //
    // The net VSC cannot run safely inside of an isolated VM, so refuse to
    // start up if this VM is isolated.
    //

    if (IsIsolated())
    {
        return EFI_DEVICE_ERROR;
    }

    //
    // Initialize variables.
    //
    AdapterInfo->RxBufferAllocation = NULL;
    AdapterInfo->RxBuffer = NULL;

    AdapterInfo->TxBufferAllocation = NULL;
    AdapterInfo->TxBuffer = NULL;

    AdapterInfo->TxGpadl = NULL;
    AdapterInfo->RxGpadl = NULL;

    AdapterInfo->ReceiveStarted = FALSE;

    AdapterInfo->InitRndisStatus = (EFI_STATUS) -1;
    AdapterInfo->SetRxFilterStatus = (EFI_STATUS) -1;
    AdapterInfo->GetStnAddrStatus = (EFI_STATUS) -1;

    //
    // When the host has disabled media present notifications, NetvscDxe
    // must default to TRUE or PXE won't work
    //
    AdapterInfo->MediaPresent = PcdGetBool(PcdMediaPresentEnabledByDefault);

    //
    // Locate the protocol for waiting for events without the TPL restrictions.
    //
    if (mInternalEventServices == NULL)
    {
        status = gBS->LocateProtocol(
                        &gInternalEventServicesProtocolGuid,
                        NULL,
                        (VOID **)&mInternalEventServices);
        ASSERT_EFI_ERROR(status);
    }

    NetvscResetStatistics(AdapterInfo);

    //
    // Create events to synchronize RNDIS Initialization, setting Filters and getting station addr.
    //
    status = gBS->CreateEvent(
        0,
        0,
        NULL,
        NULL,
        &AdapterInfo->RxFilterEvt);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = gBS->CreateEvent(
        0,
        0,
        NULL,
        NULL,
        &AdapterInfo->StnAddrEvt);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = gBS->CreateEvent(
        0,
        0,
        NULL,
        NULL,
        &AdapterInfo->InitRndisEvt);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Create the EMCL channel
    // The ReceiveCallback function must be set before starting the channel.
    //
    status = AdapterInfo->Emcl->SetReceiveCallback(
        AdapterInfo->Emcl,
        NetvscReceiveCallback,
        (VOID *) AdapterInfo,
        TPL_NETVSC_CALLBACK);

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    //
    // Allocate receive and transmit buffers as a multiple of pages.  This is
    // required for isolated VMs and is acceptable in all VMs.
    //
    AdapterInfo->RxBufferPageCount = (NVSC_DEFAULT_RECEIVE_BUFFER_SIZE + EFI_PAGE_SIZE - 1) >> EFI_PAGE_SHIFT;
    AdapterInfo->TxBufferPageCount = (NVSC_DEFAULT_SEND_BUFFER_SIZE + EFI_PAGE_SIZE - 1) >> EFI_PAGE_SHIFT;

    status = AdapterInfo->Emcl->StartChannel(
        AdapterInfo->Emcl,
        AdapterInfo->RxBufferPageCount,
        AdapterInfo->TxBufferPageCount);

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    AdapterInfo->EmclStarted = TRUE;

    //
    // Find the protocol version that VSP understands.
    // Use only the current network version.
    //
    ZeroMem(&nvspMessage, sizeof(nvspMessage));
    nvspMessage.Header.MessageType = NvspMessageTypeInit;
    nvspMessage.Messages.InitMessages.Init.ProtocolVersion = NVSP_PROTOCOL_VERSION_CURRENT;
    nvspMessage.Messages.InitMessages.Init.ProtocolVersion2 = NVSP_PROTOCOL_VERSION_CURRENT;

    status = EmclSendPacketSync(
        AdapterInfo->Emcl,
        &nvspMessage,
        sizeof(nvspMessage),
        NULL,
        0);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    if (nvspMessage.Header.MessageType != NvspMessageTypeInitComplete)
    {
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    if (nvspMessage.Messages.InitMessages.InitComplete.Status != NvspStatusSuccess)
    {
        status = NvspStatusToEfiStatus(nvspMessage.Messages.InitMessages.InitComplete.Status);
        goto Cleanup;
    }

    //
    // Send NDIS config info and set version to be 6.
    //
    ZeroMem(&nvspMessage, sizeof(nvspMessage));
    nvspMessage.Header.MessageType = NvspMessage2TypeSendNdisConfig;
    nvspMessage.Messages.Version2Messages.SendNdisConfig.MTU = MAXIMUM_ETHERNET_PACKET_SIZE;
    nvspMessage.Messages.Version2Messages.SendNdisConfig.Capabilities.CorrelationId = 0;
    nvspMessage.Messages.Version2Messages.SendNdisConfig.Capabilities.Ieee8021q = 0;
    nvspMessage.Messages.Version2Messages.SendNdisConfig.Capabilities.SRIOV = 0;
    nvspMessage.Messages.Version2Messages.SendNdisConfig.Capabilities.Teaming = 0;
    nvspMessage.Messages.Version2Messages.SendNdisConfig.Capabilities.VMQ = 0;

    status = EmclSendPacketSync(
        AdapterInfo->Emcl,
        &nvspMessage,
        sizeof(nvspMessage),
        NULL,
        0);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    ZeroMem(&nvspMessage, sizeof(nvspMessage));
    nvspMessage.Header.MessageType = NvspMessage1TypeSendNdisVersion;
    nvspMessage.Messages.Version1Messages.SendNdisVersion.NdisMajorVersion = 6;
    nvspMessage.Messages.Version1Messages.SendNdisVersion.NdisMinorVersion = 0;

    status = EmclSendPacketSync(
        AdapterInfo->Emcl,
        &nvspMessage,
        sizeof(nvspMessage),
        NULL,
        0);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Allocate the Receive buffers and report them to the VSP.
    //
    AdapterInfo->RxBufferAllocation = AllocatePages(AdapterInfo->RxBufferPageCount);
    if (AdapterInfo->RxBufferAllocation == NULL)
    {
        goto Cleanup;
    }

    status = AdapterInfo->Emcl->CreateGpadl(
        AdapterInfo->Emcl,
        AdapterInfo->RxBufferAllocation,
        AdapterInfo->RxBufferPageCount * EFI_PAGE_SIZE,
        HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE,
        &AdapterInfo->RxGpadl);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    AdapterInfo->RxBuffer = AdapterInfo->Emcl->GetGpadlBuffer(
        AdapterInfo->Emcl,
        AdapterInfo->RxGpadl);

    ZeroMem(&nvspMessage, sizeof(nvspMessage));
    nvspMessage.Header.MessageType = NvspMessage1TypeSendReceiveBuffer;
    nvspMessage.Messages.Version1Messages.SendReceiveBuffer.GpadlHandle =
        AdapterInfo->Emcl->GetGpadlHandle(AdapterInfo->Emcl, AdapterInfo->RxGpadl);
    nvspMessage.Messages.Version1Messages.SendReceiveBuffer.Id = RECEIVE_BUFFER_ID;

    status = EmclSendPacketSync(
        AdapterInfo->Emcl,
        &nvspMessage,
        sizeof(nvspMessage),
        NULL,
        0);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    if (nvspMessage.Header.MessageType != NvspMessage1TypeSendReceiveBufferComplete)
    {
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    if (nvspMessage.Messages.Version1Messages.SendReceiveBufferComplete.Status != NvspStatusSuccess)
    {
        status = NvspStatusToEfiStatus(nvspMessage.Messages.Version1Messages.SendReceiveBufferComplete.Status);
        goto Cleanup;
    }

    //
    // Calculating the number of receive slots.
    //
    AdapterInfo->RxQueueCount = 0;
    for (index = 0; index < nvspMessage.Messages.Version1Messages.SendReceiveBufferComplete.NumSections; index++)
    {
        AdapterInfo->RxQueueCount += nvspMessage.Messages.Version1Messages.SendReceiveBufferComplete.Sections[index].NumSubAllocations;
    }

    //
    // The Ring Buffer should never be completely full as the Open-Slot solution
    // is being used to differentiate between a full and an empty buffer.
    // Hence the BufferLength is incremented.
    //
    AdapterInfo->RxQueueCount++;

    //
    // Allocate the Send buffers and report them to the VSP.
    //
    AdapterInfo->TxBufferAllocation = AllocatePages(AdapterInfo->TxBufferPageCount);
    if (AdapterInfo->TxBufferAllocation == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    //
    // SNP hardware does not support read-only pages. But only allow read
    // access for the software isolation case where more restricted access is possible.
    //
    status = AdapterInfo->Emcl->CreateGpadl(
        AdapterInfo->Emcl,
        AdapterInfo->TxBufferAllocation,
        AdapterInfo->TxBufferPageCount * EFI_PAGE_SIZE,
        IsSoftwareIsolated() ? HV_MAP_GPA_READABLE : HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE,
        &AdapterInfo->TxGpadl);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    AdapterInfo->TxBuffer = AdapterInfo->Emcl->GetGpadlBuffer(
        AdapterInfo->Emcl,
        AdapterInfo->TxGpadl);

    ZeroMem(&nvspMessage, sizeof(nvspMessage));
    nvspMessage.Header.MessageType = NvspMessage1TypeSendSendBuffer;
    nvspMessage.Messages.Version1Messages.SendSendBuffer.GpadlHandle =
        AdapterInfo->Emcl->GetGpadlHandle(AdapterInfo->Emcl, AdapterInfo->TxGpadl);
    nvspMessage.Messages.Version1Messages.SendSendBuffer.Id = SEND_BUFFER_ID;

    status = EmclSendPacketSync(
        AdapterInfo->Emcl,
        &nvspMessage,
        sizeof(nvspMessage),
        NULL,
        0);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    if (nvspMessage.Header.MessageType != NvspMessage1TypeSendSendBufferComplete)
    {
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    if (nvspMessage.Messages.Version1Messages.SendSendBufferComplete.Status != NvspStatusSuccess)
    {
        status = NvspStatusToEfiStatus(nvspMessage.Messages.Version1Messages.SendSendBufferComplete.Status);
        goto Cleanup;
    }

    AdapterInfo->TxSectionSize = nvspMessage.Messages.Version1Messages.SendSendBufferComplete.SectionSize;

    //
    // The Ring Buffer should always have an empty slot to differentiate
    // between full and empty buffers. Hence the +1.
    //
    AdapterInfo->TxBufCount = (NVSC_DEFAULT_SEND_BUFFER_SIZE/AdapterInfo->TxSectionSize) + 1;

    //
    // Initializing various queues
    //
    status = RxQueueInit(&AdapterInfo->RxPacketQueue, AdapterInfo->RxQueueCount);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = TxQueueInit(&AdapterInfo->FreeTxBuffersQueue,AdapterInfo->TxBufCount);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Create a circular buffer to save the transmitted buffers.
    //
    status = TxQueueInit(&AdapterInfo->TxedBuffersQueue, AdapterInfo->TxBufCount);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    for (txBuffer = (UINT64) AdapterInfo->TxBuffer; (txBuffer + AdapterInfo->TxSectionSize)<=((UINT64)AdapterInfo->TxBuffer + NVSC_DEFAULT_SEND_BUFFER_SIZE); txBuffer += (AdapterInfo->TxSectionSize))
    {
        if (TxQueueIsFull(&AdapterInfo->FreeTxBuffersQueue))
        {
            break;
        }
        TxQueueEnqueue(&AdapterInfo->FreeTxBuffersQueue, (VOID *) (UINTN) txBuffer);
    }

    AdapterInfo->RxFilter = 0;
    AdapterInfo->ReceiveStarted = TRUE;

    //
    // Send an RNDIS message to initialize the RNDIS device
    //
    if (TxQueueIsEmpty(&AdapterInfo->FreeTxBuffersQueue))
    {
        DEBUG((EFI_D_ERROR, "SNP Initialize Error: EFI_BUFFER_TOO_SMALL\n"));
        status = EFI_BUFFER_TOO_SMALL;
        goto Cleanup;
    }
    else
    {
        //
        // The buffer is used temporarily for multiple sync transactions.
        // Hence, dequeueing the buffer from the FreeTxBufferQueue isn't required.
        //
        TxQueueDequeue(&AdapterInfo->FreeTxBuffersQueue, &pRndisMessage);
        TxQueueEnqueue(&AdapterInfo->FreeTxBuffersQueue, pRndisMessage);
        rndisBufferIndex = (UINT32)((((UINT64) pRndisMessage) - ((UINT64) AdapterInfo->TxBuffer))/AdapterInfo->TxSectionSize);
    }

    rndisMsgSize = RNDIS_MESSAGE_SIZE(RNDIS_INITIALIZE_REQUEST);

    pInitRequest = &pRndisMessage->Message.InitializeRequest;
    pInitRequest->RequestId = 0xBEEF;
    pInitRequest->MajorVersion = RNDIS_MAJOR_VERSION;
    pInitRequest->MinorVersion = RNDIS_MINOR_VERSION;
    pInitRequest->MaxTransferSize = MAXIMUM_ETHERNET_PACKET_SIZE;

    pRndisMessage->NdisMessageType = REMOTE_NDIS_INITIALIZE_MSG;
    pRndisMessage->MessageLength = rndisMsgSize;

    ZeroMem(&nvspMessage, sizeof(nvspMessage));
    nvspMessage.Header.MessageType = NvspMessage1TypeSendRNDISPacket;
    nvspMessage.Messages.Version1Messages.SendRNDISPacket.ChannelType = 1;
    nvspMessage.Messages.Version1Messages.SendRNDISPacket.SendBufferSectionIndex = rndisBufferIndex;
    nvspMessage.Messages.Version1Messages.SendRNDISPacket.SendBufferSectionSize = rndisMsgSize;

    status = EmclSendPacketSync(
        AdapterInfo->Emcl,
        &nvspMessage,
        sizeof(nvspMessage),
        NULL,
        0);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    if (nvspMessage.Header.MessageType != NvspMessage1TypeSendRNDISPacketComplete)
    {
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    if (nvspMessage.Messages.Version1Messages.SendRNDISPacketComplete.Status != NvspStatusSuccess)
    {
        status = NvspStatusToEfiStatus(nvspMessage.Messages.Version1Messages.SendRNDISPacketComplete.Status);
        goto Cleanup;
    }

    //
    // This can be called from TPL_CALLBACK. Use WaitForEventInternal instead of gBS->WaitForEvent
    // which enforces a TPL check for TPL_APPLICATION.
    //
    status = mInternalEventServices->WaitForEventInternal(1, &AdapterInfo->InitRndisEvt, &eventIndex);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    if (EFI_ERROR(AdapterInfo->InitRndisStatus))
    {
        status = AdapterInfo->InitRndisStatus;
        AdapterInfo->InitRndisStatus = (EFI_STATUS)-1;
        goto Cleanup;
    }

    AdapterInfo->InitRndisStatus = (EFI_STATUS)-1;

    //
    // Retrieving the Node Addresses.
    // This is done during Initialization only as MAC spoofing is not enabled.
    // So it can be safely assumed that these addresses will not change.
    //
    AdapterInfo->PermNodeAddress[0] = 0;
    AdapterInfo->PermNodeAddress[1] = 0;
    AdapterInfo->PermNodeAddress[2] = 0;
    AdapterInfo->PermNodeAddress[3] = 0;
    AdapterInfo->PermNodeAddress[4] = 0;
    AdapterInfo->PermNodeAddress[5] = 0;

    //
    // Use the transmit buffer from the previous operation
    //
    rndisMsgSize = RNDIS_MESSAGE_SIZE(RNDIS_QUERY_REQUEST);

    pQueryRequest = &pRndisMessage->Message.QueryRequest;
    pQueryRequest->RequestId = PERM_NODE_ADDR_REQUEST_ID;
    pQueryRequest->Oid = RNDIS_OID_802_3_CURRENT_ADDRESS;
    pQueryRequest->InformationBufferLength = 0;
    pQueryRequest->InformationBufferOffset = sizeof(RNDIS_QUERY_REQUEST);
    pQueryRequest->DeviceVcHandle = 0;

    pRndisMessage->NdisMessageType = REMOTE_NDIS_QUERY_MSG;
    pRndisMessage->MessageLength = rndisMsgSize;

    ZeroMem(&nvspMessage, sizeof(nvspMessage));
    nvspMessage.Header.MessageType = NvspMessage1TypeSendRNDISPacket;
    nvspMessage.Messages.Version1Messages.SendRNDISPacket.ChannelType = 1;
    nvspMessage.Messages.Version1Messages.SendRNDISPacket.SendBufferSectionIndex = rndisBufferIndex;
    nvspMessage.Messages.Version1Messages.SendRNDISPacket.SendBufferSectionSize = rndisMsgSize;

    status = EmclSendPacketSync(
        AdapterInfo->Emcl,
        &nvspMessage,
        sizeof(nvspMessage),
        NULL,
        0);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    if (nvspMessage.Header.MessageType != NvspMessage1TypeSendRNDISPacketComplete)
    {
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    if (nvspMessage.Messages.Version1Messages.SendRNDISPacketComplete.Status != NvspStatusSuccess)
    {
        status = NvspStatusToEfiStatus(nvspMessage.Messages.Version1Messages.SendRNDISPacketComplete.Status);
        goto Cleanup;
    }

    //
    // This can be called from TPL_CALLBACK. Use WaitForEventInternal instead of gBS->WaitForEvent
    // which enforces a TPL check for TPL_APPLICATION.
    //
    status = mInternalEventServices->WaitForEventInternal(1, &AdapterInfo->StnAddrEvt, &eventIndex);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    if (EFI_ERROR(AdapterInfo->GetStnAddrStatus))
    {
        status = AdapterInfo->GetStnAddrStatus;
        AdapterInfo->GetStnAddrStatus = (EFI_STATUS)-1;
        goto Cleanup;
    }
    AdapterInfo->GetStnAddrStatus = (EFI_STATUS)-1;

    AdapterInfo->CurrentNodeAddress[0] = 0;
    AdapterInfo->CurrentNodeAddress[1] = 0;
    AdapterInfo->CurrentNodeAddress[2] = 0;
    AdapterInfo->CurrentNodeAddress[3] = 0;
    AdapterInfo->CurrentNodeAddress[4] = 0;
    AdapterInfo->CurrentNodeAddress[5] = 0;


    //
    // Use the RNDIS message from the previous message.
    // Change the OID and refresh the NVSP message.
    //
    rndisMsgSize = RNDIS_MESSAGE_SIZE(RNDIS_QUERY_REQUEST);

    pQueryRequest->RequestId = CURR_NODE_ADDR_REQUEST_ID;
    pQueryRequest->Oid = RNDIS_OID_802_3_CURRENT_ADDRESS;
    pQueryRequest->InformationBufferLength = 0;
    pQueryRequest->InformationBufferOffset = sizeof(RNDIS_QUERY_REQUEST);
    pQueryRequest->DeviceVcHandle = 0;

    pRndisMessage->NdisMessageType = REMOTE_NDIS_QUERY_MSG;
    pRndisMessage->MessageLength = rndisMsgSize;

    ZeroMem(&nvspMessage, sizeof(nvspMessage));
    nvspMessage.Header.MessageType = NvspMessage1TypeSendRNDISPacket;
    nvspMessage.Messages.Version1Messages.SendRNDISPacket.ChannelType = 1;
    nvspMessage.Messages.Version1Messages.SendRNDISPacket.SendBufferSectionIndex = rndisBufferIndex;
    nvspMessage.Messages.Version1Messages.SendRNDISPacket.SendBufferSectionSize = rndisMsgSize;

    status = EmclSendPacketSync(
        AdapterInfo->Emcl,
        &nvspMessage,
        sizeof(nvspMessage),
        NULL,
        0);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    if (nvspMessage.Header.MessageType != NvspMessage1TypeSendRNDISPacketComplete)
    {
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    if (nvspMessage.Messages.Version1Messages.SendRNDISPacketComplete.Status != NvspStatusSuccess)
    {
        status = NvspStatusToEfiStatus(nvspMessage.Messages.Version1Messages.SendRNDISPacketComplete.Status);
        goto Cleanup;
    }

    //
    // This can be called from TPL_CALLBACK. Use WaitForEventInternal instead of gBS->WaitForEvent
    // which enforces a TPL check for TPL_APPLICATION.
    //
    status = mInternalEventServices->WaitForEventInternal(1, &AdapterInfo->StnAddrEvt, &eventIndex);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    if (EFI_ERROR(AdapterInfo->GetStnAddrStatus))
    {
        AdapterInfo->GetStnAddrStatus = (EFI_STATUS)-1;
        status = AdapterInfo->GetStnAddrStatus;
        goto Cleanup;
    }
    AdapterInfo->GetStnAddrStatus = (EFI_STATUS)-1;

    //
    // Hard-coding Broadcast address
    //
    AdapterInfo->BroadcastNodeAddress[0] = 0xFF;
    AdapterInfo->BroadcastNodeAddress[1] = 0xFF;
    AdapterInfo->BroadcastNodeAddress[2] = 0xFF;
    AdapterInfo->BroadcastNodeAddress[3] = 0xFF;
    AdapterInfo->BroadcastNodeAddress[4] = 0xFF;
    AdapterInfo->BroadcastNodeAddress[5] = 0xFF;

Cleanup:

    if (EFI_ERROR(status))
    {
        NetvscShutdown(AdapterInfo);
    }

Exit:

    return status;
}


EFI_STATUS
NetvscSetFilter(
    IN  NIC_DATA_INSTANCE   *AdapterInfo,
    IN  UINT32              NewFilter
    )
/*++

Routine Description:

    Sets the Filters on the VSP for this vNIC.

Arguments:

    AdapterInfo                 - Pointer to the NIC data structure information

    NewFilter                     - The bitmask representing the new filter

Returns:

    EFI_SUCCESS                               - Successful

    Other       - Failure

--*/
{
    UINT16 oldFilter;
    UINT32 ndisFilter = 0;
    PRNDIS_MESSAGE pRndisMessage;
    NVSP_MESSAGE nvspMessage;
    UINT32 rndisMsgSize;
    PRNDIS_SET_REQUEST pSetRequest;
    UINT32 rndisBufferIndex = 0;
    EFI_STATUS status;
    UINTN eventIndex;

    oldFilter = AdapterInfo->RxFilter;

    AdapterInfo->RxFilter = (UINT8) NewFilter;

    if (NewFilter == oldFilter)
    {
        status = EFI_SUCCESS;
        goto Exit;
    }

    if (AdapterInfo->RxFilter & EFI_SIMPLE_NETWORK_RECEIVE_UNICAST)
    {
        ndisFilter |= NDIS_PACKET_TYPE_DIRECTED;
    }
    if (AdapterInfo->RxFilter & EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS)
    {
        ndisFilter |= NDIS_PACKET_TYPE_PROMISCUOUS;
    }
    if (AdapterInfo->RxFilter & EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST)
    {
        ndisFilter |= NDIS_PACKET_TYPE_BROADCAST;
    }
    if (AdapterInfo->RxFilter & EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST)
    {
        ndisFilter |= NDIS_PACKET_TYPE_MULTICAST;
    }
    if (AdapterInfo->RxFilter & EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS_MULTICAST)
    {
        ndisFilter |= NDIS_PACKET_TYPE_ALL_MULTICAST;
    }

    //
    // Send RNDIS control message to set Receive Filters.
    //
    if (TxQueueIsEmpty(&AdapterInfo->FreeTxBuffersQueue))
    {
        status  = EFI_DEVICE_ERROR;
        goto Exit;
    }
    else
    {
        //
        // The buffer is used temporarily for a sync transaction.
        // Hence, dequeueing the buffer from the FreeTxBufferQueue isn't required.
        //
        TxQueueDequeue(&AdapterInfo->FreeTxBuffersQueue, &pRndisMessage);
        TxQueueEnqueue(&AdapterInfo->FreeTxBuffersQueue, pRndisMessage);
        rndisBufferIndex = (UINT32)((((UINT64) pRndisMessage) - ((UINT64) AdapterInfo->TxBuffer))/AdapterInfo->TxSectionSize);
    }

    //
    // Set RndisMessage properties as needed to set Receive Filters
    //
    rndisMsgSize = RNDIS_MESSAGE_SIZE(RNDIS_SET_REQUEST) + sizeof(ndisFilter);

    pSetRequest = &pRndisMessage->Message.SetRequest;
    pSetRequest->RequestId = SET_FILTER_REQUEST_ID;
    pSetRequest->Oid = RNDIS_OID_GEN_CURRENT_PACKET_FILTER;
    pSetRequest->InformationBufferLength = sizeof(ndisFilter);
    pSetRequest->InformationBufferOffset = sizeof(RNDIS_SET_REQUEST);
    pSetRequest->DeviceVcHandle = 0;

    CopyMem((UINT8*)(pSetRequest) + pSetRequest->InformationBufferOffset, &ndisFilter, sizeof(ndisFilter));

    pRndisMessage->NdisMessageType = REMOTE_NDIS_SET_MSG;
    pRndisMessage->MessageLength = rndisMsgSize;

    ZeroMem(&nvspMessage, sizeof(nvspMessage));
    nvspMessage.Header.MessageType = NvspMessage1TypeSendRNDISPacket;
    nvspMessage.Messages.Version1Messages.SendRNDISPacket.ChannelType = 1;
    nvspMessage.Messages.Version1Messages.SendRNDISPacket.SendBufferSectionIndex = rndisBufferIndex;
    nvspMessage.Messages.Version1Messages.SendRNDISPacket.SendBufferSectionSize = rndisMsgSize;

    status = EmclSendPacketSync(
        AdapterInfo->Emcl,
        &nvspMessage,
        sizeof(nvspMessage),
        NULL,
        0);

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    if (nvspMessage.Header.MessageType != NvspMessage1TypeSendRNDISPacketComplete)
    {
        status = EFI_DEVICE_ERROR;
        goto Exit;
    }

    if (nvspMessage.Messages.Version1Messages.SendRNDISPacketComplete.Status != NvspStatusSuccess)
    {
        status = NvspStatusToEfiStatus(nvspMessage.Messages.Version1Messages.SendRNDISPacketComplete.Status);
        goto Exit;
    }

    //
    // This can be called from TPL_CALLBACK. Use WaitForEventInternal instead of gBS->WaitForEvent
    // which enforces a TPL check for TPL_APPLICATION.
    //
    status = mInternalEventServices->WaitForEventInternal(1, &AdapterInfo->RxFilterEvt, &eventIndex);

    if (EFI_ERROR(status))
    {
        AdapterInfo->RxFilter = (UINT8) oldFilter;
        AdapterInfo->SetRxFilterStatus = (EFI_STATUS) -1;
        goto Exit;
    }

    status = AdapterInfo->SetRxFilterStatus;
    if (EFI_ERROR(status))
    {
        AdapterInfo->RxFilter = (UINT8) oldFilter;
    }
    AdapterInfo->SetRxFilterStatus = (EFI_STATUS) -1;

Exit:

    return status;
}


VOID
NetvscTransmitCallback(
    IN  VOID                    *Context OPTIONAL,
    IN  VOID                    *Buffer,
    IN  UINT32                  BufferLength
    )
/*++

Routine Description:

    This routine is a callback called by EMCL when it has finished processing a transmitted packet.
    It is the equivalent of the Emcl->CompletePacket call for the packets received by the VSP.

Arguments:

    Context         - Context provided while sending the packet.
                           It's the pointer to TX_PACKET_CONTEXT for the sent packet

    Buffer           - Buffer associated with the completion packet. Should be of type NVSP_MESSAGE

    BufferLength  - Length of the Buffer

--*/
{
    TX_PACKET_CONTEXT *txPacketContext;
    NIC_DATA_INSTANCE *adapterInfo;

    ASSERT(Context != NULL);

    txPacketContext = (TX_PACKET_CONTEXT *) Context;
    adapterInfo = txPacketContext->AdapterInfo;

    TxQueueEnqueue(&adapterInfo->FreeTxBuffersQueue, txPacketContext->TxBuffer);
    TxQueueEnqueue(&adapterInfo->TxedBuffersQueue, txPacketContext->BufferInfo.Buffer);
    adapterInfo->TxedInterrupt = TRUE;

    FreePool(txPacketContext);
}


EFI_STATUS
NetvscTransmit(
    IN  NIC_DATA_INSTANCE       *AdapterInfo,
    IN  VOID                    *Buffer,
    IN  UINT32                  BufferSize
    )
/*++

Routine Description:

    Transmits a packet to the network.

Arguments:

    AdapterInfo      - Pointer to the vNIC's NIC_DATA_INSTANCE

    Buffer              - A pointer to the buffer to be transmitted

    BufferSize        - Size of the Buffer

Returns:

    EFI_SUCCESS  -Packet sent successfully.

    Other STATCODEs        -Transmit failed

--*/
{
    EFI_STATUS status = EFI_SUCCESS;
    TX_PACKET_CONTEXT *txPacketContext = NULL;
    NVSP_MESSAGE rndisMessage;
    UINT32 bufferIndex;
    UINT32 bufferOffset;
    BOOLEAN bufferIsEmpty;
    BOOLEAN bufferIsFull;
    PRNDIS_MESSAGE currentTxBuffer;
    PRNDIS_PACKET currentTxPacket;
    VOID *currentTxData;

    ASSERT(BufferSize <= AdapterInfo->TxSectionSize);

    AdapterInfo->Statistics.TxTotalFrames++;
    AdapterInfo->Statistics.TxTotalBytes += BufferSize;

    bufferIsEmpty = TxQueueIsEmpty(&AdapterInfo->FreeTxBuffersQueue);
    if (bufferIsEmpty)
    {
        AdapterInfo->Statistics.TxDroppedFrames++;
        status =  EFI_NOT_READY;
        goto Cleanup;
    }

    bufferIsFull = TxQueueIsFull(&AdapterInfo->TxedBuffersQueue);
    if (bufferIsFull)
    {
        AdapterInfo->Statistics.TxDroppedFrames++;
        status = EFI_NOT_READY;
        goto Cleanup;
    }

    TxQueueDequeue(&AdapterInfo->FreeTxBuffersQueue, &currentTxBuffer);

    txPacketContext = AllocatePool(sizeof(TX_PACKET_CONTEXT));

    if (txPacketContext == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    txPacketContext->AdapterInfo = AdapterInfo;
    txPacketContext->BufferInfo.Buffer = Buffer;
    txPacketContext->BufferInfo.BufferSize = (UINT32) (sizeof(RNDIS_MESSAGE) - sizeof(RNDIS_MESSAGE_CONTAINER) + sizeof(RNDIS_PACKET) + BufferSize);
    txPacketContext->TxBuffer = currentTxBuffer;

    bufferOffset = (UINT32) ((UINT64) currentTxBuffer - (UINT64) AdapterInfo->TxBuffer);
    ASSERT(bufferOffset%AdapterInfo->TxSectionSize == 0);
    bufferIndex = bufferOffset/AdapterInfo->TxSectionSize;

    //
    // Populating the Rndis Message with appropriate values and packet data.
    //
    currentTxBuffer->NdisMessageType = REMOTE_NDIS_PACKET_MSG;
    currentTxBuffer->MessageLength = sizeof(RNDIS_MESSAGE) + BufferSize;

    currentTxPacket = &currentTxBuffer->Message.Packet;
    currentTxPacket->DataOffset = sizeof(RNDIS_MESSAGE_CONTAINER);
    currentTxPacket->DataLength = BufferSize;

    //
    // Zero out the unneeded variables.
    //
    currentTxPacket->OOBDataLength = 0;
    currentTxPacket->OOBDataOffset = 0;
    currentTxPacket->NumOOBDataElements = 0;
    currentTxPacket->VcHandle = 0;
    currentTxPacket->PerPacketInfoLength = 0;
    currentTxPacket->PerPacketInfoOffset = 0;

    currentTxData = (VOID *)((UINT8*)currentTxPacket + currentTxPacket->DataOffset);
    CopyMem(currentTxData, Buffer, BufferSize);

    rndisMessage.Header.MessageType = NvspMessage1TypeSendRNDISPacket;
    rndisMessage.Messages.Version1Messages.SendRNDISPacket.ChannelType = 2;
    rndisMessage.Messages.Version1Messages.SendRNDISPacket.SendBufferSectionIndex = bufferIndex;
    rndisMessage.Messages.Version1Messages.SendRNDISPacket.SendBufferSectionSize = currentTxBuffer->MessageLength;

    status = AdapterInfo->Emcl->SendPacket(
        AdapterInfo->Emcl,
        &rndisMessage,
        sizeof(rndisMessage),
        NULL,
        0,
        NetvscTransmitCallback,
        txPacketContext);

    if (EFI_ERROR(status))
    {
        TxQueueEnqueue(&AdapterInfo->FreeTxBuffersQueue, currentTxBuffer);
        DEBUG((EFI_D_ERROR, "TRANSMIT ERROR: %x\n", status));
        goto Cleanup;
    }

    AdapterInfo->Statistics.TxGoodFrames++;

    if (BufferSize < 64)
    {
        AdapterInfo->Statistics.TxUndersizeFrames++;
    }

Cleanup:

    if (EFI_ERROR(status) && txPacketContext != NULL)
    {
        FreePool(txPacketContext);
    }

    return status;
}


EFI_STATUS
NetvscReceive(
    IN      NIC_DATA_INSTANCE               *AdapterInfo,
    OUT     VOID                            *Buffer,
    IN OUT  UINTN                           *BufferSize,
    OUT     UINTN                           *HeaderSize OPTIONAL,
    OUT     EFI_MAC_ADDRESS                 *SrcAddr OPTIONAL,
    OUT     EFI_MAC_ADDRESS                 *DestAddr OPTIONAL,
    OUT     UINT16                          *Protocol OPTIONAL
    )
/*++

Routine Description:

    Receives a packet from the network.

Arguments:

    AdapterInfo         Pointer to the vNIC's NIC_DATA_INSTANCE

    Buffer                 A pointer to PXE_DB_RECEIVE where a packet and other relevant data is to be copied

    BufferSize           A pointer which points to the Command Parameter Block PXE_CPB_RECEIVE

    HeaderSize         A pointer (may be NULL) to output the size of the header

    SrcAddr              A pointer (may be NULL) to output the Source of the packet

    DestAddr            A pointer (may be NULL) to output the Destination of the packet

    Protocol              A pointer (may be NULL) to output the Protocol of the packet

Returns:

    EFI_SUCCESS        -Packet received successfully.

    EFI_NOT_READY     -There were no outstanding packets

--*/
{
    BOOLEAN bufferIsEmpty;
    ETHERNET_HEADER *headerPtr;
    RX_PACKET_INSTANCE currPacket;
    UINT32 bytesToBeCopied;
    INT32 index;
    NVSP_MESSAGE message;
    EFI_STATUS status = EFI_SUCCESS;
    static UINT32 counter;

    ZeroMem(&message, sizeof(NVSP_MESSAGE));

    if (!AdapterInfo->ReceiveStarted)
    {
        status = EFI_NOT_READY;
        goto Exit;
    }

    bufferIsEmpty = RxQueueIsEmpty(&AdapterInfo->RxPacketQueue);
    if (bufferIsEmpty)
    {
        status = EFI_NOT_READY;
        goto Exit;
    }

    RxQueueDequeue(&AdapterInfo->RxPacketQueue, &currPacket);

    bytesToBeCopied = MIN(currPacket.BufferLength, (UINT32) *BufferSize);
    CopyMem(Buffer, currPacket.Buffer, bytesToBeCopied);

    //
    // Extracting relevant Packet data for the caller
    //
    headerPtr = (ETHERNET_HEADER *) Buffer;
    if (Protocol != NULL)
    {
        *Protocol = headerPtr->Type;
    }

    *BufferSize = currPacket.BufferLength;
    if (HeaderSize != NULL)
    {
        *HeaderSize = PXE_MAC_HEADER_LEN_ETHER;
    }

    if (SrcAddr != NULL)
    {
        CopyMem(SrcAddr->Addr, headerPtr->SrcAddr, PXE_HWADDR_LEN_ETHER * sizeof(UINT8));
    }

    if (DestAddr != NULL)
    {
        CopyMem(DestAddr->Addr, headerPtr->DestAddr, PXE_HWADDR_LEN_ETHER * sizeof(UINT8));
    }

    //
    // Finding Packet type
    //
    for (index = 0; index < PXE_HWADDR_LEN_ETHER; index++)
    {
        if (headerPtr->DestAddr[index] != AdapterInfo->CurrentNodeAddress[index])
        {
            break;
        }
    }

    if (index > PXE_HWADDR_LEN_ETHER)
    {
        AdapterInfo->Statistics.RxUnicastFrames++;
    }
    else
    {
        for (index = 0; index < PXE_HWADDR_LEN_ETHER; index++)
        {
            if (headerPtr->DestAddr[index] != AdapterInfo->BroadcastNodeAddress[index])
            {
                break;
            }
        }

        if (index > PXE_HWADDR_LEN_ETHER)
        {
            AdapterInfo->Statistics.RxBroadcastFrames++;
        }
        else if ((headerPtr->DestAddr[0] >= 224 && headerPtr->DestAddr[0] < 239) ||
                 (headerPtr->DestAddr[0] == 0xFF && headerPtr->DestAddr[1] == 0))
        {
            AdapterInfo->Statistics.RxMulticastFrames++;
        }
    }

    if (currPacket.CompletionNeeded)
    {

        //
        // Send an ACK to NetVSP
        //
        message.Header.MessageType = NvspMessage1TypeSendRNDISPacketComplete;
        message.Messages.Version1Messages.SendRNDISPacketComplete.Status = NvspStatusSuccess;

        AdapterInfo->Emcl->CompletePacket(
            AdapterInfo->Emcl,
            currPacket.PacketContext,
            &message,
            sizeof(message));
    }
    else
    {
        FreePool(currPacket.Buffer);
    }

Exit:

    return status;
}


VOID
NetvscReceiveCallback(
    IN  VOID                                    *ReceiveContext,
    IN  VOID                                    *PacketContext,
    IN  VOID                                    *Buffer,
    IN  UINT32                                  BufferLength,
    IN  UINT16                                  TransferPageSetId,
    IN  UINT32                                  RangeCount,
    IN  EFI_TRANSFER_RANGE                      *Ranges
    )
/*++

Routine Description:

    The callback function called by EMCL when it has a packet for this vNIC.

Arguments:

    ReceiveContext       - The context for each receive. It's the pointer to the NIC_DATA_INSTANCE

    PacketContext         - The context to be used for Emcl->CompletePacket

    Buffer                     - Pointer to the NVSP_MESSAGE

    BufferLength           -  Length of the NVSP_MESSAGE

    TransferPageSetId   -Not Applicable

    RangeCount            - No. of memory ranges. Each memory range corresponds to a separate packet.

    Ranges                   -An array of ranges where each range is specified by the offset from the channel's
                                   Receive Buffer and the length

--*/
{
    NIC_DATA_INSTANCE *adapterInfo;
    BOOLEAN bufferIsFull;
    NVSP_MESSAGE outNvspMessage;
    PRNDIS_MESSAGE pRndisMessage;
    UINT32 bufferIndex;
    PRNDIS_PACKET pRndisPacket;
    PRNDIS_QUERY_COMPLETE pQueryReqComplete;
    VOID *packetBuffer, *copyPacketBuffer;
    UINT32 rangeIndex;
    UINT8 *nodeAddr;
    UINT32 numDataPkts = 0;
    RX_PACKET_INSTANCE newPacketInfo;

    ASSERT(RangeCount > 0);
    ZeroMem(&outNvspMessage, sizeof(NVSP_MESSAGE));
    bufferIndex = ((PNVSP_MESSAGE)Buffer)->Messages.Version1Messages.SendRNDISPacket.SendBufferSectionIndex;

    adapterInfo = (NIC_DATA_INSTANCE *) ReceiveContext;

    if (!adapterInfo->ReceiveStarted)
    {
        goto Exit;
    }

    //
    // Assumption: Every range is a new packet.
    //
    for (rangeIndex = 0; rangeIndex < RangeCount; rangeIndex++)
    {
        pRndisMessage = (PRNDIS_MESSAGE)((UINT8*)adapterInfo->RxBuffer + Ranges[rangeIndex].ByteOffset);

        switch (pRndisMessage->NdisMessageType)
        {
        case REMOTE_NDIS_PACKET_MSG:
            adapterInfo->Statistics.RxTotalFrames++;

            pRndisPacket = &pRndisMessage->Message.Packet;
            adapterInfo->Statistics.RxTotalBytes += pRndisPacket->DataLength;

            if (pRndisPacket->DataLength < 64)
            {
                adapterInfo->Statistics.RxUndersizeFrames++;
            }

            //
            // Leaving an empty slot in the receive buffers for the Control packets.
            // Otherwise "unreceived" packets can deadlock the system
            //
            bufferIsFull = RxQueueIsAlmostFull(&adapterInfo->RxPacketQueue);

            if (bufferIsFull)
            {
                adapterInfo->Statistics.RxDroppedFrames++;
                break;
            }

            numDataPkts++;
            packetBuffer = ((UINT8*)pRndisPacket) + pRndisPacket->DataOffset;

            //
            // The packet should start and ends in the specified range.
            //
            ASSERT((UINT64)packetBuffer < ((UINT64)pRndisMessage + Ranges[rangeIndex].ByteCount));
            ASSERT(((UINT64)packetBuffer + pRndisPacket->DataLength) <= ((UINT64)pRndisMessage + Ranges[rangeIndex].ByteCount));

            //
            // If there are multiple packets, the extra data packets will be copied into separate pools
            //
            if (numDataPkts > 1)
            {
                copyPacketBuffer = AllocatePool(pRndisPacket->DataLength);
                if (copyPacketBuffer == NULL)
                {
                    adapterInfo->Statistics.RxDroppedFrames++;
                    break;
                }
                CopyMem(copyPacketBuffer, packetBuffer, pRndisPacket->DataLength);
                packetBuffer = copyPacketBuffer;
            }

            newPacketInfo.PacketContext = PacketContext;
            newPacketInfo.Buffer = packetBuffer;
            newPacketInfo.BufferLength = pRndisPacket->DataLength;
            newPacketInfo.CompletionNeeded = (numDataPkts == 1);
            RxQueueEnqueue(&adapterInfo->RxPacketQueue, &newPacketInfo);

            adapterInfo->RxInterrupt = TRUE;

            adapterInfo->Statistics.RxGoodFrames++;

            break;

        case REMOTE_NDIS_QUERY_CMPLT:
            pQueryReqComplete = &pRndisMessage->Message.QueryComplete;
            if (pQueryReqComplete->Status != RNDIS_STATUS_SUCCESS ||
                pQueryReqComplete->InformationBufferLength != PXE_HWADDR_LEN_ETHER)
            {
                adapterInfo->GetStnAddrStatus = EFI_DEVICE_ERROR;
            }
            else
            {
                ASSERT(pQueryReqComplete->RequestId == PERM_NODE_ADDR_REQUEST_ID ||
                       pQueryReqComplete->RequestId == CURR_NODE_ADDR_REQUEST_ID);

                switch(pQueryReqComplete->RequestId)
                {
                case PERM_NODE_ADDR_REQUEST_ID:
                    nodeAddr = (UINT8*)((UINTN) pQueryReqComplete + pQueryReqComplete->InformationBufferOffset);
                    CopyMem(
                        &adapterInfo->PermNodeAddress,
                        nodeAddr,
                        PXE_HWADDR_LEN_ETHER * sizeof(UINT8));
                    break;

                case CURR_NODE_ADDR_REQUEST_ID:
                    nodeAddr = (UINT8*)((UINTN) pQueryReqComplete + pQueryReqComplete->InformationBufferOffset);
                    CopyMem(
                        &adapterInfo->CurrentNodeAddress,
                        nodeAddr,
                        PXE_HWADDR_LEN_ETHER * sizeof(UINT8));
                    break;
                }
                adapterInfo->GetStnAddrStatus = EFI_SUCCESS;
            }
            gBS->SignalEvent(adapterInfo->StnAddrEvt);
            break;

        case REMOTE_NDIS_INITIALIZE_CMPLT:
            DEBUG((EFI_D_NET, "RNDIS Initialization Complete.\n"));
            if (pRndisMessage->Message.InitializeComplete.Status != RNDIS_STATUS_SUCCESS)
            {
                adapterInfo->InitRndisStatus = EFI_DEVICE_ERROR;
            }
            else
            {
                adapterInfo->InitRndisStatus = EFI_SUCCESS;
            }

            gBS->SignalEvent(adapterInfo->InitRndisEvt);
            break;

        case REMOTE_NDIS_SET_CMPLT:
            DEBUG((EFI_D_NET, "RNDIS SetFilter Complete.\n"));
            if (pRndisMessage->Message.SetComplete.Status != RNDIS_STATUS_SUCCESS)
            {
               adapterInfo->SetRxFilterStatus = EFI_DEVICE_ERROR;
            }
            else
            {
                adapterInfo->SetRxFilterStatus = EFI_SUCCESS;
            }

            gBS->SignalEvent(adapterInfo->RxFilterEvt);
            break;

        case REMOTE_NDIS_INDICATE_STATUS_MSG:
            switch (pRndisMessage->Message.IndicateStatus.Status)
            {
            case NDIS_STATUS_NETWORK_CHANGE:
            case RNDIS_STATUS_MEDIA_CONNECT:
                adapterInfo->MediaPresent = TRUE;
                break;

            case RNDIS_STATUS_MEDIA_DISCONNECT:
                adapterInfo->MediaPresent = FALSE;
                break;

                //
                // Ignore all other Status values
                //
            }

            break;

        default:
            DEBUG((EFI_D_WARN, "RNDIS Packet of Unknown type received. Type = %d\n", pRndisMessage->NdisMessageType));
        }
    }

Exit:

    if (numDataPkts == 0)
    {
        outNvspMessage.Header.MessageType = NvspMessage1TypeSendRNDISPacketComplete;
        outNvspMessage.Messages.Version1Messages.SendRNDISPacketComplete.Status = NvspStatusSuccess;

        adapterInfo->Emcl->CompletePacket(
            adapterInfo->Emcl,
            PacketContext,
            &outNvspMessage,
            sizeof(outNvspMessage));
    }
}


EFI_STATUS
NetvscShutdown(
    IN  NIC_DATA_INSTANCE *AdapterInfo
    )
/*++

Routine Description:

    Shutdown the vNIC. Closes the EMCL channel and destroys all pools and events.

Arguments:

    AdapterInfo - Pointer to the vNIC's NIC_DATA_INSTANCE

Returns:

    EFI_SUCCESS     - Shutdown succeeded

    Other values       - Shutdown failed

--*/
{
    if (AdapterInfo->EmclStarted)
    {
        AdapterInfo->Emcl->StopChannel(AdapterInfo->Emcl);
        AdapterInfo->EmclStarted = FALSE;

        //
        // GPADLs need to be destroyed after the channel is closed to
        // make sure the VSP has torn down its view mapping.
        //
        if (AdapterInfo->RxGpadl != NULL)
        {
            AdapterInfo->Emcl->DestroyGpadl(AdapterInfo->Emcl, AdapterInfo->RxGpadl);
            AdapterInfo->RxGpadl = NULL;
            AdapterInfo->RxBuffer = NULL;
        }

        if (AdapterInfo->TxGpadl != NULL)
        {
            AdapterInfo->Emcl->DestroyGpadl(AdapterInfo->Emcl, AdapterInfo->TxGpadl);
            AdapterInfo->TxGpadl = NULL;
            AdapterInfo->TxBuffer = NULL;
        }
    }

    AdapterInfo->ReceiveStarted = FALSE;

    if (AdapterInfo->InitRndisEvt != NULL)
    {
        gBS->CloseEvent(AdapterInfo->InitRndisEvt);
        AdapterInfo->InitRndisEvt = NULL;
    }

    if (AdapterInfo->StnAddrEvt != NULL)
    {
        gBS->CloseEvent(AdapterInfo->StnAddrEvt);
        AdapterInfo->StnAddrEvt = NULL;
    }

    if (AdapterInfo->RxFilterEvt)
    {
        gBS->CloseEvent(AdapterInfo->RxFilterEvt);
        AdapterInfo->RxFilterEvt = NULL;
    }

    RxQueueDestroy(&AdapterInfo->RxPacketQueue);
    TxQueueDestroy(&AdapterInfo->TxedBuffersQueue);
    TxQueueDestroy(&AdapterInfo->FreeTxBuffersQueue);

    if (AdapterInfo->RxBufferAllocation != NULL)
    {
        ASSERT(AdapterInfo->RxBuffer == NULL);
        FreePages(AdapterInfo->RxBufferAllocation, AdapterInfo->RxBufferPageCount);
        AdapterInfo->RxBufferAllocation = NULL;
    }

    if (AdapterInfo->TxBufferAllocation != NULL)
    {
        ASSERT(AdapterInfo->TxBuffer == NULL);
        FreePages(AdapterInfo->TxBufferAllocation, AdapterInfo->TxBufferPageCount);
        AdapterInfo->TxBufferAllocation = NULL;
    }

    return EFI_SUCCESS;
}


VOID
NetvscResetStatistics(
    IN  NIC_DATA_INSTANCE *AdapterInfo
    )
/*++

Routine Description:

    Resets the Statistics to 0 for Supported statistics and -1 for the rest.

Arguments:

    AdapterInfo                     - Pointer to the NIC data structure information

--*/
{
    UINT64 *lastSupportedStatistic;
    //
    // The Supported Statistics are initialized to 0.
    // The unsupported statistics are initialized to -1.
    //
    AdapterInfo->Statistics.RxTotalFrames       = 0;
    AdapterInfo->Statistics.RxGoodFrames        = 0;
    AdapterInfo->Statistics.RxUndersizeFrames   = 0;
    AdapterInfo->Statistics.RxOversizeFrames    = (UINT64) -1;
    AdapterInfo->Statistics.RxDroppedFrames     = 0;
    AdapterInfo->Statistics.RxUnicastFrames     = 0;
    AdapterInfo->Statistics.RxBroadcastFrames   = 0;
    AdapterInfo->Statistics.RxMulticastFrames   = 0;
    AdapterInfo->Statistics.RxCrcErrorFrames    = (UINT64) -1;
    AdapterInfo->Statistics.RxTotalBytes        = 0;
    AdapterInfo->Statistics.TxTotalFrames       = 0;
    AdapterInfo->Statistics.TxGoodFrames        = 0;
    AdapterInfo->Statistics.TxUndersizeFrames   = 0;
    AdapterInfo->Statistics.TxOversizeFrames    = (UINT64) -1;
    AdapterInfo->Statistics.TxDroppedFrames     = 0;
    AdapterInfo->Statistics.TxUnicastFrames     = (UINT64) -1;
    AdapterInfo->Statistics.TxBroadcastFrames   = (UINT64) -1;
    AdapterInfo->Statistics.TxMulticastFrames   = (UINT64) -1;
    AdapterInfo->Statistics.TxCrcErrorFrames    = (UINT64) -1;
    AdapterInfo->Statistics.TxTotalBytes        = 0;

    //
    // End of supported statistics
    //
    lastSupportedStatistic = &AdapterInfo->Statistics.TxTotalBytes;

    //
    // The rest of the statistics are unsupported.
    //
    AdapterInfo->Statistics.Collisions          = (UINT64) -1;
    AdapterInfo->Statistics.UnsupportedProtocol = (UINT64) -1;

    //
    // Setting the buffer size required for retrieving all Supported Statistics
    //
    AdapterInfo->SupportedStatisticsSize = (UINTN)(((UINT8*)(lastSupportedStatistic + 1)) - ((UINT8*) &AdapterInfo->Statistics));
}


EFI_STATUS
NvspStatusToEfiStatus(
    NVSP_STATUS NvspStatus
)
/*++

Routine Description:

    Converts NVSP_STATUS values to EFI_STATUS values

Arguments:

    NvspStatus - The NVSP_STATUS value

Returns:

    Corresponding EFI_STATUS value

--*/
{
    EFI_STATUS status;

    switch(NvspStatus)
    {
        case NvspStatusSuccess:
            status = EFI_SUCCESS;
            break;

        case NvspStatusFailure:
            status = EFI_DEVICE_ERROR;
            break;

        case NvspStatusInvalidRndisPacket:
            status = EFI_INVALID_PARAMETER;
            break;

        case NvspStatusBusy:
            status = EFI_NOT_READY;
            break;

        case NvspStatusProtocolVersionUnsupported:
            status = EFI_UNSUPPORTED;
            break;

        default:
            status = EFI_DEVICE_ERROR;
    }

    return status;
}


EFI_STATUS
RxQueueInit(
    IN  RX_QUEUE    *Queue,
    IN  UINT32      Length
    )
/*++

Routine Description:

    Initializes the Queue.

Arguments:

    Queue - The Queue to be initialized.

    Length - The number of elements the Queue needs to have.

Return Value:

    EFI_SUCCESS                    - Queue Initialized.

    EFI_OUT_OF_RESOURCES   - Memory allocation failed.

--*/
{
    EFI_STATUS status = EFI_SUCCESS;

    ASSERT(Length > 0);

    Queue->Head = 0;
    Queue->Tail = 0;
    Queue->Length = Length;
    Queue->Buffer = AllocatePool(Length * sizeof(RX_PACKET_INSTANCE));
    if (Queue->Buffer == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
    }
    else
    {
        ZeroMem(Queue->Buffer, Length * sizeof(RX_PACKET_INSTANCE));
    }

    return status;
}


VOID
RxQueueDestroy(
    IN  RX_QUEUE *Queue
    )
/*++

Routine Description:

    Destroys the Queue i.e. deallocates memory and zeroes all the variables.

Arguments:

    Queue - The Queue to be destroyed.

--*/
{
    if (Queue->Buffer != NULL)
    {
        FreePool(Queue->Buffer);
    }

    ZeroMem(Queue, sizeof(RX_QUEUE));
}


BOOLEAN
RxQueueIsAlmostFull(
    IN  RX_QUEUE *Queue
    )
/*++

Routine Description:

    Checks if the queue has just one slot open.

    We cannot use up all the VSP-VSC Receive Buffers for packets as driver might be waiting
    for a Control Packet (e.g. Set-Filter-Complete) to start running again. The lack of an open
    Receive Buffer will block the packet and deadlock the system.

Arguments:

    Queue - The Queue to be checked.

Return Value:

    TRUE - There is only one empty slot.

    FALSE - There is more than one empty slot.

--*/
{
    ASSERT((Queue->Tail + 1)%Queue->Length != (Queue->Head));
    return ((Queue->Tail + 2)%Queue->Length == (Queue->Head));
}


BOOLEAN
RxQueueIsEmpty(
    IN  RX_QUEUE *Queue
    )
/*++

Routine Description:

    Checks if the queue is empty.

Arguments:

    Queue - The Queue to be checked.

Return Value:

    TRUE - Queue is empty.

    FALSE - Queue is not empty.

--*/
{
    return (Queue->Head == Queue->Tail);
}


VOID
RxQueueEnqueue(
    IN  RX_QUEUE              *Queue,
    IN  RX_PACKET_INSTANCE    *PacketInfo
    )
/*++

Routine Description:

    Inserts an element in the queue.

    The function DOES NOT CHECK whether the queue is full. This has to be done by the user.
    If this function is called for a full queue, it implies a coding error or a race condition.

Arguments:

    Queue       - The Queue.

    PacketInfo - The element to be inserted in the queue.

--*/
{
    ASSERT(RxQueueIsAlmostFull(Queue) == FALSE);
    ASSERT(Queue->Buffer[Queue->Tail].PacketContext == NULL &&
           Queue->Buffer[Queue->Tail].Buffer == NULL &&
           Queue->Buffer[Queue->Tail].BufferLength == 0);

    CopyMem(&Queue->Buffer[Queue->Tail], PacketInfo, sizeof(RX_PACKET_INSTANCE));
    Queue->Tail = (Queue->Tail + 1)%Queue->Length;
}


VOID
RxQueueDequeue(
    IN  RX_QUEUE               *Queue,
    OUT RX_PACKET_INSTANCE    *PacketInfo
    )
/*++

Routine Description:

    Dequeues an element from the queue.

    The function DOES NOT CHECK whether the queue is empty. This has to be done by the user.
    If this function is called for an empty queue, it implies a coding error or a race condition.

Arguments:

    Queue       - The Queue.

    PacketInfo - The pointer for outputting the dequeued element.

--*/
{
    ASSERT(RxQueueIsEmpty(Queue) == FALSE);
    ASSERT(Queue->Buffer[Queue->Head].PacketContext != NULL &&
           Queue->Buffer[Queue->Head].Buffer != NULL &&
           Queue->Buffer[Queue->Head].BufferLength != 0);

    CopyMem(PacketInfo, &Queue->Buffer[Queue->Head], sizeof(RX_PACKET_INSTANCE));
    ZeroMem(&Queue->Buffer[Queue->Head], sizeof(RX_PACKET_INSTANCE));
    Queue->Head = (Queue->Head + 1)%Queue->Length;
}


EFI_STATUS
TxQueueInit(
    IN  TX_QUEUE    *Queue,
    IN  UINT32      Length
    )
/*++

Routine Description:

    Initializes the Queue.

Arguments:

    Queue - The Queue to be initialized.

    Length - The number of elements the Queue needs to have.

Returns:

    EFI_SUCCESS                    - Queue Initialized.

    EFI_OUT_OF_RESOURCES   - Memory allocation failed.

--*/
{
    EFI_STATUS status = EFI_SUCCESS;

    ASSERT(Length > 0);

    Queue->Head = 0;
    Queue->Tail = 0;
    Queue->Length = Length;
    Queue->Buffer = AllocatePool(Length * sizeof(VOID *));
    if (Queue->Buffer == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
    }
    else
    {
        ZeroMem(Queue->Buffer, Length * sizeof(VOID *));
    }

    return status;
}


VOID
TxQueueDestroy(
    IN  TX_QUEUE *Queue
    )
/*++

Routine Description:

    Destroys the Queue i.e. deallocates memory and zeroes all the variables.

Arguments:

    Queue - The Queue to be destroyed.

--*/
{
    if (Queue->Buffer != NULL)
    {
        FreePool(Queue->Buffer);
    }

    ZeroMem(Queue, sizeof(TX_QUEUE));
}


BOOLEAN
TxQueueIsFull(
    IN  TX_QUEUE *Queue
    )
/*++

Routine Description:

    Checks if the queue is full.

Arguments:

    Queue - The Queue to be checked.

Return Value:

    TRUE - Queue is full.

    FALSE - Queue is not full.

--*/
{
    return ((Queue->Tail + 1)%Queue->Length == (Queue->Head));
}


BOOLEAN
TxQueueIsEmpty(
    IN  TX_QUEUE *Queue
    )
/*++

Routine Description:

    Checks if the queue is empty.

Arguments:

    Queue - The Queue to be checked.

Return Value:

    TRUE - Queue is empty.

    FALSE - Queue is not empty.

--*/
{
    return (Queue->Head == Queue->Tail);
}


VOID
TxQueueEnqueue(
    IN  TX_QUEUE    *Queue,
    IN  VOID        *TxBuffer
    )
/*++

Routine Description:

    Inserts an element in the queue.

    The function DOES NOT CHECK whether the queue is full. This has to be done by the user.
    If this function is called for a full queue, it implies a coding error or a race condition.

Arguments:

    Queue    - The Queue.

    TxBuffer - The element to be inserted in the queue.

--*/
{
    ASSERT(TxQueueIsFull(Queue) == FALSE);
    ASSERT(Queue->Buffer[Queue->Tail] == NULL);

    Queue->Buffer[Queue->Tail] = TxBuffer;
    Queue->Tail = (Queue->Tail + 1)%Queue->Length;
}


VOID
TxQueueDequeue(
    IN  TX_QUEUE    *Queue,
    OUT VOID        **TxBuffer
    )
/*++

Routine Description:

    Dequeues an element from the queue.

    The function DOES NOT CHECK whether the queue is empty. This has to be done by the user.
    If this function is called for an empty queue, it implies a coding error or a race condition.

Arguments:

    Queue    - The Queue.

    TxBuffer - The pointer for outputting the dequeued element.

--*/
{
    ASSERT(TxQueueIsEmpty(Queue) == FALSE);
    ASSERT(Queue->Buffer[Queue->Head] != NULL);

    *TxBuffer = Queue->Buffer[Queue->Head];
    Queue->Buffer[Queue->Head] = NULL;
    Queue->Head = (Queue->Head + 1)%Queue->Length;
}
