/** @file
  VMBUS Keyboard Channel implementation for UEFI.  This contains the VMBUS
  specific layer of the synthetic keyboard driver. It will manage the VMBUS
  channel and process keystroke messages, translate and then queue them.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SynthKeyDxe.h"
#include "SynthKeyChannel.h"
#include "SynthSimpleTextIn.h"
#include "SynthKeyLayout.h"


#define RING_OUTGOING_PAGE_COUNT 1
#define RING_INCOMING_PAGE_COUNT 1


VOID
SynthKeyChannelReceivePacketCallback(
    IN          VOID                       *ReceiveContext,
    IN          VOID                       *PacketContext,
    IN OPTIONAL VOID                       *Buffer,
    IN          UINT32                      BufferLength,
    IN          UINT16                      TransferPageSetId,
    IN          UINT32                      RangeCount,
    IN          EFI_TRANSFER_RANGE         *Ranges
    );

EFI_STATUS
SynthKeyChannelEstablishCommunications(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice
    );

EFI_STATUS
OnProtocolResponse(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice,
    IN          PHK_MESSAGE_HEADER          Message
    );

EFI_STATUS
OnMessageEvent(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice,
    IN          PHK_MESSAGE_HEADER          Message
    );


EFI_STATUS
SynthKeyChannelOpen(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice
    )
/*++

Routine Description:

    This function creates the client-side vmbus channel for the device.

Arguments:

    pDevice - The SynthKey instance for which to open the channel.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;

    ASSERT(pDevice->Emcl);
    ASSERT(pDevice->State.ChannelOpen == FALSE);

    //
    // Default to an error/not connected state.
    // This will be changed once the protocol version has been
    // ack'd by the vdev.
    //
    pDevice->State.ChannelConnected = FALSE;

    status = pDevice->Emcl->SetReceiveCallback(pDevice->Emcl,
                                               SynthKeyChannelReceivePacketCallback,
                                               pDevice,
                                               TPL_KEYBOARD_CALLBACK);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to set channel callback - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    status = pDevice->Emcl->StartChannel(pDevice->Emcl,
                                         RING_INCOMING_PAGE_COUNT,
                                         RING_OUTGOING_PAGE_COUNT);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to start channel - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    pDevice->State.ChannelOpen = TRUE;

    status = SynthKeyChannelEstablishCommunications(pDevice);
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to establish communication - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

Cleanup:

    if (EFI_ERROR(status))
    {
        SynthKeyChannelClose(pDevice);
    }

    return status;
}


EFI_STATUS
SynthKeyChannelClose(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice
    )
/*++

Routine Description:

    Closes the vmbus channel created with SynthKeyChannelOpen.

Arguments:

    pDevice - The device to close the channel on.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status = EFI_SUCCESS;

    ASSERT(pDevice->Emcl);

    if (pDevice->State.ChannelOpen)
    {
        pDevice->Emcl->StopChannel(pDevice->Emcl);
        pDevice->State.ChannelOpen = FALSE;
        pDevice->State.ChannelConnected = FALSE;
    }

    if (pDevice->InitCompleteEvent != NULL)
    {
        gBS->CloseEvent(pDevice->InitCompleteEvent);
        pDevice->InitCompleteEvent = NULL;
    }

    return status;
}


EFI_STATUS
SynthKeyChannelSendMessage(
    IN   PSYNTH_KEYBOARD_DEVICE     pDevice,
    IN   PHK_MESSAGE_HEADER         Message,
    IN   UINT32                     MessageSize
    )
/*++

Routine Description:

    Sends a keyboard message.

Arguments:

    pDevice - Device to send the message with.

    Message - Keyboard message to send.

    MessageSize - Message size.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;

    ASSERT(pDevice->State.ChannelOpen);

    //
    // The synthetic keyboard uses vmbus pipes in raw mode so there is no
    // header to prepend. Just send the callers buffer directly.
    //
    status = pDevice->Emcl->SendPacket(pDevice->Emcl,
                                       Message,
                                       MessageSize,
                                       NULL,   // No external buffer
                                       0,
                                       NULL,   // No completion routine or context
                                       NULL);

    if (EFI_ERROR(status))
    {
        DEBUG ((EFI_D_WARN, "--- %a: failed to send the message (type %d, size %d) --%r \n",
            __FUNCTION__, Message->MessageType, MessageSize, status));
    }

    return status;
}


VOID
SynthKeyChannelReceivePacketCallback(
    IN          VOID                       *ReceiveContext,
    IN          VOID                       *PacketContext,
    IN OPTIONAL VOID                       *Buffer,
    IN          UINT32                      BufferLength,
    IN          UINT16                      TransferPageSetId,
    IN          UINT32                      RangeCount,
    IN          EFI_TRANSFER_RANGE         *Ranges
    )
/*++

Routine Description:

    This function is called when a packet from EMCL is received. It will call the
    appropriate handler based on the message type.

Arguments:

    ReceiveContext      - The device associated with the received packet,
                          type PSYNTH_KEYBOARD_DEVICE

    PacketContext       - Caller allocated context to be sent to the completion function.

    Buffer              - The completion buffer for the packet. This will contain a HK_MESSAGE_xxxx

    BufferLength        - The size of the completion buffer.

    TransferPageSetId   - Not used in SynthKeyDxe.

    RangeCount          - Not used in SynthKeyDxe.

    Ranges              - Not used in SynthKeyDxe.

Return Value:

    None.

--*/
{
    PSYNTH_KEYBOARD_DEVICE  pDevice = (PSYNTH_KEYBOARD_DEVICE)ReceiveContext;
    PHK_MESSAGE_HEADER      message = (PHK_MESSAGE_HEADER)Buffer;

    ASSERT(BufferLength > sizeof(HK_MESSAGE_HEADER));

    switch (message->MessageType)
    {
    case HkMessageProtocolResponse:
        ASSERT(BufferLength >= sizeof(HK_MESSAGE_PROTOCOL_RESPONSE));
        OnProtocolResponse(pDevice, message);
        break;

    case HkMessageEvent:
        ASSERT(BufferLength >= sizeof(HK_MESSAGE_KEYSTROKE));
        OnMessageEvent(pDevice, message);
        break;

    case HkMessageSetLedIndicators:
        // We should never get this as it is only for VM -> Host communication.
        ASSERT(FALSE);
        break;

    default:
        DEBUG ((EFI_D_WARN, "--- %a: unknown message type (type %d, size %d) \n",
            __FUNCTION__, message->MessageType, BufferLength));
        ASSERT(FALSE);
        break;
    }

    pDevice->Emcl->CompletePacket(pDevice->Emcl,
                                  PacketContext,
                                  Buffer,
                                  BufferLength);
}


