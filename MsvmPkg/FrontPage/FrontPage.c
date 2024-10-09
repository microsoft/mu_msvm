/** @file
  Implements the Hyper-V UEFI Front Page (Settings Menu).

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/


#include "FrontPage.h"
#include "FrontPageUi.h"
#include "FrontPageConfigAccess.h"

#include <IndustryStandard/SmBios.h>

#include <Guid/FrontPageEventDataStruct.h>
#include <Guid/GlobalVariable.h>
#include <Guid/MdeModuleHii.h>
#include <Guid/DebugImageInfoTable.h>

#include <Pi/PiFirmwareFile.h>

#include <Protocol/GraphicsOutput.h>
#include <Protocol/Smbios.h>
#include <Protocol/OnScreenKeyboard.h>
#include <Protocol/SimpleWindowManager.h>
#include <Protocol/FirmwareManagement.h>

#include <Library/DebugLib.h>
#include <Library/CpuLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HiiLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/BmpSupportLib.h>
#include <Library/MsUiThemeLib.h>
#include <Library/ResetSystemLib.h>
#include <Library/MsLogoLib.h>
#include <Library/BootEventLogLib.h>
#include <Library/MsColorTableLib.h>

#include <MsDisplayEngine.h>
#include <UIToolKit/SimpleUIToolKit.h>

#include <Resources/MicrosoftLogo.h>
#include "PlatformConsole.h"

#define FP_OSK_WIDTH_PERCENT        75      // On-screen keyboard is 75% the width of the screen.

UINTN       mCallbackKey;
CHAR8       *mLanguageString;
EFI_HANDLE  mImageHandle;

//
// Protocols.
//
EFI_GRAPHICS_OUTPUT_PROTOCOL        *mGop;
EFI_HII_FONT_PROTOCOL               *mFont;

//
// UI Elements.
//
UINT32  mTitleBarWidth, mTitleBarHeight;
UINT32  mMasterFrameWidth, mMasterFrameHeight;
BOOLEAN mShowFullMenu = FALSE;      // By default we won't show the full FrontPage menu (requires validation if there's a system password).

//
// About Menu is only needed if there is an about bitmap.
//
BOOLEAN mEnableAboutMenu;

//
// Master Frame - Form Notifications.
//
UINT32                            mCurrentFormIndex;
EFI_EVENT                         mMasterFrameNotifyEvent;
DISPLAY_ENGINE_SHARED_STATE       mDisplayEngineState;
BOOLEAN                           mTerminateFrontPage = FALSE;
BOOLEAN                           mResetRequired;
EFI_HII_CONFIG_ROUTING_PROTOCOL   *mHiiConfigRouting;

extern EFI_HII_HANDLE gStringPackHandle;
extern EFI_GUID  gMsEventMasterFrameNotifyGroupGuid;

//
// Boot video resolution and text mode.
//
UINT32    mBootHorizontalResolution    = 0;
UINT32    mBootVerticalResolution      = 0;
UINT32    mBootTextModeColumn          = 0;
UINT32    mBootTextModeRow             = 0;

//
// BIOS setup video resolution and text mode.
//
UINT32    mSetupTextModeColumn         = 0;
UINT32    mSetupTextModeRow            = 0;
UINT32    mSetupHorizontalResolution   = 0;
UINT32    mSetupVerticalResolution     = 0;

EFI_FORM_BROWSER2_PROTOCOL          *mFormBrowser2;
MS_ONSCREEN_KEYBOARD_PROTOCOL       *mOSKProtocol;
MS_SIMPLE_WINDOW_MANAGER_PROTOCOL   *mSWMProtocol;

//
// Map Top Menu entries to HII Form IDs.
//
#define UNUSED_INDEX    (UINT16)-1
struct
{
    UINT16          FullMenuIndex;      // Master Frame full menu index.
    UINT16          LimitedMenuIndex;   // Master Frame limited menu index.
    EFI_STRING_ID   MenuString;         // Master Frame menu string.
    EFI_GUID        FormSetGUID;        // HII FormSet GUID.
    EFI_FORM_ID     FormId;             // HII Form ID.

} mFormMap[] =
{
//
//    Index (Full)  Index (Limited)     String                                      Formset Guid                       Form ID
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------
    { 0,            0,                  STRING_TOKEN(STR_MF_MENU_OP_BOOT_SUMMARY),  FRONT_PAGE_CONFIG_FORMSET_GUID,    FRONT_PAGE_FORM_ID_BOOT_SUMMARY     },  // Boot Summary
};

//
// Frontpage form set GUID
//
EFI_GUID gMsFrontPageConfigFormSetGuid = FRONT_PAGE_CONFIG_FORMSET_GUID;

#pragma pack(1)

//
// HII specific Vendor Device Path definition.
//
typedef struct
{
    VENDOR_DEVICE_PATH             VendorDevicePath;
    EFI_DEVICE_PATH_PROTOCOL       End;
} HII_VENDOR_DEVICE_PATH;

#pragma pack()

FRONT_PAGE_CALLBACK_DATA  gFrontPagePrivate = {
    FRONT_PAGE_CALLBACK_DATA_SIGNATURE,
    NULL,
    NULL,
    NULL,
    {
        ExtractConfig,    // Lives in FrontPageConfigAccess.c
        RouteConfig,      // Lives in FrontPageConfigAccess.c
        UiCallback        // Lives in FrontPageUi.c
    }
};

HII_VENDOR_DEVICE_PATH  mFrontPageHiiVendorDevicePath = {
    {
        {
            HARDWARE_DEVICE_PATH,
            HW_VENDOR_DP,
            {
                (UINT8) (sizeof (VENDOR_DEVICE_PATH)),
                (UINT8) ((sizeof (VENDOR_DEVICE_PATH)) >> 8)
            }
        },
        FRONT_PAGE_CONFIG_FORMSET_GUID
    },
    {
        END_DEVICE_PATH_TYPE,
        END_ENTIRE_DEVICE_PATH_SUBTYPE,
        {
            (UINT8) (END_DEVICE_PATH_LENGTH),
            (UINT8) ((END_DEVICE_PATH_LENGTH) >> 8)
        }
    }
};

EFI_STATUS GetAndDisplayBitmap(EFI_GUID *FileGuid, UINTN XCoord, BOOLEAN XCoordAdj);
VOID GenerateBootSummary();
VOID DisplayBootSummary();


/**
  Initialize HII information for the FrontPage


  @param InitializeHiiData    TRUE if HII elements need to be initialized.

  @retval  EFI_SUCCESS        The operation is successful.
  @retval  EFI_DEVICE_ERROR   If the dynamic opcode creation failed.

**/
EFI_STATUS
InitializeFrontPage (
                    IN BOOLEAN    InitializeHiiData
                    )
{
    EFI_STATUS                  Status = EFI_SUCCESS;
    CHAR16                      *StringBuffer;

    StringBuffer = NULL;

    if (InitializeHiiData)
    {

        mCallbackKey  = 0;

        //
        // Locate Hii relative protocols
        //
        Status = gBS->LocateProtocol (&gEfiFormBrowser2ProtocolGuid, NULL, (VOID **) &mFormBrowser2);
        if (EFI_ERROR (Status))
        {
            return Status;
        }
        Status = gBS->LocateProtocol (&gEfiHiiConfigRoutingProtocolGuid, NULL, (VOID **) &mHiiConfigRouting);
        if (EFI_ERROR (Status))
        {
            return Status;
        }

        //
        // Install Device Path Protocol and Config Access protocol to driver handle
        //
        Status = gBS->InstallMultipleProtocolInterfaces (
                                                        &gFrontPagePrivate.DriverHandle,
                                                        &gEfiDevicePathProtocolGuid,
                                                        &mFrontPageHiiVendorDevicePath,
                                                        &gEfiHiiConfigAccessProtocolGuid,
                                                        &gFrontPagePrivate.ConfigAccess,
                                                        NULL
                                                        );

        ASSERT_EFI_ERROR (Status);

        //
        // Publish our HII data
        //
        gFrontPagePrivate.HiiHandle = HiiAddPackages (
                                                     &gMsFrontPageConfigFormSetGuid,
                                                     gFrontPagePrivate.DriverHandle,
                                                     FrontPageVfrBin,
                                                     FrontPageStrings,
                                                     NULL
                                                     );
        if (gFrontPagePrivate.HiiHandle == NULL)
        {
            return EFI_OUT_OF_RESOURCES;
        }
    }

    return Status;
}


