/** @file
  Library implementation for host EFI diagnostics notifications.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BiosDeviceLib.h>
#include <BiosInterface.h>

/**
  Notify the host that EFI diagnostics should be processed.

  Writes BiosConfigProcessEfiDiagnostics with TRUE to the BIOS device.
**/
VOID
NotifyHostToProcessEfiDiagnostics (
  VOID
  )
{
  WriteBiosDevice (BiosConfigProcessEfiDiagnostics, TRUE);
}

/**
  Notify the host of the EFI diagnostics buffer GPA.

  @param[in]  EfiDiagnosticsGpa  Guest physical address of the diagnostics buffer.
**/
VOID
NotifyHostToUpdateEfiDiagnosticsGpa (
  UINT32 EfiDiagnosticsGpa
  )
{
  WriteBiosDevice (BiosConfigSetEfiDiagnosticsGpa, EfiDiagnosticsGpa);
}
