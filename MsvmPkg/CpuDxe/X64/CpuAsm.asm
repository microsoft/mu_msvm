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

EXTRN ExternalVectorTable:QWORD ; The external interrupt vector table

;
; ErrorCodeBitmap specifies which handlers have error codes associated with them.
; For all other handlers, a dummy error code is pushed.
;

ErrorCodeBitmap EQU 0x00027d00

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

;-----------------------------------------------------------------------------
; AsmIdtVector00
;-----------------------------------------------------------------------------
;
; These are the actual interrupt vector entry points. The macros below ensure
; that the vector always pushes an error code and the vector number to the
; stack before calling into CommonInterruptEntryMsvm. Some interrupts come
; pre-populated with an error code and do not need an extra one pushed.
;
; N.B. Each entry must be no more than 8 bytes and must be 8-byte aligned.
; This allows the C code to compute the address of each vector as an offset
; from AsmIdtVector00.
;

ALIGN   8

PUBLIC  AsmIdtVector00

AsmIdtVector00 LABEL BYTE

vector = 0

REPEAT 256

ALIGN   8

IF (vector GE 32) OR (((1 SHL vector) AND ErrorCodeBitmap) EQ 0)

    push rax           ; Push a dummy error code. Use rax to get a 1-byte instruction to fit.

ENDIF

    push LOW(vector)
    jmp CommonInterruptEntryMsvm

vector = vector + 1

ENDM

;---------------------------------------;
; CommonInterruptEntryMsvm                  ;
;---------------------------------------;
; The follow algorithm is used for the common interrupt routine.

CommonInterruptEntryMsvm PROC PUBLIC FRAME
    .pushframe code
    .allocstack 8
    cli

    ;
    ; All interrupt handlers are invoked through interrupt gates, so
    ; IF flag automatically cleared at the entry point
    ;

    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    .endprolog

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

;; UINT64  Rdi, Rsi, Rbp, Rsp, Rbx, Rdx, Rcx, Rax;
;; UINT64  R8, R9, R10, R11, R12, R13, R14, R15;
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
    push qword ptr [rbp + 48]  ; RSP
    push qword ptr [rbp]       ; RBP
    push rsi
    push rdi

;; UINT64  Gs, Fs, Es, Ds, Cs, Ss;  insure high 16 bits of each is zero
    movzx   rax, word ptr [rbp + 56]
    push    rax                      ; for ss
    movzx   rax, word ptr [rbp + 32]
    push    rax                      ; for cs
    mov     rax, ds
    push    rax
    mov     rax, es
    push    rax
    mov     rax, fs
    push    rax
    mov     rax, gs
    push    rax

;; UINT64  Rip;
    push    qword ptr [rbp + 24]

;; UINT64  Gdtr[2], Idtr[2];
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

;; UINT64  Ldtr, Tr;
    xor     rax, rax
    str     ax
    push    rax
    sldt    ax
    push    rax

;; UINT64  RFlags;
    push    qword ptr [rbp + 40]

;; UINT64  Cr0, Cr1, Cr2, Cr3, Cr4, Cr8;
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

;; UINT64  Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
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

;; FX_SAVE_STATE_X64 FxSaveState;
    sub rsp, 512
    mov rcx, rsp
    fxsave [rcx]

;; UEFI calling convention for x64 requires that Direction flag in EFLAGs is clear
    cld

;; UINT32  ExceptionData;
    push    qword ptr [rbp + 16]

;; call into exception handler
    movzx   rcx, byte ptr [rbp + 8]
    lea     rax, ExternalVectorTable
    mov     rax, [rax + rcx * 8]
    test    rax, rax                        ; NULL?
    jz      nonNullValue;

;; Prepare parameter and call
    mov     rdx, rsp
    ;
    ; Per X64 calling convention, allocate maximum parameter stack space
    ; and make sure RSP is 16-byte aligned
    ;
    sub     rsp, 4 * 8 + 8
    call    rax
    add     rsp, 4 * 8 + 8

nonNullValue:
    cli ; BUGBUG: This should not be necessary, but it's currently true that interrupt handlers enable interrupts
;; UINT64  ExceptionData;
    add     rsp, 8

;; FX_SAVE_STATE_X64 FxSaveState;

    mov rcx, rsp
    fxrstor [rcx]
    add rsp, 512

;; UINT64  Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
;; Skip restoration of DRx registers to support in-circuit emualators
;; or debuggers set breakpoint in interrupt/exception context
    add     rsp, 8 * 6

;; UINT64  Cr0, Cr1, Cr2, Cr3, Cr4, Cr8;
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

;; UINT64  RFlags;
    pop     qword ptr [rbp + 40]

;; UINT64  Ldtr, Tr;
;; UINT64  Gdtr[2], Idtr[2];
;; Best not let anyone mess with these particular registers...
    add     rsp, 48

;; UINT64  Rip;
    pop     qword ptr [rbp + 24]

;; UINT64  Gs, Fs, Es, Ds, Cs, Ss;
    pop     rax
    ; mov     gs, rax ; not for gs
    pop     rax
    ; mov     fs, rax ; not for fs
    ; (X64 will not use fs and gs, so we do not restore it)
    pop     rax
    mov     es, rax
    pop     rax
    mov     ds, rax
    pop     qword ptr [rbp + 32]  ; for cs
    pop     qword ptr [rbp + 56]  ; for ss

;; UINT64  Rdi, Rsi, Rbp, Rsp, Rbx, Rdx, Rcx, Rax;
;; UINT64  R8, R9, R10, R11, R12, R13, R14, R15;
    pop     rdi
    pop     rsi
    add     rsp, 8               ; not for rbp
    pop     qword ptr [rbp + 48] ; for rsp
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
    mov     rbp, qword ptr[rbp]
    add     rsp, 24
    iretq

CommonInterruptEntryMsvm ENDP

END