/**
  Uninitialize HII information for the FrontPage


  @param InitializeHiiData    TRUE if HII elements need to be initialized.

  @retval  EFI_SUCCESS        The operation is successful.
  @retval  EFI_DEVICE_ERROR   If the dynamic opcode creation failed.

**/
EFI_STATUS
UninitializeFrontPage (
                      VOID
                      )
{
    EFI_STATUS Status = EFI_SUCCESS;

    Status = gBS->UninstallMultipleProtocolInterfaces (
                                                      gFrontPagePrivate.DriverHandle,
                                                      &gEfiDevicePathProtocolGuid,
                                                      &mFrontPageHiiVendorDevicePath,
                                                      &gEfiHiiConfigAccessProtocolGuid,
                                                      &gFrontPagePrivate.ConfigAccess,
                                                      NULL
                                                      );
    ASSERT_EFI_ERROR (Status);

    //
    // Remove our published HII data
    //
    HiiRemovePackages (gFrontPagePrivate.HiiHandle);
    if (gFrontPagePrivate.LanguageToken != NULL)
    {
        FreePool (gFrontPagePrivate.LanguageToken);
        gFrontPagePrivate.LanguageToken = (EFI_STRING_ID *)NULL;
    }

    gBS->CloseEvent(mMasterFrameNotifyEvent);

    // TODO: Destroy the FrontPageContext variable. This way others can know whether we're in Setup.
    // TODO: Enable UserPhysicalPresence:
    //              - Before ReadyToBoot()
    //              - IN Setup

    return Status;
}


