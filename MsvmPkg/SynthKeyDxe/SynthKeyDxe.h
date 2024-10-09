/** @file
  EFI driver for Hyper-V synthetic keyboard.

  This code is derived from:
    IntelFrameworkModulePkg\Bus\Isa\Ps2KeyboardDxe\Ps2Keyboard.h

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Protocol/Vmbus.h>
#include <Protocol/Emcl.h>
#include <Protocol/DevicePath.h>

#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextInEx.h>

#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PcdLib.h>

#include <Library/EmclLib.h>


//
// Global Variables
//
extern EFI_DRIVER_BINDING_PROTOCOL  gSynthKeyDriverBinding;
extern EFI_COMPONENT_NAME_PROTOCOL  gSynthKeyComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL gSynthKeyComponentName2;

//
// Keyboard version used for the purpose of UEFI driver ranking.
// Higher values will be preferred.
// 0x10 is the start of the IHV reserved range
//
#define SYNTH_KEYBOARD_VERSION      0x10


#define SYNTHKEY_KEY_BUFFER_SIZE    256

//
// Simple ring buffer for queuing EFI_KEY_DATA
// The actual capacity is SYNTHKEY_KEY_BUFFER_SIZE - 1
// since there is a one entry "buffer" between Head and Tail
// used to detect when the buffer is full.
//
typedef struct _EFI_KEY_BUFFER
{
  EFI_KEY_DATA  Buffer[SYNTHKEY_KEY_BUFFER_SIZE];
  UINTN         Head;
  UINTN         Tail;
} EFI_KEY_BUFFER, *PEFI_KEY_BUFFER;


//
// Encapsulates the EFI KeyState (shift key and toggle key state) plus
// internal state specific to the Hyper-V driver.
//
#pragma warning(disable : 4201)
typedef struct _SYNTH_KEYBOARD_STATE
{
    EFI_KEY_STATE   KeyState;

    struct
    {
        // TRUE -> EMCL Channel has been opened.
        unsigned    ChannelOpen:1;

        // TRUE -> vdev protocol negotiation was successfull.
        unsigned    ChannelConnected:1;

        // TRUE -> both of the input protocols are installed.
        unsigned    SimpleTextInstalled:1;

        // TRUE -> Pause key mulit-scancode sequence is in progress.
        unsigned    PauseSequence:1;
    };

}SYNTH_KEYBOARD_STATE, *PSYNTH_KEYBOARD_STATE;
#pragma warning(default : 4201)


#define SYNTH_KEYBOARD_DEVICE_SIGNATURE         SIGNATURE_32('S', 'k', 'e', 'y')

//
//  Device context for a synthetic keyboard instance.
//
typedef struct _SYNTH_KEYBOARD_DEVICE
{
    UINTN                               Signature;

    EFI_HANDLE                          Handle;
    EFI_DEVICE_PATH_PROTOCOL           *DevicePath;
    EFI_EMCL_PROTOCOL                  *Emcl;

    EFI_SIMPLE_TEXT_INPUT_PROTOCOL      ConIn;
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL   ConInEx;

    SYNTH_KEYBOARD_STATE                State;
    EFI_EVENT                           InitCompleteEvent;

    EFI_KEY_BUFFER                      EfiKeyQueue;

    //
    // Notification Function List
    //
    LIST_ENTRY                          NotifyList;

} SYNTH_KEYBOARD_DEVICE, *PSYNTH_KEYBOARD_DEVICE;


__forceinline
VOID
SynthKeyReportStatus(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice,
    IN          EFI_STATUS_CODE_TYPE        Type,
    IN          EFI_STATUS_CODE_VALUE       Value
    )
/*++

Routine Description:

    Reports device status for the given keyboard device.

Arguments:

    Device      Device for which to report status.

    Type        One of EFI_PROGRESS_CODE, EFI_ERROR_CODE, EFI_DEBUG_CODE, etc. optionally
                combined with a severity, EFI_ERROR_MAJOR, EFI_ERROR_MINOR, etc.

    Value       Status to report. Typically this will be one of the Peripheral Class Progress Codes
                such as EFI_P_PC_RESET, or EFI_P_EC_CONTROLLER_ERROR.

Return Value:

    None.

--*/
{
    Type |= EFI_PERIPHERAL_KEYBOARD;

    REPORT_STATUS_CODE_WITH_DEVICE_PATH(Type, Value, pDevice->DevicePath);
}


//
// Helpers to get the device context from
// EFI_SIMPLE_TEXT_INPUT_PROTOCOL or EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL pointers
//
#define SYNTH_KEYBOARD_DEVICE_FROM_THIS(a)      \
    CR((a), SYNTH_KEYBOARD_DEVICE, ConIn, SYNTH_KEYBOARD_DEVICE_SIGNATURE)

#define SYNTH_KEYBOARD_DEVICE_FROM_THIS_EX(a)   \
    CR((a), SYNTH_KEYBOARD_DEVICE, ConInEx, SYNTH_KEYBOARD_DEVICE_SIGNATURE)


#define TPL_KEYBOARD_CALLBACK (TPL_CALLBACK + 1)
#define TPL_KEYBOARD_NOTIFY   TPL_NOTIFY

