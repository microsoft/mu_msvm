/** @file
  This file implements support routines for PEI hypercalls.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <PiPei.h>
#include <HvHypercallLibP.h>
#include <Uefi/UefiSpec.h>
#include "UnreferencedParameter.h"

EFI_TPL
HvHypercallpDisableInterrupts(
    VOID
    )
{
    // In PEI, interrupts are always disabled. This value is passed
    // back to HvHypercallpEnableInterrupts, which ignores it.
    // This function is effectively no-op.
    return TPL_APPLICATION;
}

VOID
HvHypercallpEnableInterrupts(
    EFI_TPL Tpl
    )
{
    // In PEI, interrupts are always disabled. This function is a no-op.
    UNREFERENCED_PARAMETER (Tpl);
}
