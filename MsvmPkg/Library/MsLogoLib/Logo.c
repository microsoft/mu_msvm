/** @file
  BDS Lib functions which contain all the code to connect console device.

  Copyright (c) 2011 - 2013, Intel Corporation. All rights reserved.
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <PiDxe.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/BootLogo2.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/PcdLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/DisplayDeviceStateLib.h>
#include <Library/BmpSupportLib.h>
#include <Library/MsLogoLib.h>

#include <Guid/GlobalVariable.h>

//
// Platform defined native resolution.
//
static UINT32    mNativeHorizontalResolution;
static UINT32    mNativeVerticalResolution;
static UINT32    mNativeTextModeColumn;
static UINT32    mNativeTextModeRow;

//
// Mode for PXE booting and other places that VGA is necessary.
//
static UINT32    mVgaTextModeColumn;
static UINT32    mVgaTextModeRow;
static UINT32    mVgaHorizontalResolution;
static UINT32    mVgaVerticalResolution;

static BOOLEAN   mModeTableInitialized = FALSE;
static UINT8     BackgroundColoringSkipCounter;   // Color background only when counter reaches 0;

#define MS_MAX_HEIGHT_PERCENTAGE     40  //40%
#define MS_MAX_WIDTH_PERCENTAGE      40  //40%


/**
  Use SystemTable Conout to stop video based Simple Text Out consoles from going
  to the video device. Put up LogoFile on every video device that is a console.

  @param[in]  LogoFile   File name of logo to display on the center of the screen.

  @retval EFI_SUCCESS     ConsoleControl has been flipped to graphics and logo displayed.
  @retval EFI_UNSUPPORTED Logo not found

**/
EFI_STATUS
EFIAPI
EnableQuietBoot(
IN  EFI_GUID  *LogoFile
)
{
    EFI_STATUS                    Status;
    UINT32                        SizeOfX;
    UINT32                        SizeOfY;
    INTN                          DestX;
    INTN                          DestY;
    UINT8                         *ImageData;
    UINTN                         ImageSize;
    UINTN                         BltSize;
    UINTN                         Height;
    UINTN                         Width;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL  *GraphicsOutput;
    EDKII_BOOT_LOGO2_PROTOCOL     *BootLogo;
    BOOLEAN                       IsLandscape = FALSE;
    BOOLEAN                       IsSystemLogo = FALSE;
    UINT32                        Color;

    Blt = NULL;
    ImageData = NULL;
    ImageSize = 0;
    BootLogo = NULL;

    //
    // get GOP
    //
    //Status = gBS->HandleProtocol(gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid, (VOID **)&GraphicsOutput);
    Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&GraphicsOutput);
    if (EFI_ERROR(Status)) {
        GraphicsOutput = NULL;
        Status = EFI_UNSUPPORTED;
        goto CleanUp;
    }

    SizeOfX = GraphicsOutput->Mode->Info->HorizontalResolution;
    SizeOfY = GraphicsOutput->Mode->Info->VerticalResolution;
    ///
    /// Print Mode information recived from Graphics Output Protocol
    ///
    DEBUG((DEBUG_INFO, "MaxMode:0x%x \n", GraphicsOutput->Mode->MaxMode));
    DEBUG((DEBUG_INFO, "Mode:0x%x \n", GraphicsOutput->Mode->Mode));
    DEBUG((DEBUG_INFO, "SizeOfInfo:0x%x \n", GraphicsOutput->Mode->SizeOfInfo));
    DEBUG((DEBUG_INFO, "FrameBufferBase:0x%x \n", GraphicsOutput->Mode->FrameBufferBase));
    DEBUG((DEBUG_INFO, "FrameBufferSize:0x%x \n", GraphicsOutput->Mode->FrameBufferSize));
    DEBUG((DEBUG_INFO, "Version:0x%x \n", GraphicsOutput->Mode->Info->Version));
    DEBUG((DEBUG_INFO, "HorizontalResolution:0x%x \n", GraphicsOutput->Mode->Info->HorizontalResolution));
    DEBUG((DEBUG_INFO, "VerticalResolution:0x%x \n", GraphicsOutput->Mode->Info->VerticalResolution));
    DEBUG((DEBUG_INFO, "PixelFormat:0x%x \n", GraphicsOutput->Mode->Info->PixelFormat));
    DEBUG((DEBUG_INFO, "PixelsPerScanLine:0x%x \n", GraphicsOutput->Mode->Info->PixelsPerScanLine));

    // Color POST background as per the gMsvmPkgTokenSpaceGuid..PcdPostBackgroundColor

    Color = PcdGet32 (PcdPostBackgroundColor);

    // Color background if not black (default) and only when counter BackgroundColoringSkipCounter reaches 0
    if (Color != 0 && BackgroundColoringSkipCounter == 0) {
        Blt = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)(&Color); // only pixel (0,0) is used for EfiBltVideoFill

        Status = GraphicsOutput->Blt(
                  GraphicsOutput,
                  Blt,
                  EfiBltVideoFill,
                  0,
                  0,
                  0,
                  0,
                  SizeOfX,
                  SizeOfY,
                  0              // not used for EfiBltVideoFill
                  );

        DEBUG((DEBUG_INFO, "Status of Background coloring of 0x%x in DXE:0x%x \n", Color, Status));

        Blt = NULL;
    }

    if (BackgroundColoringSkipCounter > 0) BackgroundColoringSkipCounter--;

    //
    // Try to open Boot Logo Protocol.
    //
    gBS->LocateProtocol(&gEdkiiBootLogo2ProtocolGuid, NULL, (VOID **)&BootLogo);

    //
    // Erase Cursor from screen
    //
    gST->ConOut->EnableCursor(gST->ConOut, FALSE);

    DisplayDeviceState(
        (INT32)SizeOfX,
        (INT32)SizeOfY
        );

    //
    // Get the specified image from FV.
    //
    Status = GetSectionFromAnyFv(LogoFile, EFI_SECTION_RAW, 0, (VOID **)&ImageData, &ImageSize);
    if (EFI_ERROR(Status)) {
        Status = EFI_UNSUPPORTED;
        goto CleanUp;
    }

    //
    // Try BMP decoder
    //
    Status = TranslateBmpToGopBlt(
        ImageData,
        ImageSize,
        &Blt,
        &BltSize,
        &Height,
        &Width
        );
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Failed to TranslateBmpToGopBlt In Logo.c %r\n", Status));
        goto CleanUp;
    }

    //
    // Process Logo files differently
    // than other files
    //
    if (LogoFile == PcdGetPtr(PcdLogoFile)) {
        IsSystemLogo = TRUE;

        if (SizeOfX >= SizeOfY) {
            DEBUG((DEBUG_VERBOSE, "Landscape mode detected.\n"));
            IsLandscape = TRUE;
        }

        //check if the image is appropriate size as per data defined in the windows engineering guide.
        if (Width > ((SizeOfX * MS_MAX_WIDTH_PERCENTAGE) / 100) || Height > ((SizeOfY * MS_MAX_HEIGHT_PERCENTAGE) / 100)) {
            DEBUG((DEBUG_ERROR, "Logo dimensions are not according to Specification. Screen size is %d by %d, Logo size is %d by %d   \n", SizeOfX, SizeOfY, Width, Height));
            //ASSERT_EFI_ERROR(EFI_INVALID_PARAMETER);
            Status = EFI_INVALID_PARAMETER;
            goto CleanUp;
        }

        //For Surface we ignore WEG.  Just place in middle of screen
        //according to WEG specs.
        DestX = (SizeOfX - Width) / 2;
        DestY = (SizeOfY - Height) / 2;
    }
    //other files...just center
    else
    {
        DestX = (SizeOfX - Width) / 2;
        DestY = (SizeOfY - Height) / 2;
    }

    //Display the bmp.
    if ((DestX >= 0) && (DestY >= 0)) {
        DEBUG((DEBUG_INFO, "Logo.c BLTing bmp.\n"));
        Status = GraphicsOutput->Blt(
            GraphicsOutput,
            Blt,
            EfiBltBufferToVideo,
            0,
            0,
            (UINTN)DestX,
            (UINTN)DestY,
            Width,
            Height,
            Width * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
            );
        DEBUG((DEBUG_INFO, "Status of BLT: %r\n", Status));
    }
    else
    {
        DEBUG((DEBUG_ERROR, "Something really wrong with logo size and orientation.  DestX = 0x%X DestY = 0x%X\n", DestX, DestY));
        goto CleanUp;
    }

    if (IsSystemLogo && (BootLogo != NULL) && (Blt != NULL)) {
        BootLogo->SetBootLogo (BootLogo, Blt, DestX, DestY, Width, Height);
    }