EFI_STATUS
SynthKeyChannelEstablishCommunications(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice
    )
/*++

Routine Description:

    Negotiate the version and channel properties with the keyboard vsp.
    Note that the keyboard is not fully functional until the vdev responds
    and agrees on the protocol version.

Arguments:

    pDevice - Device to setup communications with.

Return Value:

    EFI_STATUS.

--*/
{
    HK_MESSAGE_PROTOCOL_REQUEST request;
    UINTN signaledEventIndex;
    EFI_STATUS status;

    SynthKeyChannelInitMessage((PHK_MESSAGE_HEADER)&request, HkMessageProtocolRequest, sizeof(request));

    request.Version = HK_VERSION_WIN8;

    DEBUG ((EFI_D_VERBOSE, "--- %a protocol version requested 0x%x\n", __FUNCTION__, request.Version));

    //
    // Create an event to wait for the negotiation to complete
    //
    status = gBS->CreateEvent(0, 0, NULL, NULL, &pDevice->InitCompleteEvent);

    if (EFI_ERROR(status))
    {
        DEBUG ((EFI_D_ERROR,
            "--- %a:failed to create event - %r \n", __FUNCTION__, status));
        goto Exit;
    }

    status = SynthKeyChannelSendMessage(pDevice, (PHK_MESSAGE_HEADER)&request, sizeof(request));

    if (EFI_ERROR(status))
    {
        DEBUG ((EFI_D_ERROR,
            "--- %a: failed to send message - %r \n", __FUNCTION__, status));
        goto Exit;
    }

    status = gBS->WaitForEvent(1, &pDevice->InitCompleteEvent, &signaledEventIndex);

    if (EFI_ERROR(status))
    {
        DEBUG ((EFI_D_ERROR,
            "--- %a: failed to wait For event - %r \n", __FUNCTION__, status));
        goto Exit;
    }

    if (!pDevice->State.ChannelConnected)
    {
        status = EFI_NOT_READY;
        DEBUG ((EFI_D_ERROR,
            "--- %a: failed to connect the channel - %r \n", __FUNCTION__, status));
    }

Exit:

    return status;
}


EFI_STATUS
SynthKeyChannelSetIndicators(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice
    )
/*++

Routine Description:

    Informs the vdev of the LED state of the device.

Arguments:

    pDevice - The keyboard device

Return Value:

    EFI_STATUS.

--*/
{
    HK_MESSAGE_LED_INDICATORS_STATE indicatorsState;
    EFI_STATUS status;

    ASSERT(pDevice->State.ChannelOpen);

    SynthKeyChannelInitMessage((PHK_MESSAGE_HEADER)&indicatorsState, HkMessageSetLedIndicators, sizeof(indicatorsState));

    //
    // Luckily the EFI lock state flags match what the keyboard vdev wants
    // Mask off unused flags and send the rest directly.
    // For reference the host flags are defined in Ntddkbd.h
    //   KEYBOARD_NUM_LOCK_ON, KEYBOARD_SCROLL_LOCK_ON, KEYBOARD_CAPS_LOCK_ON
    //
    indicatorsState.LedFlags = (pDevice->State.KeyState.KeyToggleState &
                                (EFI_SCROLL_LOCK_ACTIVE | EFI_NUM_LOCK_ACTIVE | EFI_CAPS_LOCK_ACTIVE));

    DEBUG((EFI_D_VERBOSE, "--- %a: set indicators state: 0x%02x\n", __FUNCTION__, indicatorsState.LedFlags));

    status = SynthKeyChannelSendMessage(pDevice, (PHK_MESSAGE_HEADER)&indicatorsState, sizeof(indicatorsState));

    return status;
}


