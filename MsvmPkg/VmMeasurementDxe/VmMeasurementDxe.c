/** @file
  Measure VM specific data to TPM using PCR[06]

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <IndustryStandard/UefiTcgPlatform.h>
#include <Library/TpmMeasurementLib.h>

EFI_STATUS
EFIAPI
VmMeasurementEntry (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
/*++

Routine Description:

  Entry to VmMeasurementDxe driver. Measures VM Identity info to TPM.

--*/
{
  EFI_STATUS Status;
  CHAR8 EventLog[64];
  UINT32 EventSize;

  DEBUG((DEBUG_INFO, __FUNCTION__"() - Measuring VM data to PCR[06]\n"));

  //
  // Include the UUID in the event log, and hash the entire event log
  //
  EventSize = (UINT32)AsciiSPrint(EventLog, sizeof(EventLog), "UUID: %g", (EFI_GUID *)PcdGet64(PcdBiosGuidPtr));

  Status = TpmMeasureAndLogData (
            6,
            EV_COMPACT_HASH,
            EventLog,
            EventSize,
            EventLog,
            EventSize
            );

  DEBUG((DEBUG_INFO, __FUNCTION__"() - Logged %a (size=0x%x) status 0x%x\n", EventLog, EventSize, Status));

  return EFI_SUCCESS;
}