CleanUp:
    if (Blt != NULL){
        FreePool(Blt);
    }
    if (ImageData != NULL)
    {
        FreePool(ImageData);
    }
    return Status;
}



/**
  Use SystemTable Conout to turn on video based Simple Text Out consoles. The
  Simple Text Out screens will now be synced up with all non video output devices

  @retval EFI_SUCCESS     UGA devices are back in text mode and synced up.

**/
EFI_STATUS
EFIAPI
DisableQuietBoot (
    VOID
    )
{

    //
    // Enable Cursor on Screen
    //
    gST->ConOut->EnableCursor (gST->ConOut, TRUE);
    return EFI_SUCCESS;
}


//
// Constructor
//
EFI_STATUS
EFIAPI
MsLogoLibConstructor (
    IN EFI_HANDLE        ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
    )
{
    BackgroundColoringSkipCounter = PcdGet8(PcdPostBackgroundColoringSkipCount);  // Color background only when counter reaches 0;

  return EFI_SUCCESS;
}


/**
 * InitializeModeTable - When the Consplitter Gop is published,
 * initialize our local mode table
 * @param
 *
 * @return VOID
 */
static
VOID
InitializeModeTable (
    VOID
    )
{
    EFI_STATUS  Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL           *Gop = NULL;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION   *Info = NULL;
    UINT32                                  Indx;
    UINT32                                  MaxHRes = 800;
    UINTN                                   Size = 0;

    if (!mModeTableInitialized) {
        mVgaTextModeColumn          = 100; // Standard VGA resolution with
        mVgaTextModeRow             = 31;  // EFI standard glyphs
        mVgaHorizontalResolution    = 800;
        mVgaVerticalResolution      = 600;

        mNativeTextModeColumn       = 100; // Default to VGA resolution
        mNativeTextModeRow          = 31;  // but set to highest native resolution.
        mNativeHorizontalResolution = 800;
        mNativeVerticalResolution   = 600;

        Status = gBS->HandleProtocol (gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid, (VOID **) &Gop);
        if (!EFI_ERROR(Status) && (Gop != NULL))
        {
            for( Indx = 0; Indx < Gop->Mode->MaxMode; Indx++)
            {
                Status = Gop->QueryMode(Gop, Indx, &Size, &Info);
                if (!EFI_ERROR(Status))
                {
                    if (MaxHRes < Info->HorizontalResolution)
                    {
                        mNativeHorizontalResolution = Info->HorizontalResolution;
                        mNativeVerticalResolution   = Info->VerticalResolution;
                        MaxHRes = Info->HorizontalResolution;
                    }
                    DEBUG((DEBUG_INFO, "Mode Info for Mode %d\n", Indx));
                    DEBUG((DEBUG_INFO, "HRes: %d VRes: %d PPScanLine: %d \n", Info->HorizontalResolution, Info->VerticalResolution, Info->PixelsPerScanLine));
                    FreePool (Info);

                    mModeTableInitialized = TRUE;
                }
            }
            mNativeTextModeColumn = mNativeHorizontalResolution / EFI_GLYPH_WIDTH;
            mNativeTextModeRow = mNativeVerticalResolution / EFI_GLYPH_HEIGHT;
        }
    }
}

