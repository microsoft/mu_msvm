; MS_HYP_CHANGE This entire file.

;------------------------------------------------------------------------------
;   Copyright (c) Microsoft Corporation.
;   SPDX-License-Identifier: BSD-2-Clause-Patent
;
;   Interrupt vector for boot debugger.
;------------------------------------------------------------------------------

extern ExternalVectorTable
ErrorCodeBitmap equ 0x00027d00

    default rel
    section .text

;-----------------------------------------------------------------------------
; AsmIdtVector00
;-----------------------------------------------------------------------------
;
; These are the interrupt vector entry points. An error code and vector
; number is always pushed. Some interrupts come pre-populated with an
; error code and do not need an extra one pushed.
;
; Each is no more than 8 bytes and 8-byte aligned so their addresses be
; computed as an offset from AsmIdtVector00.
;

; align is important before the loop, before the label
; because otherwise the first align within the loop could
; use up as much as 7 bytes on nops.
;
  align 8

global AsmIdtVector00
AsmIdtVector00:

%assign vector 0

%rep 256

  align 8

AsmIdtVector %+ vector:

%if (vector >= 32) || (((1 << vector) & ErrorCodeBitmap) = 0)
  push rax ; Push a dummy error code. Use rax to get a 1-byte instruction to fit.
%endif

%if vector < 128
  push vector
%else
  push vector - 256 ; "push byte vector" warns.
%endif
  jmp CommonInterruptEntryMsvm

AsmIdtVectorEnd %+ vector: ;

%if AsmIdtVectorEnd %+ vector - AsmIdtVector %+ vector > 8
%error AsmIdtVector %+ vector too big
%endif

%assign vector vector + 1
%endrep

;---------------------------------------;
; CommonInterruptEntryMsvm                  ;
;---------------------------------------;

global CommonInterruptEntryMsvm
CommonInterruptEntryMsvm:
    cli

    ;
    ; All interrupt handlers are invoked through interrupt gates, so
    ; IF flag automatically cleared at the entry point
    ;

    push    rbp
    mov     rbp, rsp

    ;
    ; Stack:
    ; +---------------------+ <-- 16-byte aligned ensured by processor
    ; +    Old SS           +
    ; +---------------------+
    ; +    Old RSP          +
    ; +---------------------+
    ; +    RFlags           +
    ; +---------------------+
    ; +    CS               +
    ; +---------------------+
    ; +    RIP              +
    ; +---------------------+
    ; +    Error Code       +
    ; +---------------------+
    ; +    Vector Number    +
    ; +---------------------+
    ; +    RBP              +
    ; +---------------------+ <-- RBP, 16-byte aligned
    ;


    ;
    ; Since here the stack pointer is 16-byte aligned, so
    ; EFI_FX_SAVE_STATE_X64 of EFI_SYSTEM_CONTEXT_x64
    ; is 16-byte aligned
    ;

; UINT64  Rdi, Rsi, Rbp, Rsp, Rbx, Rdx, Rcx, Rax;
; UINT64  R8, R9, R10, R11, R12, R13, R14, R15;
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rax
    push rcx
    push rdx
    push rbx
    push qword [rbp + 48]  ; RSP
    push qword [rbp]       ; RBP
    push rsi
    push rdi

; UINT64  Gs, Fs, Es, Ds, Cs, Ss;  insure high 16 bits of each is zero
    movzx   rax, word [rbp + 56]
    push    rax                      ; for ss
    movzx   rax, word [rbp + 32]
    push    rax                      ; for cs
    mov     rax, ds
    push    rax
    mov     rax, es
    push    rax
    mov     rax, fs
    push    rax
    mov     rax, gs
    push    rax

; UINT64  Rip;
    push    qword [rbp + 24]

; UINT64  Gdtr[2], Idtr[2];
    xor     rax, rax
    push    rax
    push    rax
    sidt    [rsp]
    xchg    rax, [rsp + 2]
    xchg    rax, [rsp]
    xchg    rax, [rsp + 8]

    xor     rax, rax
    push    rax
    push    rax
    sgdt    [rsp]
    xchg    rax, [rsp + 2]
    xchg    rax, [rsp]
    xchg    rax, [rsp + 8]