/**
  Call the browser and display the front page

  @return   Status code that will be returned by
            EFI_FORM_BROWSER2_PROTOCOL.SendForm ().

**/
EFI_STATUS
CallFrontPage (IN UINT32    FormIndex)
{
    EFI_STATUS                    Status = EFI_SUCCESS;
    UINT16  Count, Index = 0;
    EFI_BROWSER_ACTION_REQUEST    ActionRequest;
    EFI_HII_HANDLE                Handles[4];
    UINTN                         HandleCount;

    Handles[0] = gFrontPagePrivate.HiiHandle;
    HandleCount = 1;

    ActionRequest = EFI_BROWSER_ACTION_REQUEST_NONE;

    //
    // Search through the form mapping table to find the form set GUID and ID corresponding to the selected index.
    //
    for (Count=0 ; Count < (sizeof(mFormMap) / sizeof(mFormMap[0])); Count++)
    {
        Index = ((FALSE == mShowFullMenu) ? mFormMap[Count].LimitedMenuIndex : mFormMap[Count].FullMenuIndex);

        if (Index == FormIndex)
        {
            break;
        }
    }

    //
    // If we didn't find it, exit with an error.
    //
    if (Index != FormIndex)
    {
        Status = EFI_NOT_FOUND;
        goto Exit;
    }

    //
    // Call the browser to display the selected form.
    //
    Status = mFormBrowser2->SendForm (mFormBrowser2,
                                      Handles,
                                      HandleCount,
                                      &mFormMap[Count].FormSetGUID,
                                      mFormMap[Count].FormId,
                                      (EFI_SCREEN_DESCRIPTOR *)NULL,
                                      &ActionRequest
                                     );

    //
    // If the user selected the "Restart now" button to exit the Frontpage, set the exit flag.
    //
    if (ActionRequest == EFI_BROWSER_ACTION_REQUEST_EXIT)
    {
        mTerminateFrontPage = TRUE;
    }

    //
    // Check whether user change any option setting which needs a reset to be effective
    //
    if (ActionRequest == EFI_BROWSER_ACTION_REQUEST_RESET)
    {
        mResetRequired = TRUE;
    }

Exit:

    return Status;
}


/**
  RemoveMenuFromList -
    Updates mFormMap so that the menu item specified by MenuId is omitted.
    The item in question will have FullMenuIndex and LimitedMenuIndex set
    to UNUSED_INDEX, and the other indexes in the list are adjusted accordingly.
*/
VOID RemoveMenuFromList (UINT16 MenuId) {
    BOOLEAN FullMenuRemoved = FALSE;
    BOOLEAN LimitedMenuRemoved = FALSE;
    UINTN   Count;

    UINT16  MenuOptionCount  = (sizeof(mFormMap) / sizeof(mFormMap[0]));

    for (Count=0 ; Count < MenuOptionCount ; Count++)
    {
        if (mFormMap[Count].MenuString == MenuId)
        {
            if (mFormMap[Count].FullMenuIndex != UNUSED_INDEX) {
                FullMenuRemoved = TRUE;
                mFormMap[Count].FullMenuIndex = UNUSED_INDEX;
            }
            if (mFormMap[Count].LimitedMenuIndex != UNUSED_INDEX) {
                LimitedMenuRemoved = TRUE;
                mFormMap[Count].LimitedMenuIndex = UNUSED_INDEX;
            }
        }
        if (FullMenuRemoved && mFormMap[Count].FullMenuIndex != UNUSED_INDEX) {
            mFormMap[Count].FullMenuIndex -= 1;
        }
        if (LimitedMenuRemoved && mFormMap[Count].LimitedMenuIndex != UNUSED_INDEX) {
            mFormMap[Count].LimitedMenuIndex -= 1;
        }
    }
}

