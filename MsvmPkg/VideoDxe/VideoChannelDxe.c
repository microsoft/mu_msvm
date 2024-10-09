/** @file
  VMBUS Video channel implementation for EFI.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/


#include "VideoDxe.h"


#define TPL_VIDEO_CALLBACK (TPL_CALLBACK + 1)
#define RING_OUTGOING_PAGE_COUNT 10
#define RING_INCOMING_PAGE_COUNT 10


EFI_STATUS
VideoChannelSendSituationUpdate(
    IN  PVIDEODXE_CONTEXT Context
    )
/*++

Description:

    Send a situation update message to the VSP and wait for the ACK.

Arguments:

    Context - The driver context.

Return Value:

    EFI_STATUS

--*/
{
    EFI_STATUS status = EFI_SUCCESS;
    SYNTHVID_SITUATION_UPDATE_MESSAGE message;

    //
    // Construct request message.
    //
    ZeroMem((VOID*)&message, sizeof(message));
    message.Header.Type = SynthvidSituationUpdate;
    message.Header.Size = sizeof(SYNTHVID_SITUATION_UPDATE_MESSAGE);
    message.UserContext = 0;
    message.VideoOutputCount = 1;
    message.VideoOutput[0].Active = FALSE;
    message.VideoOutput[0].PrimarySurfaceVramOffset = 0;
    message.VideoOutput[0].DepthBits = DEFAULT_SCREEN_BYTES_PER_PIXEL * BITS_PER_BYTE;
    message.VideoOutput[0].WidthPixels = DEFAULT_SCREEN_WIDTH;
    message.VideoOutput[0].HeightPixels = DEFAULT_SCREEN_HEIGHT;
    message.VideoOutput[0].PitchBytes = DEFAULT_SCREEN_BYTES_PER_PIXEL *
                                        message.VideoOutput[0].WidthPixels;

    //
    // Send message.  Response will occur asynchronously.
    //
    status = Context->Emcl->SendPacket(Context->Emcl,
                                       (VOID*)&message,
                                       sizeof(message),
                                       NULL,
                                       0,
                                       NULL,
                                       NULL);
    //
    // Nothing interesting in ACK message.
    //
    if (EFI_ERROR(status))
    {
        DEBUG ((EFI_D_ERROR,
                "VideoChannelSendSituationUpdate failed. Status %x\n",
                status));
    }

    return status;
}


VOID
VideoChannelOnSituatioUpdateAck(
    IN  VOID *Context,
    IN  VOID *Buffer,
    IN  UINT32 BufferLength
    )
/*++

Routine Description:

    This function is called when a Situation Update Ack response message is received.

    Results are communicated in the context.

Arguments:

    Context - The Context of the received message.

    Buffer - The message buffer.

    BufferLength - The size of the message buffer.

Return Value:

    None.

--*/
{
    PVIDEODXE_CONTEXT context = (PVIDEODXE_CONTEXT) Context;

    //
    // Just receiving the message means Situation Update was acknowledged.
    //
    // Initialization is complete.
    //
    context->InitStatus = EFI_SUCCESS;
    gBS->SignalEvent(context->InitCompleteEvent);
}


EFI_STATUS
VideoChannelSendVramLocation(
    IN  PVIDEODXE_CONTEXT Context
    )
/*++

Description:

    Send the VRAM location address to the VSP and wait for the ACK.

Arguments:

    Context - The driver context.

Return Value:

    EFI_STATUS

--*/
{
    EFI_STATUS status = EFI_SUCCESS;
    SYNTHVID_VRAM_LOCATION_MESSAGE message;

    //
    // Construct message.
    //
    ZeroMem((VOID*)&message, sizeof(message));
    message.Header.Type = SynthvidVramLocation;
    message.Header.Size = sizeof(SYNTHVID_VRAM_LOCATION_MESSAGE);
    message.UserContext = Context->Mode.FrameBufferSize;
    message.IsVramGpaAddressSpecified = TRUE;
    message.VramGpaAddress = Context->Mode.FrameBufferBase;

    //
    // Send message.  Response will occur asynchronously.
    //
    status = Context->Emcl->SendPacket(Context->Emcl,
                                       (VOID*)&message,
                                       sizeof(message),
                                       NULL,
                                       0,
                                       NULL,
                                       NULL);

    if (EFI_ERROR(status))
    {
        DEBUG ((EFI_D_ERROR,
                "VideoChannelSendVramLocation failed. Status %x\n",
                status));
    }

    return status;
}


