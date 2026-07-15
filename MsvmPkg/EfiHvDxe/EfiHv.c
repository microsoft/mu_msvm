/** @file
  Provides the EfiHvDxe driver entry point and protocol tables.

  The protocol implementations live in the companion EfiHv*.c files.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "EfiHvInternal.h"

//
// Module globals.
//

//
// When hardware isolation is in use, the main hypercall context is used to
// communicate with the paravisor, while the bypass context is used to
// communicate with the host hypervisor. If hardware isolated without a
// paravisor, only the bypass context is used.
//

HV_HYPERCALL_CONTEXT mHvContext;
HV_HYPERCALL_CONTEXT mHvBypassContext;
BOOLEAN mUseBypassContext;
BOOLEAN mBypassOnly;
PEFI_HV_PAGES mHvPages;

#if defined(MDE_CPU_X64)
UINT8 *mHypercallPage;
#endif

VOID *mHvInputPage;
EFI_HANDLE mHvHandle;
BOOLEAN mSynicConnected;
EFI_EVENT mExitBootServicesEvent;
BOOLEAN mAutoEoi;
BOOLEAN mDirectTimerSupported;
LIST_ENTRY mHostVisiblePageList;
LIST_ENTRY mPinnedPageList;
UINT64 mSharedGpaBoundary;
UINT64 mCanonicalizationMask;
UINT32 mIsolationType;
VOID *mSvsmCallingArea;

EFI_HV_SINT_CONFIGURATION mSintConfiguration[HV_SYNIC_SINT_COUNT];
UINT8 mVectorSint[256];

EFI_HV_INTERRUPT_HANDLER mDirectTimerInterruptHandlers[256];
HV_X64_MSR_STIMER_CONFIG_CONTENTS mTimerConfiguration[HV_SYNIC_STIMER_COUNT];

#if defined(MDE_CPU_X64)
EFI_CPU_ARCH_PROTOCOL *mCpu;
#elif defined(MDE_CPU_AARCH64)
EFI_HARDWARE_INTERRUPT_PROTOCOL *mHwInt;
#endif

//
// Protocol instances produced by this driver.
//

EFI_HV_PROTOCOL mHv =
{
  EfiHvConnectSint,
  EfiHvConnectSintToEvent,
  EfiHvDisconnectSint,
  EfiHvGetSintMessage,
  EfiHvCompleteSintMessage,
  EfiHvGetSintEventFlags,
  EfiHvGetReferenceTime,
  EfiHvGetCurrentVpIndex,
  EfiHvDirectTimerSupported,
  EfiHvConfigureTimer,
  EfiHvSetTimer,
  EfiHvPostMessage,
  EfiHvSignalEvent,
  EfiHvStartApplicationProcessor
};

EFI_HV_IVM_PROTOCOL mHvIvm =
{
  EfiHvMakeAddressRangeHostVisible,
  EfiHvMakeAddressRangeNotHostVisible,
  EfiHvPinAddressRange,
  EfiHvUnpinAddressRange
};

//
// Event callbacks.
//

VOID
EFIAPI
EfiHvExitBootServices (
  IN  EFI_EVENT Event,
  IN  VOID *Context
  )
/*++
  Called when ExitBootServices() is called. Tears down the hypervisor
  connection so that the new OS sees a clean state.

  @param Event An EFI event.

  @param Context A pointer to the context.

  @returns nothing.

--*/
{
  EfiHvDisconnectFromSynic();
  EfiHvDisconnectFromHypervisor();
}

//
// Driver entry point.
//

EFI_STATUS
EFIAPI
EfiHvInitialize (
  IN  EFI_HANDLE ImageHandle,
  IN  EFI_SYSTEM_TABLE *SystemTable
  )
/*++
  Entrypoint. Initializes the EfiHv driver.

  @param ImageHandle The handle of the loaded image.

  @param SystemTable A pointer to the system table.

  @returns EFI status.

--*/
{
  EFI_STATUS status;

  if (!PcdGetBool(PcdHvEnabled))
  {
    return EFI_UNSUPPORTED;
  }

  InitializeListHead(&mHostVisiblePageList);
  InitializeListHead(&mPinnedPageList);

#if defined(MDE_CPU_X64)

  //
  // For Intel find the CPU protocol.
  //
  status = gBS->LocateProtocol(&gEfiCpuArchProtocolGuid, NULL, (VOID **)&mCpu);


#elif defined(MDE_CPU_AARCH64)

  //
  // For ARM find the hardware interrupt protocol.
  //
  status = gBS->LocateProtocol(&gHardwareInterruptProtocolGuid, NULL, (VOID **)&mHwInt);

#endif

  if (EFI_ERROR(status))
  {
    DEBUG((DEBUG_ERROR, "--- %a: failed to locate protocol - %r \n", __func__, status));
    goto Cleanup;
  }

  //
  // Register notify function for EVT_SIGNAL_EXIT_BOOT_SERVICES.
  //
  status = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,
                TPL_CALLBACK,
                EfiHvExitBootServices,
                NULL,
                &gEfiEventExitBootServicesGuid,
                &mExitBootServicesEvent);
  if (EFI_ERROR(status))
  {
    DEBUG((DEBUG_ERROR, "--- %a: failed to create event - %r \n", __func__, status));
    goto Cleanup;
  }

  //
  // Connect to the hypervisor and synic.
  //
  status = EfiHvConnectToHypervisor();
  if (EFI_ERROR(status))
  {
    DEBUG((DEBUG_ERROR, "--- %a: failed to connect to the hypervisor - %r \n", __func__, status));
    goto Cleanup;
  }

  status = EfiHvConnectToSynic();
  if (EFI_ERROR(status))
  {
    DEBUG((DEBUG_ERROR, "--- %a: failed to connect to Synic - %r \n", __func__, status));
    goto Cleanup;
  }

  //
  // Register the HV protocols.
  //
  status = gBS->InstallMultipleProtocolInterfaces(
          &mHvHandle,
          &gEfiHvProtocolGuid, &mHv,
          &gEfiHvIvmProtocolGuid, &mHvIvm,
          NULL);

  if (EFI_ERROR(status))
  {
    DEBUG((DEBUG_ERROR, "--- %a: failed to install the protocol - %r \n", __func__, status));
    goto Cleanup;
  }

Cleanup:
  if (EFI_ERROR(status))
  {
    if (mExitBootServicesEvent != NULL)
    {
      gBS->CloseEvent(mExitBootServicesEvent);
      mExitBootServicesEvent = NULL;
    }

    EfiHvDisconnectFromSynic();
    EfiHvDisconnectFromHypervisor();
  }

  return status;
}
