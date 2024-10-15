/** @file
  This file contains architecture specific functions for debugging a failure.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

.code

;++
;
; TripleFault
;
; This function causes a triple fault.
; It places the four parameters in the respective registers prior to the triple fault.
; The VMM logs the VP state which includes these registers.
;
; @param[in] RCX - Rax  - Value to place in rax register.
; @param[in] RDX - Rbx  - Value to place in rbx register.
; @param[in] R8  - Rcx  - Value to place in rcx register.
; @param[in] R9  - Rdx  - Value to place in rdx register.
;
; @return   None  This routine does not return
;
;--
TripleFault PROC PUBLIC

                mov     rax, rcx
                mov     rbx, rdx
                mov     rcx, r8
                mov     rdx, r9

                push    0
                push    0
                lidt    fword ptr [rsp]   ; SET EMPTY IDT
;
; Generate #UD using UD2 instruction
;
EternalUD:
                db      0FH, 0Bh        ; #UD -> #DF -> TRIPLE FAULT

                jmp     EternalUD

TripleFault ENDP

end