/**
  Creates the top-level menu in the Master Frame for selecting amongst the various HII forms.

  NOTE: Selectable menu options are dependent on whether there is a System firmware password and on whether the user knows it.


  @param OrigX          Menu's origin (x-axis).
  @param OrigY          Menu's origin (y-axis).
  @param CellWidth      Menu's width.
  @param CellHeight     Menu's height.
  @param CellTextXOffset   Menu entry text indentation.

  @retval  EFI_SUCCESS        The operation is successful.
  @retval  EFI_DEVICE_ERROR   Failed to create the menu.

**/
static
ListBox*
CreateTopMenu(IN UINT32 OrigX,
              IN UINT32 OrigY,
              IN UINT32 CellWidth,
              IN UINT32 CellHeight,
              IN UINT32 CellTextXOffset)
{
    EFI_FONT_INFO   FontInfo;

    //
    // Create a listbox with menu options.  The contents of the menu depend on whether a system password is
    // set and whether the user entered the password correctly or not.  If the user cancels the password dialog
    // then only a limited menu is available.
    //
    UINT16  Count, Index;
    UINT16  MenuOptionCount  = (sizeof(mFormMap) / sizeof(mFormMap[0]));
    UIT_LB_CELLDATA  *MenuOptions = AllocateZeroPool((MenuOptionCount + 1) * sizeof(UIT_LB_CELLDATA));     // NOTE: the list relies on a zero-initialized list terminator (hence +1).

    ASSERT (NULL != MenuOptions);
    if (NULL == MenuOptions)
    {
        return NULL;
    }

    for (Count=0 ; Count < MenuOptionCount ; Count++)
    {
        Index = ((FALSE == mShowFullMenu) ? mFormMap[Count].LimitedMenuIndex : mFormMap[Count].FullMenuIndex);

        if (UNUSED_INDEX != Index && Index < MenuOptionCount)
        {
            MenuOptions[Index].CellText = HiiGetString (gStringPackHandle, mFormMap[Count].MenuString, NULL);
        }
    }

    //
    // Create the ListBox that encapsulates the top-level menu.
    //
    FontInfo.FontSize    = FP_MFRAME_MENU_TEXT_FONT_HEIGHT;
    FontInfo.FontStyle   = EFI_HII_FONT_STYLE_NORMAL;

    ListBox *TopMenu = new_ListBox(OrigX,
                                   OrigY,
                                   CellWidth,
                                   CellHeight,
                                   0,
                                   &FontInfo,
                                   CellTextXOffset,
                                   &gMsColorTable.MasterFrameCellNormalColor,
                                   &gMsColorTable.MasterFrameCellHoverColor,
                                   &gMsColorTable.MasterFrameCellSelectColor,
                                   &gMsColorTable.MasterFrameCellGrayoutColor,
                                   MenuOptions,
                                   NULL
                                  );

    //
    // Free HII string buffer.
    //
    if (NULL != MenuOptions)
    {
        FreePool(MenuOptions);
    }


    return TopMenu;
}


