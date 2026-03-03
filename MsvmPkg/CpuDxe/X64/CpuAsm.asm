; MS_HYP_CHANGE This entire file, can go away and be replaced by CpuAsm.nasm.

      TITLE   CpuAsm.asm:
;++
;
; Copyright (c) 2013  Microsoft Corporation
;
; Module Name:
;
;   CpuAsm.asm
;
; Abstract:
;
;   This module implements functions to support the CPU.
;
;--

    .code

;------------------------------------------------------------------------------
; VOID
; SetCodeSelector (
;   UINT16 Selector
;   );
;------------------------------------------------------------------------------
SetCodeSelector PROC PUBLIC
    sub     rsp, 0x10
    lea     rax, setCodeSelectorLongJump
    mov     [rsp], rax
    mov     [rsp+4], cx
    jmp     fword ptr [rsp]
setCodeSelectorLongJump:
    add     rsp, 0x10
    ret
SetCodeSelector ENDP

;------------------------------------------------------------------------------
; VOID
; SetDataSelectors (
;   UINT16 Selector
;   );
;------------------------------------------------------------------------------
SetDataSelectors PROC PUBLIC
    mov     ss, cx
    mov     ds, cx
    mov     es, cx
    mov     fs, cx
    mov     gs, cx
    ret
SetDataSelectors ENDP

END