EFI_STATUS
OnProtocolResponse(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice,
    IN          PHK_MESSAGE_HEADER          Message
    )
/*++

Description:

    Message handler for the HkMessageProtocolResponse message.

Arguments:

    pDevice - SynthKey channel context.

    Message - The protocol version response message.

Return Value:

    EFI_STATUS.

--*/
{
    PHK_MESSAGE_PROTOCOL_RESPONSE pResponse;
    EFI_STATUS status = EFI_SUCCESS;

    ASSERT(Message->MessageType == HkMessageProtocolResponse);

    pResponse = (PHK_MESSAGE_PROTOCOL_RESPONSE)Message;

    if (pResponse->Accepted)
    {
        DEBUG((EFI_D_VERBOSE, "SynthKey:OnProtocolResponse - Protocol Version Accepted\n"));
        pDevice->State.ChannelConnected = TRUE;
        SynthKeyReportStatus(pDevice, EFI_PROGRESS_CODE, EFI_P_PC_DETECTED);
    }
    else
    {
        DEBUG((EFI_D_ERROR, "SynthKey:OnProtocolResponse - Protocol Version NOT Accepted\n"));
        SynthKeyReportStatus(pDevice, EFI_ERROR_CODE, EFI_P_EC_CONTROLLER_ERROR);
    }

    gBS->SignalEvent(pDevice->InitCompleteEvent);

    return status;
}


EFI_STATUS
OnMessageEvent(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice,
    IN          PHK_MESSAGE_HEADER          Message
    )
/*++

Description:

    Message handler for the HkMessageEvent message.
    This handles key press/release events, translating and queuing the
    key.

Arguments:

    pDevice - Keyboard device that received the event.

    Message - The protocol version response message.

Return Value:

    EFI_STATUS.

--*/
{
    PHK_MESSAGE_KEYSTROKE   pKeyMessage = (PHK_MESSAGE_KEYSTROKE)Message;
    EFI_KEY_DATA            keyData;
    SynthKeyStateChangeType changeType;

    EFI_STATUS  status   = EFI_SUCCESS;
    BOOLEAN     queueKey = FALSE;

    ASSERT(pKeyMessage->Header.MessageType == HkMessageEvent);

    ZeroMem(&keyData, sizeof(keyData));

    //
    // Translate the message to an EFI_KEY
    //   1st update the key state - if this is updated no further processing is needed.
    //   2nd translate the key to an EFI key
    //   3rd Queue the key if needed.
    //
    changeType = SynthKeyLayoutUpdateKeyState(pKeyMessage, &pDevice->State);

    if (changeType != KeyChangeNone)
    {
        //
        // Only toggle key state changes need to propagate to the vdev.
        //
        if (changeType == KeyChangeToggle)
        {
            SynthKeyChannelSetIndicators(pDevice);
        }

        if (pDevice->State.KeyState.KeyToggleState & EFI_KEY_STATE_EXPOSED)
        {
            //
            // UEFI is pretty vague as to what EFI_KEY_STATE_EXPOSED should actually return
            // Should it be the actual scan code or just the State.KeyState with a NULL scan code?
            // For now we'll follow the behavior of other UEFI drivers (PS2 & USB) which is to
            // only return shift and toggle key states with no scan code.
            // The windows boot environment also expects this.
            //
            keyData.KeyState = pDevice->State.KeyState;
            queueKey = TRUE;
        }

    }
    else
    {
        //
        // A non-shift/toggle, process it as a normal keypress.
        // NB:
        //  Duplicate shift state changes (which are ignored) will get here,
        //  but be dropped very quickly since SynthKeyLayoutTranslateKey doesn't handle
        //  shift or toggle keys.
        //  See comment in SynthKeyLayoutUpdateKeyState in newShiftState procesing
        //  for complete context.
        //
        // TODO: Pass a current layout pointer
        status = SynthKeyLayoutTranslateKey(pKeyMessage, &pDevice->State, &keyData);

        if (!EFI_ERROR(status))
        {
            queueKey = TRUE;
        }
    }

    if (queueKey)
    {
        SimpleTextInQueueKey(pDevice, &keyData);
    }

    return status;
}