/**
  Draws the Front Page Title Bar.


  @param None.

  @retval  EFI_SUCCESS        Success.

**/
EFI_STATUS
RenderTitlebar(VOID)
{
    EFI_STATUS                Status = EFI_SUCCESS;
    EFI_FONT_DISPLAY_INFO     StringInfo;
    EFI_IMAGE_OUTPUT          *pBltBuffer;
    EFI_LOADED_IMAGE_PROTOCOL *ImageInfo;
    CHAR8                     Parameter;
    EFI_GUID                  *IconFile = NULL;
    UINTN                     DataSize;
    CHAR8                     RebootReason[MSP_REBOOT_REASON_LENGTH];

    //
    // Draw the titlebar background.
    //
    mGop->Blt(mGop,
              &gMsColorTable.TitleBarBackgroundColor,
              EfiBltVideoFill,
              0,
              0,
              0,
              0,
              mTitleBarWidth,
              mTitleBarHeight,
              mTitleBarWidth * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
             );

    GetAndDisplayBitmap(PcdGetPtr(PcdFpMsLogoFile), (mMasterFrameWidth  * FP_TBAR_MSLOGO_X_PERCENT) / 100, FALSE);   // 2nd param is x coordinate

    Status = gBS->HandleProtocol(mImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&ImageInfo);
    ASSERT_EFI_ERROR(Status);

    if ((ImageInfo->LoadOptionsSize == 0) ||
        (ImageInfo->LoadOptions == NULL)) {
        DataSize = MSP_REBOOT_REASON_LENGTH;
        Status = gRT->GetVariable (
                   MSP_REBOOT_REASON_VAR_NAME,
                   &gFrontPageNVVarGuid,
                   NULL,
                   &DataSize,
                   &RebootReason[0]
                   );
        if (EFI_ERROR(Status)) {
            if (EFI_NOT_FOUND != Status) {
                DEBUG((DEBUG_ERROR,__FUNCTION__ " error reading RebootReason. Code = %r\n",Status));
            }
            Parameter = 'B';
        } else {
            Parameter = RebootReason[0];
            Status = gRT->SetVariable (
                MSP_REBOOT_REASON_VAR_NAME,
                &gFrontPageNVVarGuid,
                EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                0,
                NULL
                );
        }
    } else {
        Parameter = *((CHAR8 *) ImageInfo->LoadOptions);
    }
    DEBUG((DEBUG_ERROR, __FUNCTION__ " Parameter = %c - LoadOption=%p\n",Parameter,ImageInfo->LoadOptions));

    switch (Parameter) {
    case 'V' :  // VOL+
        //IconFile = PcdGetPtr(PcdVolumeUpIndicatorFile);
        break;
    case 'B' : // BOOTFAIL
        //IconFile = PcdGetPtr(PcdBootFailIndicatorFile);
        break;
    case 'O' : // OSIndication
        //IconFile = PcdGetPtr(PcdFirmwareSettingsIndicatorFile);
        break;
    default:
        IconFile = NULL;
    }

    if (NULL != IconFile) {
        GetAndDisplayBitmap(IconFile, (mTitleBarWidth * FP_TBAR_ENTRY_INDICATOR_X_PERCENT) / 100, TRUE);
    }

    //
    // Prepare string blitting buffer.
    //
    pBltBuffer = (EFI_IMAGE_OUTPUT *) AllocateZeroPool (sizeof (EFI_IMAGE_OUTPUT));

    ASSERT (pBltBuffer != NULL);
    if (NULL == pBltBuffer)
    {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    pBltBuffer->Width        = (UINT16)mBootHorizontalResolution;
    pBltBuffer->Height       = (UINT16)mBootVerticalResolution;
    pBltBuffer->Image.Screen = mGop;

    // Select a font (size & style) and font colors.
    //
    StringInfo.FontInfoMask         = EFI_FONT_INFO_ANY_FONT;
    StringInfo.FontInfo.FontSize    = FP_TBAR_TEXT_FONT_HEIGHT;
    StringInfo.FontInfo.FontStyle   = EFI_HII_FONT_STYLE_NORMAL;

    CopyMem (&StringInfo.ForegroundColor, &gMsColorTable.TitleBarTextColor,       sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    CopyMem (&StringInfo.BackgroundColor, &gMsColorTable.TitleBarBackgroundColor, sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));

    //
    // Determine the size the TitleBar text string will occupy on the screen.
    //
    UINT32      MaxDescent;
    SWM_RECT    StringRect;

    GetTextStringBitmapSize (HiiGetString (gStringPackHandle, STRING_TOKEN (STR_FRONT_PAGE_TITLE), NULL),
                             &StringInfo.FontInfo,
                             FALSE,
                             EFI_HII_OUT_FLAG_CLIP |
                             EFI_HII_OUT_FLAG_CLIP_CLEAN_X | EFI_HII_OUT_FLAG_CLIP_CLEAN_Y |
                             EFI_HII_IGNORE_LINE_BREAK,
                             &StringRect,
                             &MaxDescent
                            );

    //
    // Render the string to the screen, vertically centered.
    //
    mSWMProtocol->StringToWindow (mSWMProtocol,
                                  mImageHandle,
                                  EFI_HII_OUT_FLAG_CLIP |
                                  EFI_HII_OUT_FLAG_CLIP_CLEAN_X | EFI_HII_OUT_FLAG_CLIP_CLEAN_Y |
                                  EFI_HII_IGNORE_LINE_BREAK | EFI_HII_DIRECT_TO_SCREEN,
                                  HiiGetString (gStringPackHandle, STRING_TOKEN (STR_FRONT_PAGE_TITLE), NULL),
                                  &StringInfo,
                                  &pBltBuffer,
                                  ((mMasterFrameWidth  * FP_TBAR_TEXT_X_PERCENT) / 100),                     // Based on Master Frame width - so the logo bitmap aligns with the text in the menu.
                                  ((mTitleBarHeight / 2) - ((StringRect.Bottom - StringRect.Top + 1) / 2)),  // Vertically center.
                                  NULL,
                                  NULL,
                                  NULL
                                 );

Exit:

    if (NULL != pBltBuffer)
    {
        FreePool(pBltBuffer);
    }

    return Status;
}


/**
  Draws the FrontPage Master Frame and its Top-Level menu.


  @param None.

  @retval  EFI_SUCCESS        Success.

**/
EFI_STATUS
RenderMasterFrame(VOID)
{
    EFI_STATUS  Status      = EFI_SUCCESS;

    //
    // Draw the master frame background.
    //
    mGop->Blt(mGop,
              &gMsColorTable.MasterFrameBackgroundColor,
              EfiBltVideoFill,
              0,
              0,
              0,
              mTitleBarHeight,
              mMasterFrameWidth,
              mMasterFrameHeight,
              0
             );

    return Status;
}


/**
  Master Frame callback (signalled by Display Engine) for receiving user input data (i.e., key, touch, mouse, etc.).


  @param    None.

  @retval   None.

**/
VOID
EFIAPI
MasterFrameNotifyCallback (IN  EFI_EVENT    Event,
                           IN  VOID         *Context)
{
    mDisplayEngineState.NotificationType = NONE;

    return;
}


static
EFI_STATUS
InitializeFrontPageUI (VOID)
{
    EFI_STATUS  Status = EFI_SUCCESS;


    // Establish initial FrontPage TitleBar and Master Frame dimensions based on the current screen size.
    //
    mTitleBarWidth              = mBootHorizontalResolution;
    mTitleBarHeight             = ((mBootVerticalResolution   * FP_TBAR_HEIGHT_PERCENT)  / 100);
    mMasterFrameWidth           = mBootHorizontalResolution;
    mMasterFrameHeight          = (mBootVerticalResolution - mTitleBarHeight);

    DEBUG((DEBUG_INFO, "INFO [FP]: FP Dimensions: %d, %d, %d, %d, %d, %d\r\n", \
           mBootHorizontalResolution, mBootVerticalResolution, mTitleBarWidth, mTitleBarHeight, mMasterFrameWidth, mMasterFrameHeight));

    //
    // Render the TitleBar at the top of the screen.
    //
    RenderTitlebar();

    //
    // Render the Master Frame and its Top-Level menu contents.
    //
    RenderMasterFrame();

    //
    // Create the Master Frame notification event.  This event is signalled by the display engine to note that
    // there is a user input event outside the form area to consider.
    //
    Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL,
                                 TPL_CALLBACK,
                                 MasterFrameNotifyCallback,
                                 NULL,
                                 &gMsEventMasterFrameNotifyGroupGuid,
                                 &mMasterFrameNotifyEvent
                                );

    if (EFI_SUCCESS != Status)
    {
        DEBUG((DEBUG_ERROR, "ERROR [FP]: Failed to create master frame notification event.  Status = %r\r\n", Status));
        goto Exit;
    }

    // Set shared pointer to user input context structure in a PCD so it can be shared.
    //
    Status = PcdSet64S(PcdCurrentPointerState, (UINT64) (UINTN)&mDisplayEngineState);
    if (EFI_SUCCESS != Status)
    {
        DEBUG((DEBUG_ERROR, "Failed to set the PCD PcdCurrentPointerState::0x%x \n", Status));
        goto Exit;
    }

Exit:

    return Status;
}


