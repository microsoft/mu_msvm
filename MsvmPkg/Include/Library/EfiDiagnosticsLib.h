/** @file
  Library APIs for notifying host EFI diagnostics actions.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

#include <Library/DebugLib.h>

//
// Informational messages that are intended to bypass the platform's
// default filtering behavior.
//
#define EFI_DIAGNOSTICS_IMPORTANT_INFO  (DEBUG_WARN | DEBUG_INFO)

VOID
NotifyHostToProcessEfiDiagnostics (
  VOID
  );

VOID
NotifyHostToUpdateEfiDiagnosticsGpa (
  UINT32  EfiDiagnosticsGpa
  );
