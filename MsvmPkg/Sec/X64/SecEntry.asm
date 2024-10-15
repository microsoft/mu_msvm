/** @file
  SecEntry

  Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>

.code

EXTERN SecCoreStartupWithStack:PROC
EXTERN SecProcessVirtualCommunicationException:PROC
EXTERN SecProcessVirtualizationException:PROC

;
; SecCore Entry Point
;
; Processor is in flat protected mode
;
; @param[in]  RAX   Initial value of the EAX register (BIST: Built-in Self Test)
; @param[in]  DI    'BP': boot-strap processor, or 'AP': application processor
; @param[in]  RBP   Pointer to the start of the Boot Firmware Volume
; @param[in]  R8, R9, R10, R11  Hypervisor isolation configuration CPUID leaf
; @param[in]  R12   Pointer to UEFI IGVM config header if required
;
; @return     None  This routine does not return
;
_ModuleEntryPoint PROC PUBLIC

    ;
    ; Load temporary stack top at very low memory.  The C code
    ; can reload to a better address.
    ;
    mov     rsp, BASE_512KB
    nop

    ;
    ; Setup parameters and call SecCoreStartupWithStack
    ;   rcx: BootFirmwareVolumePtr
    ;   rdx: TopOfCurrentStack
    ;   r8:  IsolationConfiguration
    ;   r9:  IgvmConfigHeader
    ;
    mov     rcx, rbp
    mov     rdx, rsp
    sub     rsp, 30h
    mov     20h[rsp], r8d
    mov     24h[rsp], r9d
    mov     28h[rsp], r10d
    mov     2ch[rsp], r11d
    lea     r8, 20h[rsp]
    mov     r9, r12
    call    SecCoreStartupWithStack

    ;
    ; If SecCoreStartupWithStack returns, then startup has failed.  Invoke a
    ; fatal exception.
    ;
    int     3

_ModuleEntryPoint ENDP

;
; #VC exception handler
;

TrapFrame   struct      ; keep in sync with SecP.h
            P1Home      dq ?
            P2Home      dq ?
            P3Home      dq ?
            P4Home      dq ?
            SavedXmm0   oword ?
            SavedXmm1   oword ?
            SavedXmm2   oword ?
            SavedXmm3   oword ?
            SavedXmm4   oword ?
            SavedXmm5   oword ?
            SavedRax    dq ?
            SavedRcx    dq ?
            SavedRdx    dq ?
            SavedRbx    dq ?
            SavedR8     dq ?
            SavedR9     dq ?
            SavedR10    dq ?
            SavedR11    dq ?
            TrErrorCode dq ?
TrapFrame   ends

BEGIN_TRAP_HANDLER macro ErrorCode

ifb <ErrorCode>

            sub     rsp, (sizeof TrapFrame) ; allocate trap frame storage

else

            sub     rsp, TrapFrame.TrErrorCode ; allocate trap frame storage

endif

            mov     TrapFrame.SavedRax[rsp], rax ; save volatile registers
            mov     TrapFrame.SavedRcx[rsp], rcx
            mov     TrapFrame.SavedRdx[rsp], rdx
            mov     TrapFrame.SavedR8[rsp], r8
            mov     TrapFrame.SavedR9[rsp], r9
            mov     TrapFrame.SavedR10[rsp], r10
            mov     TrapFrame.SavedR11[rsp], r11
            movdqa  TrapFrame.SavedXmm0[rsp], xmm0
            movdqa  TrapFrame.SavedXmm1[rsp], xmm1
            movdqa  TrapFrame.SavedXmm2[rsp], xmm2
            movdqa  TrapFrame.SavedXmm3[rsp], xmm3
            movdqa  TrapFrame.SavedXmm4[rsp], xmm4
            movdqa  TrapFrame.SavedXmm5[rsp], xmm5
            mov     TrapFrame.SavedRbx[rsp], rbx ; non-volatile but required by trap handler

            endm

END_TRAP_HANDLER macro

            movdqa  xmm0, TrapFrame.SavedXmm0[rsp] ; restore volatile registers
            movdqa  xmm1, TrapFrame.SavedXmm1[rsp]
            movdqa  xmm2, TrapFrame.SavedXmm2[rsp]
            movdqa  xmm3, TrapFrame.SavedXmm3[rsp]
            movdqa  xmm4, TrapFrame.SavedXmm4[rsp]
            movdqa  xmm5, TrapFrame.SavedXmm5[rsp]
            mov     r11, TrapFrame.SavedR11[rsp]
            mov     r10, TrapFrame.SavedR10[rsp]
            mov     r9, TrapFrame.SavedR9[rsp]
            mov     r8, TrapFrame.SavedR8[rsp]
            mov     rbx, TrapFrame.SavedRbx[rsp]
            mov     rdx, TrapFrame.SavedRdx[rsp]
            mov     rcx, TrapFrame.SavedRcx[rsp]
            mov     rax, TrapFrame.SavedRax[rsp]
            add     rsp, (sizeof TrapFrame) ; deallocate trap frame
            iretq

            endm

SecVirtualCommunicationExceptionHandler PROC PUBLIC

            BEGIN_TRAP_HANDLER <ErrorCode>

            mov     rcx, rsp                ; load address of trap frame
            call    SecProcessVirtualCommunicationException ; attempt to handle
            test    al, al                  ; check return value
            jnz     @f                      ; if nz, successful
            int     3                       ; force unrecoverable exception
@@:

            END_TRAP_HANDLER

SecVirtualCommunicationExceptionHandler ENDP

;
; SecVmgexit
;
; Executes the VMGEXIT instruction
;

SecVmgexit PROC PUBLIC

            db      0f3h                ; VMGEXIT prefix
            vmmcall
            ret

SecVmgexit ENDP

;
; #VE exception handler
;

SecVirtualizationExceptionHandler PROC PUBLIC

            BEGIN_TRAP_HANDLER <>

            mov     rcx, rsp                ; load address of trap frame
            call    SecProcessVirtualizationException ; attempt to handle
            test    al, al                  ; check return value
            jnz     @f                      ; if nz, successful
            int     3                       ; force unrecoverable exception
@@:

            END_TRAP_HANDLER

SecVirtualizationExceptionHandler ENDP

;
; SecGetTdxVeInfo
;
; Retrieves VE_INFO for the most recent #VE exception.
;
; @param[out] RCX The VE info buffer to populate.
;

SecGetTdxVeInfo PROC PUBLIC

            mov     r11, rcx            ; preserve output argument
            mov     eax, 3              ; TDG.VP.VEINFO.GET call code
            db      66h                 ; TDCALL instruction sequence
            db      0fh
            db      01h
            db      0cch
            mov     [r11], rcx          ; save output information
            mov     8[r11], rdx         ;
            mov     10h[r11], r8        ;
            mov     18h[r11], r9        ;
            mov     20h[r11], r10       ;
            ret

SecGetTdxVeInfo ENDP

;
; SecGetTdInfo
;
; Retrieves the TD information from TDG.VP.INFO.
;
; @param[out] RCX Variable to receive the shared GPA width.
;

SecGetTdInfo PROC PUBLIC

            push    r12                 ; preserve non-volatile register
            mov     r12, rcx            ; capture argument register
            mov     eax, 1              ; TDG.VP.INFO call code
            db      66h                 ; TDCALL instruction sequence
            db      0fh
            db      01h
            db      0cch
            and     ecx, 3fh            ; mask GPA width
            mov     [r12], ecx          ; save output parameter
            pop     r12                 ; restore non-volatile register
            ret

SecGetTdInfo ENDP

;
; SecTdCallRdmsr
;
; Reads a virtual MSR using the TDCALL GHCI interface.
;
; @param[in] RCX  The MSR to read.
;
; @return         MSR value.
;

SecTdCallRdmsr PROC PUBLIC

            push    r12                 ; preserve non-volatile register
            mov     r12d, ecx           ; capture argument register
            xor     eax, eax            ; TDG.VP.VMCALL call code
            mov     ecx, 1C00h          ; pass R10-R12
            xor     r10d, r10d          ; indicate GHCI call
            mov     r11d, 31            ; indicate RDMSR
            db      66h                 ; TDCALL instruction sequence
            db      0fh
            db      01h
            db      0cch
            test    rax, rax            ; verify successful call
            jz      Stcr20              ; if z, successful
Stcr10:     int     3                   ; failure
Stcr20:     test    r10, r10            ; verify successful read
            jnz     Stcr10              ; if nz, not successful
            mov     rax, r11            ; capture return value
            pop     r12                 ; restore non-volatile register
            ret

SecTdCallRdmsr ENDP

;
; SecTdCallWrmsr
;
; Writes a virtual MSR using the TDCALL GHCI interface.
;
; @param[in] RCX  The MSR to write.
; @param[in] RDX  MSR value.
;

SecTdCallWrmsr PROC PUBLIC

            push    r12                 ; preserve non-volatile registers
            push    r13                 ;
            mov     r12d, ecx           ; capture argument register
            mov     r13, rdx            ; capture MSR value
            xor     eax, eax            ; TDG.VP.VMCALL call code
            mov     ecx, 3C00h          ; pass R10-R13
            xor     r10d, r10d          ; indicate GHCI call
            mov     r11d, 32            ; indicate WRMSR
            db      66h                 ; TDCALL instruction sequence
            db      0fh
            db      01h
            db      0cch
            test    rax, rax            ; verify successful call
            jz      Stcw20              ; if z, successful
Stcw10:     int     3                   ; failure
Stcw20:     test    r10, r10            ; verify successful write
            jnz     Stcw10              ; if nz, not successful
            pop     r13                 ; restore non-volatile registers
            pop     r12                 ;
            ret

SecTdCallWrmsr ENDP

;
; SecTdCallHlt
;
; Issues an enlightened HLT
;

SecTdCallHlt PROC PUBLIC

            push    r12                 ; preserve non-volatile registers
            xor     r12d, r12d          ; clear the interrupt blocked flag
            xor     eax, eax            ; TDG.VP.VMCALL call code
            mov     ecx, 1C00h          ; pass R10-R12
            xor     r10d, r10d          ; indicate GHCI call
            mov     r11d, 12            ; indicate HLT
            db      66h                 ; TDCALL instruction sequence
            db      0fh
            db      01h
            db      0cch
            test    rax, rax            ; verify successful call
            jz      Stch20              ; if z, successful
Stch10:     int     3                   ; failure
Stch20:     test    r10, r10            ; verify successful HLT
            jnz     Stch10              ; if nz, not successful
            pop     r12                 ;
            ret

SecTdCallHlt ENDP

;
; SecTdCallReadIoPort
;
; Reads an IO port using the TD call GHCI interface.
;
; @param[in] ECX - The port to read.
; @param[in] EDX - The access size.
; @return        - Value read.
;

SecTdCallReadIoPort PROC PUBLIC

            push    r12                 ; preserve non-volatile registers
            push    r13                 ; preserve non-volatile registers
            push    r14                 ; preserve non-volatile registers
            mov     r12d, edx           ; pass access size
            xor     r13d, r13d          ; indicate a read
            mov     r14d, ecx           ; pass port number
            xor     eax, eax            ; TDG.VP.VMCALL call code
            mov     ecx, 7C00h          ; pass R10-R14
            xor     r10d, r10d          ; indicate GHCI call
            mov     r11d, 30            ; indicate IO instruction
            db      66h                 ; TDCALL instruction sequence
            db      0fh
            db      01h
            db      0cch
            test    rax, rax            ; verify successful call
            mov     eax, 0ffffffffh     ; load eax with the default read value for failures.
            jnz     exitIoRead          ; if nz, not successful
            test    r10, r10            ; verify vmcall return code
            jnz     exitIoRead          ; if nz, not successful
            mov     eax, r11d           ; get return value
exitIoRead: pop     r14
            pop     r13
            pop     r12
            ret

SecTdCallReadIoPort ENDP

;
; SecTdCallWriteIoPort
;
; Writes an IO port using the TD call GHCI interface.
;
; @param[in] ECX - The port to write.
; @param[in] EDX - The access size.
; @param[in] R8d - Value to write.
;

SecTdCallWriteIoPort PROC PUBLIC

            push    r12                 ; preserve non-volatile registers
            push    r13                 ; preserve non-volatile registers
            push    r14                 ; preserve non-volatile registers
            push    r15                 ; preserve non-volatile registers
            mov     r12d, edx           ; pass access size
            mov     r13d, 1             ; indicate a write
            mov     r14d, ecx           ; pass port number
            mov     r15d, r8d           ; pass value
            xor     eax, eax            ; TDG.VP.VMCALL call code
            mov     ecx, 0fC00h         ; pass R10-R15
            xor     r10d, r10d          ; indicate GHCI call
            mov     r11d, 30            ; indicate IO instruction
            db      66h                 ; TDCALL instruction sequence
            db      0fh
            db      01h
            db      0cch

    ; ignore failures and move on.

            pop     r15
            pop     r14
            pop     r13
            pop     r12
            ret

SecTdCallWriteIoPort ENDP

;
; MulDiv64
;
; Multiply two 64-bit numbers and divide by a third.
;
; @param[in] RCX  Value
; @param[in] RDX  Multiplier
; @param[in] R8   Divisor
;
; @return         Result
;

MulDiv64 PROC PUBLIC

            mov     rax, rdx            ; move multiplier to correct register
            mul     rcx                 ; multiply into RDX:RAX
            div     r8                  ; divide RDX:RAX by R8
            ret                         ; result is in rax

MulDiv64 ENDP

END
