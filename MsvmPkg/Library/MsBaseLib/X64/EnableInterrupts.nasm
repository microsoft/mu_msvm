;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Temporarily forked from upstream because it only builds for Msvc and not Gcc/Clang.
;------------------------------------------------------------------------------

    default rel
    section .text

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; MsEnableInterruptsAndSleep (
;   VOID
;   );
;------------------------------------------------------------------------------
global MsEnableInterruptsAndSleep
MsEnableInterruptsAndSleep:
    sti
    hlt
    ret
