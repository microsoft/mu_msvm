/** @file
  EFI Diagnostics DXE driver.

  Advertises the Advanced Logger buffer GPA to the host VMM so that EFI
  diagnostics can be collected. At ExitBootServices, signals the host to
  process the buffer.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BiosDeviceLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Guid/EventGroup.h>
#include <AdvancedLoggerInternal.h>
#include <BiosInterface.h>

extern EFI_GUID  gAdvancedLoggerHobGuid;

/**
    ExitBootServices callback that tells the host to collect EFI diagnostics.

    Registered at TPL_CALLBACK so that it fires AFTER all TPL_NOTIFY
    ExitBootServices handlers (e.g., VmbusRoot GPADL cleanup, SecurityLock).
    This ensures the AdvLogger buffer contains entries from those late handlers
    before the host processes it.
**/
STATIC
VOID
EFIAPI
OnExitBootServicesProcessDiagnostics (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  WriteBiosDevice (BiosConfigProcessEfiDiagnostics, TRUE);
}

/**
    Entry point for the EFI Diagnostics DXE driver.

    Sets the initial advanced logger GPA for the host VMM and registers
    ReadyToBoot/UnableToBoot callbacks to re-advertise the GPA after the
    buffer is relocated, and an ExitBootServices callback to trigger
    diagnostic collection.

    @retval     EFI_SUCCESS  Always returns success.
**/
EFI_STATUS
EFIAPI
EfiDiagnosticsDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  ADVANCED_LOGGER_PTR  *AdvancedLoggerPtr;
  EFI_HOB_GUID_TYPE    *GuidHob;
  EFI_STATUS           Status;
  EFI_EVENT            ExitBootServicesEvent;

  //
  // Locate the Advanced Logger HOB created during PEI to find the current
  // buffer address.
  //
  GuidHob = GetFirstGuidHob (&gAdvancedLoggerHobGuid);
  if (GuidHob == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Advanced Logger HOB not found. Setting GPA to 0.\n", __func__));
    WriteBiosDevice (BiosConfigSetEfiDiagnosticsGpa, 0);
    return EFI_SUCCESS;
  }

  AdvancedLoggerPtr = (ADVANCED_LOGGER_PTR *)GET_GUID_HOB_DATA (GuidHob);
  if (AdvancedLoggerPtr == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Advanced Logger Ptr is NULL. Setting GPA to 0.\n", __func__));
    WriteBiosDevice (BiosConfigSetEfiDiagnosticsGpa, 0);
    return EFI_SUCCESS;
  }

  if (AdvancedLoggerPtr->LogBuffer >= MAX_UINT32) {
    DEBUG ((DEBUG_ERROR, "%a: Advanced Logger buffer address 0x%llx >= 4GB. Setting GPA to 0.\n", __func__, AdvancedLoggerPtr->LogBuffer));
    WriteBiosDevice (BiosConfigSetEfiDiagnosticsGpa, 0);
    return EFI_SUCCESS;
  }

  //
  // Set the initial GPA.
  //
  {
    DEBUG ((DEBUG_INFO, "%a: Advanced Logger buffer address 0x%08x\n", __func__, (UINT32)(AdvancedLoggerPtr->LogBuffer)));
    WriteBiosDevice (BiosConfigSetEfiDiagnosticsGpa, (UINT32)(AdvancedLoggerPtr->LogBuffer));
  }

  //
  // Register an ExitBootServices callback at TPL_CALLBACK to tell the host
  // to collect EFI diagnostics. TPL_CALLBACK is lower priority than
  // TPL_NOTIFY, so this fires AFTER all TPL_NOTIFY ExitBootServices handlers
  // (e.g., VmbusRoot GPADL cleanup), capturing their log entries.
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnExitBootServicesProcessDiagnostics,
                  NULL,
                  &gEfiEventExitBootServicesGuid,
                  &ExitBootServicesEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create ExitBootServices event. Status = %r\n", __func__, Status));
  }

  return EFI_SUCCESS;
}
