/** @file
  This file contains architecture specific functions for debugging a failure.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Hv/HvGuestCpuid.h>
#include <Hv/HvGuestMsr.h>

#include "CrashLibConstants.h"

VOID
TripleFault(
    IN  UINTN              ErrorCode,
    IN  UINTN              Param1,
    IN  UINTN              Param2,
    IN  UINTN              Param3
);

VOID
ResetAfterCrash(
    IN  UINTN              ErrorCode,
    IN  UINTN              Param1,
    IN  UINTN              Param2
    )
{
    DEBUG((EFI_D_ERROR, "Initiating crash reset...\n"));
    TripleFault(ErrorCode, Param1, Param2, 0);
}

VOID
ReportCrash(
    IN  UINTN              Param0,
    IN  UINTN              Param1,
    IN  UINTN              Param2,
    IN  UINTN              MessageBuffer,
    IN  UINTN              MessageLength
    )
{
    //
    // Determine if crash MSRs are supported
    //
    HV_CPUID_RESULT cpuidResult;
    __cpuid(cpuidResult.AsUINT32, HvCpuIdFunctionMsHvFeatures);

    if (!cpuidResult.MsHvFeatures.GuestCrashRegsAvailable)
    {
        DEBUG((EFI_D_INFO, "GuestCrashRegister enlightenment is not available.\n"));
        return;
    }

    //
    // Read the crash control register to check whether crash notification from
    // firmware is supported.
    //
    HV_CRASH_CTL_REG_CONTENTS readCrashCtlReg;
    readCrashCtlReg.AsUINT64 = AsmReadMsr64(HvSyntheticMsrCrashCtl);

    if (readCrashCtlReg.CrashNotify == 0)
    {
        DEBUG((EFI_D_INFO, "CrashNotify is not available.\n"));
        return;
    }

    //
    // Write the information registers.
    //
    AsmWriteMsr64(HvSyntheticMsrCrashP0, Param0);
    AsmWriteMsr64(HvSyntheticMsrCrashP1, Param1);
    AsmWriteMsr64(HvSyntheticMsrCrashP2, Param2);
    AsmWriteMsr64(HvSyntheticMsrCrashP3, MessageBuffer);
    AsmWriteMsr64(HvSyntheticMsrCrashP4, MessageLength);

    //
    // Write the control register.
    //
    HV_CRASH_CTL_REG_CONTENTS writeCrashCtlReg;
    writeCrashCtlReg.AsUINT64 = 0;
    writeCrashCtlReg.CrashNotify = 1;

    if (readCrashCtlReg.CrashMessage)
    {
        writeCrashCtlReg.CrashMessage = 1;
    }

    if (readCrashCtlReg.NoCrashDump)
    {
        // UEFI does not currently support crash dump
        writeCrashCtlReg.NoCrashDump = 1;
    }

    if (readCrashCtlReg.PreOSId < MSVM_PKG_CRASH_ID)
    {
        DEBUG((EFI_D_INFO, "PreOSId is not available.\n"));
    }
    else
    {
        writeCrashCtlReg.PreOSId = MSVM_PKG_CRASH_ID;
    }

    AsmWriteMsr64(HvSyntheticMsrCrashCtl, writeCrashCtlReg.AsUINT64);
    DEBUG((EFI_D_INFO, "ReportCrash successful.\n"));
}
