; MS_HYP_CHANGE This entire file.

;------------------------------------------------------------------------------
;   Copyright (c) Microsoft Corporation.
;   SPDX-License-Identifier: BSD-2-Clause-Patent
;
;   Interrupt vector for boot debugger.
;------------------------------------------------------------------------------

extern ExternalVectorTable
ErrorCodeBitmap equ 0x20227d00

extern CommonInterruptEntryMsvmC

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
    ; +    Old SS           + 56 UINT16
    ; +---------------------+
    ; +    Old RSP          + 48
    ; +---------------------+
    ; +    RFlags           + 40
    ; +---------------------+
    ; +    CS               + 32 UINT16
    ; +---------------------+
    ; +    RIP              + 24
    ; +---------------------+
    ; +    Error Code       + 16
    ; +---------------------+
    ; +    Vector Number    + 8 UINT8
    ; +---------------------+
    ; +    RBP              + 0
    ; +---------------------+ <-- RBP, 16-byte aligned
    ;


    ;
    ; Since here the stack pointer is 16-byte aligned, so
    ; EFI_FX_SAVE_STATE_X64 of EFI_SYSTEM_CONTEXT_X64
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
; UINT64  Rip;
; UINT64  Gdtr[2], Idtr[2];
; UINT64  Ldtr, Tr;
; UINT64  RFlags;
; UINT64  Cr0, Cr1, Cr2, Cr3, Cr4, Cr8;
; UINT64  Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
; FX_SAVE_STATE_X64 FxSaveState;
; UINT32  ExceptionData;
;;
;; CommonInterruptEntryMsvmC handles these
;;
    sub rsp, 8*27 + 512 ; 512 for FX_SAVE_STATE_X64

; FX_SAVE_STATE_X64 FxSaveState;
    mov rcx, rsp
    fxsave [rcx + 8]

; Calling convention requires that Direction flag is clear
    cld

; Prepare parameter and call
    mov     rcx, rbp            ; rcx = Frame
    mov     rdx, rsp            ; rdx = Context
    ;
    ; Per calling convention, allocate maximum parameter stack space
    ; and make sure RSP is 16-byte aligned
    ;
    sub     rsp, 4 * 8 + 8
    call    CommonInterruptEntryMsvmC
    add     rsp, 4 * 8 + 8

    cli ; BUGBUG: This should not be necessary, but it's currently true that interrupt handlers enable interrupts

; UINT64  ExceptionData;

; FX_SAVE_STATE_X64 FxSaveState;

    mov rcx, rsp
    fxrstor [rcx + 8]

; UINT64  Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
; Skip restoration of DRx registers to support in-circuit emulators
; or debuggers set breakpoint in interrupt/exception context
; UINT64  Cr0, Cr1, Cr2, Cr3, Cr4, Cr8;
; UINT64  RFlags; // handled by CommonInterruptEntryMsvmC
; UINT64  Ldtr, Tr; ; Do not let these registers change
; UINT64  Gdtr[2], Idtr[2] ; Do not let these registers change.
; UINT64  Rip; // handled by CommonInterruptEntryMsvmC
; UINT64  Gs;  // skip
; UINT64  Fs;  // skip
; UINT64  Es;  // handled by CommonInterruptEntryMsvmC
; UINT64  Ds;  // handled by CommonInterruptEntryMsvmC
; UINT64  Cs;  // skip
; UINT64  Ss;  // skip
    add rsp, 27*8 + 512

; UINT64  Rdi, Rsi, Rbp, Rsp, Rbx, Rdx, Rcx, Rax;
; UINT64  R8, R9, R10, R11, R12, R13, R14, R15;
    pop     rdi
    pop     rsi
    add     rsp, 16 ; skip rbp and rsp, handled by CommonInterruptEntryMsvmC and below
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
    add     rsp, 24 ; pop Rbp, VectorNumber, ErrorCode
    iretq
