/** @file
  This file contains architecture specific functions for debugging a failure.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <IndustryStandard/ArmStdSmc.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseLib.h>
#include <Library/BiosDeviceLib.h>
#include <Library/DebugLib.h>
#include <Library/HvHypercallLib.h>
#include <Hv/HvGuestCpuid.h>
#include <Hv/HvGuestMsr.h>
#include <BiosInterface.h>

#include "CrashLibConstants.h"

VOID
ResetAfterCrash(
    IN  UINTN              ErrorCode,
    IN  UINTN              Param1,
    IN  UINTN              Param2
    )
{
    DEBUG((DEBUG_INFO, "Initiating crash reset...\n"));

    // Is SYSTEM_RESET2 supported?
    UINTN advancedReset = ARM_SMC_ID_PSCI_SYSTEM_RESET2_AARCH64;
    UINTN psciReturn = ArmCallSmc1(ARM_SMC_ID_PSCI_FEATURES, &advancedReset, NULL, NULL);
    if (psciReturn == ARM_SMC_PSCI_RET_SUCCESS)
    {
        UINTN resetType = 0x80000002;   // TODO: Proposed machine check reset
        UINTN cookie = ErrorCode;

        // Send PSCI SYSTEM_RESET2 command.  This should not return on success.
        DEBUG ((DEBUG_INFO, "Issuing PSCI_SYSTEM_RESET2...\n"));
        psciReturn = ArmCallSmc0(advancedReset, &resetType, &cookie, NULL);

        DEBUG ((DEBUG_INFO, "PSCI_SYSTEM_RESET2 not successful. %x\n", psciReturn));
    }
    else
    {
        DEBUG ((DEBUG_INFO, "PSCI_SYSTEM_RESET2 not supported by platform. %x\n", psciReturn));
    }

    DEBUG ((DEBUG_INFO, "Issuing PSCI_SYSTEM_RESET...\n"));
    ArmCallSmc0(ARM_SMC_ID_PSCI_SYSTEM_RESET, NULL, NULL, NULL);
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
    // Set the guest ID before writing crash registers, if necessary.
    //
    HV_REGISTER_VALUE registerValue;
    HV_STATUS status = AsmGetVpRegister(HvRegisterGuestOsId, &registerValue);
    ASSERT(status == HV_STATUS_SUCCESS);

    if (registerValue.Reg64 == 0)
    {
        DEBUG((EFI_D_INFO, "GuestOsId is not set in ReportCrash(); setting now.\n"));

        HV_GUEST_OS_ID_CONTENTS guestOsId;
        guestOsId.AsUINT64 = 0;
        guestOsId.OsId = HvGuestOsMicrosoftUndefined;
        guestOsId.VendorId = HvGuestOsVendorMicrosoft;

        status = AsmSetVpRegister64(HvRegisterGuestOsId, guestOsId.AsUINT64);
        ASSERT(status == HV_STATUS_SUCCESS);
    }
    else
    {
        DEBUG((EFI_D_VERBOSE, "GuestOsId is 0x%llx.\n", (UINTN)registerValue.Reg64));
    }

    //
    // Determine if crash MSRs are supported
    //
    status = AsmGetVpRegister(HvRegisterPrivilegesAndFeaturesInfo, &registerValue);
    ASSERT(status == HV_STATUS_SUCCESS);

    DEBUG((EFI_D_VERBOSE, "HvRegisterFeaturesInfo (low) is 0x%llx.\n", (UINTN)registerValue.Reg128.Low64));
    DEBUG((EFI_D_VERBOSE, "HvRegisterFeaturesInfo (high) is 0x%llx.\n", (UINTN)registerValue.Reg128.High64));

    HV_HYPERVISOR_FEATURES hvFeatures;
    *((PHV_UINT128)&hvFeatures) = registerValue.Reg128;

    if (!hvFeatures.GuestCrashRegsAvailable)
    {
        DEBUG((EFI_D_INFO, "GuestCrashRegister enlightenment is not available.\n"));
        return;
    }

    //
    // N.B. For ARM64, the crash control registers cannot currently be read for capabilities.
    //

    AsmSetVpRegister64(HvRegisterGuestCrashP0, Param0);
    AsmSetVpRegister64(HvRegisterGuestCrashP1, Param1);
    AsmSetVpRegister64(HvRegisterGuestCrashP2, Param2);
    AsmSetVpRegister64(HvRegisterGuestCrashP3, MessageBuffer);
    AsmSetVpRegister64(HvRegisterGuestCrashP4, MessageLength);

    //
    // Write the control register.
    //
    HV_CRASH_CTL_REG_CONTENTS writeCrashCtlReg;
    writeCrashCtlReg.AsUINT64 = 0;
    writeCrashCtlReg.CrashNotify = 1;

    writeCrashCtlReg.CrashMessage = 1;
    writeCrashCtlReg.NoCrashDump = 1;
    writeCrashCtlReg.PreOSId = MSVM_PKG_CRASH_ID;

    AsmSetVpRegister64(HvRegisterGuestCrashCtl, writeCrashCtlReg.AsUINT64);
    DEBUG((EFI_D_INFO, "ReportCrash successful.\n"));

    //
    // Tell the host to collect EFI diagnostics.
    //
    DEBUG((EFI_D_INFO, "Signaling BIOS device to collect EFI diagnostics...\n"));
    WriteBiosDevice(BiosConfigProcessEfiDiagnostics, TRUE);
}
