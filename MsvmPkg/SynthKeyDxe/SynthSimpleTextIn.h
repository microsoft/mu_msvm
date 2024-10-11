/** @file
  Functions and Prototypes for implementing UEFI simple text input protocols
  This is a generic as possible implementation and provides an API set
  for drivers to initialize the text input layer and for lower layers (like VMBUS or PS2)
  to queue processed key presses.

  This code is dervied from:
    IntelFrameworkModulePkg\Bus\Isa\Ps2KeyboardDxe\Ps2Keyboard.h

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

//
// Public Simple Text In APIs
//
EFI_STATUS
SimpleTextInInitialize(
    IN OUT      PSYNTH_KEYBOARD_DEVICE      pDevice
    );


VOID
SimpleTextInCleanup(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice
    );


VOID
SimpleTextInQueueKey(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice,
    IN          EFI_KEY_DATA               *Key
    );


