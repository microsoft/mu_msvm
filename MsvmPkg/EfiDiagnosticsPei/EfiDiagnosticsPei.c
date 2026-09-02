/** @file
  EFI Diagnostics PEI module.

  Publishes the AdvancedLogger buffer GPA to the host VMM so host-side
  diagnostics can find the in-memory log. Publishes once at entry (so an
  early-PEI crash is still recoverable) and again on a memory-discovered
  notify (so the host tracks the buffer if it gets moved into permanent RAM).

  Must dispatch before PlatformPei to cover early-PEI crashes.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiPei.h>

#include <Library/DebugLib.h>
#include <Library/EfiDiagnosticsLib.h>
#include <Library/HobLib.h>
#include <Library/PeiServicesLib.h>

#include <Ppi/EndOfPeiPhase.h>
#include <Ppi/MemoryDiscovered.h>
#include <AdvancedLoggerInternal.h>

extern EFI_GUID  gAdvancedLoggerHobGuid;

STATIC
EFI_STATUS
EFIAPI
PublishCurrentAdvancedLoggerGpa (
  IN EFI_PEI_SERVICES           **PeiServices    OPTIONAL,
  IN EFI_PEI_NOTIFY_DESCRIPTOR  *NotifyDescriptor OPTIONAL,
  IN VOID                       *Ppi             OPTIONAL
  );

STATIC CONST EFI_PEI_NOTIFY_DESCRIPTOR  mMemoryDiscoveredNotifyList[] = {
  {
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_DISPATCH | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEfiPeiMemoryDiscoveredPpiGuid,
    PublishCurrentAdvancedLoggerGpa
  }
};

//
// End-of-PEI republish is a belt-and-suspenders against ordering. The
// memory-discovered notify above only publishes the migrated GPA correctly
// if our notify fires *after* AdvancedLoggerLib's migration notify (which
// updates the HOB). EDK2 PeiCore currently walks the notify list in
// registration order, but that isn't a PI-spec guarantee. End-of-PEI is
// signaled after all PEI dispatch is complete, so the HOB is guaranteed
// final by then. If we won the memory-discovered race this is a harmless
// idempotent re-publish; if we lost it, this corrects a stale GPA before
// DXE starts.
//
STATIC CONST EFI_PEI_NOTIFY_DESCRIPTOR  mEndOfPeiNotifyList[] = {
  {
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_DISPATCH | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEfiEndOfPeiSignalPpiGuid,
    PublishCurrentAdvancedLoggerGpa
  }
};

/**
  Read the AdvancedLogger buffer GPA from the HOB and publish it to the host.

  Publishes a GPA of 0 on any failure so the host knows there is nothing to
  collect.

  Signature matches EFI_PEIM_NOTIFY_ENTRY_POINT so it can serve double duty
  as both the entry-time publisher and the memory-discovered notify callback.
  All parameters are unused.

  @param[in] PeiServices       Indirect pointer to the PEI Services Table.
                               Unused. NULL when called directly from the
                               PEIM entry point.
  @param[in] NotifyDescriptor  Pointer to the notify descriptor that fired.
                               Unused. NULL when called directly from the
                               PEIM entry point.
  @param[in] Ppi               Pointer to the PPI instance that was just
                               installed. Unused. NULL when called directly
                               from the PEIM entry point.

  @retval EFI_SUCCESS  Always.
**/
STATIC
EFI_STATUS
EFIAPI
PublishCurrentAdvancedLoggerGpa (
  IN EFI_PEI_SERVICES           **PeiServices    OPTIONAL,
  IN EFI_PEI_NOTIFY_DESCRIPTOR  *NotifyDescriptor OPTIONAL,
  IN VOID                       *Ppi             OPTIONAL
  )
{
  EFI_HOB_GUID_TYPE     *GuidHob;
  ADVANCED_LOGGER_PTR   *LogPtr;
  ADVANCED_LOGGER_INFO  *LogInfo;

  DEBUG ((DEBUG_INFO, "%a: Publishing Advanced Logger GPA.\n", __func__));

  GuidHob = GetFirstGuidHob (&gAdvancedLoggerHobGuid);
  if (GuidHob == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Advanced Logger HOB not found. Setting GPA to 0.\n", __func__));
    NotifyHostToUpdateEfiDiagnosticsGpa (0);
    return EFI_SUCCESS;
  }

  LogPtr = (ADVANCED_LOGGER_PTR *)GET_GUID_HOB_DATA (GuidHob);
  if (LogPtr == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Advanced Logger Ptr is NULL. Setting GPA to 0.\n", __func__));
    NotifyHostToUpdateEfiDiagnosticsGpa (0);
    return EFI_SUCCESS;
  }

  if (LogPtr->LogBuffer >= MAX_UINT32) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Advanced Logger buffer address 0x%llx >= 4GB. Setting GPA to 0.\n",
      __func__,
      LogPtr->LogBuffer
      ));
    NotifyHostToUpdateEfiDiagnosticsGpa (0);
    return EFI_SUCCESS;
  }

  LogInfo = (ADVANCED_LOGGER_INFO *)(UINTN)LogPtr->LogBuffer;
  DEBUG ((
    DEBUG_INFO,
    "%a: Advanced Logger buffer address 0x%016llx size 0x%08x\n",
    __func__,
    LogPtr->LogBuffer,
    LogInfo->LogBufferSize
    ));

  //
  // The cast to UINT32 is safe: the AdvancedLogger buffer is always
  // allocated below 4GB. See InitializeMemoryMap() in PlatformPei/Platform.c.
  // The runtime guard above protects release builds; ASSERT catches any
  // future memory-layout change at debug-build time.
  //
  ASSERT (LogPtr->LogBuffer < MAX_UINT32);
  NotifyHostToUpdateEfiDiagnosticsGpa ((UINT32)LogPtr->LogBuffer);
  return EFI_SUCCESS;
}

/**
  Entry point for the EFI Diagnostics PEI module.

  Publishes the current AdvancedLogger buffer GPA, then registers a
  memory-discovered notify to republish it if the buffer moves.

  @param[in] FileHandle   Handle of the file being invoked.
  @param[in] PeiServices  Indirect reference to the PEI Services Table.

  @retval EFI_SUCCESS  Always.
**/
EFI_STATUS
EFIAPI
EfiDiagnosticsPeiEntry (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS  Status;

  //
  // Publish whatever the HOB currently points at so any early-PEI crash is
  // still recoverable from the host side.
  //
  PublishCurrentAdvancedLoggerGpa (NULL, NULL, NULL);

  //
  // Re-publish on memory-discovered in case the buffer has been moved.
  //
  Status = PeiServicesNotifyPpi (mMemoryDiscoveredNotifyList);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to register memory-discovered notify. Status = %r\n",
      __func__,
      Status
      ));
  }

  //
  // Re-publish at end-of-PEI as a final, order-independent backstop. See
  // the comment on mEndOfPeiNotifyList above.
  //
  Status = PeiServicesNotifyPpi (mEndOfPeiNotifyList);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to register end-of-PEI notify. Status = %r\n",
      __func__,
      Status
      ));
  }

  return EFI_SUCCESS;
}
