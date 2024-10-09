/** @file
  User interaction functions for the Hyper-V FrontPage.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _FRONT_PAGE_UI_H_
#define _FRONT_PAGE_UI_H_

#define MAX_STRING_LENGTH               1024


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
  );

STATIC
EFI_STATUS
HandleAssetTagDisplay(
  IN  EFI_IFR_TYPE_VALUE                     *Value,
  OUT EFI_BROWSER_ACTION_REQUEST             *ActionRequest
  );

STATIC
EFI_STATUS
SetSystemPassword (
  IN  EFI_IFR_TYPE_VALUE                     *Value,
  OUT EFI_BROWSER_ACTION_REQUEST             *ActionRequest
  );

/**
  Presents the user with a (hopefully) helpful dialog
  with more info about a particular subject.

  NOTE: Subject is determined by the state of mCallbackKey.

  @retval   EFI_SUCCESS     Message successfully displayed.
  @retval   EFI_NOT_FOUND   mCallbackKey not recognized or string could not be loaded.
  @retval   Others          Return value of mSWMProtocol->MessageBox().

**/
STATIC
EFI_STATUS
HandleInfoPopup(
              IN  EFI_IFR_TYPE_VALUE                     *Value,
              OUT EFI_BROWSER_ACTION_REQUEST             *ActionRequest
              );

STATIC
EFI_STATUS
HandleLanguage(
  IN  EFI_IFR_TYPE_VALUE                     *Value,
  OUT EFI_BROWSER_ACTION_REQUEST             *ActionRequest
  );

STATIC
EFI_STATUS
HandleBootMenu(
  IN  EFI_IFR_TYPE_VALUE                     *Value,
  OUT EFI_BROWSER_ACTION_REQUEST             *ActionRequest
  );


/**
  Handle a request to change the SecureBoot configuration.

  @retval EFI_SUCCESS         Successfully installed SecureBoot default variables.
  @retval Others              Failed to install. SecureBoot is still disabled.

**/
STATIC
EFI_STATUS
HandleSecureBootChange (
  IN  EFI_IFR_TYPE_VALUE                     *Value,
  OUT EFI_BROWSER_ACTION_REQUEST             *ActionRequest
  );


/**
  Determines the current SecureBoot state and updates the status strings accordingly.

  @param[in]  RefreshScreen     BOOLEAN indicating whether to force a screen refresh after updating the strings.

**/
VOID
UpdateSecureBootStatusStrings (
  IN BOOLEAN        RefreshScreen
  );


/**
  Handle a request to change the TPM enablement.

  @retval EFI_SUCCESS         Successfully updated TPM state.
  @retval Others              Failed to update. TPM state remains unchanged.

**/
STATIC
EFI_STATUS
HandleTpmChange (
  IN  EFI_IFR_TYPE_VALUE                     *Value,
  OUT EFI_BROWSER_ACTION_REQUEST             *ActionRequest
  );


/**
  Handle a request to reboot back into FrontPage.

  @retval EFI_SUCCESS

**/
STATIC
EFI_STATUS
HandleRebootToFrontPage (
  IN  EFI_IFR_TYPE_VALUE                     *Value,
  OUT EFI_BROWSER_ACTION_REQUEST             *ActionRequest
  );

#endif // _FRONT_PAGE_UI_H_
