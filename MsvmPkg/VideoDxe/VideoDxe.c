/** @file
  EFI GOP Driver for Hyper-V Synthetic Video

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "VideoDxe.h"
#include <VirtualDeviceId.h>
#include <VramSize.h>


EFI_DRIVER_BINDING_PROTOCOL gVideoDxeDriverBinding =
{
    VideoDxeDriverBindingSupported,
    VideoDxeDriverBindingStart,
    VideoDxeDriverBindingStop,
    VIDEODXE_VERSION,
    NULL,
    NULL
};


EFI_HANDLE VideoDxeImageHandle;
EFI_PHYSICAL_ADDRESS FrameBufferBaseAddress;


EFI_STATUS
EFIAPI
VideoDxeDriverEntryPoint (
    IN  EFI_HANDLE ImageHandle,
    IN  EFI_SYSTEM_TABLE *SystemTable
    )
/*++

Routine Description:

    Driver entry point.

Arguments:

    ImageHandle - The firmware allocated handle for the EFI image.

    SystemTable - A pointer to the EFI System Table.

Return Value:

    EFI_STATUS

--*/
{
    VideoDxeImageHandle = ImageHandle;

    //
    // Install UEFI Driver Model protocols.
    //
    return EfiLibInstallDriverBindingComponentName2(ImageHandle,
                                                    SystemTable,
                                                    &gVideoDxeDriverBinding,
                                                    ImageHandle,
                                                    &gVideoDxeComponentName,
                                                    &gVideoDxeComponentName2);
}


// -------------------------------------------
//
// EFI_DRIVER_BINDING_PROTOCOL implementation.
//
// -------------------------------------------


EFI_STATUS
EFIAPI
VideoDxeDriverBindingSupported(
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    )
/*++

Routine Description:

    Tests to see if this driver supports a given controller.

Arguments:

    This - A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.

    ControllerHandle - The handle of the controller to test.

    RemainingDevicePath - A pointer to the remaining portion of a device path.

Return Value:

    EFI_STATUS

--*/
{
    EFI_STATUS status;
    EFI_VMBUS_PROTOCOL *vmbus;

    status = gBS->OpenProtocol(ControllerHandle,
                               &gEfiVmbusProtocolGuid,
                               (VOID **) &vmbus,
                               This->DriverBindingHandle,
                               ControllerHandle,
                               EFI_OPEN_PROTOCOL_TEST_PROTOCOL);

    if (EFI_ERROR(status))
    {
        return status;
    }

    status = EmclChannelTypeSupported(ControllerHandle,
                                      &gSyntheticVideoClassGuid,
                                      This->DriverBindingHandle);

    if (status == EFI_SUCCESS)
    {
        return status;
    }

    return EmclChannelTypeSupported(ControllerHandle,
                                    &gSynthetic3dVideoClassGuid,
                                    This->DriverBindingHandle);
}


