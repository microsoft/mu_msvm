/** @file
  This file implements support routines for GHCB-based calls.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/UefiBootServicesTableLib.h>

EFI_TPL
GhcbpDisableInterrupts(
    VOID
    )
{
    // In DXE, raise TPL to high level that will be restored when enable interrupts
    // is called.
    return gBS->RaiseTPL(TPL_HIGH_LEVEL);
}

VOID
GhcbpEnableInterrupts(
    EFI_TPL tpl
    )
{
    gBS->RestoreTPL(tpl);
}
