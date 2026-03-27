; @file
; Asm implementations of TDX calls.
;
; Copyright (c) Microsoft Corporation.
; SPDX-License-Identifier: BSD-2-Clause-Patent

include macamd64.inc

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
