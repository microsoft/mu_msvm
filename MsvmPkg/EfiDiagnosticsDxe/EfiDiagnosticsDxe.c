/** @file
  EFI Diagnostics DXE driver.

  Tells the host VMM to process the AdvancedLogger buffer at the relevant
  late-DXE / runtime transition points. Currently binds to:
    - ExitBootServices
    - ResetSystem() (via EFI_RESET_NOTIFICATION_PROTOCOL)
    - The platform's UnableToBoot event

  Also logs a marker at ReadyToBoot so log readers can tell whether a
  subsequent failure occurred in firmware or the boot manager / OS loader.

  Publishing the buffer's GPA to the host is handled by
  MsvmPkg/EfiDiagnosticsPei in PEI.

  Note: On the crash path, MsvmPkg/Library/CrashLib invokes
  NotifyHostToProcessEfiDiagnostics() directly rather than going through this
  driver, because event dispatch may be broken by the time we get there.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/DebugLib.h>
#include <Library/EfiDiagnosticsLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Guid/EventGroup.h>
#include <Guid/UnableToBootEvent.h>
#include <Protocol/ResetNotification.h>

STATIC EFI_RESET_NOTIFICATION_PROTOCOL  *mResetNotify;

/**
  Reset notification callback that tells the host to collect EFI diagnostics
  before reset completes.
**/
STATIC
VOID
EFIAPI
OnResetProcessDiagnostics (
  IN EFI_RESET_TYPE  ResetType,
  IN EFI_STATUS      ResetStatus,
  IN UINTN           DataSize,
  IN VOID            *ResetData OPTIONAL
  )
{
  DEBUG((DEBUG_ERROR, "%a: Reset notification callback called. ResetType = %d, ResetStatus = %r\n", __func__, ResetType, ResetStatus));
  NotifyHostToProcessEfiDiagnostics ();
}

/**
  Protocol-notify callback that registers OnResetProcessDiagnostics with
  the reset-notification protocol once it's available. Closes the event
  on success to avoid re-registering on protocol reinstalls.
**/
STATIC
VOID
EFIAPI
OnResetNotificationProtocolInstalled (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;

  Status = gBS->LocateProtocol (&gEfiResetNotificationProtocolGuid, NULL, (VOID **)&mResetNotify);
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = mResetNotify->RegisterResetNotify (mResetNotify, OnResetProcessDiagnostics);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to register reset notify callback. Status = %r\n", __func__, Status));
    //
    // Clear mResetNotify so a subsequent protocol reinstall re-enters this
    // path cleanly. Leave the event open so the retry can happen.
    //
    mResetNotify = NULL;
    return;
  }

  gBS->CloseEvent (Event);
}

/**
  BeforeExitBootServices callback that unregisters the reset notification
  before this driver's boot-services image is reclaimed.

  Hooked on BeforeExitBootServices (not ExitBootServices) so that
  UnregisterResetNotify's internal FreePool runs while memory services are
  unambiguously legal per the UEFI spec. Without unregistering at all,
  gRT->ResetSystem() at runtime would dereference a freed function pointer.
  The EBS diagnostics-flush notify still runs after this and captures any
  final log entries.
**/
STATIC
VOID
EFIAPI
OnBeforeExitBootServicesUnregisterResetNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;

  if (mResetNotify == NULL) {
    return;
  }

  Status = mResetNotify->UnregisterResetNotify (mResetNotify, OnResetProcessDiagnostics);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to unregister reset notify callback. Status = %r\n", __func__, Status));
  }

  mResetNotify = NULL;
}

/**
  Generic event callback that tells the host to collect EFI diagnostics.

  Used for both the ExitBootServices and UnableToBoot event groups. For
  ExitBootServices, registered at TPL_CALLBACK so it fires after TPL_NOTIFY
  handlers, capturing their final log entries.
**/
STATIC
VOID
EFIAPI
OnEventProcessDiagnostics (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  NotifyHostToProcessEfiDiagnostics ();
}

/**
  ReadyToBoot marker. We can't hook a fault in the boot manager itself
  (it dodges the FailFast path), so the best we can do is leave a
  breadcrumb: anything after this line in the log that isn't followed
  by ExitBootServices points at a boot manager / OS loader issue.
**/
STATIC
VOID
EFIAPI
OnReadyToBoot (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  DEBUG ((DEBUG_WARN, "%a: Transitioning to boot manager; errors past this point and before ExitBootServices suggest a boot manager issue.\n", __func__));
}

/**
  Entry point for the EFI Diagnostics DXE driver.

  Registers ExitBootServices, ResetNotification, and UnableToBoot callbacks
  so the host is told to process diagnostics at the relevant late-DXE /
  runtime transition points.

  @retval EFI_SUCCESS  Always returns success.
**/
EFI_STATUS
EFIAPI
EfiDiagnosticsDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   ResetNotifyProtocolEvent;
  EFI_EVENT   ExitBootServicesEvent;
  EFI_EVENT   BeforeExitBootServicesUnregisterEvent;
  EFI_EVENT   UnableToBootEvent;
  EFI_EVENT   ReadyToBootEvent;
  VOID        *Registration;

  //
  // Process diagnostics on ResetSystem() before reset completes. The notify
  // callback fires immediately if the protocol is already installed, and
  // again whenever ResetSystemRuntimeDxe installs it later.
  //
  ResetNotifyProtocolEvent = EfiCreateProtocolNotifyEvent (
                               &gEfiResetNotificationProtocolGuid,
                               TPL_CALLBACK,
                               OnResetNotificationProtocolInstalled,
                               NULL,
                               &Registration
                               );
  if (ResetNotifyProtocolEvent == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create ResetNotification protocol notify event.\n", __func__));
  }

  //
  // Process diagnostics on ExitBootServices. TPL_CALLBACK runs after
  // TPL_NOTIFY handlers so we capture their final log entries.
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnEventProcessDiagnostics,
                  NULL,
                  &gEfiEventExitBootServicesGuid,
                  &ExitBootServicesEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create ExitBootServices event. Status = %r\n", __func__, Status));
  }

  //
  // Separate BeforeExitBootServices notify that unregisters the reset
  // notification before this driver's boot-services image is reclaimed.
  // BeforeExitBootServices (rather than ExitBootServices) keeps the
  // UnregisterResetNotify-internal FreePool inside spec-legal territory for
  // memory services, while still removing the function pointer from the
  // runtime reset-notify list before image reclamation. Avoids a UAF when
  // gRT->ResetSystem() is invoked at runtime.
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnBeforeExitBootServicesUnregisterResetNotify,
                  NULL,
                  &gEfiEventBeforeExitBootServicesGuid,
                  &BeforeExitBootServicesUnregisterEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create BeforeExitBootServices unregister event. Status = %r\n", __func__, Status));
  }

  //
  // Process diagnostics when BDS signals UnableToBoot, before it falls
  // through to the boot manager menu.
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnEventProcessDiagnostics,
                  NULL,
                  &gMsvmUnableToBootEventGuid,
                  &UnableToBootEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create UnableToBoot event. Status = %r\n", __func__, Status));
  }

  //
  // Log a marker at ReadyToBoot. No host notify here -- this just helps
  // readers attribute later failures to firmware vs. boot manager.
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnReadyToBoot,
                  NULL,
                  &gEfiEventReadyToBootGuid,
                  &ReadyToBootEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create ReadyToBoot event. Status = %r\n", __func__, Status));
  }

  return EFI_SUCCESS;
}