/**
  This function is the main entry of the platform setup entry.
  The function will present the main menu of the system setup,
  this is the platform reference part and can be customize.
**/
EFI_STATUS
EFIAPI
UefiMain(IN EFI_HANDLE        ImageHandle,
         IN EFI_SYSTEM_TABLE  *SystemTable
        )
{
    EFI_STATUS  Status  = EFI_SUCCESS;
    UINT32      OSKMode = 0;

    //
    // Delete BootNext if entry to BootManager.
    //
    Status = gRT->SetVariable(
        L"BootNext",
        &gEfiGlobalVariableGuid,
        0,
        0,
        NULL
        );

    //
    // Save image handle for later.
    //
    mImageHandle = ImageHandle;

    //
    // Disable the watchdog timer
    //
    gBS->SetWatchdogTimer (0, 0, 0, (CHAR16 *)NULL);

    mResetRequired = FALSE;

    //
    // Force-connect all controllers.
    //
    EfiBootManagerConnectAll();

    //
    // Set console mode: *not* VGA, no splashscreen logo.
    // Ensure Gop is in Big Display mode prior to accessing GOP.
    //
    MsLogoLibSetConsoleMode (FALSE, FALSE);

    GenerateBootSummary();

    //
    // Check if the frontpage interface should be disabled, which will result in the
    // VM being shutdown.
    //
    if (PcdGetBool(PcdDisableFrontpage))
    {
        DEBUG((DEBUG_INFO, "[FP] PcdDisableFrontpage set, skipping frontpage display.\n"));
        goto Exit;
    }

    //
    // After the console is ready, get current video resolution
    // and text mode before launching setup for the first time.
    //
    Status = gBS->HandleProtocol (gST->ConsoleOutHandle,
                                  &gEfiGraphicsOutputProtocolGuid,
                                  (VOID**)&mGop
                                 );

    if (EFI_ERROR (Status))
    {
        mGop = (EFI_GRAPHICS_OUTPUT_PROTOCOL *)NULL;
        goto Exit;
    }

    //
    // Determine if the Font Protocol is available
    //
    Status = gBS->LocateProtocol (&gEfiHiiFontProtocolGuid,
                                  NULL,
                                  (VOID **)&mFont
                                 );

    ASSERT_EFI_ERROR(Status);
    if (EFI_ERROR(Status))
    {
        mFont = (EFI_HII_FONT_PROTOCOL *)NULL;
        Status = EFI_UNSUPPORTED;
        DEBUG((DEBUG_ERROR, "ERROR [FP]: Failed to find Font protocol (%r).\r\n", Status));
        goto Exit;
    }

    //
    // Locate the Simple Window Manager protocol.
    //
    Status = gBS->LocateProtocol (&gMsSWMProtocolGuid,
                                  NULL,
                                  (VOID **)&mSWMProtocol
                                 );

    if (EFI_ERROR(Status))
    {
        mSWMProtocol = NULL;
        Status = EFI_UNSUPPORTED;
        DEBUG((DEBUG_ERROR, "ERROR [FP]: Failed to find the window manager protocol (%r).\r\n", Status));
        goto Exit;
    }

    //
    // Locate the on-screen keyboard (OSK) protocol.
    //
    Status = gBS->LocateProtocol (&gMsOSKProtocolGuid,
                                  NULL,
                                  (VOID **)&mOSKProtocol
                                 );

    if (EFI_ERROR(Status))
    {
        mOSKProtocol = (MS_ONSCREEN_KEYBOARD_PROTOCOL *)NULL;
        DEBUG((DEBUG_WARN, "WARN [FP]: Failed to find the on-screen keyboard protocol (%r).\r\n", Status));
    }
    else
    {

        //
        // Set default on-screen keyboard size and position.  Disable icon auto-activation (set by BDS) since
        // we'll display the OSK ourselves when appropriate.
        //

        //
        // Disable OSK icon auto-activation and self-refresh, and ensure keyboard is disabled.
        //
        mOSKProtocol->GetKeyboardMode(mOSKProtocol, &OSKMode);
        OSKMode &= ~(OSK_MODE_AUTOENABLEICON | OSK_MODE_SELF_REFRESH);
        mOSKProtocol->ShowKeyboard(mOSKProtocol,FALSE);
        mOSKProtocol->ShowKeyboardIcon(mOSKProtocol,FALSE);
        mOSKProtocol->SetKeyboardMode(mOSKProtocol, OSKMode);

        //
        // Set keyboard size and position (75% of screen width, bottom-right corner, docked).
        //
        mOSKProtocol->SetKeyboardSize(mOSKProtocol, FP_OSK_WIDTH_PERCENT);
        mOSKProtocol->SetKeyboardPosition(mOSKProtocol, BottomRight, Docked);
    }

    if (mGop != NULL)
    {
        //
        // Get current video resolution and text mode.
        //
        mBootHorizontalResolution = mGop->Mode->Info->HorizontalResolution;
        mBootVerticalResolution   = mGop->Mode->Info->VerticalResolution;
    }

    //
    // Ensure screen is clear when switch Console from Graphics mode to Text mode
    //
    gST->ConOut->EnableCursor (gST->ConOut, FALSE);
    gST->ConOut->ClearScreen (gST->ConOut);

    //
    // Initialize the Simple UI ToolKit.
    //
    Status = InitializeUIToolKit(ImageHandle);

    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "ERROR [FP]: Failed to initialize the UI toolkit (%r).\r\n", Status));
        goto Exit;
    }

    //
    // Register Front Page strings with the HII database.
    //
    InitializeStringSupport();

    //
    // Initialize HII data (ex: register strings, etc.).
    //
    InitializeFrontPage(TRUE);

    //
    // Initialize the FrontPage User Interface.
    //
    Status = InitializeFrontPageUI();

    if (EFI_SUCCESS != Status)
    {
        DEBUG((DEBUG_ERROR, "ERROR [FP]: Failed to initialize the FrontPage user interface.  Status = %r\r\n", Status));
        goto Exit;
    }

    DisplayBootSummary();

    //
    // Set the default form ID to show on the canvas.
    //
    mCurrentFormIndex   = 0;
    Status              = EFI_SUCCESS;

    //
    // Display the specified FrontPage form.
    //
    do
    {
        // By default, we'll terminate FrontPage after processing the next Form unless the flag is reset.
        //
        mTerminateFrontPage = TRUE;

        CallFrontPage (mCurrentFormIndex);

    } while (FALSE == mTerminateFrontPage);

    if (mLanguageString != NULL)
    {
        FreePool (mLanguageString);
        mLanguageString = (CHAR8 *)NULL;
    }

    if (mResetRequired)
    {
        ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
    }

    //ProcessBootNext ();

    //
    // Clean-up
    //
    UninitializeFrontPage();

    return Status;