EFI_STATUS
EFIAPI
VideoDxeDriverBindingStart (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    )
/*++

Routine Description:

    Starts the device.
     -  Binds to the EMCL protocol.
     -  Creates the driver context.
     -  Opens the vmbus channel and intializes with the VSP.
     -  Exposes the GOP protocol interface.

Arguments:

    This - A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.

    ControllerHandle - The handle of the controller to start.

    RemainingDevicePath - A pointer to the remaining portion of a device path.

Return Value:

    EFI_STATUS

--*/
{
    EFI_STATUS status;
    BOOLEAN driverStarted = FALSE;
    BOOLEAN emclInstalled = FALSE;
    VIDEODXE_CONTEXT* context = NULL;

    status = EmclInstallProtocol(ControllerHandle);

    if (status == EFI_ALREADY_STARTED)
    {
        //
        // It might be already installed so no more work is needed.
        //
        driverStarted = TRUE;
        goto Cleanup;
    }
    else if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    emclInstalled = TRUE;

    //
    // Allocate the private device structure for video device
    //
    context = AllocateZeroPool (sizeof (VIDEODXE_CONTEXT));
    if (context == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    status = gBS->OpenProtocol(ControllerHandle,
                               &gEfiEmclProtocolGuid,
                               (VOID **) &context->Emcl,
                               This->DriverBindingHandle,
                               ControllerHandle,
                               EFI_OPEN_PROTOCOL_BY_DRIVER);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR,
                "VideoDxeDriverBindingStart - OpenProtocol(Emcl) failed. Status %x\n",
                status));
        goto Cleanup;
    }

    context->Signature = VIDEODXE_CONTEXT_SIGNATURE;
    context->Handle = ControllerHandle;

    //
    // Fill in the Graphics Output Protocol
    //
    context->GraphicsOutput.QueryMode = VideoGraphicsOutputQueryMode;
    context->GraphicsOutput.SetMode   = VideoGraphicsOutputSetMode;
    context->GraphicsOutput.Blt       = VideoGraphicsOutputBlt;
    context->GraphicsOutput.Mode      = &context->Mode;
    context->Mode.MaxMode         = 1;

    // Set Mode to the current and only supported mode.
    // FUTURE: If more modes are added, use a PCD to specify a default.
    context->Mode.Mode            = 0;
    context->Mode.Info            = &context->ModeInfo;
    context->Mode.SizeOfInfo      = sizeof (context->ModeInfo);

    //
    // Allocate physical MMIO space for the frame buffer.
    //
    context->Mode.FrameBufferSize = DEFAULT_VRAM_SIZE_WIN8;
    status = gDS->AllocateMemorySpace(EfiGcdAllocateAnySearchBottomUp,
                                      EfiGcdMemoryTypeMemoryMappedIo,
                                      0,
                                      context->Mode.FrameBufferSize,
                                      &FrameBufferBaseAddress,
                                      VideoDxeImageHandle,
                                      NULL);
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR,
               "VideoDxeDriverBindingStart - "
               "AllocateMemorySpace(MMIO) failed. Status %x\n",
               status));
        goto Cleanup;
    }

    context->Mode.FrameBufferBase = FrameBufferBaseAddress;

    context->ModeInfo.Version              = 0;
    context->ModeInfo.HorizontalResolution = DEFAULT_SCREEN_WIDTH;
    context->ModeInfo.VerticalResolution   = DEFAULT_SCREEN_HEIGHT;
    context->ModeInfo.PixelFormat          = PixelBlueGreenRedReserved8BitPerColor;
    context->ModeInfo.PixelsPerScanLine    = DEFAULT_SCREEN_WIDTH;


    //
    // "Open" the channel to the VSP.
    //
    status = VideoChannelOpen(context);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR,
               "VideoDxeDriverBindingStart - "
               "VideoChannelOpen failed. Status %x\n",
               status));
        goto Cleanup;
    }

    //
    // Create child handle and install Graphics Output Protocol
    //
    status = gBS->InstallMultipleProtocolInterfaces(&context->Handle,
                                                    &mMsGopOverrideProtocolGuid,
                                                    &context->GraphicsOutput,
                                                    NULL);
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR,
               "VideoDxeDriverBindingStart - GOP install failed. Status %x\n",
               status));
        goto Cleanup;
    }

    driverStarted = TRUE;

Cleanup:

    if (!driverStarted)
    {
        if (context != NULL)
        {
            VideoChannelClose(context);
        }

        gBS->CloseProtocol(ControllerHandle,
                           &gEfiEmclProtocolGuid,
                           This->DriverBindingHandle,
                           ControllerHandle);

        if (emclInstalled)
        {
            EmclUninstallProtocol(ControllerHandle);
        }
    }

    return status;
}


EFI_STATUS
EFIAPI
VideoDxeDriverBindingStop (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  UINTN NumberOfChildren,
    IN  EFI_HANDLE *ChildHandleBuffer
    )
