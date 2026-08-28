#------------------------------------------------------------------------------
#
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
# VOID
# EFIAPI
# MsEnableInterruptsAndSleep (
#   VOID
#   );
#
# Unmasks IRQs and waits for an interrupt in a single, race-free sequence.
# WFI completes as soon as a wakeup event is pending, so unmasking immediately
# before WFI ensures no wakeup interrupt can be delivered and lost in between.
#------------------------------------------------------------------------------

#include <AArch64/AArch64.h>

.text
.p2align 2
GCC_ASM_EXPORT(MsEnableInterruptsAndSleep)

ASM_PFX(MsEnableInterruptsAndSleep):
  AARCH64_BTI(c)
  msr   daifclr, #2      // Unmask IRQ (clear PSTATE.I)
  wfi                    // Wait for interrupt
  ret