Exit:

    //
    // If unable to enter front page, either hang the system or shutdown. We
    // should have already flushed the reason why we didn't boot to the host
    // event log.
    //
    if (PcdGetBool(PcdDisableFrontpage))
    {
        DEBUG((DEBUG_INFO, "[FP] Configured to shutdown instead of displaying frontpage.\n"));
        ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL);
    }

    while (TRUE)
    {
        CpuSleep();
    }

    CpuDeadLoop();  // Should not get here
    return Status;
}


EFI_STATUS  GetAndDisplayBitmap (EFI_GUID *FileGuid, UINTN XCoord, BOOLEAN XCoordAdj) {
    EFI_STATUS                       Status;
    UINT8                           *BMPData          = NULL;
    UINTN                            BMPDataSize      = 0;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *BltBuffer        = NULL;
    UINTN                            BltBufferSize;
    UINTN                            BitmapHeight;
    UINTN                            BitmapWidth;

    //
    // Get the specified image from FV.
    //
    Status = GetSectionFromAnyFv(FileGuid,
                                 EFI_SECTION_RAW,
                                 0,
                                 (VOID **)&BMPData,
                                 &BMPDataSize
                                );

    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "ERROR [DE]: Failed to find bitmap file (GUID=%g) (%r).\r\n", FileGuid, Status));
        return Status;
    }

    //
    // Convert the bitmap from BMP format to a GOP framebuffer-compatible form.
    //
    Status = TranslateBmpToGopBlt(BMPData,
                                BMPDataSize,
                                &BltBuffer,
                                &BltBufferSize,
                                &BitmapHeight,
                                &BitmapWidth
                               );
    if (EFI_ERROR(Status)) {
        FreePool(BMPData);
        DEBUG((DEBUG_ERROR, "ERROR [DE]: Failed to convert bitmap file to GOP format (%r).\r\n", Status));
        return Status;
    }

    if (XCoordAdj == TRUE)
    {
       XCoord -= BitmapWidth;
    }

    mGop->Blt(mGop,
              BltBuffer,
              EfiBltBufferToVideo,
              0,
              0,
              XCoord,   //Upper Right corner
              ((mTitleBarHeight / 2) - (BitmapHeight / 2)),
              BitmapWidth,
              BitmapHeight,
              0
             );

    FreePool(BMPData);
    FreePool(BltBuffer );
    return Status;
}


