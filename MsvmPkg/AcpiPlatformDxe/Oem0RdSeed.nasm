; @file
; X64 RDSEED support for the OEM0 ACPI table.
;
; Copyright (c) Microsoft Corporation.
; SPDX-License-Identifier: BSD-2-Clause-Patent
;

    default rel
    section .text

; BOOLEAN
; AsmRdSeed64 (
;   OUT UINT64 *Value
;   );
global AsmRdSeed64
AsmRdSeed64:
    rdseed  rax
    jnc     RdSeedUnavailable

    mov     [rcx], rax
    mov     eax, 1
    ret

RdSeedUnavailable:
    xor     eax, eax
    ret
