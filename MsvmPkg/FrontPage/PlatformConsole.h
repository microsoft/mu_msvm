/** @file
  Platform Console routines for showing the Hyper-V diagnostic console

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

EFI_STATUS
PlatformConsoleInitialize();

VOID
PlatformConsoleBootSummary(
    IN  EFI_STRING_ID   Id
    );

//
// Platform String Helpers.
//

EFI_STATUS
PlatformStringInitialize();


CHAR16*
PlatformStringById(
    IN  EFI_STRING_ID   Id
    );

UINTN
PlatformStringPrintById(
    IN  EFI_STRING_ID   Id,
    ...
    );

UINTN
PlatformStringPrintSById(
    OUT CHAR16          *StartOfBuffer,
    IN  UINTN           BufferSize,
    IN  EFI_STRING_ID   Id,
    ...
    );

UINTN
PlatformStringPrint(
    IN  CHAR16          *Format,
    ...
    );