VOID GenerateBootSummary() {
    EVENT_CHANNEL_STATISTICS      stats;

    ZeroMem(&stats, sizeof(stats));
    BootDeviceEventStatistics(&stats);

    if (stats.Written == 0)
    {
      //
      // Log a specific event for no boot devices.
      //
      BootDeviceEventStart(NULL, (UINT16)-1, BootDeviceNoDevices, EFI_NOT_FOUND);
      DEBUG((DEBUG_INFO, "[HVBE] Starting new boot event. DP Ptr: 0x%X, OptionNumber: %d\n", NULL, (UINT16)-1));
      BootDeviceEventComplete();
      DEBUG((DEBUG_INFO, "[HVBE] Completing boot event\n"));
    }

    BootDeviceEventFlushLog();
    DEBUG((DEBUG_INFO, "[HVBE] Flushing boot event log\n"));
}

VOID DisplayBootSummary() {

    //
    // Enumerate and display the current boot entries.
    //
    PlatformConsoleInitialize();
    PlatformConsoleBootSummary(STRING_TOKEN(STR_BOOT_RETRY));

    //
    // Clear the event log before trying the boot list again.
    //
    BootDeviceEventResetLog();
    DEBUG((DEBUG_INFO, "[HVBE] Resetting boot event log\n"));
}


EFI_STATUS
SetStringEntry (
   EFI_STRING_ID IdName,
   CHAR16 *StringValue
   )
{
    EFI_STATUS  Status = EFI_SUCCESS;

    if (IdName != HiiSetString(gFrontPagePrivate.HiiHandle, IdName, StringValue, NULL))
    {
       DEBUG((DEBUG_ERROR, __FUNCTION__ " - Failed to set string for %d: %s. \n", IdName, StringValue));
       Status = EFI_NO_MAPPING;
    }

    return Status;
}
