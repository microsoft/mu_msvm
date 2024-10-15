/** @file
  Asm implementations of TDX instructions that will become compiler
  intrinsics.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

include macamd64.inc

;*++
;
; UINT64
; _tdx_tdg_mem_page_accept(
;     IN TDX_ACCEPT_GPA AcceptGpa
;     );
;
; Routine Description:
;
;   This routine executes TDG.MEM.PAGE.ACCEPT.
;
; Arguments:
;
;   AcceptGpa (rcx) - Supplies the packed GPA information.
;
; Return Value:
;
;   TDX error code.
;
;--*

        LEAF_ENTRY _tdx_tdg_mem_page_accept, _TEXT$00

        mov     eax, 6                  ; TDG.MEM.PAGE.ACCEPT call code
        db      66h                     ; TDCALL instruction sequence
        db      0fh
        db      01h
        db      0cch
        ret

        LEAF_END _tdx_tdg_mem_page_accept, _TEXT$00

;*++
;
; BOOLEAN
; _tdx_vmcall_map_gpa(
;                   UINT64 Gpa,
;                   UINT64 Size,
;     OUT OPTIONAL  UINT64 *FailedGpa
;     );
;
; Routine Description:
;
;   This routine executes TDG.VP.VMCALL<MapGpa>.
;
; Arguments:
;
;   Gpa (rcx) - Supplies the first GPA to convert.
;
;   Size (rdx) - Supplies the number of bytes in the GPA range to convert.
;
;   FailedGpa (r8) - Supplies an optional pointer to a variable to receive the
;                    first GPA that failed conversion.
;
; Return Value:
;
;   Result of call.
;
;--*

        NESTED_ENTRY _tdx_vmcall_map_gpa, _TEXT$00

        push_reg    r13
        push_reg    r12

        END_PROLOGUE

        xor     r10d, r10d              ; indicate standard VMCALL
        mov     r11d, 10001h            ; MapGpa call code
        mov     r12, rcx                ; load base GPA
        mov     r13, rdx                ; load size
        mov     ecx, 3C00h              ; pass r10-r13
        xor     eax, eax                ; load TDVMCALL call code
        db      66h                     ; TDCALL instruction sequence
        db      0fh
        db      01h
        db      0cch
        test    r10, r10                ; check if successful
        mov     rax, r10                ; load return code
        jz      @f                      ; if z, no error
        test    r8, r8                  ; check if output argument present
        jz      @f                      ; if z, not present
        mov     [r8], r11               ; save failed GPA if argument present
@@:

        BEGIN_EPILOGUE

        pop     r12
        pop     r13

        ret

        NESTED_END _tdx_vmcall_map_gpa, _TEXT$00

        end
