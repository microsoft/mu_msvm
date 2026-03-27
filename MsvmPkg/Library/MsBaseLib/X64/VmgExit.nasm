; Copyright (c) Microsoft Corporation.
; SPDX-License-Identifier: BSD-2-Clause-Patent

  default rel
  section .text

;
; UINT64
; MsVmgExit(
;     UINT64 VmgExitRax,
;     UINT64 VmgExitRcx
;     );
;
; Routine Description:
;
;   This routine performs a VMGEXIT.
;
; Arguments:
;
;   (rcx) VmgExitRax: rax for VMGEXIT
;   (rdx) VmgExitRcx: rcx for VMGEXIT
;
; Return Value:
;
;   Rax from VMGEXIT
;
extern AsmVmgExit
global MsVmgExit
MsVmgExit:  mov     rax, rcx            ; rax = VmgExitRax
            mov     rcx, rdx            ; rcx = VmgExitRcx
            jmp     AsmVmgExit          ; BaseLib
