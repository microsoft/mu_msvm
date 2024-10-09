/** @file
  This instance of the Tcg2PhysicalPresencePromptLib approves all requests without prompting.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Library/BaseLib.h>


/**
  Simple function to inform any callers of whether the lib is ready to present a prompt.
  Since the prompt itself only returns TRUE or FALSE, make sure all other technical requirements
  are out of the way.

  @retval     EFI_SUCCESS       Prompt is ready.
  @retval     EFI_NOT_READY     Prompt does not have sufficient resources at this time.
  @retval     EFI_DEVICE_ERROR  Library failed to prepare resources.

**/
EFI_STATUS
EFIAPI
IsPromptReady (
  VOID
  )
{
  return EFI_SUCCESS;
}  // IsPromptReady()


/**
  This function will take in a prompt string to present to the user in a
  OK/Cancel dialog box and return TRUE if the user actively pressed OK. Returns
  FALSE on Cancel or any errors.

  @param[in]  PromptString  The string that should occupy the body of the prompt.

  @retval     TRUE    User confirmed action.
  @retval     FALSE   User rejected action or a failure occurred.

**/
BOOLEAN
EFIAPI
PromptForUserConfirmation (
  IN  CHAR16    *PromptString
  )
{
  return TRUE;
}  // PromptForUserConfirmation()
