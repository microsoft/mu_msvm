/** @file
  TEMPORARY support to link with ClangPdb (stuart_build -c MsvmPkg\PlatformBuild.py BLD_*_LEGACY_DEBUGGER=1 TOOL_CHAIN_TAG=CLANGPDB),
  until porting is finished esp. assembly. The result does NOT run.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#if (defined(__clang__) || defined(__GNUC__)) && defined(MDE_CPU_X64)
void _ModuleEntryPoint(void) { }
void _sev_pvalidate(void) { }
void _sev_vmgexit(void) { }
void _tdx_tdg_mem_page_accept(void) { }
void _tdx_vmcall_map_gpa(void) { }
void _tdx_vmcall_rdmsr(void) { }
void _tdx_vmcall_wrmsr(void) { }
void ApWaitInMailbox(void) { }
void ApWaitInMailboxEnd(void) { }
void BdArchDestroy(void) { }
void BdArchInitialize(void) { }
void BdArchStart(void) { }
void BdBreakPointWithStatus(void) { }
void CpPortInit(void) { }
void CpPortRead(void) { }
void CpPortWrite(void) { }
void DebugPrint(void) { }
void DebugService2(void) { }
void EnableInterruptsAndSleep(void) { }
void HvHypercallpIssueTdxHypercall(void) { }
void InitializeCrashDumpAgent(void) { }
void SetCodeSelector(void) { }
void SetDataSelectors(void) { }
void SpecialGhcbCall(void) { }
void VispCallSvsm(void) { }
#endif
