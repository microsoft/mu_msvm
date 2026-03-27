; @file
; Asm implementations of SNP instructions.
;
; Copyright (c) Microsoft Corporation.
; SPDX-License-Identifier: BSD-2-Clause-Patent

include macamd64.inc

;*++
;
; UINT64
; _sev_pvalidate(
;     IN  VOID   *Address,
;         UINT32 PageSize,
;         UINT32 Validate,
;     OUT UINT64 *ErrorCode
;     );
;
; Routine Description:
;
;   This routine executes the PVALIDATE instruction.
;
; Arguments:
;
;   Address (rcx) - Supplies the linear address argument (RAX).
;
;   PageSize (edx) - Supplies the page size argument (RCX).
;
;   Validate (r8d) - Supplies the validate argument (RDX).
;
;   ErrorCode (r9) - Supplies a pointer to a variable that will receive the
;                    error code following the instruction (RAX).
;
; Return Value:
;
;   Value of EFLAGS.CF following the instruction.
;
;--*

        LEAF_ENTRY _sev_pvalidate, _TEXT$00

        mov     rax, rcx                ; move arguments to correct registers.
        mov     ecx, edx
        mov     edx, r8d
        xor     r8d, r8d                ; load zero
        db      0f2h                    ; PVALIDATE instruction (F2 0F 01 FF)
        db      00fh
        db      001h
        db      0ffh

        jnc     .f                      ; if NC, operation completed as expected
        inc     r8d                     ; indicate presence of carry flag
.f:
        mov     [r9], rax               ; save error code
        mov     eax, r8d                ; return value of EFLAGS.CF
        ret

        LEAF_END _sev_pvalidate, _TEXT$00
        end
