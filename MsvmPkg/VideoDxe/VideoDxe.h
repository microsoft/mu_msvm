/** @file
  EFI Driver for Synthetic Video Controller.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/


#pragma once

#include <Uefi.h>
#include <PiDxe.h>

#include <Protocol/Vmbus.h>
#include <Protocol/Emcl.h>
#include <Protocol/GraphicsOutput.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/EmclLib.h>
#include <Library/DxeServicesTableLib.h>

#include <Guid/EventGroup.h>

#define BITS_PER_BYTE                   (8)
#define DEFAULT_SCREEN_BYTES_PER_PIXEL  (4)
#define DEFAULT_SCREEN_WIDTH            (1024)
#define DEFAULT_SCREEN_HEIGHT           (768)

typedef struct _RECT {
  INT32 left;
  INT32 top;
  INT32 right;
  INT32 bottom;
} RECT, *PRECT;

#define BYTE UINT8

#include <SynthVidProtocol.h>
#include <Vmbus/VmBusPacketFormat.h>

#define VIDEODXE_VERSION 1
#define VIDEODXE_CONTEXT_SIGNATURE SIGNATURE_32('V','D','X','E')

#define VIDEO_DXE_MAX_PACKET_SIZE (512)

typedef struct _VIDEODXE_CONTEXT VIDEODXE_CONTEXT;

typedef struct _VIDEODXE_CONTEXT
{
    //
    // Device State
    //
    UINTN Signature;
    EFI_HANDLE Handle;
    EFI_EMCL_PROTOCOL *Emcl;
    BOOLEAN ChannelStarted;
    EFI_STATUS InitStatus;
    EFI_EVENT InitCompleteEvent;

    //
    // Produced Protocols
    //
    EFI_GRAPHICS_OUTPUT_PROTOCOL          GraphicsOutput;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE     Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  ModeInfo;

} VIDEODXE_CONTEXT, *PVIDEODXE_CONTEXT;

#define VIDEODXE_CONTEXT_FROM_GRAPHICS_OUTPUT_THIS(a) \
    CR( \
        a, \
        VIDEODXE_CONTEXT, \
        GraphicsOutput, \
        VIDEODXE_CONTEXT_SIGNATURE \
        )


extern EFI_DRIVER_BINDING_PROTOCOL gVideoDxeDriverBinding;
extern EFI_COMPONENT_NAME2_PROTOCOL gVideoDxeComponentName2;
extern EFI_COMPONENT_NAME_PROTOCOL gVideoDxeComponentName;

EFI_STATUS
EFIAPI
VideoDxeDriverEntryPoint (
    IN  EFI_HANDLE ImageHandle,
    IN  EFI_SYSTEM_TABLE *SystemTable
    );

//
// EFI_DRIVER_BINDING_PROTOCOL functions, used to determine if this driver supports the controller,
// to start and stop the controller.
//
EFI_STATUS
EFIAPI
VideoDxeDriverBindingSupported(
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    );

EFI_STATUS
EFIAPI
VideoDxeDriverBindingStart(
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    );

EFI_STATUS
EFIAPI
VideoDxeDriverBindingStop(
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  UINTN NumberOfChildren,
    IN  EFI_HANDLE *ChildHandleBuffer
    );

//
// EFI_COMPONENT_NAME_PROTOCOL and EFI_COMPONENT_NAME2_PROTOCOL functions.
// Used to get a user friendly string.
//
EFI_STATUS
EFIAPI
VideoDxeComponentNameGetDriverName(
    IN  EFI_COMPONENT_NAME2_PROTOCOL *This,
    IN  CHAR8 *Language,
    OUT CHAR16 **DriverName
    );

EFI_STATUS
EFIAPI
VideoDxeComponentNameGetControllerName(
    IN  EFI_COMPONENT_NAME2_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_HANDLE ChildHandle OPTIONAL,
    IN  CHAR8 *Language,
    OUT CHAR16 **ControllerName
    );

//
// Video Channel Functions.
//
EFI_STATUS
VideoChannelOpen(
    IN  PVIDEODXE_CONTEXT Context
    );

VOID
VideoChannelClose(
    IN  PVIDEODXE_CONTEXT Context
    );

EFI_STATUS
VideoChannelStartInitialize(
    IN  PVIDEODXE_CONTEXT Context
    );

//
// EFI_GRAPHICS_OUTPUT_PROTOCOL Protocol functions.
//
EFI_STATUS
EFIAPI
VideoGraphicsOutputQueryMode(
  IN   EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
  IN   UINT32 ModeNumber,
  IN   UINTN *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
  );

EFI_STATUS
EFIAPI
VideoGraphicsOutputSetMode(
  IN   EFI_GRAPHICS_OUTPUT_PROTOCOL * This,
  IN   UINT32 ModeNumber
  );

EFI_STATUS
EFIAPI
VideoGraphicsOutputBlt(
  IN   EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
  IN   EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer, OPTIONAL
  IN   EFI_GRAPHICS_OUTPUT_BLT_OPERATION  BltOperation,
  IN   UINTN SourceX,
  IN   UINTN SourceY,
  IN   UINTN DestinationX,
  IN   UINTN DestinationY,
  IN   UINTN Width,
  IN   UINTN Height,
  IN   UINTN Delta
  );

