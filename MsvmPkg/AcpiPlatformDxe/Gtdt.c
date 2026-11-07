/** @file
  This module is responsible for runtime initialization of the GTDT APCI table.
  ARM64 generic timer descriptor table

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifdef MDE_CPU_AARCH64

#include <Base.h>
#include <AcpiPlatform.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PcdLib.h>
#include <PiDxe.h>

EFI_ACPI_6_3_GENERIC_TIMER_DESCRIPTION_TABLE  GTDT =
{
  STANDARD_HEADER (
    EFI_ACPI_6_3_GENERIC_TIMER_DESCRIPTION_TABLE_SIGNATURE,
    sizeof (EFI_ACPI_6_3_GENERIC_TIMER_DESCRIPTION_TABLE),
    EFI_ACPI_6_3_GENERIC_TIMER_DESCRIPTION_TABLE_REVISION
    )
  0xFFFFFFFFFFFFFFFF, // CntControlBase Physical Address: doesn't exist so -1
  0,                  // Reserved
  0,                  // Secure EL1 timer GSIV: doesn't exist so zero
  0,                  // Secure EL1 timer Flags: doesn't exist so zero
  0,                  // PcdArmArchTimerIntrNum Non-Secure EL1 timer GSIV SecurePL1TimerGSIV CNTP 19 or 30
  0,                  // Non-Secure EL1 timer Flags: doesn't exist so zero
  0,                  // PcdArmArchTimerVirtIntrNum Virtual EL1 timer GSIV VirtualTimerGSIV CNTV 20 or 27
  0,                  // Virtual Timer Flags: doesn't exist so zero
  0,                  // PcdArmArchTimerHypIntrNum Non-Secure EL2 timer GSIV NonSecurePL2TimerGSIV 21 or 26
  0,                  // Non-Secure EL2 timer Flags: doesn't exist so zero
  0xFFFFFFFFFFFFFFFF, // CntReadBase Physical address: doesn't exist so -1
  0,                  // Platform Timer Count
  0,                  // Platform Timer Offset
  // 6.2 end
  0,   // PcdArmArchTimerHypVirtIntrNum VirtualPL2TimerGSIV CNTHV 0 or 28, if 0 downgrade table to historical 6.2
  0,   // VirtualPL2TimerFlags
};

VOID
AcpiInitGtdt (
  VOID
  )

/*++

Routine Description:

    Initializes the GTDT table based on configuration data.

Arguments:

    None.

Return Value:

    None.

--*/
{
  GTDT.NonSecurePL1TimerGSIV = PcdGet32 (PcdArmArchTimerIntrNum);        // 19 or 30
  GTDT.VirtualTimerGSIV      = PcdGet32 (PcdArmArchTimerVirtIntrNum);    // 20 or 27
  GTDT.NonSecurePL2TimerGSIV = PcdGet32 (PcdArmArchTimerHypIntrNum);     // 21 or 26
  GTDT.VirtualPL2TimerGSIV   = PcdGet32 (PcdArmArchTimerHypVirtIntrNum); //  0 or 28

  if ((GTDT.VirtualPL2TimerGSIV == 0) && (GTDT.VirtualPL2TimerFlags == 0)) {
    // Downgrade from 6.3 to historical 6.2.
    GTDT.Header.Length   = sizeof (EFI_ACPI_6_2_GENERIC_TIMER_DESCRIPTION_TABLE);
    GTDT.Header.Revision = EFI_ACPI_6_2_GENERIC_TIMER_DESCRIPTION_TABLE_REVISION;
  }
}

#endif // MDE_CPU_AARCH64
