/** @file
  EFI Diagnostics DXE driver.

  Advertises the Advanced Logger buffer GPA to the host VMM so that EFI
  diagnostics can be collected. At ReadyToBoot/UnableToBoot, follows the
  NewLoggerInfoAddress chain to advertise the final relocated buffer GPA
  (in reserved memory), which survives after the OS reclaims boot-services
  memory.

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
#include <Guid/UnableToBootEvent.h>
#include <AdvancedLoggerInternal.h>
#include <BiosInterface.h>

extern EFI_GUID  gAdvancedLoggerHobGuid;

//
// Module-global pointer to the original ADVANCED_LOGGER_INFO from the PEI HOB.
// The ReadyToBoot/UnableToBoot callback follows the NewLoggerInfoAddress chain
// from this pointer to find the final relocated buffer in reserved memory.
//
static ADVANCED_LOGGER_INFO  *mOriginalLoggerInfo = NULL;

//
// Event handles for ReadyToBoot and UnableToBoot. Both are closed in the
// callback to ensure the GPA re-advertisement only happens once.
//
static EFI_EVENT  mReadyToBootEvent  = NULL;
static EFI_EVENT  mUnableToBootEvent = NULL;

/**
    Callback that follows the advanced logger chain and advertises the final
    buffer GPA to the host.

    AdvLoggerPkg relocates the PEI buffer from boot-services memory to reserved
    memory at EndOfDxe, setting NewLoggerInfoAddress on the old header. This
    callback follows that chain to find the final buffer and sends its GPA to
    the host. The final buffer is in reserved memory, so it survives after the
    OS reclaims boot-services memory — enabling reprocessing via inspect.

    Registered for both ReadyToBoot and UnableToBoot to cover all boot paths.
    A static flag ensures this only runs once.
**/
STATIC
VOID
EFIAPI
OnBootUpdateLoggerGpa (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  ADVANCED_LOGGER_INFO  *LoggerInfo;
  EFI_PHYSICAL_ADDRESS  LoggerAddress;
  UINTN                 Depth;

  //
  // Close both events so the callback cannot fire a second time.
  //
  if (mReadyToBootEvent != NULL) {
    gBS->CloseEvent (mReadyToBootEvent);
    mReadyToBootEvent = NULL;
  }

  if (mUnableToBootEvent != NULL) {
    gBS->CloseEvent (mUnableToBootEvent);
    mUnableToBootEvent = NULL;
  }

  if (mOriginalLoggerInfo == NULL) {
    return;
  }

  //
  // Follow the NewLoggerInfoAddress chain to find the relocated buffer.
  //
  LoggerInfo = mOriginalLoggerInfo;
  for (Depth = 0; Depth < ADVANCED_LOGGER_MAX_LOGGER_CHAIN_DEPTH; Depth++) {
    if (LoggerInfo->NewLoggerInfoAddress == 0) {
      break;
    }

    LoggerInfo = (ADVANCED_LOGGER_INFO *)(UINTN)LoggerInfo->NewLoggerInfoAddress;
  }

  LoggerAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)LoggerInfo;

  if (Depth == ADVANCED_LOGGER_MAX_LOGGER_CHAIN_DEPTH && LoggerInfo->NewLoggerInfoAddress != 0) {
    DEBUG ((DEBUG_ERROR, "%a: Depth limit hit without reaching final logger. Setting GPA to 0.\n", __func__));
    WriteBiosDevice (BiosConfigSetEfiDiagnosticsGpa, 0);
  } else if (LoggerAddress >= MAX_UINT32) {
    DEBUG ((DEBUG_ERROR, "%a: Advanced Logger buffer address 0x%llx >= 4GB. Setting GPA to 0.\n", __func__, LoggerAddress));
    WriteBiosDevice (BiosConfigSetEfiDiagnosticsGpa, 0);
  } else {
    DEBUG ((DEBUG_INFO, "%a: Advanced Logger buffer address 0x%08x\n", __func__, (UINT32)LoggerAddress));
    WriteBiosDevice (BiosConfigSetEfiDiagnosticsGpa, (UINT32)LoggerAddress);
  }
}

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
  // Set the initial GPA and save the pointer for the boot callback.
  //
  {
    ADVANCED_LOGGER_INFO  *AdvancedLoggerInfo;
    AdvancedLoggerInfo = (ADVANCED_LOGGER_INFO *)AdvancedLoggerPtr->LogBuffer;
    DEBUG ((DEBUG_INFO, "%a: Advanced Logger buffer address 0x%08x\n", __func__, (UINT32)(AdvancedLoggerPtr->LogBuffer)));
    WriteBiosDevice (BiosConfigSetEfiDiagnosticsGpa, (UINT32)(AdvancedLoggerPtr->LogBuffer));
    mOriginalLoggerInfo = AdvancedLoggerInfo;
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

  //
  // Register ReadyToBoot and UnableToBoot callbacks to follow the chain
  // and advertise the final relocated buffer GPA (in reserved memory).
  // This must happen after EndOfDxe so that the chain is populated.
  //
  Status = EfiCreateEventReadyToBootEx (
             TPL_CALLBACK,
             OnBootUpdateLoggerGpa,
             NULL,
             &mReadyToBootEvent
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create ReadyToBoot event. Status = %r\n", __func__, Status));
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnBootUpdateLoggerGpa,
                  NULL,
                  &gMsvmUnableToBootEventGuid,
                  &mUnableToBootEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create UnableToBoot event. Status = %r\n", __func__, Status));
  }

  return EFI_SUCCESS;
}
