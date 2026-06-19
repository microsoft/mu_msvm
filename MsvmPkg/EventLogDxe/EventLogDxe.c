/** @file
  Entry point and initialization for combined status code and event logging driver

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "EventLogDxe.h"
#include <IsolationTypes.h>
#include <Library/UefiDriverEntryPoint.h>
#include "StatusCode.h"
#include "EventLogger.h"

EFI_HV_IVM_PROTOCOL *mHvIvm;

EFI_STATUS
EFIAPI
EventLogDxeEntry (
    IN      EFI_HANDLE          ImageHandle,
    IN      EFI_SYSTEM_TABLE   *SystemTable
    )
/*++

Routine Description:

  Entry point of DXE Status Code Driver.

  This function is the entry point of this DXE Status Code Driver.
  It installs Status Code Runtime Protocol

Arguments:

    ImageHandle     The firmware allocated handle for the EFI image.

    SystemTable     A pointer to the EFI System Table.

Return Value:

    EFI_SUCCESS on success

--*/
{
    EFI_STATUS  status;

    DEBUG((DEBUG_INIT, "EventLog Driver Starting\n"));

    //
    // The IVM protocol is required for hardware-isolated VMs. Check this
    // before EventLoggerInitialize, which installs protocols — returning
    // error after that would leave dangling protocol entries.
    //
    if (IsHardwareIsolatedNoParavisor())
    {
        status = gBS->LocateProtocol(&gEfiHvIvmProtocolGuid, NULL, (VOID **)&mHvIvm);

        if (EFI_ERROR(status))
        {
            goto Exit;
        }
    }

    //
    // Initialize the event channel management and then the status code protocol
    //
    status = EventLoggerInitialize();

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    //
    // Don't fail driver initialization if this fails.
    //
    StatusCodeRuntimeInitialize();

    //
    // Workaroud: Initialize BootEventLogLib library.  This is done because BootEventLogLib
    //            library requires gEfiEventLogProtocolGuid, which is not available at the
    //            time of its constructor execution.
    //
    BootEventLogLibInit(ImageHandle, SystemTable);

Exit:

    return status;
}
