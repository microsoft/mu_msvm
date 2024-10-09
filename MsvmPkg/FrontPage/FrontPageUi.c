/** @file
  User interaction functions for the Hyper-V FrontPage.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "FrontPage.h"        // TODO: Perhaps wrap the keys in their own .h file.
#include "FrontPageUi.h"

#include <PiDxe.h>          // This has to be here so Protocol/FirmwareVolume2.h doesn't throw errors.

#include <Guid/FrontPageEventDataStruct.h>
#include <Guid/GlobalVariable.h>
#include <Guid/MdeModuleHii.h>

#include <Protocol/OnScreenKeyboard.h>
#include <Protocol/SimpleWindowManager.h>
#include <Protocol/FirmwareVolume2.h>

#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/HiiLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

extern BOOLEAN                              mResetRequired;


/**
  This function processes the results of changes in configuration.

  @param This            Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param Action          Specifies the type of action taken by the browser.
  @param QuestionId      A unique value which is sent to the original exporting driver
                         so that it can identify the type of data to expect.
  @param Type            The type of value for the question.
  @param Value           A pointer to the data being sent to the original exporting driver.
  @param ActionRequest   On return, points to the action requested by the callback function.

  @retval  EFI_SUCCESS           The callback successfully handled the action.
  @retval  EFI_OUT_OF_RESOURCES  Not enough storage is available to hold the variable and its data.
  @retval  EFI_DEVICE_ERROR      The variable could not be saved.
  @retval  EFI_UNSUPPORTED       The specified Action is not supported by the callback.

**/
EFI_STATUS
EFIAPI
UiCallback (
           IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL   *This,
           IN  EFI_BROWSER_ACTION                     Action,
           IN  EFI_QUESTION_ID                        QuestionId,
           IN  UINT8                                  Type,
           IN  EFI_IFR_TYPE_VALUE                     *Value,
           OUT EFI_BROWSER_ACTION_REQUEST             *ActionRequest
           )
{
    EFI_STATUS    Status = EFI_SUCCESS;

    DEBUG ((DEBUG_INFO, "FrontPage:UiCallback() - Question ID=0x%08x Type=0x%04x Action=0x%04x ShortValue=0x%02x\n", QuestionId, Type, Action, *(UINT8*)Value));

    //
    // Sanitize input values.
    //
    if (Value == NULL || ActionRequest == NULL)
    {
        DEBUG ((DEBUG_INFO, "FrontPage:UiCallback - Bailing from invalid input.\n"));
        return EFI_INVALID_PARAMETER;
    }

    //
    // Filter responses.
    // NOTE: For now, let's only consider elements that have CHANGED.
    //
    if (Action != EFI_BROWSER_ACTION_CHANGED)
    {
        DEBUG ((DEBUG_INFO, "FrontPage:UiCallback - Bailing from unimportant input.\n"));
        return EFI_UNSUPPORTED;
    }

    //
    // Set a default action request.
    //
    *ActionRequest = EFI_BROWSER_ACTION_REQUEST_NONE;

    //
    // Handle the specific callback.
    // We'll record the callback event as mCallbackKey so that other processes can make decisions
    // on how we exited the run loop (if that occurs).
    //
    mCallbackKey = QuestionId;
    switch (mCallbackKey)
    {
    //
    // FRONT_PAGE_KEY_CONTINUE is the "Exit Menu" option.
    //
    case FRONT_PAGE_ACTION_CONTINUE:
        *ActionRequest = EFI_BROWSER_ACTION_REQUEST_SUBMIT;
        //
        // mCallbackKey set to FRONT_PAGE_KEY_CONTINUE will cause the main run loop to exit
        // once the form browser exits.
        //
        break;
    case FRONT_PAGE_ACTION_EXIT_FRONTPAGE:
        *ActionRequest = EFI_BROWSER_ACTION_REQUEST_EXIT;
        break;
    case FRONT_PAGE_ACTION_REBOOT_TO_FRONTPAGE:
        Status = HandleRebootToFrontPage( Value, ActionRequest );
        break;
    default:
        DEBUG ((DEBUG_INFO, "FrontPage:UiCallback - Unknown event passed.\n"));
        Status = EFI_UNSUPPORTED;
        mCallbackKey = 0;
        break;
    }

    return Status;
}


/**
  Handle a request to reboot back into FrontPage.

  @retval EFI_SUCCESS

**/
STATIC
EFI_STATUS
HandleRebootToFrontPage (
  IN  EFI_IFR_TYPE_VALUE                     *Value,
  OUT EFI_BROWSER_ACTION_REQUEST             *ActionRequest
  )
{
    EFI_STATUS  Status;
    UINT32      Attributes = EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE;
    UINTN       DataSize;
    UINT8       OsIndications[sizeof( UINT64 )] = {0};      // For UINT32 Compatibility

    DEBUG(( DEBUG_INFO, "INFO [SFP] %a()\n", __FUNCTION__ ));

    //
    // Step 1: Read the current OS indications variable.
    //
    DataSize = sizeof( OsIndications );
    Status = gRT->GetVariable( L"OsIndications",
                               &gEfiGlobalVariableGuid,
                               &Attributes,
                               &DataSize,
                               OsIndications );
    DEBUG(( DEBUG_VERBOSE, "VERBOSE [SFP] %a - GetVariable(OsIndications) = %r\n", __FUNCTION__, Status ));

    //
    // Step 2: Update OS indications variable to enable the boot to FrontPage.
    //
    if (!EFI_ERROR( Status ) || Status == EFI_NOT_FOUND)
    {
        OsIndications[0] |= EFI_OS_INDICATIONS_BOOT_TO_FW_UI;   // Flag is located in the lowest byte.
        Status = gRT->SetVariable( L"OsIndications",
                                   &gEfiGlobalVariableGuid,
                                   Attributes,
                                   DataSize,
                                   OsIndications );
        DEBUG(( DEBUG_VERBOSE, "VERBOSE [SFP] %a - SetVariable(OsIndications) = %r\n", __FUNCTION__, Status ));
    }

    //
    // Step 3: Reboot!
    //
    if (!EFI_ERROR( Status ))
    {
        DEBUG(( DEBUG_INFO, "INFO [SFP] %a - Requesting reboot...\n", __FUNCTION__ ));
        *ActionRequest = EFI_BROWSER_ACTION_REQUEST_EXIT;
        mResetRequired = TRUE;
    }

    return Status;
}
