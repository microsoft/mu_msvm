/** @file
  Low level hypercalls.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Hv/HvGuestCpuid.h>
#include <Hv/HvGuestHypercall.h>
#include <Hv/HvStatus.h>

#if defined(MDE_CPU_AARCH64)

HV_HYPERCALL_OUTPUT
AsmHyperCall(
    IN  HV_HYPERCALL_INPUT  InputControl,
        UINT64              InputPhysicalAddress,
        UINT64              OutputPhysicalAddress
    );

HV_STATUS
AsmGetVpRegister64(
        UINT32  RegisterIndex,
    OUT UINT64  *RegisterBuffer
    );

HV_STATUS
AsmGetVpRegister(
        UINT32              RegisterIndex,
    OUT HV_REGISTER_VALUE  *RegisterBuffer
    );

HV_STATUS
AsmSetVpRegister64(
    UINT32  RegisterIndex,
    UINT64  RegisterBuffer
    );

#endif

typedef struct _EFI_SYNIC_COMPONENT
{
    VOID *Page;
    BOOLEAN DisableOnCleanup;

} EFI_SYNIC_COMPONENT, *PEFI_SYNIC_COMPONENT;

typedef struct _HV_HYPERCALL_CONTEXT
{
    BOOLEAN Connected;
    BOOLEAN IsTdx;
    VOID* Ghcb;
    EFI_SYNIC_COMPONENT EventFlagsPage;
    EFI_SYNIC_COMPONENT MessagePage;

#if defined(MDE_CPU_X64)

    UINT64 SharedGpaBoundary;
    UINT64 CanonicalizationMask;
    VOID* HypercallPage;
    BOOLEAN ParavisorPresent;

#endif
} HV_HYPERCALL_CONTEXT, *PHV_HYPERCALL_CONTEXT;

#if defined(MDE_CPU_X64)

VOID
HvHypercallConnect(
    IN  VOID                    *HypercallPage,
        UINT32                  IsolationType,
        BOOLEAN                 ParavisorPresent,
    OUT HV_HYPERCALL_CONTEXT    *Context
    );

#elif defined(MDE_CPU_AARCH64)

VOID
HvHypercallConnect(
    OUT HV_HYPERCALL_CONTEXT *Context
    );

#endif

VOID
HvHypercallDisconnect(
    IN OUT  HV_HYPERCALL_CONTEXT *Context
    );

HV_STATUS
HvHypercallIssue(
    IN              HV_HYPERCALL_CONTEXT    *Context,
                    HV_CALL_CODE            CallCode,
                    BOOLEAN                 Fast,
                    UINT32                  CountOfElements,
                    UINT64                  FirstRegister,
                    UINT64                  SecondRegister,
    OUT OPTIONAL    UINT32                  *ElementsProcessed
    );

UINT64
HvHypercallGetVpRegister64Self(
    IN  HV_HYPERCALL_CONTEXT    *Context,
        HV_REGISTER_NAME        RegisterName
    );

VOID
HvHypercallSetVpRegister64Self(
    IN  HV_HYPERCALL_CONTEXT    *Context,
        HV_REGISTER_NAME        RegisterName,
        UINT64                  RegisterValue
    );

VOID
HvHypercallRequestHypervisorCpuid(
    IN  HV_HYPERCALL_CONTEXT    *Context,
        UINT32                  CpuidLeaf,
    OUT PHV_CPUID_RESULT        CpuidResult
    );
