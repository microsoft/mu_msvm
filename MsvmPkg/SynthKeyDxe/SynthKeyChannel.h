/** @file
  VMBUS Keyboard Channel implementation for EFI.  This contains the VMBUS
  specific implementation of the synthetic keyboard driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Protocol/SynthKeyProtocol.h>
#include <Vmbus/VmBusPacketFormat.h>

EFI_STATUS
SynthKeyChannelOpen(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice
    );

EFI_STATUS
SynthKeyChannelClose(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice
    );

EFI_STATUS
SynthKeyChannelSetIndicators(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice
    );

__forceinline
VOID
SynthKeyChannelInitMessage(
    IN OUT      PHK_MESSAGE_HEADER          Header,
    IN          HK_MESSAGE_TYPE             Type,
    IN          UINT32                      ByteCount
    )
/*++

Routine Description:

    A utility function to initialize a message header.

Arguments:

    Header    - Message header

    Type      - Message type

    ByteCount - Size of the message header in bytes

Return Value:

    None.

--*/
{
    ZeroMem(Header, ByteCount);
    Header->MessageType = Type;
}
