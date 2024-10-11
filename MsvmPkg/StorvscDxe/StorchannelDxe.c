/** @file

    VMBUS storage channel implementation for EFI.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "StorvscDxe.h"

#include <Industrystandard/Scsi.h>
#include <Vmbus/NtStatus.h>

#include "StorportDxe.h"


typedef struct _STOR_CHANNEL_PROTOCOL_VERSION
{
    UINT16  ProtocolVersion;
    UINT16  MaxPacketSize;
    UINT16  MaxSrbLength;
    UINT8   MaxSrbSenseDataLength;
} STOR_CHANNEL_PROTOCOL_VERSION;

//
// Array of supported protocol versions. Ordered by preference.
//
const STOR_CHANNEL_PROTOCOL_VERSION g_StorChannelSupportedVersions[] =
{
    {
        VMSTOR_PROTOCOL_VERSION_CURRENT,
        sizeof(VSTOR_PACKET),
        sizeof(VMSCSI_REQUEST),
        VMSCSI_SENSE_BUFFER_SIZE
    }
};

#define RING_OUTGOING_PAGE_COUNT 10
#define RING_INCOMING_PAGE_COUNT 10

//
// This operation is missing from Industrystandard/Scsi.h
//
#define EFI_SCSI_OP_REPORT_LUNS 0xA0

INTERNAL_EVENT_SERVICES_PROTOCOL *mInternalEventServices = NULL;

__forceinline
BOOLEAN
StorChannelIsValidDataBuffer (
    IN  const VOID* Buffer,
    IN  UINT32 BufferLength
    )
/*++

Routine Description:

    Verifies if the specified buffer is a valid data buffer according to the UEFI specs.

Arguments:

    Buffer - Buffer to be verified.

    BufferLength - Length of the buffer.

Return Value:

    TRUE - if the buffer specified is valid.
    FALSE - if the buffer specified is invalid.

--*/
{
    if ((BufferLength > 0 && Buffer == NULL) ||
        (((UINT64)Buffer & VSTORAGE_ALIGNMENT_MASK) != 0))
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}


EFI_STATUS
StorChannelOpen (
    IN  EFI_EMCL_V2_PROTOCOL* Emcl,
    OUT PSTORVSC_CHANNEL_CONTEXT *ChannelContext
    )
/*++

Routine Description:

    This function creates the client-side vmbus channel for the device.

Arguments:

    EmclProtocol - The instance of Emcl protocol on top of which
        the channel is opened.

    ChannelContext - The context created as a result of opening the channel.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    PSTORVSC_CHANNEL_CONTEXT context = NULL;

    context = AllocateZeroPool(sizeof(*context));
    if (context == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    status = Emcl->SetReceiveCallback(
        (EFI_EMCL_PROTOCOL*)Emcl,
        StorChannelReceivePacketCallback,
        context,
        TPL_STORVSC_CALLBACK
        );

    status = Emcl->StartChannel(
        (EFI_EMCL_PROTOCOL*)Emcl,
        RING_INCOMING_PAGE_COUNT,
        RING_OUTGOING_PAGE_COUNT);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    context->Emcl = Emcl;

    //
    // Initialize the channel context.
    //
    context->MaxPacketSize = VMSTORAGE_SIZEOF_VSTOR_PACKET_REVISION_1;
    context->MaxSrbLength = VMSTORAGE_SIZEOF_VMSCSI_REQUEST_REVISION_1;
    context->MaxSrbSenseDataLength = VMSCSI_SENSE_BUFFER_SIZE_REVISION_1;

    status = StorChannelEstablishCommunications(context);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    *ChannelContext = context;

Cleanup:

    if (EFI_ERROR(status))
    {
        if (context != NULL)
        {
            StorChannelClose(context);
        }
    }

    return status;
}


VOID
StorChannelClose (
    IN  PSTORVSC_CHANNEL_CONTEXT ChannelContext
    )
/*++

Routine Description:

    This function closes a vmbus channel and releases all the allocated resources.

Arguments:

    ChannelContext - The context of the channel that is being closed.

Return Value:

    None.

--*/
{
    if (ChannelContext->Emcl != NULL)
    {
        ChannelContext->Emcl->StopChannel((EFI_EMCL_PROTOCOL*)ChannelContext->Emcl);
    }
    FreePool(ChannelContext);
}