/*++

Routine Description:

    Stops a device controller.

Arguments:

    This - A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.

    ControllerHandle - A handle to the device being stopped.

    NumberOfChildren - The number of child device handles in ChildHandleBuffer.

    ChildHandleBuffer - An array of child handles to be freed.

Return Value:

    EFI_STATUS

--*/
{
    EFI_STATUS status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput;
    VIDEODXE_CONTEXT* context = NULL;

    status = gBS->OpenProtocol(ControllerHandle,
                               &mMsGopOverrideProtocolGuid,
                               (VOID **) &GraphicsOutput,
                               This->DriverBindingHandle,
                               ControllerHandle,
                               EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (EFI_ERROR(status))
    {
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    context = VIDEODXE_CONTEXT_FROM_GRAPHICS_OUTPUT_THIS(GraphicsOutput);

    VideoChannelClose(context);

    //
    // Uninstall protocols on child handle
    //
    status = gBS->UninstallMultipleProtocolInterfaces(context->Handle,
                                                      &mMsGopOverrideProtocolGuid,
                                                      &context->GraphicsOutput,
                                                      NULL);
    //
    // Unhook EMCL
    //
    gBS->CloseProtocol(ControllerHandle,
                       &gEfiEmclProtocolGuid,
                       This->DriverBindingHandle,
                       ControllerHandle);

    EmclUninstallProtocol(ControllerHandle);

Cleanup:
    return status;
}


//---------------------------------------------
//
// EFI_GRAPHICS_OUTPUT_PROTOCOL implementation.
//
//---------------------------------------------


EFI_STATUS
EFIAPI
VideoGraphicsOutputQueryMode (
    IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
    IN  UINT32                                ModeNumber,
    OUT UINTN                                 *SizeOfInfo,
    OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **Info
  )
/*++

Routine Description:

  Graphics Output protocol interface to get video mode

Arguments:

    This - Protocol instance pointer.

    ModeNumber - The mode number to return information on.

    Info - Caller allocated buffer that returns information about ModeNumber.

    SizeOfInfo - A pointer to the size, in bytes, of the Info buffer.

Return Value:

    EFI_STATUS

--*/
{
    VIDEODXE_CONTEXT* context = NULL;

    if (This == NULL ||
        Info == NULL ||
        SizeOfInfo == NULL ||
        ModeNumber >= This->Mode->MaxMode)
    {
        return EFI_INVALID_PARAMETER;
    }

    context = VIDEODXE_CONTEXT_FROM_GRAPHICS_OUTPUT_THIS(This);

    *Info = AllocateCopyPool(sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
                             &context->ModeInfo);
    if (*Info == NULL)
    {
        return EFI_OUT_OF_RESOURCES;
    }

    *SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
VideoGraphicsOutputSetMode(
    IN   EFI_GRAPHICS_OUTPUT_PROTOCOL * This,
    IN   UINT32                         ModeNumber
  )
/*++

Routine Description:

    Graphics Output protocol interface to set video mode

Arguments:

    This - Protocol instance pointer.

    ModeNumber - The mode number to be set.

Return Value:

    EFI_STATUS

--*/
{
    VIDEODXE_CONTEXT* context = NULL;

    if (This == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    if (ModeNumber >= This->Mode->MaxMode)
    {
        return EFI_UNSUPPORTED;
    }

    context = VIDEODXE_CONTEXT_FROM_GRAPHICS_OUTPUT_THIS(This);

    if (ModeNumber == This->Mode->Mode)
    {
        return EFI_SUCCESS;
    }

    This->Mode->Mode = ModeNumber;

    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
VideoGraphicsOutputBlt (
    IN   EFI_GRAPHICS_OUTPUT_PROTOCOL       *This,
    IN   EFI_GRAPHICS_OUTPUT_BLT_PIXEL      *BltBuffer, OPTIONAL
    IN   EFI_GRAPHICS_OUTPUT_BLT_OPERATION  BltOperation,
    IN   UINTN                              SourceX,
    IN   UINTN                              SourceY,
    IN   UINTN                              DestinationX,
    IN   UINTN                              DestinationY,
    IN   UINTN                              Width,
    IN   UINTN                              Height,
    IN   UINTN                              Delta
  )
/*++

Routine Description:

      Graphics Output protocol instance to block transfer for VBE device. Writing to the
      framebuffer (VRAM) will be caught and sent to the Synth Video Device.

Arguments:

      This          - Pointer to Graphics Output protocol instance

      BltBuffer     - The data to transfer to screen

      BltOperation  - The operation to perform

      SourceX       - The X coordinate of the source for BltOperation

      SourceY       - The Y coordinate of the source for BltOperation

      DestinationX  - The X coordinate of the destination for BltOperation

      DestinationY  - The Y coordinate of the destination for BltOperation

      Width         - The width of a rectangle in the blt rectangle in pixels

      Height        - The height of a rectangle in the blt rectangle in pixels

      Delta         - Not used for EfiBltVideoFill and EfiBltVideoToVideo operation.
                      If a Delta of 0 is used, the entire BltBuffer will be operated on.
                      If a subrectangle of the BltBuffer is used, then Delta represents
                      the number of bytes in a row of the BltBuffer.

Return Value:

    EFI_STATUS

--*/
{
    VIDEODXE_CONTEXT* context = NULL;
    EFI_TPL         OriginalTPL;
    UINTN           DstY;
    UINTN           SrcY;
    UINTN           BytesPerScanLine;
    UINTN           BytesPerLine;
    UINTN           Index;

    //
    // Check parameters
    //
    if (This == NULL || ((UINTN) BltOperation) >= EfiGraphicsOutputBltOperationMax)
    {
        return EFI_INVALID_PARAMETER;
    }
    if (Width == 0 || Height == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Get the private context of VideoDxe
    //
    context = VIDEODXE_CONTEXT_FROM_GRAPHICS_OUTPUT_THIS(This);

    if (BltOperation == EfiBltVideoToBltBuffer)
    {
        //
        // Video to BltBuffer: Source is Video, destination is BltBuffer
        //
        if (SourceY + Height > context->ModeInfo.VerticalResolution)
        {
            return EFI_INVALID_PARAMETER;
        }
        if (SourceX + Width > context->ModeInfo.HorizontalResolution)
        {
            return EFI_INVALID_PARAMETER;
        }
    } else {

        //
        // BltBuffer to Video: Source is BltBuffer, destination is Video
        //
        if (DestinationY + Height > context->ModeInfo.VerticalResolution)
        {
            return EFI_INVALID_PARAMETER;
        }
        if (DestinationX + Width > context->ModeInfo.HorizontalResolution)
        {
            return EFI_INVALID_PARAMETER;
        }
    }

    BytesPerScanLine = context->ModeInfo.PixelsPerScanLine *
                       sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
    BytesPerLine     = Width * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);

    //
    // If Delta is zero, then the entire BltBuffer is being used.
    //
    if (Delta == 0)
    {
        Delta = Width * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
    }

    //
    // Raise to TPL Notify to synchronize writes the frame buffer.
    //
    OriginalTPL = gBS->RaiseTPL (TPL_NOTIFY);

    //
    // Perform the Blt.
    //
    switch (BltOperation)
    {

    case EfiBltVideoToBltBuffer:
        for (SrcY = SourceY, DstY = DestinationY;
             DstY < (Height + DestinationY);
             SrcY++, DstY++)
        {
            CopyMem((UINT8 *)BltBuffer + (DstY * Delta) +
                        (DestinationX * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)),
                    (UINT8 *)(UINTN)context->Mode.FrameBufferBase + (SrcY * BytesPerScanLine) +
                        (SourceX * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)),
                    BytesPerLine);
        }
        break;

    case EfiBltVideoToVideo:
        for (Index = 0; Index < Height; Index++)
        {
            if (DestinationY <= SourceY)
            {
                SrcY  = SourceY + Index;
                DstY  = DestinationY + Index;
            } else {
                SrcY  = SourceY + Height - Index - 1;
                DstY  = DestinationY + Height - Index - 1;
            }
            CopyMem((UINT8 *)(UINTN)context->Mode.FrameBufferBase + (DstY * BytesPerScanLine) +
                        (DestinationX * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)),
                    (UINT8 *)(UINTN)context->Mode.FrameBufferBase + (SrcY * BytesPerScanLine) +
                        (SourceX * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)),
                    BytesPerLine);
        }
        break;

    case EfiBltVideoFill:
        SetMem32((UINT8 *)(UINTN)context->Mode.FrameBufferBase +
                    (DestinationY * BytesPerScanLine) +
                    (DestinationX * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)),
                 BytesPerLine,
                 *(UINT32 *)BltBuffer);
        for (DstY = DestinationY + 1; DstY < (Height + DestinationY); DstY++)
        {
            CopyMem ((UINT8 *)(UINTN)context->Mode.FrameBufferBase + (DstY * BytesPerScanLine) +
                         (DestinationX * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)),
                     (UINT8 *)(UINTN)context->Mode.FrameBufferBase + (DestinationY * BytesPerScanLine) +
                         (DestinationX * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)),
                     BytesPerLine);
        }
        break;

    case EfiBltBufferToVideo:
        for (SrcY = SourceY, DstY = DestinationY; SrcY < (Height + SourceY); SrcY++, DstY++)
        {
            CopyMem((UINT8 *)(UINTN)context->Mode.FrameBufferBase + (DstY * BytesPerScanLine) +
                        (DestinationX * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)),
                    (UINT8 *)BltBuffer + (SrcY * Delta) +
                        (SourceX * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)),
                    BytesPerLine);
        }
        break;
    }

    gBS->RestoreTPL (OriginalTPL);

    return EFI_SUCCESS;
}
