/** @file
  HiiConfigAccess definitions for Hyper-V FrontPage.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "FrontPageConfigAccess.h"
#include "FrontPage.h"
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Library/PrintLib.h>   // Should be temporary.

#include <Guid/DxePhaseVariables.h>

/**
  Quick helper function to see if ReadyToBoot has already been signalled.

  @retval     TRUE    ReadyToBoot has been signalled.
  @retval     FALSE   Otherwise...

**/
STATIC
BOOLEAN
IsPostReadyToBoot (
  VOID
  )
{
  EFI_STATUS        Status;
  UINT32            Attributes;
  PHASE_INDICATOR   Indicator;
  UINTN             Size = sizeof( Indicator );
  static BOOLEAN    Result, Initialized = FALSE;

  if (!Initialized)
  {
    Status = gRT->GetVariable( READY_TO_BOOT_INDICATOR_VAR_NAME,
                               &gMsDxePhaseVariablesGuid,
                               &Attributes,
                               &Size,
                               &Indicator );
    Result = (!EFI_ERROR( Status ) && Attributes == READY_TO_BOOT_INDICATOR_VAR_ATTR);
    Initialized = TRUE;
  }

  return Result;
} // IsPostReadyToBoot()


/**
  This function allows a caller to extract the current configuration for one
  or more named elements from the target driver.


  @param This            Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param Request         A null-terminated Unicode string in <ConfigRequest> format.
  @param Progress        On return, points to a character in the Request string.
                         Points to the string's null terminator if request was successful.
                         Points to the most recent '&' before the first failing name/value
                         pair (or the beginning of the string if the failure is in the
                         first name/value pair) if the request was not successful.
  @param Results         A null-terminated Unicode string in <ConfigAltResp> format which
                         has all values filled in for the names in the Request string.
                         This string will be allocated by the called function.

  @retval  EFI_SUCCESS            The Results is filled with the requested values.
  @retval  EFI_OUT_OF_RESOURCES   Not enough memory to store the results.
  @retval  EFI_INVALID_PARAMETER  Request is illegal syntax, or unknown name.
  @retval  EFI_NOT_FOUND          Routing data doesn't match any storage in this driver.

**/
EFI_STATUS
EFIAPI
ExtractConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL   *This,
  IN  CONST EFI_STRING                       Request,
  OUT EFI_STRING                             *Progress,
  OUT EFI_STRING                             *Results
  )
{
  return EFI_NOT_FOUND;
}

/**
  This function processes the results of changes in configuration.


  @param This            Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param Configuration   A null-terminated Unicode string in <ConfigResp> format.
  @param Progress        A pointer to a string filled in with the offset of the most
                         recent '&' before the first failing name/value pair (or the
                         beginning of the string if the failure is in the first
                         name/value pair), or the terminating NULL if all was successful.

  @retval  EFI_SUCCESS            The results are processed successfully.
  @retval  EFI_INVALID_PARAMETER  Configuration is NULL.
  @retval  EFI_NOT_FOUND          Routing data doesn't match any storage in this driver.

**/
EFI_STATUS
EFIAPI
RouteConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL   *This,
  IN  CONST EFI_STRING                       Configuration,
  OUT EFI_STRING                             *Progress
  )
{
  return EFI_NOT_FOUND;
}
