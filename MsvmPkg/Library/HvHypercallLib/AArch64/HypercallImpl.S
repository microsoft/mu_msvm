// @file
// This module implements the hypercall API.
//
// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
#include <AsmMacroLib.h>

#define ARM64_HYPERCALL_IMM16 1

//
// HV_HYPERCALL_OUTPUT
// AsmArm64HyperCall(
//     IN           HV_HYPERCALL_INPUT InputControl,
//     IN OPTIONAL  UINT64             InputPhysicalAddress
//     IN OPTIONAL  UINT64             OutputPhysicalAddress
// );
//
ASM_FUNC(AsmHyperCall)
    hvc #ARM64_HYPERCALL_IMM16
    ret

//
// HV_STATUS
// AsmGetVpRegister64(
//     IN  UINT32      RegisterIndex,
//     OUT UINT64      *RegisterBuffer
// );
//
ASM_FUNC(AsmGetVpRegister64)

    mov w3, w0                      // IN  InputList[0].RegisterName0, padding, clear high bits
    stp x0, x1, [sp, #-0x10]!       // Push RegisterIndex, RegisterBuffer onto stack

    ldr x0, =0x0000000100010050     // hypercall input (fast rep call 1 argument HvCallGetVpRegisters)
    ldr x1, =0xFFFFFFFFFFFFFFFF     // partition id (self)
    mov x2, #0xFFFFFFFE             // vp index (self), padding
    mov x4, #0                      // OUT padding to 16 byte alignment
    mov x5, #0                      // OUT InputList[0].RegisterValue.Low
    mov x6, #0                      // OUT InputList[0].RegisterValue.High

    hvc #ARM64_HYPERCALL_IMM16

    ldp x2, x1, [sp], #0x10         // Pop RegisterBuffer into x1
    str x15, [x1]                   // Store result in x15 into *RegisterBuffer
    and x0, x0, #0xFFFF

    ret

//
// HV_STATUS
// AsmGetVpRegister(
//     IN  UINT32               RegisterIndex,
//     OUT HV_REGISTER_VALUE    *RegisterBuffer
// );
//
ASM_FUNC(AsmGetVpRegister)

    mov w3, w0                      // IN  InputList[0].RegisterName0, padding, clear high bits
    stp x0, x1, [sp, #-0x10]!       // Push RegisterIndex, RegisterBuffer onto stack

    ldr x0, =0x0000000100010050     // hypercall input (fast rep call 1 argument HvCallGetVpRegisters)
    ldr x1, =0xFFFFFFFFFFFFFFFF     // partition id (self)
    mov x2, #0xFFFFFFFE             // vp index (self), padding
    mov x4, #0                      // OUT padding to 16 byte alignment
    mov x5, #0                      // OUT InputList[0].RegisterValue.Low
    mov x6, #0                      // OUT InputList[0].RegisterValue.High

    hvc #ARM64_HYPERCALL_IMM16

    ldp x2, x1, [sp], #0x10         // Pop RegisterBuffer into x1
    stp x15, x16, [x1], #0x10       // Store 128 bits into *RegisterBuffer
    and x0, x0, #0xFFFF

    ret

//
// HV_STATUS
// AsmSetVpRegister64(
//     IN  UINT32      RegisterIndex,
//     IN  UINT64      RegisterBuffer
// );
//
ASM_FUNC(AsmSetVpRegister64)

    mov w3, w0                      // IN InputList[0].RegisterName0, padding, clear high bits
    mov x5, x1                      // IN InputList[0].RegisterValue.Low

    ldr x0, =0x0000000100010051     // hypercall input  (fast rep call 1 argument HvCallSetVpRegisters)
    ldr x1, =0xFFFFFFFFFFFFFFFF     // partition id (self)
    mov x2, #0xFFFFFFFE             // vp index (self), padding
    mov x4, #0                      // IN padding to 16 byte alignment
    mov x6, #0                      // IN InputList[0].RegisterValue.High

    hvc #ARM64_HYPERCALL_IMM16

    and x0, x0, #0xFFFF

    ret