VOID
VideoChannelOnVramLocationAck(
    IN  VOID *Context,
    IN  VOID *Buffer,
    IN  UINT32 BufferLength
    )
/*++

Routine Description:

    This function is called when a VRAM Location Ack response message is received.

    Results are communicated in the context.

Arguments:

    Context - The Context of the received message.

    Buffer - The message buffer.

    BufferLength - The size of the message buffer.

Return Value:

    None.

--*/
{
    EFI_STATUS status;
    PVIDEODXE_CONTEXT context = (PVIDEODXE_CONTEXT) Context;

    //
    // Just receiving the message means VRAM location was acknowledged.
    //
    // Send next init message.
    //
    status = VideoChannelSendSituationUpdate(context);

    if (EFI_ERROR(status))
    {
        //
        // Record error and end initialization.
        //
        context->InitStatus = status;

        gBS->SignalEvent(context->InitCompleteEvent);
    }
}


EFI_STATUS
VideoChannelSendVersionRequest(
    IN  PVIDEODXE_CONTEXT Context
    )
/*++

Routine Description:

    Send a version request message to the VSP.

Arguments:

    Context - The driver context.

Return Value:

    EFI_STATUS

--*/
{
    EFI_STATUS status = EFI_SUCCESS;
    SYNTHVID_VERSION_REQUEST_MESSAGE message;

    //
    // Construct message.
    //
    ZeroMem((VOID*)&message, sizeof(message));
    message.Header.Type = SynthvidVersionRequest;
    message.Header.Size = sizeof(SYNTHVID_VERSION_REQUEST_MESSAGE);
    message.Version.AsDWORD = SYNTHVID_VERSION_CURRENT;


    //
    // Send message.  Response will occur asynchronously.
    //
    status = Context->Emcl->SendPacket(Context->Emcl,
                                       (VOID*)&message,
                                       sizeof(message),
                                       NULL,
                                       0,
                                       NULL,
                                       NULL);

    if (EFI_ERROR(status))
    {
        DEBUG ((EFI_D_ERROR,
                "VideoChannelNegotiateProtocol failed. Status %x\n",
                status));
    }

    return status;
}


VOID
VideoChannelOnVersionResponse(
    IN  VOID *Context,
    IN  VOID *Buffer,
    IN  UINT32 BufferLength
    )
/*++

Routine Description:

    This function is called when a version response message is received.

    Results are communicated in the context.

Arguments:

    Context - The Context of the received message.

    Buffer - The message buffer.

    BufferLength - The size of the message buffer.

Return Value:

    None.

--*/
{
    EFI_STATUS status;
    PVIDEODXE_CONTEXT context = (PVIDEODXE_CONTEXT) Context;
    PSYNTHVID_VERSION_RESPONSE_MESSAGE response =
        (PSYNTHVID_VERSION_RESPONSE_MESSAGE)Buffer;

    ASSERT(response != NULL);

    //
    // Version was acknowledged.
    //
    if (response->IsAccepted == TRUE_WITH_VERSION_EXCHANGE)
    {
        //
        // Version was accepted so send next init message.
        //
        status = VideoChannelSendVramLocation(context);

        if (EFI_ERROR(status))
        {
            //
            // Record error and end initialization.
            //
            context->InitStatus = status;

            gBS->SignalEvent(context->InitCompleteEvent);
        }
    }
    else
    {
        DEBUG ((EFI_D_VERBOSE,
                "VideoChannelOnVersionResponse - Version %x not accepted\n",
                response->Version));

        //
        // Record error and end initialization.
        //
        context->InitStatus = EFI_PROTOCOL_ERROR;

        gBS->SignalEvent(context->InitCompleteEvent);
    }
}


