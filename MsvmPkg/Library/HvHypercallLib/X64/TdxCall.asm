/** @file
  Asm implementations of TDX calls.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

include macamd64.inc

;*++
;
; UINT64
; _tdx_vmcall_rdmsr(
;     _In_ UINT32 MsrIndex
;     );
;
; Routine Description:
;
;   This routine performs a TDVMCALL(RDMSR).
;
; Arguments:
;
;   MsrIndex (rcx) - Supplies the MSR index.
;
; Return Value:
;
;   Value of the MSR.
;
;--*

        NESTED_ENTRY _tdx_vmcall_rdmsr, _TEXT$00

        push_reg r12                    ; save non-volatile register used by call
        alloc_stack 20h                 ; allocate frame

        END_PROLOGUE

        mov     r12, rcx                ; load parameters into the correct argument registers
        xor     r10d, r10d              ; indicate standard TDVMCALL function
        mov     r11d, 1fh               ; load RDMSR call code
        mov     ecx, 1C00h              ; pass R10-R12
        xor     eax, eax                ; TDVMCALL call code

        db      66h                     ; TDCALL instruction sequence
        db      0fh
        db      01h
        db      0cch

        mov     rax, r11                ; return MSR value

        add     rsp, 20h                ; deallocate stack frame

        BEGIN_EPILOGUE

        pop     r12
        ret

        NESTED_END _tdx_vmcall_rdmsr, _TEXT$00

;*++
;
; VOID
; _tdx_vmcall_wrmsr(
;     UINT32 MsrIndex,
;     UINT64 MsrValue
;     );
;
; Routine Description:
;
;   This routine performs a TDVMCALL(WRMSR).
;
; Arguments:
;
;   MsrIndex (rcx) - Supplies the MSR index.
;
;   MsrValue (rdx) - Supplies the MSR value.
;
; Return Value:
;
;   None.
;
;--*

        NESTED_ENTRY _tdx_vmcall_wrmsr, _TEXT$00

        push_reg r12                    ; save non-volatile registers used by call
        push_reg r13
        alloc_stack 28h                 ; allocate frame

        END_PROLOGUE

        mov     r12, rcx                ; load parameters into the correct argument registers
        mov     r13, rdx
        xor     r10d, r10d              ; indicate standard TDVMCALL function
        mov     r11d, 20h               ; load WRMSR call code
        mov     ecx, 3C00h              ; pass R10-R13
        xor     eax, eax                ; TDVMCALL call code

        db      66h                     ; TDCALL instruction sequence
        db      0fh
        db      01h
        db      0cch

        add     rsp, 28h                ; deallocate stack frame

        BEGIN_EPILOGUE

        pop     r13
        pop     r12
        ret

        NESTED_END _tdx_vmcall_wrmsr, _TEXT$00

        subttl  "HvHypercallpIssueTdxHypercall"
;++
;
; void
; HV_HYPERCALL_OUTPUT
; HvHypercallpIssueTdxHypercall(
;     IN    HV_HYPERCALL_INPUT InputControl,
;           UINT64 InputPhysicalAddress,
;           UINT64 OutputPhysicalAddress
;     )
;
; Routine Description:
;
; Hypercall routine for TDX-based systems.
;
;--

        LEAF_ENTRY HvHypercallpIssueTdxHypercall, _TEXT$00

        mov     r10, rcx                ; load call code into call code register
        xor     r11, r11                ; clear return code register
        mov     ecx, 0d04h              ; pass rdx, r8, r10, r11 to host
        xor     eax, eax                ; load TDG.VP.VMCALL code
        db      066h                    ; TDCALL instruction sequence
        db      00fh                    ;
        db      001h                    ;
        db      0cch                    ;
        mov     rax, r11                ; capture return code
        ret

        LEAF_END HvHypercallpIssueTdxHypercall, _TEXT$00

        end
