/** @file
  MsLogoLib library definition.

  Copyright (c) 2011 - 2013, Intel Corporation. All rights reserved.
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/


#ifndef _MS_LOGO_LIB_H_
#define _MS_LOGO_LIB_H_

/**
  Use SystemTable ConOut to stop video based Simple Text Out consoles from going
  to the video device. Put up LogoFile on every video device that is a console.

  @param[in]  LogoFile   The file name of logo to display on the center of the screen.

  @retval EFI_SUCCESS     ConsoleControl has been flipped to graphics and logo displayed.
  @retval EFI_UNSUPPORTED Logo not found.

**/
EFI_STATUS
EFIAPI
EnableQuietBoot (
  IN  EFI_GUID  *LogoFile
  );


/**
  Use SystemTable ConOut to turn on video based Simple Text Out consoles. The
  Simple Text Out screens will now be synced up with all non-video output devices.

  @retval EFI_SUCCESS     UGA devices are back in text mode and synced up.

**/
EFI_STATUS
EFIAPI
DisableQuietBoot (
  VOID
  );

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
    );

#endif

