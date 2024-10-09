/** @file
  Private include for the hypercall support routine library.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Base.h>

#include <Library/HvHypercallLib.h>

#if defined(MDE_CPU_X64)

HV_STATUS
HvHypercallpIssueGhcbHypercall(
    IN              HV_HYPERCALL_CONTEXT    *Context,
                    HV_CALL_CODE            CallCode,
    IN OPTIONAL     VOID                    *InputPage,
                    UINT32                  CountOfElements,
    OUT OPTIONAL    UINT32                  *ElementsProcessed
    );

VOID
_tdx_vmcall_wrmsr(
    UINT32 MsrIndex,
    UINT64 MsrValue
    );

UINT64
_tdx_vmcall_rdmsr(
    UINT32 MsrIndex
    );

/// Functions that enable and disable interrupts, that are implemented based
/// on the environment the library is built for.

VOID
HvHypercallpDisableInterrupts(
    VOID
    );

VOID
HvHypercallpEnableInterrupts(
    VOID
    );

#endif
