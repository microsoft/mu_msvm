;------------------------------------------------------------------------------
;
; Copyright (c) Microsoft Corporation.
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; VOID
; EFIAPI
; MsEnableInterruptsAndSleep (
;   VOID
;   );
;
; Unmasks IRQs and waits for an interrupt in a single, race-free sequence.
; WFI completes as soon as a wakeup event is pending, so unmasking immediately
; before WFI ensures no wakeup interrupt can be delivered and lost in between.
;------------------------------------------------------------------------------

  EXPORT MsEnableInterruptsAndSleep
  AREA MsEventSleep, CODE, READONLY

MsEnableInterruptsAndSleep
    msr   daifclr, #2      // Unmask IRQ (clear PSTATE.I)
    wfi                    // Wait for interrupt
    ret

  END
