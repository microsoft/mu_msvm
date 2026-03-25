/** @file
  Hypercall library support.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <HvHypercallLibP.h>

EFI_TPL
HvHypercallpDisableInterrupts(
    VOID
    )
{
    // In DXE, raise TPL to high level that will be restored when enable interrupts
    // is called.
    return gBS->RaiseTPL(TPL_HIGH_LEVEL);
}

VOID
HvHypercallpEnableInterrupts(
    EFI_TPL Tpl
    )
{
    gBS->RestoreTPL(Tpl);
}