; UINT64  Ldtr, Tr;
    xor     rax, rax
    str     ax
    push    rax
    sldt    ax
    push    rax

; UINT64  RFlags;
    push    qword [rbp + 40]

; UINT64  Cr0, Cr1, Cr2, Cr3, Cr4, Cr8;
    mov     rax, cr8
    push    rax
    mov     rax, cr4
    or      rax, 208h
    mov     cr4, rax
    push    rax
    mov     rax, cr3
    push    rax
    mov     rax, cr2
    push    rax
    xor     rax, rax
    push    rax
    mov     rax, cr0
    push    rax

; UINT64  Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
    mov     rax, dr7
    push    rax
    mov     rax, dr6
    push    rax
    mov     rax, dr3
    push    rax
    mov     rax, dr2
    push    rax
    mov     rax, dr1
    push    rax
    mov     rax, dr0
    push    rax

; FX_SAVE_STATE_X64 FxSaveState;
    sub rsp, 512
    mov rcx, rsp
    fxsave [rcx]

; Calling convention requires that Direction flag is clear
    cld

; UINT32  ExceptionData;
    push    qword [rbp + 16]

; call into exception handler
    movzx   rcx, byte [rbp + 8]
    lea     rax, ExternalVectorTable
    mov     rax, [rax + rcx * 8]
    test    rax, rax                        ; NULL?
    jz      nonNullValue;

; Prepare parameter and call
    mov     rdx, rsp
    ;
    ; Per calling convention, allocate maximum parameter stack space
    ; and make sure RSP is 16-byte aligned
    ;
    sub     rsp, 4 * 8 + 8
    call    rax
    add     rsp, 4 * 8 + 8

nonNullValue:
    cli ; BUGBUG: This should not be necessary, but it's currently true that interrupt handlers enable interrupts
; UINT64  ExceptionData;
    add     rsp, 8

; FX_SAVE_STATE_X64 FxSaveState;

    mov rcx, rsp
    fxrstor [rcx]
    add rsp, 512

; UINT64  Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
; Skip restoration of DRx registers to support in-circuit emulators
; or debuggers set breakpoint in interrupt/exception context
    add     rsp, 8 * 6

; UINT64  Cr0, Cr1, Cr2, Cr3, Cr4, Cr8;
    pop     rax
    mov     cr0, rax
    add     rsp, 8   ; not for Cr1
    pop     rax
    mov     cr2, rax
    pop     rax
    mov     cr3, rax
    pop     rax
    mov     cr4, rax
    pop     rax
    mov     cr8, rax

; UINT64  RFlags;
    pop     qword [rbp + 40]

; UINT64  Ldtr, Tr;
; UINT64  Gdtr[2], Idtr[2];
; Do not let these registers change.
    add     rsp, 48

; UINT64  Rip;
    pop     qword [rbp + 24]

; UINT64  Gs, Fs, Es, Ds, Cs, Ss;
    pop     rax
    ; mov     gs, rax ; not for gs
    pop     rax
    ; mov     fs, rax ; not for fs
    ; (FS and GS are not used, so not restored.)
    pop     rax
    mov     es, rax
    pop     rax
    mov     ds, rax
    pop     qword [rbp + 32]  ; cs for iretq
    pop     qword [rbp + 56]  ; ss for iretq

; UINT64  Rdi, Rsi, Rbp, Rsp, Rbx, Rdx, Rcx, Rax;
; UINT64  R8, R9, R10, R11, R12, R13, R14, R15;
    pop     rdi
    pop     rsi
    add     rsp, 8           ; not for rbp
    pop     qword [rbp + 48] ; rsp for iretq
    pop     rbx
    pop     rdx
    pop     rcx
    pop     rax
    pop     r8
    pop     r9
    pop     r10
    pop     r11
    pop     r12
    pop     r13
    pop     r14
    pop     r15

    mov     rsp, rbp
    mov     rbp, qword [rbp]
    add     rsp, 24
    iretq
