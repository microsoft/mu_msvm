/** @file
  Asm implementations of SNP instructions that will become compiler
  intrinsics.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

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

        jnc     @f                      ; if NC, operation completed as expected
        inc     r8d                     ; indicate presence of carry flag
@@:
        mov     [r9], rax               ; save error code
        mov     eax, r8d                ; return value of EFLAGS.CF
        ret

        LEAF_END _sev_pvalidate, _TEXT$00

;*++
;
; UINT64
; SpecialGhcbCall(
;     UINT64 GhcbValue
;     );
;
; Routine Description:
;
;   This routine temporarily updates the GHCB MSR to a specified value and
;   executes VMGEXIT.
;
; Arguments:
;
;   GhcbValue (rcx) - Supplies the GHCB MSR value to use for the call.
;
;   Optional value in rdx supplies the value to move into rax at the time of
;       VMGEXIT.
;
;   Optional value in r8 supplies the value to move into rcx at the time of
;       VMGEXIT.
;
; Return Value:
;
;   Value of GHCB MSR following the exit.
;
;   Register r9 will hold what was in rax upon return from the VMGEXIT.
;
;--*

        NESTED_ENTRY SpecialGhcbCall, _TEXT$00

        push    r15                     ; save registers
        push    r14
        rex_push_eflags                 ; save current eflags

        END_PROLOGUE

        cli                             ; disable interrupts to prevent conflicting GHCB MSR use

        mov     r9, rcx                 ; copy input parameters
        mov     r10, rdx
        mov     ecx, 0C0010130h         ; load GHCB MSR index
        mov     r11d, ecx               ; capture MSR index
        rdmsr                           ; capture current GHCB value
        mov     r15d, eax               ; copy low 32 bits
        mov     r14d, edx               ; copy high 32 bits
        mov     eax, r9d                ; copy low 32 bits of new GHCB value
        mov     rdx, r9                 ; copy full 64-bit value
        shr     rdx, 32                 ; capture high 32 bits of GHCB value
        wrmsr                           ; update GHCB value
        mov     rax, r10                ; capture RAX value
        mov     rcx, r8                 ; capture RCX value

        db      0f3h                    ; rep prefix for VMGEXIT
        vmmcall                         ;

        mov     r9, rax                 ; capture RAX value
        mov     ecx, r11d               ; reload MSR index
        rdmsr                           ; read current GHCB value
        shl     rdx, 32                 ; shift high 32 bits
        or      rax, rdx                ; combine with low 32 bits
        mov     r8, rax                 ; save 64-bit output value
        mov     eax, r15d               ; copy low 32 bits of previous value
        mov     edx, r14d               ; copy high 32 bits of previous value
        wrmsr                           ; restore previous GHCB value

        mov     rax, r8                 ; copy output value to return register

        popfq                           ; restore saved eflags

        BEGIN_EPILOGUE

        pop     r14
        pop     r15

        ret

        NESTED_END SpecialGhcbCall, _TEXT$00

;*++
;
; UINT64
; VispCallSvsm(
;     UINT64 RequestCode,
;     UINT64 Parameter
;     );
;
; Routine Description:
;
;   This routine invokes the SVSM to execute the specified request.
;
; Arguments:
;
;   RequestCode (rcx) - Supplies the request code.
;
;   Parameter (rdx) - Supplies the call parameter.
;
; Return Value:
;
;   Return code from SVSM.
;
;--*

        NESTED_ENTRY VispCallSvsm, _TEXT$00

        alloc_stack 28h                 ; create stack frame

        END_PROLOGUE

        mov     r8, rdx                 ; move parameter value into correct register
        mov     rdx, rcx                ; move request code into correct register
        mov     ecx, 016h               ; load GHCB value to run VMPL zero
        call    SpecialGhcbCall         ; invoke VMPL zero
        mov     rax, r9                 ; capture SVSM exit value
        add     rsp, 28h                ; restore stack pointer

        BEGIN_EPILOGUE

        ret

        NESTED_END VispCallSvsm, _TEXT$00

        end
