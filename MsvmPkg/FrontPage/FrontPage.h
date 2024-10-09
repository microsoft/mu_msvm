/** @file
  FrontPage routines to handle the callbacks and browser calls

  Copyright (c) 2004 - 2012, Intel Corporation. All rights reserved.
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _FRONT_PAGE_H_
#define _FRONT_PAGE_H_

#include "String.h"

#include <Protocol/FormBrowser2.h>
#include <Protocol/HiiConfigAccess.h>
#include "FrontPageVfr.h"  // all shared VFR / C constants here.
#include <Library/HiiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/HiiConfigRouting.h>

//
// These are the VFR compiler generated data representing our VFR data.
//
extern UINT8  FrontPageVfrBin[];

extern EFI_FORM_BROWSER2_PROTOCOL      *mFormBrowser2;

extern UINTN    mCallbackKey;

//
// Boot video resolution and text mode.
//
extern UINT32    mBootHorizontalResolution;
extern UINT32    mBootVerticalResolution;
extern UINT32    mBootTextModeColumn;
extern UINT32    mBootTextModeRow;
//
// BIOS setup video resolution and text mode.
//
extern UINT32    mSetupTextModeColumn;
extern UINT32    mSetupTextModeRow;
extern UINT32    mSetupHorizontalResolution;
extern UINT32    mSetupVerticalResolution;


#define ONE_SECOND  10000000



#define FRONT_PAGE_CALLBACK_DATA_SIGNATURE  SIGNATURE_32 ('F', 'P', 'C', 'B')

typedef struct {
  UINTN                           Signature;

  //
  // HII relative handles
  //
  EFI_HII_HANDLE                  HiiHandle;
  EFI_HANDLE                      DriverHandle;
  EFI_STRING_ID                   *LanguageToken;

  //
  // Produced protocols
  //
  EFI_HII_CONFIG_ACCESS_PROTOCOL  ConfigAccess;
} FRONT_PAGE_CALLBACK_DATA;

#define EFI_FP_CALLBACK_DATA_FROM_THIS(a) \
  CR (a, \
      FRONT_PAGE_CALLBACK_DATA, \
      ConfigAccess, \
      FRONT_PAGE_CALLBACK_DATA_SIGNATURE \
      )

extern FRONT_PAGE_CALLBACK_DATA  gFrontPagePrivate;

extern EFI_HII_CONFIG_ROUTING_PROTOCOL   *mHiiConfigRouting;

/**
  Initialize HII information for the FrontPage


  @param InitializeHiiData    TRUE if HII elements need to be initialized.

  @retval  EFI_SUCCESS        The operation is successful.
  @retval  EFI_DEVICE_ERROR   If the dynamic opcode creation failed.

**/
EFI_STATUS
InitializeFrontPage (
  IN BOOLEAN    InitializeHiiData
  );


#endif // _FRONT_PAGE_H_

