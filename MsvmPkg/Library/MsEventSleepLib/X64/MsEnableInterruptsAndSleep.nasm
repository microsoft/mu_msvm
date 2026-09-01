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
; Enables interrupts and halts the processor in a single, race-free sequence.
; The STI instruction defers interrupt delivery until after the following
; instruction executes, so the subsequent HLT is guaranteed to be entered with
; no window in which a wakeup interrupt could be delivered and lost.
;------------------------------------------------------------------------------

    default rel
    section .text

global MsEnableInterruptsAndSleep
MsEnableInterruptsAndSleep:
    sti
    hlt
    ret
