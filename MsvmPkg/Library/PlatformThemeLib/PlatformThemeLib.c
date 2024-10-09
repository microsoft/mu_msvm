/*++
    This module will provide the fonts used in the  UI

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Uefi.h>

#include <Protocol/MsUiThemeProtocol.h>
#include <Library/PlatformThemeLib.h>

#define FILLED_AT_RUNTIME  0

#define FONT_DECL(TABLE, NAME )                       \
                                                      \
    static MS_UI_FONT_DESCRIPTION TABLE = {           \
        MS_UI_CUSTOM_FONT_ ## NAME ## _CELL_HEIGHT,   \
        MS_UI_CUSTOM_FONT_ ## NAME ## _CELL_WIDTH,    \
        MS_UI_CUSTOM_FONT_ ## NAME ## _MAX_ADVANCE,   \
        sizeof (mMsUiFontPackageHdr_ ## NAME ## ),    \
        sizeof (mMsUiFontPackageGlyphs_ ## NAME ## ), \
        FILLED_AT_RUNTIME, \
        FILLED_AT_RUNTIME \
    };

#define SCALE 75

// The fonts for this platform are:
#include <Resources/FontPackage_Selawik_Regular_8pt_Fixed.h>
FONT_DECL (FixedFont, Selawik_Regular_8pt_Fixed)

#include <Resources/FontPackage_Selawik_Regular_9pt.h>
FONT_DECL (SmallOSKFont, Selawik_Regular_9pt)

#include <Resources/FontPackage_Selawik_Regular_10pt.h>
FONT_DECL (SmallFont, Selawik_Regular_10pt)

#include <Resources/FontPackage_Selawik_Regular_18pt.h>
FONT_DECL (StandardFont, Selawik_Regular_18pt)

#include <Resources/FontPackage_Selawik_Regular_22pt.h>
FONT_DECL (MediumFont, Selawik_Regular_22pt)

#include <Resources/FontPackage_Selawik_Regular_24pt.h>
FONT_DECL (LargeFont, Selawik_Regular_24pt)

static  MS_UI_THEME_DESCRIPTION gMsUiPlatformTheme = {
  MS_UI_THEME_PROTOCOL_SIGNATURE,
  MS_UI_THEME_PROTOCOL_VERSION,
  SCALE,
  0,
  FILLED_AT_RUNTIME,
  FILLED_AT_RUNTIME,
  FILLED_AT_RUNTIME,
  FILLED_AT_RUNTIME,
  FILLED_AT_RUNTIME,
  FILLED_AT_RUNTIME
};

MS_UI_THEME_DESCRIPTION *
EFIAPI
PlatformThemeGet (
  VOID
  )
{
  FixedFont.Package = FONT_PTR_SET &mMsUiFontPackageHdr_Selawik_Regular_8pt_Fixed;
  FixedFont.Glyphs  = GLYPH_PTR_SET &mMsUiFontPackageGlyphs_Selawik_Regular_8pt_Fixed;

  SmallOSKFont.Package = FONT_PTR_SET &mMsUiFontPackageHdr_Selawik_Regular_9pt;
  SmallOSKFont.Glyphs  = GLYPH_PTR_SET &mMsUiFontPackageGlyphs_Selawik_Regular_9pt;

  SmallFont.Package = FONT_PTR_SET &mMsUiFontPackageHdr_Selawik_Regular_10pt;
  SmallFont.Glyphs  = GLYPH_PTR_SET &mMsUiFontPackageGlyphs_Selawik_Regular_10pt;

  StandardFont.Package = FONT_PTR_SET &mMsUiFontPackageHdr_Selawik_Regular_18pt;
  StandardFont.Glyphs  = GLYPH_PTR_SET &mMsUiFontPackageGlyphs_Selawik_Regular_18pt;

  MediumFont.Package = FONT_PTR_SET &mMsUiFontPackageHdr_Selawik_Regular_22pt;
  MediumFont.Glyphs  = GLYPH_PTR_SET &mMsUiFontPackageGlyphs_Selawik_Regular_22pt;

  LargeFont.Package = FONT_PTR_SET &mMsUiFontPackageHdr_Selawik_Regular_24pt;
  LargeFont.Glyphs  = GLYPH_PTR_SET &mMsUiFontPackageGlyphs_Selawik_Regular_24pt;

  gMsUiPlatformTheme.FixedFont    = FONT_PTR_SET &FixedFont;
  gMsUiPlatformTheme.SmallOSKFont = FONT_PTR_SET &SmallOSKFont;
  gMsUiPlatformTheme.SmallFont    = FONT_PTR_SET &SmallFont;
  gMsUiPlatformTheme.StandardFont = FONT_PTR_SET &StandardFont;
  gMsUiPlatformTheme.MediumFont   = FONT_PTR_SET &MediumFont;
  gMsUiPlatformTheme.LargeFont    = FONT_PTR_SET &LargeFont;

  return &gMsUiPlatformTheme;
}