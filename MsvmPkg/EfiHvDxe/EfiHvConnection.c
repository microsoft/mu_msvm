/** @file
  Manages the Hyper-V connection lifecycle for EfiHvDxe.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "EfiHvInternal.h"
#include "MsCpuid.h"

EFI_STATUS
EfiHvConnectToHypervisor (
  VOID
  )

/*++
  Initializes a connection to the hypervisor.

  @param None.

  @returns EFI status.

--*/
{
  EFI_STATUS  status;

 #if defined (MDE_CPU_X64)

  HV_CPUID_RESULT  cpuidResult;
  BOOLEAN          paravisorPresent;

  //
  // Determine the isolation type for this system.  If there is any
  // isolation, then a Microsoft hypervisor can be assumed.
  //
  mIsolationType = GetIsolationType ();
  if (!IsIsolatedEx (mIsolationType)) {
    //
    // Validate that the hypervisor is present, is a Microsoft hypervisor,
    // and has all the required features.
    //
    MsCpuid (cpuidResult.AsUINT32, HvCpuIdFunctionVersionAndFeatures);
    if (!cpuidResult.VersionAndFeatures.HypervisorPresent) {
      status = EFI_UNSUPPORTED;
      DEBUG ((DEBUG_ERROR, "--- %a: no hypervisor present - %r \n", __func__, status));
      goto Exit;
    }

    MsCpuid (cpuidResult.AsUINT32, HvCpuIdFunctionHvInterface);
    if (cpuidResult.HvInterface.Interface != HvMicrosoftHypervisorInterface) {
      status = EFI_UNSUPPORTED;
      DEBUG ((DEBUG_ERROR, "--- %a: hypervisor present is not a Microsoft hypervisor - %r \n", __func__, status));
      goto Exit;
    }
  }

  mSharedGpaBoundary    = PcdGet64 (PcdIsolationSharedGpaBoundary);
  mCanonicalizationMask = PcdGet64 (PcdIsolationSharedGpaCanonicalizationBitmask);
  paravisorPresent      = IsParavisorPresent ();

  if ((mIsolationType == UefiIsolationTypeSnp) && !paravisorPresent) {
    mSvsmCallingArea = (VOID *)PcdGet64 (PcdSvsmCallingArea);
  }

  //
  // Allocate hypervisor communication pages.
  //
  mHypercallPage = NULL;
  mHvPages       = AllocatePages (sizeof (*mHvPages) / EFI_PAGE_SIZE);
  if (mHvPages == NULL) {
    status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "--- %a: failed to allocate HV pages - %r \n", __func__, status));
    goto Exit;
  }

  ZeroMem (mHvPages, sizeof (*mHvPages));

  //
  // If this is a hardware-isolated system with no paravisor, then only the
  // direct, untrusted hypervisor connection is required.
  //
  if (IsHardwareIsolatedNoParavisorEx (mIsolationType, paravisorPresent)) {
    //
    // Make all of the pages visible to the host.
    //
    status = EfiHvMakeAddressRangeHostVisible (
               NULL,
               HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE,
               mHvPages,
               sizeof (*mHvPages),
               TRUE,
               NULL
               );

    if (EFI_ERROR (status)) {
      DEBUG ((DEBUG_ERROR, "--- %a: failed to make pages host visible - %r \n", __func__, status));
      goto Exit;
    }

    mHvPages = (PEFI_HV_PAGES)EfiHvpSharedVa (mHvPages);
    ZeroMem (mHvPages, sizeof (*mHvPages));
  } else {
    mHypercallPage = AllocatePages (1);
    if (mHypercallPage == NULL) {
      status = EFI_OUT_OF_RESOURCES;
      DEBUG ((DEBUG_ERROR, "--- %a: failed to allocate the hypercall page - %r \n", __func__, status));
      goto Exit;
    }

    ZeroMem (mHypercallPage, EFI_PAGE_SIZE);

    HvHypercallConnect (
      mHypercallPage,
      UefiIsolationTypeNone,
      FALSE,
      &mHvContext
      );

    //
    // Check to see if the hypercall page was mapped. If it wasn't, abort here.
    //
    if ((mHypercallPage[0] == 0) &&
        (mHypercallPage[1] == 0) &&
        (mHypercallPage[2] == 0))
    {
      FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR ();
    }

    //
    // Mark the Hypercall page as executable.
    //
    status = mCpu->SetMemoryAttributes (mCpu, (EFI_PHYSICAL_ADDRESS)mHypercallPage, EFI_PAGE_SIZE, EFI_MEMORY_RO);
    if (EFI_ERROR (status)) {
      FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR ();
    }
  }

 #elif defined (MDE_CPU_AARCH64)

  //
  // Direct timers are always supported on ARM64.
  //
  mDirectTimerSupported = TRUE;

  //
  // Allocate hypervisor communication pages.
  //
  mHvPages = AllocatePages (sizeof (*mHvPages) / EFI_PAGE_SIZE);
  if (mHvPages == NULL) {
    status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "--- %a: failed to allocate HV pages - %r \n", __func__, status));
    goto Exit;
  }

  HvHypercallConnect (&mHvContext);

  //
  // AutoEoi is not possible on ARM.
  //
  mAutoEoi = FALSE;

 #else
  #error Unsupported architecture
 #endif

  //
  // Initialize the hypercall input page.
  //
  mHvInputPage = mHvPages->HypercallInputPage;

 #if defined (MDE_CPU_X64)

  //
  // Determine whether this system uses a hardware isolation architecture
  // that will require a direct connection to the hypervisor that bypasses
  // the paravisor.
  //
  if (IsHardwareIsolatedEx (mIsolationType)) {
    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE (mSharedGpaBoundary != 0);

    //
    // TDX systems require a host-visible page to use as the hypercall
    // input page when making hypercalls that bypass the paravisor.
    // Allocate such a page if required.  SNP systems always copy
    // hypercall input into the GHCB page so no additional allocation is
    // required for those systems.
    //
    if ((mIsolationType != UefiIsolationTypeSnp) && paravisorPresent) {
      VOID  *hvInputPage;

      hvInputPage = AllocatePages (1);
      if (hvInputPage == NULL) {
        status = EFI_OUT_OF_RESOURCES;
        DEBUG ((DEBUG_ERROR, "--- %a: failed to allocate HV input page - %r \n", __func__, status));
        goto Exit;
      }

      //
      // Make this page visible to the hypervisor.  It should not be
      // possible for this to fail.
      //
      status = EfiHvpModifySparseGpaPageHostVisibility (
                 HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE,
                 1,
                 (UINTN)hvInputPage / EFI_PAGE_SIZE,
                 NULL
                 );

      if (EFI_ERROR (status)) {
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR ();
      }

      mHvInputPage = EfiHvpSharedVa (hvInputPage);
    } else {
      mBypassOnly = !paravisorPresent;
    }

    HvHypercallConnect (
      NULL,
      mIsolationType,
      paravisorPresent,
      &mHvBypassContext
      );

    mUseBypassContext = TRUE;
  }

  //
  // Cache some enlightenment information.  If this system requires
  // bypassing the paravisor, then assume a set of features that are present
  // instead of asking the hypervisor what it supports.
  //
  if (mUseBypassContext) {
    mAutoEoi              = FALSE;
    mDirectTimerSupported = TRUE;
  } else {
    MsCpuid (cpuidResult.AsUINT32, HvCpuIdFunctionMsHvEnlightenmentInformation);
    mAutoEoi = !cpuidResult.MsHvEnlightenmentInformation.DeprecateAutoEoi;
    DEBUG ((DEBUG_VERBOSE, "--- %a: mAutoEoi 0x%x\n", __func__, mAutoEoi));

    MsCpuid (cpuidResult.AsUINT32, HvCpuIdFunctionMsHvFeatures);
    if (!(cpuidResult.MsHvFeatures.PartitionPrivileges.AccessPartitionReferenceCounter &&
          cpuidResult.MsHvFeatures.PartitionPrivileges.AccessSynicRegs &&
          cpuidResult.MsHvFeatures.PartitionPrivileges.AccessSyntheticTimerRegs &&
          cpuidResult.MsHvFeatures.PartitionPrivileges.AccessHypercallMsrs))
    {
      status = EFI_UNSUPPORTED;
      DEBUG ((DEBUG_ERROR, "--- %a: missing hypervisor features - %r \n", __func__, status));
      goto Exit;
    }

    if (cpuidResult.MsHvFeatures.DirectSyntheticTimers) {
      mDirectTimerSupported = TRUE;
    }
  }

  if (IsIsolatedEx (mIsolationType)) {
    DEBUG ((EFI_D_INFO, "--- %a: Partition is Isolated\n", __func__));
  }

 #endif

  status = EFI_SUCCESS;