EFI_STATUS
StorChannelInitScsiPacket (
    IN  EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET *ScsiRequest,
    IN  UINT8 *Target,
    IN  UINT64 Lun,
    OUT VSTOR_PACKET *Packet,
    OUT EFI_EXTERNAL_BUFFER *ExternalBuffer
    )
/*++

Routine Description:

    Initialize a storage channel packet from a given request for use in sending
    across VMBus.

Arguments:

    ScsiRequest - The request used to initialize the packet.

    Target - The target id of the SCSI device where the packet will be sent.

    Lun - The LUN of the SCSI device where the packet will be sent.

    Packet - Caller allocated vstor packet to be initialized.

    ExternalBuffer - Caller allocated external buffer to be sent with the vstor packet.

Return Value:

    EFI_STATUS.

--*/
{
    ZeroMem(Packet, sizeof(*Packet));
    ZeroMem(ExternalBuffer, sizeof(*ExternalBuffer));

    Packet->Operation = VStorOperationExecuteSRB;

    if (ScsiRequest->CdbLength == 0 || ScsiRequest->Cdb == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    if (!StorChannelIsValidDataBuffer(
        ScsiRequest->SenseData, ScsiRequest->SenseDataLength))
    {
        return EFI_INVALID_PARAMETER;
    }

    Packet->VmSrb.CdbLength = ScsiRequest->CdbLength;
    CopyMem(&Packet->VmSrb.Cdb , ScsiRequest->Cdb, ScsiRequest->CdbLength);

    Packet->VmSrb.Length = sizeof(VMSCSI_REQUEST);
    Packet->VmSrb.PathId = 0;
    Packet->VmSrb.TargetId = *(UINT8*)Target;
    Packet->VmSrb.Lun = (UINT8) Lun;
    Packet->VmSrb.DataIn =
        (ScsiRequest->DataDirection == EFI_EXT_SCSI_DATA_DIRECTION_READ);

    Packet->VmSrb.SenseInfoExLength = MIN(ScsiRequest->SenseDataLength,
                                          sizeof(Packet->VmSrb.SenseDataEx));

    Packet->VmSrb.TimeOutValue = (UINT32)ScsiRequest->Timeout;
    Packet->Flags |= REQUEST_COMPLETION_FLAG;

    if (ScsiRequest->DataDirection == EFI_EXT_SCSI_DATA_DIRECTION_READ)
    {
        if (!StorChannelIsValidDataBuffer(
                ScsiRequest->InDataBuffer, ScsiRequest->InTransferLength))
        {
            return EFI_INVALID_PARAMETER;
        }

        Packet->VmSrb.DataTransferLength = ScsiRequest->InTransferLength;
        ExternalBuffer->Buffer = ScsiRequest->InDataBuffer;
        ExternalBuffer->BufferSize = ScsiRequest->InTransferLength;
    }
    else if (ScsiRequest->DataDirection == EFI_EXT_SCSI_DATA_DIRECTION_WRITE)
    {
        if (!StorChannelIsValidDataBuffer(
                ScsiRequest->OutDataBuffer, ScsiRequest->OutTransferLength))
        {
            return EFI_INVALID_PARAMETER;
        }

        Packet->VmSrb.DataTransferLength = ScsiRequest->OutTransferLength;
        ExternalBuffer->Buffer = ScsiRequest->OutDataBuffer;
        ExternalBuffer->BufferSize = ScsiRequest->OutTransferLength;
    }
    else if (ScsiRequest->DataDirection ==
        EFI_EXT_SCSI_DATA_DIRECTION_BIDIRECTIONAL)
    {
        ASSERT(!"Bidirectional operations are not currently supported");
        return EFI_UNSUPPORTED;
    }
    else
    {
        ASSERT(!"Invalid Operation!");
        return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
}


VOID
StorChannelCopyPacketDataToRequest (
    IN      PVSTOR_PACKET Packet,
    IN OUT  EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET *ScsiRequest
    )
/*++

Routine Description:

    Copies the various status and related fields from the VmSrb field
    in a VSTOR_PACKET into a SCSI request. Used to get the results
    from the VSP's reply.

Arguments:

    Packet - The packet to copy from.

    ScsiRequest - The SCSI request to copy into.

Return Value:

    None.

--*/
{
    if (ScsiRequest->DataDirection == EFI_EXT_SCSI_DATA_DIRECTION_READ)
    {
        ScsiRequest->InTransferLength = Packet->VmSrb.DataTransferLength;
    }
    else
    {
        ASSERT(ScsiRequest->DataDirection == EFI_EXT_SCSI_DATA_DIRECTION_WRITE);
        ScsiRequest->OutTransferLength = Packet->VmSrb.DataTransferLength;
    }

    ScsiRequest->TargetStatus = Packet->VmSrb.ScsiStatus;

    //
    // SRB_STATUS_SUCCESS and SRB_STATUS_PENDING both translate to
    // EFI_EXT_SCSI_STATUS_HOST_ADAPTER_OK. The rest of the SRB statuses
    // have one to one mapping with efi host adapter statuses.
    //
    if (Packet->VmSrb.SrbStatus == SRB_STATUS_SUCCESS ||
        Packet->VmSrb.SrbStatus == SRB_STATUS_PENDING)
    {
        ScsiRequest->HostAdapterStatus = EFI_EXT_SCSI_STATUS_HOST_ADAPTER_OK;
    }
    else
    {
        ScsiRequest->HostAdapterStatus = Packet->VmSrb.SrbStatus;
    }

    if (ScsiRequest->TargetStatus == EFI_EXT_SCSI_STATUS_TARGET_CHECK_CONDITION)
    {
        if (IsDescriptorSenseDataFormat(Packet->VmSrb.SenseDataEx))
        {
            ScsiConvertToFixedSenseFormat(
                Packet->VmSrb.SenseDataEx,
                Packet->VmSrb.SenseInfoExLength,
                ScsiRequest->SenseData,
                ScsiRequest->SenseDataLength);

            ScsiRequest->SenseDataLength = sizeof(EFI_SCSI_SENSE_DATA);
        }
        else if (ScsiRequest->SenseDataLength >= Packet->VmSrb.SenseInfoExLength)
        {
            CopyMem(
                ScsiRequest->SenseData,
                &(Packet->VmSrb.SenseDataEx[0]),
                Packet->VmSrb.SenseInfoExLength);
            ScsiRequest->SenseDataLength = Packet->VmSrb.SenseInfoExLength;
        }
    }
}


VOID
StorChannelCompletionRoutine (
    IN  VOID *Context OPTIONAL,
    IN  VOID *Buffer,
    IN  UINT32 BufferLength
    )
/*++
Routine Description:

    This is the routine called when a SCSI request has been completed.

    This routine receives/processes a message from the host and therefore
    must validate this information before using it.

Arguments:

    Context - Pointer to the channel request that was sent.

    Buffer - The completion packet received for the request.

    BufferLength - The length of the received packet.

Return Value:

    None.

--*/
{
    PSTORVSC_CHANNEL_REQUEST request;
    PVSTOR_PACKET packet;

    request = Context;
    packet = Buffer;

    //
    // Validate the response received from the host before proceeding.
    //
    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(BufferLength >= VMSTORAGE_SIZEOF_VSTOR_PACKET_REVISION_1);
    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(packet->Operation == VStorOperationCompleteIo);
    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(packet->VmSrb.SenseInfoExLength <= VMSCSI_SENSE_BUFFER_SIZE);

    switch (request->ScsiRequest->DataDirection)
    {
    case EFI_EXT_SCSI_DATA_DIRECTION_READ:
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(packet->VmSrb.DataTransferLength <= request->ScsiRequest->InTransferLength);
        break;

    case EFI_EXT_SCSI_DATA_DIRECTION_WRITE:
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(packet->VmSrb.DataTransferLength <= request->ScsiRequest->OutTransferLength)
        break;

    case EFI_EXT_SCSI_DATA_DIRECTION_BIDIRECTIONAL:
        // Bidirectional data transfer is not supported.
    default:
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();

    }

    //
    // Copy completion packet data to SRB
    //
    StorChannelCopyPacketDataToRequest(packet, request->ScsiRequest);

    if (request->Event != NULL)
    {
        gBS->SignalEvent(request->Event);
    }

    FreePool(request);
}


EFI_STATUS
StorChannelSendScsiRequest (
    IN      PSTORVSC_CHANNEL_CONTEXT ChannelContext,
    IN OUT  EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET *ScsiRequest,
    IN      UINT8 *Target,
    IN      UINT64 Lun,
    IN      EFI_EVENT Event OPTIONAL
    )
/*++

Routine Description:

    This routine sends an SCSI request.

Arguments:

    ChannelContext - The storage channel context.

    ScsiRequest - The request to send.

    Target - The target id of the SCSI device where the packet will be sent.

    Lun - The LUN of the SCSI device where the packet will be sent.

    Event - The event to be signaled when the request is completed.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    VSTOR_PACKET packet;
    EFI_EXTERNAL_BUFFER externalBuffer;
    PSTORVSC_CHANNEL_REQUEST request = NULL;
    UINT32 packetSize;
    EFI_EXTERNAL_BUFFER* buffers;
    UINT32 buffersCount;
    UINT32 sendFlags = 0;

    ASSERT(*Target <= VMSTOR_MAX_TARGETS);

    status= StorChannelInitScsiPacket(
        ScsiRequest,
        Target,
        Lun,
        &packet,
        &externalBuffer);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    if (externalBuffer.BufferSize > ChannelContext->Properties.MaxTransferBytes)
    {
        if (ScsiRequest->DataDirection == EFI_EXT_SCSI_DATA_DIRECTION_READ)
        {
            ScsiRequest->InTransferLength =
                ChannelContext->Properties.MaxTransferBytes;
        }
        else
        {
            ASSERT(ScsiRequest->DataDirection == EFI_EXT_SCSI_DATA_DIRECTION_WRITE);
            ScsiRequest->OutTransferLength =
                ChannelContext->Properties.MaxTransferBytes;
        }

        status = EFI_BAD_BUFFER_SIZE;
        goto Cleanup;
    }

    request = AllocateZeroPool(sizeof(*request));

    if (request == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    request->Event = Event;
    request->ScsiRequest = ScsiRequest;

    packetSize = ChannelContext->MaxPacketSize;

    if (packet.VmSrb.Length > ChannelContext->MaxSrbLength)
    {
        packet.VmSrb.Length = ChannelContext->MaxSrbLength;
    }

    if (packet.VmSrb.SenseInfoExLength > ChannelContext->MaxSrbSenseDataLength)
    {
        packet.VmSrb.SenseInfoExLength = ChannelContext->MaxSrbSenseDataLength;
    }

    if (externalBuffer.BufferSize > 0)
    {
        buffers = &externalBuffer;
        buffersCount = 1;

        if (ScsiRequest->DataDirection == EFI_EXT_SCSI_DATA_DIRECTION_READ)
        {
            sendFlags = EMCL_SEND_FLAG_DATA_IN_ONLY;
        }
        else
        {
            ASSERT(ScsiRequest->DataDirection == EFI_EXT_SCSI_DATA_DIRECTION_WRITE);
            sendFlags = EMCL_SEND_FLAG_DATA_OUT_ONLY;
        }
    }
    else
    {
        buffers = NULL;
        buffersCount = 0;
    }

    status = ChannelContext->Emcl->SendPacketEx(
        (EFI_EMCL_PROTOCOL*)ChannelContext->Emcl,
        &packet,
        packetSize,
        buffers,
        buffersCount,
        sendFlags,
        StorChannelCompletionRoutine,
        request
        );

Cleanup:
    if (EFI_ERROR(status))
    {
        if (request != NULL)
        {
            FreePool(request);
        }
    }

    return status;
}


EFI_STATUS
StorChannelSendScsiRequestSync (
    IN      PSTORVSC_CHANNEL_CONTEXT ChannelContext,
    IN OUT  EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET *ScsiRequest,
    IN      UINT8 *Target,
    IN      UINT64 Lun
    )
/*++

Routine Description:

    This routine sends a synchronous SCSI request. It returns when
    the request has completed.

Arguments:

    ChannelContext - The storage channel context.

    ScsiRequest - The request to send.

    Target - The target id of the SCSI device where the packet will be sent.

    Lun - The LUN of the SCSI device where the packet will be sent.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_EVENT event = NULL;
    UINTN signaledEventIndex;

    ASSERT(*Target <= VMSTOR_MAX_TARGETS);

    if (mInternalEventServices == NULL)
    {
        status = gBS->LocateProtocol(
                        &gInternalEventServicesProtocolGuid,
                        NULL,
                        (VOID **)&mInternalEventServices);
        ASSERT_EFI_ERROR(status);
    }

    status = gBS->CreateEvent(
        0,
        0,
        NULL,
        NULL,
        &event);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = StorChannelSendScsiRequest(
        ChannelContext,
        ScsiRequest,
        Target,
        Lun,
        event);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // This can be called from TPL_CALLBACK. Use WaitForEventInternal instead of gBS->WaitForEvent
    // which enforces a TPL check for TPL_APPLICATION.
    //
    status = mInternalEventServices->WaitForEventInternal(1, &event, &signaledEventIndex);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

Cleanup:
    if (event != NULL)
    {
        gBS->CloseEvent(event);
    }

    return status;
}


VOID
StorChannelReceivePacketCallback (
    IN  VOID *ReceiveContext,
    IN  VOID *PacketContext,
    IN  VOID *Buffer,
    IN  UINT32 BufferLength,
    IN  UINT16 TransferPageSetId,
    IN  UINT32 RangeCount,
    IN  EFI_TRANSFER_RANGE *Ranges
    )
/*++

Routine Description:

    This function is called when a packet from EMCL is received.

Arguments:

    ReceiveContext - The ChannelContext of the received packet.

    PacketContext - Caller allocated context to be sent to
        the completion function.

    Buffer - The completion buffer for the packet.

    BufferLength - The size of the completion buffer.

    TransferPageSetId - Not used in storvsc.

    RangeCount - Not used in storvsc.

    Ranges - Not used in storvsc.

Return Value:

    None.

--*/
{
    PSTORVSC_CHANNEL_CONTEXT context;

    context = ReceiveContext;

    //
    // Nothing to do here. Just complete the packet.
    //
    context->Emcl->CompletePacket(
        (EFI_EMCL_PROTOCOL*)context->Emcl,
        PacketContext,
        Buffer,
        BufferLength
        );
}


VOID
StorChannelInitSyntheticVstorPacket (
    OUT PVSTOR_PACKET Packet
    )
/*++

Routine Description:

    Initialize a synthetic VSTOR_PACKET.

Arguments:

    Packet - The synthetic VSTOR_PACKET to be initialized.

Return Value:

    None.

--*/
{
    ZeroMem(Packet, sizeof(*Packet));
    Packet->Flags |= REQUEST_COMPLETION_FLAG;
}


EFI_STATUS
StorChannelSendSyntheticVstorPacket (
    IN      PSTORVSC_CHANNEL_CONTEXT ChannelContext,
    IN OUT  PVSTOR_PACKET Packet
    )
/*++

Routine Description:

    This function sends a synthetic VSTOR_PACKET and returns when it receives
    the completion packet from the VSP side.

Arguments:

    ChannelContext - The context of the storage VMBUS channel.

    Packet - The synthetic VSTOR_PACKET that is to be sent synchronously and
        also receives the data from the VSP side.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    UINT32 packetSize;

    packetSize = ChannelContext->MaxPacketSize;

    status = EmclSendPacketSync(
        (EFI_EMCL_PROTOCOL*)ChannelContext->Emcl,
        Packet,
        packetSize,
        NULL,
        0);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    if (packetSize != ChannelContext->MaxPacketSize)
    {
        status = EFI_ABORTED;
        goto Cleanup;
    }

    ASSERT(Packet->Operation == VStorOperationCompleteIo);

    if (NT_SUCCESS(Packet->Status))
    {
        status = EFI_SUCCESS;
    }
    else
    {
        status = EFI_DEVICE_ERROR;
    }

Cleanup:

    return status;
}


EFI_STATUS
StorChannelEstablishCommunications (
    IN OUT  PSTORVSC_CHANNEL_CONTEXT ChannelContext
    )
/*++

Routine Description:

    Negotiate the version and channel properties with the storage VSP.

Arguments:

    ChannelContext - The context of the storage VMBus channel.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    VSTOR_PACKET packet;
    UINT32 index;
    PVMSTORAGE_CHANNEL_PROPERTIES channelProperties;

    channelProperties = &ChannelContext->Properties;

    StorChannelInitSyntheticVstorPacket(&packet);
    packet.Operation = VStorOperationBeginInitialization;
    status = StorChannelSendSyntheticVstorPacket(ChannelContext, &packet);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Loop through the available versions until one is accepted by the VSP.
    //
    for (index = 0;
        index < sizeof(g_StorChannelSupportedVersions) / sizeof(g_StorChannelSupportedVersions[0]);
        index++)
    {
        UINT16 majorMinor = g_StorChannelSupportedVersions[index].ProtocolVersion;

        StorChannelInitSyntheticVstorPacket(&packet);
        packet.Operation = VStorOperationQueryProtocolVersion;

        packet.Version.MajorMinor = majorMinor;
        status = StorChannelSendSyntheticVstorPacket(ChannelContext, &packet);
        if (!EFI_ERROR(status))
        {
            ASSERT(packet.Version.MajorMinor == majorMinor);
            ChannelContext->ProtocolVersion = packet.Version.MajorMinor;
            ChannelContext->MaxPacketSize =
                g_StorChannelSupportedVersions[index].MaxPacketSize;
            ChannelContext->MaxSrbLength =
                g_StorChannelSupportedVersions[index].MaxSrbLength;
            ChannelContext->MaxSrbSenseDataLength =
                g_StorChannelSupportedVersions[index].MaxSrbSenseDataLength;

            break;
        }
        else if (packet.Status != STATUS_REVISION_MISMATCH)
        {
            goto Cleanup;
        }
    }

    if (packet.Status == STATUS_REVISION_MISMATCH)
    {
        goto Cleanup;
    }

    //
    //  Send a packet to query channel property information.
    //
    StorChannelInitSyntheticVstorPacket(&packet);
    packet.Operation = VStorOperationQueryProperties;
    status = StorChannelSendSyntheticVstorPacket(ChannelContext, &packet);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    //  Copy all the properties into the storagechannel structure.
    //
    ASSERT(packet.StorageChannelProperties.MaxTransferBytes > 0);

    channelProperties->MaxTransferBytes = packet.StorageChannelProperties.MaxTransferBytes;

    StorChannelInitSyntheticVstorPacket(&packet);
    packet.Operation = VStorOperationEndInitialization;
    status = StorChannelSendSyntheticVstorPacket(ChannelContext, &packet);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = EFI_SUCCESS;

Cleanup:
    return status;
}


EFI_STATUS
StorChannelInitReportLunsRequest (
    IN OUT  EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET *Request
    )
/*++

Routine Description:

    Initializes a SCSI request of type EFI_SCSI_OP_REPORT_LUNS.

Arguments:

    Request - The preallocated request to be initialized.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;

    ZeroMem(Request, sizeof(*Request));

    Request->CdbLength = CDB12GENERIC_LENGTH;
    Request->Cdb = AllocateZeroPool(Request->CdbLength);
    if (Request->Cdb == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    ((UINT8*)Request->Cdb)[0] = EFI_SCSI_OP_REPORT_LUNS;
    Request->DataDirection = EFI_EXT_SCSI_DATA_DIRECTION_READ;

    Request->InTransferLength = OFFSET_OF(LUN_LIST, Lun) + STORVSC_MAX_LUN_TRANSFER_LENGTH;
    Request->InDataBuffer = AllocateZeroPool(Request->InTransferLength);
    if (Request->InDataBuffer == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    Request->SenseDataLength = 0;
    Request->SenseData = NULL;
    Request->Timeout = 0;

    status = EFI_SUCCESS;

Cleanup:
    if (EFI_ERROR(status))
    {
        StorChannelTeardownReportLunsRequest(Request);
    }

    return status;
}


VOID
StorChannelTeardownReportLunsRequest (
    OUT EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET *Request
    )
/*++

Routine Description:

    Tears down a SCSI request of type SCSIOP_REPORT_LUNS.

Arguments:

    Request - The request to tear down.

Return Value:

    None.

--*/
{
    if (Request->Cdb != NULL)
    {
        FreePool(Request->Cdb);
        Request->Cdb = NULL;
    }

    if (Request->InDataBuffer != NULL)
    {
        FreePool(Request->InDataBuffer);
        Request->InDataBuffer = NULL;
    }
}


EFI_STATUS
StorChannelParseReportLunsResponse (
    IN OUT  EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET *Request,
    OUT     LIST_ENTRY *LunList,
    IN      UINT8 Target
    )
/*++

Routine Description:

    Parses the response from a request of type SCSIOP_REPORT_LUNS.
    Inserts the reported devices into the list. If the function fails, it
    will not clean up the inserted entries.

    This routine receives/processes a message from the host and therefore
    must validate this information before using it.

Arguments:

    Request - The request to be parsed.

    LunList - A list where to add the found devices.

    Target - The target for which the LUNs are parsed.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    PLUN_LIST rawList;
    UINT32 index;
    UINT32 rawListLength;
    UINT16 lun;
    PTARGET_LUN targetLunEntry;

    if (Request->HostAdapterStatus != EFI_EXT_SCSI_STATUS_HOST_ADAPTER_OK ||
        Request->TargetStatus != EFI_EXT_SCSI_STATUS_TARGET_GOOD)
    {
        status = EFI_INVALID_PARAMETER;
        goto Cleanup;
    }

    rawList = Request->InDataBuffer;

    rawListLength =
        rawList->LunListLength[0] << 24 |
        rawList->LunListLength[1] << 16 |
        rawList->LunListLength[2] <<  8 |
        rawList->LunListLength[3] <<  0;

    //
    // The following size was used to allocate the Request->InDataBuffer when a
    // request was sent to the host.
    //
    if (rawListLength > STORVSC_MAX_LUN_TRANSFER_LENGTH)
    {
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    for (index = 0; index < rawListLength / sizeof(rawList->Lun[0]); index++)
    {
        lun = rawList->Lun[index][0] << 8 |
              rawList->Lun[index][1] << 0 ;

        targetLunEntry = AllocatePool(sizeof(*targetLunEntry));
        if (targetLunEntry == NULL)
        {
            status = EFI_OUT_OF_RESOURCES;
            goto Cleanup;
        }

        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(lun < SCSI_MAXIMUM_LUNS_PER_TARGET);

        targetLunEntry->Lun = (UINT8)lun;
        targetLunEntry->TargetId = Target;
        InsertTailList(LunList, &targetLunEntry->ListEntry);
    }

    status = EFI_SUCCESS;

Cleanup:
    return status;
}


EFI_STATUS
StorChannelBuildLunList(
    IN  PSTORVSC_CHANNEL_CONTEXT ChannelContext,
    OUT LIST_ENTRY *LunList
    )
/*++

Routine Description:

    Builds the list of the devices present on the adapter controller.

Arguments:

    ChannelContext - The channel context of the adapter controller.

    LunList - A list of the found devices. If the function fails, the list
        will be empty.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET requestTemplate;
    EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET request;
    UINT8 target;
    UINT8 adapterLun = 0;

    InitializeListHead(LunList);

    status = StorChannelInitReportLunsRequest(&requestTemplate);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    for (target = 0; target <= VMSTOR_MAX_TARGETS; target++)
    {
        CopyMem(&request, &requestTemplate, sizeof(request));
        status = StorChannelSendScsiRequestSync(
            ChannelContext,
            &request,
            &target,
            adapterLun);

        if (EFI_ERROR(status))
        {
            goto Cleanup;
        }

        status = StorChannelParseReportLunsResponse(&request, LunList, target);

        if (EFI_ERROR(status))
        {
            goto Cleanup;
        }
    }

Cleanup:
    if (EFI_ERROR(status))
    {
        StorChannelFreeLunList(LunList);
    }

    StorChannelTeardownReportLunsRequest(&requestTemplate);

    return status;
}


VOID
StorChannelFreeLunList (
    IN OUT  LIST_ENTRY *LunList
    )
/*++

Routine Description:

    Frees a list of TARGET_LUN items.

Arguments:

    LunList - A list of TARGET_LUN items to be freed.

--*/
{
    PTARGET_LUN targetLun;

    while (!IsListEmpty(LunList))
    {
        targetLun = BASE_CR(LunList->ForwardLink, TARGET_LUN, ListEntry);
        RemoveEntryList(LunList->ForwardLink);
        FreePool(targetLun);
    }
}


LIST_ENTRY*
StorChannelSearchLunList (
    IN  LIST_ENTRY *LunList,
    IN  UINT8 Target,
    IN  UINT8 Lun
    )
/*++

Routine Description:

    Searches for a device in a list.

Arguments:

    LunList - A list of devices.

    Target - The target id of the searched device.

    Lun - The LUN of the searched device.

Return Value:

    EFI_STATUS.

--*/
{
    PTARGET_LUN entry;
    LIST_ENTRY *listEntry;

    for (listEntry = LunList->ForwardLink;
         listEntry != LunList;
         listEntry = listEntry->ForwardLink)
    {
        entry = BASE_CR(listEntry , TARGET_LUN, ListEntry);
        if (entry->TargetId == Target && entry->Lun == Lun)
        {
            break;
        }
    }

    if (listEntry != LunList)
    {
        return listEntry;
    }

    return NULL;
}

