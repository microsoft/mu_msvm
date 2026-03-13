/** @file
  TEMPORARY support to link with ClangPdb:
    stuart_build -c MsvmPkg/PlatformBuild.py BLD_*_LEGACY_DEBUGGER=1 TOOL_CHAIN_TAG=CLANGPDB
  until porting is finished esp. assembly. The result does NOT run.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#if (defined(__clang__) || defined(__GNUC__)) && defined(MDE_CPU_X64)
#include "Base.h"
#include "Library/BdLib/Bd.h"
#include "UnreferencedParameter.h"
void _sev_pvalidate(void) { }
void _sev_vmgexit(void) { }
void _tdx_tdg_mem_page_accept(void) { }
void _tdx_vmcall_map_gpa(void) { }
void _tdx_vmcall_rdmsr(void) { }
void _tdx_vmcall_wrmsr(void) { }
void ApWaitInMailbox(void) { }
void ApWaitInMailboxEnd(void) { }
void BdAlignmentFault(void) { }
void BdAssertionFailureTrap(void) { }
void BdBoundFault(void) { }
void BdBreakpointTrap(void) { }
void EFIAPI BdBreakPointWithStatus(UINT32 Status) { UNREFERENCED_PARAMETER(Status); }
void BdDebugServiceTrap(void) { }
void BdDebugTrapOrFault(void) { }
void BdDivideError(void) { }
void BdDoubleFault(void) { }
void BdFastFailTrap(void) { }
void BdFloatingPointFault(void) { }
void BdGeneralProtectionFault(void) { }
void BdInvalidOpcodeFault(void) { }
void BdInvalidTss(void) { }
void BdMachineCheckAbort(void) { }
void BdNmiInterrupt(void) { }
void BdNpxNotAvailableFault(void) { }
void BdOverflowTrap(void) { }
void BdPageFault(void) { }
void BdUnhandledException(void) { }
void BdXmmException(void) { }
void* EFIAPI DebugService2(void *Param1, void *Param2, UINT32 Service)
    { UNREFERENCED_PARAMETER(Param1 && Param2 && Service); return 0; }
void EFIAPI EfiCaptureContext(PCONTEXT ContextRecord) { UNREFERENCED_PARAMETER(ContextRecord); }
void EFIAPI EnableInterruptsAndSleep(void) { }
void HvHypercallpIssueTdxHypercall(void) { }
void EFIAPI KiRestoreProcessorControlState(PKPROCESSOR_STATE ProcessorState) { UNREFERENCED_PARAMETER(ProcessorState); }
void EFIAPI KiSaveProcessorControlState(PKPROCESSOR_STATE ProcessorState) { UNREFERENCED_PARAMETER(ProcessorState); }
void SpecialGhcbCall(void) { }
void VispCallSvsm(void) { }
#endif