/**
  This function will change video resolution and text mode
  according to defined setup mode or defined boot mode

  @param  IsVgaMode     TRUE, set to VGA 800x600 mode.  FALSE, set to maximum native mode

  @retval  EFI_SUCCESS  Mode is changed successfully.
  @retval  Others       Mode failed to be changed.

**/
EFI_STATUS
EFIAPI
MsLogoLibSetConsoleMode (
    BOOLEAN  IsVgaMode,
    BOOLEAN  DrawLogo
    )
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL          *GraphicsOutput;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL       *SimpleTextOut;
    UINTN                                 SizeOfInfo;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;
    UINT32                                MaxGopMode;
    UINT32                                MaxTextMode;
    UINT32                                ModeNumber;
    UINT32                                NewHorizontalResolution;
    UINT32                                NewVerticalResolution;
    UINT32                                NewColumns;
    UINT32                                NewRows;
    UINTN                                 HandleCount;
    EFI_HANDLE                            *HandleBuffer;
    EFI_STATUS                            Status;
    UINTN                                 Index;
    UINTN                                 CurrentColumn;
    UINTN                                 CurrentRow;

    InitializeModeTable ();
    MaxGopMode  = 0;
    MaxTextMode = 0;

    //
    // Get current video resolution and text mode
    //
    Status = gBS->HandleProtocol (
                    gST->ConsoleOutHandle,
                    &gEfiGraphicsOutputProtocolGuid,
                    (VOID**)&GraphicsOutput
                    );
    if (EFI_ERROR (Status)) {
        GraphicsOutput = NULL;
    }

    Status = gBS->HandleProtocol (
                    gST->ConsoleOutHandle,
                    &gEfiSimpleTextOutProtocolGuid,
                    (VOID**)&SimpleTextOut
                    );
    if (EFI_ERROR (Status)) {
        SimpleTextOut = NULL;
    }

    if ((GraphicsOutput == NULL) || (SimpleTextOut == NULL)) {
        return EFI_UNSUPPORTED;
    }

    if (IsVgaMode) {
        //
        // The requried resolution and text mode is setup mode.
        //
        NewHorizontalResolution = mVgaHorizontalResolution;
        NewVerticalResolution   = mVgaVerticalResolution;
        NewColumns              = mVgaTextModeColumn;
        NewRows                 = mVgaTextModeRow;
    } else {
        //
        // The required resolution and text mode is boot mode.
        //
        NewHorizontalResolution = mNativeHorizontalResolution;
        NewVerticalResolution   = mNativeVerticalResolution;
        NewColumns              = mNativeTextModeColumn;
        NewRows                 = mNativeTextModeRow;
    }

    if (GraphicsOutput != NULL) {
        MaxGopMode  = GraphicsOutput->Mode->MaxMode;
    }

    if (SimpleTextOut != NULL) {
        MaxTextMode = SimpleTextOut->Mode->MaxMode;
    }

    //
    // 1. If current video resolution is same with required video resolution,
    //    video resolution need not be changed.
    //    1.1. If current text mode is same with required text mode, text mode need not be changed.
    //    1.2. If current text mode is different from required text mode, text mode need be changed.
    // 2. If current video resolution is different from required video resolution, we need restart whole console drivers.
    //
    for (ModeNumber = 0; ModeNumber < MaxGopMode; ModeNumber++) {
        Status = GraphicsOutput->QueryMode (
                                   GraphicsOutput,
                                   ModeNumber,
                                   &SizeOfInfo,
                                   &Info
                                   );
        if (!EFI_ERROR (Status)) {
            if ((Info->HorizontalResolution == NewHorizontalResolution) &&
                (Info->VerticalResolution == NewVerticalResolution)) {
                if ((GraphicsOutput->Mode->Info->HorizontalResolution == NewHorizontalResolution) &&
                    (GraphicsOutput->Mode->Info->VerticalResolution == NewVerticalResolution)) {
                    //
                    // Current resolution is same with required resolution, check if text mode need be set
                    //
                    Status = SimpleTextOut->QueryMode (SimpleTextOut, SimpleTextOut->Mode->Mode, &CurrentColumn, &CurrentRow);
                    ASSERT_EFI_ERROR (Status);
                    if (CurrentColumn == NewColumns && CurrentRow == NewRows) {
                        //
                        // If current text mode is same with required text mode. Do nothing
                        //
                        FreePool (Info);
                        Status = EFI_SUCCESS;
                        goto CheckDraw;
                    } else {
                        //
                        // If current text mode is different from requried text mode.  Set new video mode
                        //
                        for (Index = 0; Index < MaxTextMode; Index++) {
                            Status = SimpleTextOut->QueryMode (SimpleTextOut, Index, &CurrentColumn, &CurrentRow);
                            if (!EFI_ERROR(Status)) {
                                if ((CurrentColumn == NewColumns) && (CurrentRow == NewRows)) {
                                    //
                                    // Required text mode is supported, set it.
                                    //
                                    Status = SimpleTextOut->SetMode (SimpleTextOut, Index);
                                    ASSERT_EFI_ERROR (Status);
                                    //
                                    // Update text mode PCD.
                                    //
                                    Status = PcdSet32S (PcdConOutColumn, mVgaTextModeColumn);
                                    if (EFI_ERROR(Status))
                                    {
                                        DEBUG((DEBUG_ERROR, "Failed to set the PCD PcdConOutColumn::0x%x \n", Status));
                                        FreePool (Info);
                                        goto CheckDraw;
                                    }
                                    Status = PcdSet32S (PcdConOutRow, mVgaTextModeRow);
                                    if (EFI_ERROR(Status))
                                    {
                                        DEBUG((DEBUG_ERROR, "Failed to set the PCD PcdConOutRow::0x%x \n", Status));
                                        FreePool (Info);
                                        goto CheckDraw;
                                    }

                                    FreePool (Info);
                                    goto CheckDraw;
                                }
                            }
                        }
                        if (Index == MaxTextMode) {
                            //
                            // If requried text mode is not supported, return error.
                            //
                            FreePool (Info);
                            Status = EFI_UNSUPPORTED;
                            goto CheckDraw;
                        }
                    }
                } else {
                    //
                    // If current video resolution is not same with the new one, set new video resolution.
                    // In this case, the driver which produces simple text out need be restarted.
                    //
                    Status = GraphicsOutput->SetMode (GraphicsOutput, ModeNumber);
                    if (!EFI_ERROR (Status)) {
                        FreePool (Info);
                        break;
                    }
                }
            }
            FreePool (Info);
        }
    }

    if (ModeNumber == MaxGopMode) {
        //
        // If the resolution is not supported, return error.
        //
        Status = EFI_UNSUPPORTED;
        goto CheckDraw;
    }

    //
    // Set PCD to Inform GraphicsConsole to change video resolution.
    // Set PCD to Inform Consplitter to change text mode.
    //
    Status = PcdSet32S (PcdVideoHorizontalResolution, NewHorizontalResolution);
    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to set the PCD PcdVideoHorizontalResolution::0x%x \n", Status));
        goto CheckDraw;
    }
    Status = PcdSet32S (PcdVideoVerticalResolution, NewVerticalResolution);
    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to set the PCD PcdVideoVerticalResolution::0x%x \n", Status));
        goto CheckDraw;
    }
    Status = PcdSet32S (PcdConOutColumn, NewColumns);
    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to set the PCD PcdConOutColumn::0x%x \n", Status));
        goto CheckDraw;
    }
    Status = PcdSet32S (PcdConOutRow, NewRows);
    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to set the PCD PcdConOutRow::0x%x \n", Status));
        goto CheckDraw;
    }

    //
    // Video mode is changed, so restart graphics console driver and higher level driver.
    // Reconnect graphics console driver and higher level driver.
    // Locate all the handles with GOP protocol and reconnect it.
    //
    Status = gBS->LocateHandleBuffer (
                    ByProtocol,
                   &gEfiSimpleTextOutProtocolGuid,
                    NULL,
                   &HandleCount,
                   &HandleBuffer
                     );
    if (!EFI_ERROR (Status)) {
        for (Index = 0; Index < HandleCount; Index++) {
            gBS->DisconnectController (HandleBuffer[Index], NULL, NULL);
        }
        for (Index = 0; Index < HandleCount; Index++) {
            gBS->ConnectController (HandleBuffer[Index], NULL, NULL, TRUE);
        }
        if (HandleBuffer != NULL) {
            FreePool (HandleBuffer);
        }
    }
    Status = EFI_SUCCESS;

CheckDraw:
    if (DrawLogo) {
        //Find the SimpleTextOut protocol again as it could have been destroyed and re-created.
        Status = gBS->HandleProtocol (
                        gST->ConsoleOutHandle,
                        &gEfiSimpleTextOutProtocolGuid,
                        (VOID **)&SimpleTextOut
            );
        if (!EFI_ERROR(Status)) {
            SimpleTextOut->ClearScreen (SimpleTextOut);
        }
        EnableQuietBoot(PcdGetPtr(PcdLogoFile));
    }

    return Status;
}
