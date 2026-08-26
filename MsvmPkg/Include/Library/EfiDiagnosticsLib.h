/** @file
  Library APIs for notifying host EFI diagnostics actions.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

VOID
NotifyHostToProcessEfiDiagnostics (
  VOID
  );

VOID
NotifyHostToUpdateEfiDiagnosticsGpa (
  UINT32  EfiDiagnosticsGpa
  );