VOID
VideoChannelReceivePacketCallback(
    IN  VOID *ReceiveContext,
    IN  VOID *PacketContext,
    IN  VOID *Buffer OPTIONAL,
    IN  UINT32 BufferLength,
    IN  UINT16 TransferPageSetId,
    IN  UINT32 RangeCount,
    IN  EFI_TRANSFER_RANGE *Ranges OPTIONAL
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

    TransferPageSetId - Not used in synthvid.

    RangeCount - Not used in synthvid.

    Ranges - Not used in synthvid.

Return Value:

    None.

--*/
{
    PVIDEODXE_CONTEXT context = (PVIDEODXE_CONTEXT) ReceiveContext;
    PSYNTHVID_MESSAGE_HEADER messageHeader= (PSYNTHVID_MESSAGE_HEADER)Buffer;

    if (messageHeader != NULL)
    {
        switch (messageHeader->Type)
        {
        case SynthvidError:
            DEBUG ((EFI_D_VERBOSE,
                    "VideoChannelReceivePacketCallback - SynthvidError\n"));
            break;

        case SynthvidVersionRequest:
            DEBUG ((EFI_D_VERBOSE,
                    "VideoChannelReceivePacketCallback - SynthvidVersionRequest\n"));
            break;

        case SynthvidVersionResponse:
            DEBUG ((EFI_D_VERBOSE,
                    "VideoChannelReceivePacketCallback - SynthvidVersionResponse\n"));

            VideoChannelOnVersionResponse(context, Buffer, BufferLength);

            break;

        case SynthvidVramLocation:
            DEBUG ((EFI_D_VERBOSE,
                    "VideoChannelReceivePacketCallback - SynthvidVramLocation\n"));
            break;

        case SynthvidVramLocationAck:
            DEBUG ((EFI_D_VERBOSE,
                    "VideoChannelReceivePacketCallback - SynthvidVramLocation\n"));

            VideoChannelOnVramLocationAck(context, Buffer, BufferLength);

            break;

        case SynthvidSituationUpdate:
            DEBUG ((EFI_D_VERBOSE,
                    "VideoChannelReceivePacketCallback - SynthvidSituationUpdate\n"));
            break;

        case SynthvidSituationUpdateAck:
            DEBUG ((EFI_D_VERBOSE,
                    "VideoChannelReceivePacketCallback - SynthvidSituationUpdateAck\n"));

            VideoChannelOnSituatioUpdateAck(context, Buffer, BufferLength);

            break;

        case SynthvidPointerPosition:
            DEBUG ((EFI_D_VERBOSE,
                    "VideoChannelReceivePacketCallback - SynthvidPointerPosition\n"));
            break;

        case SynthvidPointerShape:
            DEBUG ((EFI_D_VERBOSE,
                    "VideoChannelReceivePacketCallback - SynthvidPointerShape\n"));
            break;

        case SynthvidFeatureChange:
            DEBUG ((EFI_D_VERBOSE,
                    "VideoChannelReceivePacketCallback - SynthvidFeatureChange\n"));
            break;

        case SynthvidDirt:
            DEBUG ((EFI_D_VERBOSE,
                    "VideoChannelReceivePacketCallback - SynthvidDirt\n"));
            break;
        default:
            DEBUG ((EFI_D_VERBOSE,
                    "VideoChannelReceivePacketCallback - unknown message type %x\n",
                    messageHeader->Type));
            break;
        }
    }
    else
    {
        DEBUG ((EFI_D_VERBOSE,
                "VideoChannelReceivePacketCallback - buffer is null\n"));
    }

    //
    // Complete the received packet.
    //
    context->Emcl->CompletePacket(
        context->Emcl,
        PacketContext,
        Buffer,
        BufferLength
        );
}


EFI_STATUS
VideoChannelOpen (
    IN  PVIDEODXE_CONTEXT Context
    )
/*++

Routine Description:

    This function creates the client-side vmbus channel for the device. It will establish the
    callback function for the received messages and then start the channel. Finally the function
    will start the communication with the VSP.

Arguments:

    Context - The driver context.

Return Value:

    EFI_STATUS

--*/
{
    EFI_STATUS status;
    UINTN signaledEventIndex;

    //
    // Create an event that is signalled upon init completion.
    //
    status = gBS->CreateEvent(0,
                              0,
                              NULL,
                              NULL,
                              &Context->InitCompleteEvent);

    if (EFI_ERROR(status))
    {
        DEBUG ((EFI_D_ERROR,
                "VideoChannelOpen - CreateEvent failed. Status %x\n",
                status));
        goto Cleanup;
    }

    //
    // Setup to handle incoming messages from VSP.
    //
    status = Context->Emcl->SetReceiveCallback(Context->Emcl,
                                               VideoChannelReceivePacketCallback,
                                               Context,
                                               TPL_VIDEO_CALLBACK);

    if (EFI_ERROR(status))
    {
        DEBUG ((EFI_D_ERROR,
                "VideoChannelOpen - SetReceiveCallback failed. Status %x\n",
                status));
        goto Cleanup;
    }

    //
    // Start the vmbus channel.
    //
    status = Context->Emcl->StartChannel(Context->Emcl,
                                         RING_INCOMING_PAGE_COUNT,
                                         RING_OUTGOING_PAGE_COUNT);

    if (EFI_ERROR(status))
    {
        DEBUG ((EFI_D_ERROR,
                "VideoChannelOpen - StartChannel failed. Status %x\n",
                status));
        goto Cleanup;
    }

    Context->ChannelStarted = TRUE;

    //
    // Start initialization with the VSP by sending the first request message.
    //
    status = VideoChannelStartInitialize(Context);

    if (EFI_ERROR(status))
    {
        DEBUG ((EFI_D_ERROR,
                "VideoChannelOpen - VideoChannelStartInitialize failed. Status %x\n",
                status));
        goto Cleanup;
    }

    //
    // Wait for event to be signalled that indicates initialization finished.
    //
    status = gBS->WaitForEvent(1, &Context->InitCompleteEvent, &signaledEventIndex);

    if (EFI_ERROR(status))
    {
        DEBUG ((EFI_D_ERROR,
                "VideoChannelOpen - WaitForEvent failed. Status %x\n",
                status));
        goto Cleanup;
    }

    status = Context->InitStatus;

    if (EFI_ERROR(status))
    {
        DEBUG ((EFI_D_ERROR,
                "VideoChannelOpen - Initialization failed. Status %x\n",
                status));
        goto Cleanup;
    }

Cleanup:

    return status;
}


VOID
VideoChannelClose (
    IN  PVIDEODXE_CONTEXT Context
    )
/*++

Routine Description:

    This function closes a vmbus channel and releases all the allocated resources.

Arguments:

    Context - The driver context.

Return Value:

    None.

--*/
{
    if (Context != NULL)
    {
        if (Context->Emcl != NULL && Context->ChannelStarted)
        {
            Context->Emcl->StopChannel(Context->Emcl);
            Context->ChannelStarted = FALSE;
        }
        if (Context->InitCompleteEvent != NULL)
        {
            gBS->CloseEvent(Context->InitCompleteEvent);
            Context->InitCompleteEvent = NULL;
        }
    }
}


EFI_STATUS
VideoChannelStartInitialize(
    IN  PVIDEODXE_CONTEXT Context
    )
/*++

Routine Description:

    Start sequence of sending init messages to the VSP and waiting
    for responses. Receipt of expected response message triggers the send
    of the next message.

    Sequence:
        -> VersionRequest
        <- VersionResponse
        -> VramLocation
        <- VramLocationAck
        -> SituationUpdate
        <- SituationUpdateAck

Arguments:

    Context - The driver context.

Return Value:

    EFI_STATUS

--*/
{
    //
    // Simply send first message.
    //
    return VideoChannelSendVersionRequest(Context);
}