Exit:

  return status;
}

VOID
EfiHvDisconnectFromHypervisor (
  VOID
  )

/*++
  Tears down a connection to the hypervisor.

  @param None.

  @returns nothing.

--*/
{
  LIST_ENTRY                *entry;
  EFI_HV_PROTECTION_OBJECT  *protectionObject;
  EFI_HV_PIN_OBJECT         *pinObject;
  EFI_STATUS                status;

  //
  // Revoke host visibility for any pages that were made visible.
  //
  while (!IsListEmpty (&mHostVisiblePageList)) {
    entry            = GetFirstNode (&mHostVisiblePageList);
    protectionObject = BASE_CR (entry, EFI_HV_PROTECTION_OBJECT, ListEntry);
    EfiHvMakeAddressRangeNotHostVisible (NULL, &protectionObject);
  }

  //
  // Unpin any GPA ranges that were pinned for host DMA. Done before the
  // hypercall context is torn down so the unpin hypercalls can still
  // reach the hypervisor. Bypass the tracking-list lookup in
  // EfiHvUnpinAddressRange by removing the entry here and issuing the
  // hypercall directly.
  //
  while (!IsListEmpty (&mPinnedPageList)) {
    entry     = GetFirstNode (&mPinnedPageList);
    pinObject = BASE_CR (entry, EFI_HV_PIN_OBJECT, ListEntry);
    RemoveEntryList (&pinObject->ListEntry);

    status =
      EfiHvpPinUnpinGpaPageRanges (
        FALSE,
        pinObject->NumberOfPages,
        pinObject->GpaPageBase,
        NULL
        );
    if (EFI_ERROR (status)) {
      FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR ();
    }

    FreePool (pinObject);
  }

  //
  // Free the bypass input page if required.
  //
  if ((mHvPages != NULL) &&
      (mHvInputPage != NULL) &&
      (mHvInputPage != mHvPages->HypercallInputPage) &&
      !mBypassOnly)
  {
    mHvInputPage = (VOID *)EfiHvpBasePa ((UINTN)mHvInputPage);

    status = EfiHvpModifySparseGpaPageHostVisibility (
               HV_MAP_GPA_PERMISSIONS_NONE,
               1,
               (UINTN)mHvInputPage / EFI_PAGE_SIZE,
               NULL
               );

    if (EFI_ERROR (status)) {
      //
      // Failure is not allowed here - need to fail fast.
      //
      FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR ();
    }

    FreePages (mHvInputPage, 1);
  }

  if (mHvContext.Connected) {
    HvHypercallDisconnect (&mHvContext);
  }

  if (mHvBypassContext.Connected) {
    HvHypercallDisconnect (&mHvBypassContext);
  }

  //
  // Free the hypercall communication pages.  If these pages were originally
  // made host-visible, then they were made host-not-visible during the
  // visibility reclaim operation above.
  //
  if (mHvPages != NULL) {
    if (mBypassOnly) {
      mHvPages = (PEFI_HV_PAGES)EfiHvpBasePa ((UINTN)mHvPages);
    }

    FreePages (mHvPages, sizeof (*mHvPages) / EFI_PAGE_SIZE);
    mHvPages     = NULL;
    mHvInputPage = NULL;
  }

 #if defined (MDE_CPU_X64)
  if (mHypercallPage != NULL) {
    //
    // Clear the EFI_MEMORY_RO attribute that was applied to this page
    // after allocation before returning it to the page allocator, so a
    // later allocator consumer does not get a still-read-only page.
    //
    status = mCpu->SetMemoryAttributes (mCpu, (EFI_PHYSICAL_ADDRESS)mHypercallPage, EFI_PAGE_SIZE, 0);
    if (EFI_ERROR (status)) {
      DEBUG ((DEBUG_ERROR, "--- %a: failed to clear memory attributes - %r \n", __func__, status));
      FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR ();
    }

    FreePages (mHypercallPage, 1);
    mHypercallPage = NULL;
  }

 #endif
}
