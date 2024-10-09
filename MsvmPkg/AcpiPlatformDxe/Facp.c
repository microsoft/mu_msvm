/** @file
  This module is responsible for runtime initialization of the FACP acpi table.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include "AcpiPlatform.h"
#include <IsolationTypes.h>
#include <Library/BaseMemoryLib.h>

#include <BiosInterface.h>

EFI_STATUS
FacpInitializeTable(
    IN OUT  EFI_ACPI_DESCRIPTION_HEADER* Facp
    )
/*++

Routine Description:

    Initializes the Facp table.

Arguments:

    Facp - The FACP Table, expressed as an EFI_ACPI_DESCRIPTION_HEADER*.

Return Value:

    EFI_SUCCESS

--*/
{
    EFI_ACPI_6_2_FIXED_ACPI_DESCRIPTION_TABLE *facp = (EFI_ACPI_6_2_FIXED_ACPI_DESCRIPTION_TABLE *)Facp;

    //
    // Get configuration to determine if headless.
    //
    UINT32 consoleMode = PcdGet8(PcdConsoleMode);

    //
    // Set headless bit if console mode is not default (no video/kbd present)
    //
    if (consoleMode != ConfigLibConsoleModeDefault)
    {
        facp->Flags |= EFI_ACPI_6_2_HEADLESS;
    }

    //
    // Set the hypervisor vendor identity to MsHyperV
    //
    CopyMem(&facp->HypervisorVendorIdentity, "MsHyperV", 8);

    if (PcdGetBool(PcdLowPowerS0IdleEnabled))
    {
        //
        // Set EFI_ACPI_6_2_LOW_POWER_S0_IDLE_CAPABLE flag.
        // Pending investigation, EFI_ACPI_6_2_LOW_POWER_S0_IDLE_CAPABLE causes negative side-effects in a VM.
        //
        facp->Flags |= EFI_ACPI_6_2_LOW_POWER_S0_IDLE_CAPABLE;
    }

    //
    // Special case if battery is enabled
    //
    if (PcdGetBool(PcdVirtualBatteryEnabled))
    {
        //
        // Set the profile to Mobile
        //
        facp->PreferredPmProfile = EFI_ACPI_6_2_PM_PROFILE_MOBILE;
    }

    //
    // If this is a HW-isolated VM, report it as hardware reduced. Zero out any of
    // filled in legacy structures.
    //
    if (IsHardwareIsolated())
    {
        BOOLEAN hostEmulatorsPresent = PcdGetBool(PcdHostEmulatorsWhenHardwareIsolated);

        facp->Flags = EFI_ACPI_6_2_WBINVD |
             EFI_ACPI_6_2_PROC_C1 |
             EFI_ACPI_6_2_PWR_BUTTON |
             EFI_ACPI_6_2_SLP_BUTTON |
             EFI_ACPI_6_2_TMR_VAL_EXT |
             EFI_ACPI_6_2_HEADLESS |
             EFI_ACPI_6_2_HW_REDUCED_ACPI;

        //
        // Zero out set fields between offsets 46 - 108
        //
        ZeroMem(&facp->SciInt,
            OFFSET_OF(EFI_ACPI_6_2_FIXED_ACPI_DESCRIPTION_TABLE, IaPcBootArch) - OFFSET_OF(EFI_ACPI_6_2_FIXED_ACPI_DESCRIPTION_TABLE, SciInt));

        if (hostEmulatorsPresent)
        {

            //
            // Advertise PM-based reset
            //
            facp->Flags |= EFI_ACPI_6_2_RESET_REG_SUP;

            //
            // Zero out set fields between offsets 148 - 244
            //
            ZeroMem(&facp->XPm1aEvtBlk,
                OFFSET_OF(EFI_ACPI_6_2_FIXED_ACPI_DESCRIPTION_TABLE, SleepControlReg) - OFFSET_OF(EFI_ACPI_6_2_FIXED_ACPI_DESCRIPTION_TABLE, XPm1aEvtBlk));
        }
        else
        {

            //
            // Zero out set fields between offsets 116 - 128, no reset registers supported
            //
            ZeroMem(&facp->ResetReg,
                OFFSET_OF(EFI_ACPI_6_2_FIXED_ACPI_DESCRIPTION_TABLE, ArmBootArch) - OFFSET_OF(EFI_ACPI_6_2_FIXED_ACPI_DESCRIPTION_TABLE, ResetReg));

            //
            // Zero out set fields between offsets 148 - 268, no sleep registers supported
            //
            ZeroMem(&facp->XPm1aEvtBlk,
                OFFSET_OF(EFI_ACPI_6_2_FIXED_ACPI_DESCRIPTION_TABLE, HypervisorVendorIdentity) - OFFSET_OF(EFI_ACPI_6_2_FIXED_ACPI_DESCRIPTION_TABLE, XPm1aEvtBlk));
        }
    }

    return EFI_SUCCESS;
}
