/** @file
  Implements EFI_HV_IVM_PROTOCOL host visibility support.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "EfiHvInternal.h"

EFI_STATUS
EFIAPI
EfiHvpModifySparseGpaPageHostVisibility(
  IN              HV_MAP_GPA_FLAGS    MapFlags,
  IN              UINT32              PageCount,
  IN              HV_GPA_PAGE_NUMBER  GpaPageBase,
  OUT OPTIONAL    UINT32              *PageCountProcessed
  )
/*++
  Handles the ModifySparseGpaPageHostVisibility hypercall.

  @param MapFlags Access permissions provided to the host.

  @param PageCount The number of pages to modify.

  @param GpaPageBase Supplies the address of the first target GPA to accept. The
              remaining pages will be modified sequentially from this GPA.

  @param PageCountProcessed If present, the number of pages that are successfully processed
                will be returned in this.

  @returns EFI status.

--*/
{

  //
  // For this rep call, it's easier to treat the input page as a pointer
  // to this structure.
  PHV_INPUT_MODIFY_SPARSE_GPA_PAGE_HOST_VISIBILITY pInputBuffer;
  HV_STATUS hvStatus;
  EFI_STATUS status;
  EFI_TPL oldTpl;
  UINT32 possibleRepsPerCall;
  UINT32 repsInCurrentCall;
  UINT32 repsProcessedThisCall;
  UINT32 gpaPageBaseIndex = 0;
  UINT32 i;
  UINT32 totalPageCountProcessed = 0;
  BOOLEAN paravisorPresent;

  if (PageCountProcessed)
  {
    *PageCountProcessed = 0;
  }

  if (PageCount == 0)
  {
    DEBUG((DEBUG_ERROR, "%a: 0 page count\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  paravisorPresent = IsParavisorPresent();

#if defined(MDE_CPU_X64)

  //
  // Check if we are running hardware isolated but do not have a paravisor.
  //
  if (IsHardwareIsolatedNoParavisorEx(mIsolationType, paravisorPresent))
  {

    //
    // If the hypervisor connection has not yet been established, then
    // visibility must be changed without using hypercalls.
    //
    if (!mHvBypassContext.Connected)
    {
      UINT64 pagesProcessed;

      if (MapFlags != 0)
      {
        status = EfiMakePageRangeHostVisible(
          mIsolationType,
          mSvsmCallingArea,
          GpaPageBase,
          PageCount,
          &pagesProcessed);
      }
      else
      {
        status = EfiMakePageRangeHostNotVisible(
          mIsolationType,
          mSvsmCallingArea,
          GpaPageBase,
          PageCount,
          &pagesProcessed);
      }

      if (EFI_ERROR(status))
      {
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
      }

      FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(pagesProcessed <= PageCount);

      if (PageCountProcessed != NULL)
      {
        *PageCountProcessed = (UINT32)pagesProcessed;
      }

      return status;
    }

    //
    // If pages are being made host visible, then revoke page acceptance
    // first.
    //
    if (MapFlags != 0)
    {
      status = EfiUpdatePageRangeAcceptance(
        mIsolationType,
        mSvsmCallingArea,
        GpaPageBase,
        PageCount,
        FALSE);
      if (EFI_ERROR(status))
      {
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
      }
    }

    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(mHvBypassContext.Connected);
  }

#endif

  oldTpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);

  //
  // Simplified version of WinHvpSpecialListRepHypercall with no output parameters
  //
  possibleRepsPerCall = (HV_PAGE_SIZE - sizeof(*pInputBuffer)) / sizeof(HV_GPA_PAGE_NUMBER);

  if (possibleRepsPerCall > WINHVP_MAX_REPS_PER_HYPERCALL)
  {
    FAIL_FAST(EFI_BAD_BUFFER_SIZE, "Invalid host visibility rep count");
  }

  pInputBuffer = (PHV_INPUT_MODIFY_SPARSE_GPA_PAGE_HOST_VISIBILITY)mHvPages->HypercallInputPage;

  for (;;)
  {
    if (PageCount == 0)
    {
      FAIL_FAST(EFI_BAD_BUFFER_SIZE, "Invalid host visibility page count");
    }

    repsProcessedThisCall = 0;

    ZeroMem(pInputBuffer, HV_PAGE_SIZE);

    //
    // Build the input.
    //
    repsInCurrentCall = MIN(possibleRepsPerCall, PageCount);

    if (repsInCurrentCall > WINHVP_MAX_REPS_PER_HYPERCALL)
    {
      FAIL_FAST(EFI_BAD_BUFFER_SIZE, "Invalid host visibility batch");
    }

    //
    // Fill header
    //
    pInputBuffer->TargetPartitionId = HV_PARTITION_ID_SELF;
    pInputBuffer->HostVisibility = MapFlags;

    //
    // Fill page numbers
    // N.B. instead of copying from an existing list of page numbers, we
    // generate a list of consecutive numbers from GpaPageBase.
    //
    for (i = 0; i < repsInCurrentCall; i++, gpaPageBaseIndex++)
    {
      pInputBuffer->GpaPageList[i] = GpaPageBase + gpaPageBaseIndex;
    }

    //
    // Call the hypervisor.
    //
    hvStatus =
      HvHypercallIssue(
        mBypassOnly ? &mHvBypassContext : &mHvContext,
        HvCallModifySparseGpaPageHostVisibility,
        FALSE, // not fast
        repsInCurrentCall,
        EfiHvpBasePa((UINTN)pInputBuffer),
        0, // no output
        &repsProcessedThisCall);
    status = EfiHvConvertStatus(hvStatus);

    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(repsProcessedThisCall <= repsInCurrentCall);
    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(repsProcessedThisCall <= PageCount);
    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE((status != EFI_SUCCESS) || (repsProcessedThisCall == repsInCurrentCall));

    //
    // Update the count of reps processed.
    //
    totalPageCountProcessed += repsProcessedThisCall;

    PageCount -= repsProcessedThisCall;

    if ((status != EFI_SUCCESS) || (PageCount == 0))
    {
      break;
    }
  }

  gBS->RestoreTPL(oldTpl);

#if defined(MDE_CPU_X64)

  if (IsHardwareIsolatedNoParavisorEx(mIsolationType, paravisorPresent))
  {
    //
    // When no paravisor is present, host-generated failure cannot be
    // tolerated.  Fail fast here.
    //
    if (EFI_ERROR(status))
    {
      FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    //
    // If pages are being made not-visible, then accept the pages in
    // hardware.
    //
    if (MapFlags == 0)
    {
      status = EfiUpdatePageRangeAcceptance(
        mIsolationType,
        mSvsmCallingArea,
        GpaPageBase,
        totalPageCountProcessed,
        TRUE);
      if (EFI_ERROR(status))
      {
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
      }
    }
  }

#endif

  if (PageCountProcessed)
  {
    *PageCountProcessed = totalPageCountProcessed;
  }

  return status;
}

EFI_STATUS
EFIAPI
EfiHvMakeAddressRangeHostVisible(
  IN              EFI_HV_IVM_PROTOCOL         *This,
  IN              HV_MAP_GPA_FLAGS            MapFlags,
  IN              VOID                        *BaseAddress,
  IN              UINT32                      ByteCount,
  IN              BOOLEAN                     ZeroPages,
  OUT OPTIONAL    EFI_HV_PROTECTION_HANDLE    *ProtectionHandle
  )
/*++
  Makes a chunk of memory visible to the host.
  Note: Memory visibility changes for hardware-isolated
      systems may change the contents of the pages.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @param MapFlags Access permissions provided to the host.

  @param BaseAddress Base address of memory range.

  @param ByteCount Size of memory block in bytes.

  @param ZeroPages If true, memory range is zeroed after making visible to host.

  @param ProtectionHandle Object used to track memory range.

  @returns EFI status.

--*/
{
  UINT32 pageCountProcessed;
  EFI_HV_PROTECTION_OBJECT *protectionObject;
  EFI_STATUS revertStatus;
  EFI_STATUS status;

  if (!IsIsolatedEx(mIsolationType))
  {
    status = EFI_INVALID_PARAMETER;
    DEBUG((DEBUG_ERROR, "--- %a: visibility changes are only permitted on isolated systems - %r \n", __func__, status));
    return status;
  }

  //
  // All arguments must be page aligned, and the access must imply host
  // visibility.
  //
  if ((((UINTN)BaseAddress & (EFI_PAGE_SIZE - 1)) != 0) ||
    ((ByteCount & (EFI_PAGE_SIZE - 1)) != 0) ||
    ((MapFlags & HV_MAP_GPA_READABLE) == 0) ||
    ((MapFlags & ~(HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE)) != 0))
  {
    status = EFI_INVALID_PARAMETER;
    DEBUG((DEBUG_ERROR, "--- %a: incorrect alignment or access - %r \n", __func__, status));
    return status;
  }

  //
  // Verify that host-read-only is not requested on a system that doesn't
  // support it.
  //
  if (IsHardwareIsolatedEx(mIsolationType) &&
    ((MapFlags & (HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE)) == HV_MAP_GPA_READABLE))
  {
    status = EFI_INVALID_PARAMETER;
    DEBUG((DEBUG_ERROR, "--- %a: invalid host read only request - %r \n", __func__, status));
    return status;
  }

  //
  // Allocate memory to use as a tracking object.
  //
  protectionObject = AllocatePool(sizeof(*protectionObject));
  if (protectionObject == NULL)
  {
    status = EFI_OUT_OF_RESOURCES;
    DEBUG((DEBUG_ERROR, "--- %a: failed to allocate memory - %r \n", __func__, status));
    return status;
  }

  protectionObject->GpaPageBase = (UINTN)BaseAddress / EFI_PAGE_SIZE;
  protectionObject->NumberOfPages = ByteCount / EFI_PAGE_SIZE;

  //
  // If this is a software-isolated VM, then memory must be zeroed before it
  // is made visible to the host, since page contents will remain intact
  // following the visibility change.  For a hardware-isolated VM, memory
  // encryption differences will obscure the original contents following the
  // visibility change.
  //
  if (IsSoftwareIsolatedEx(mIsolationType))
  {
    ZeroMem(BaseAddress, ByteCount);
    ZeroPages = FALSE;
  }

  //
  // Update the visibility as requested.
  //
  status =
    EfiHvpModifySparseGpaPageHostVisibility(
      MapFlags,
      protectionObject->NumberOfPages,
      protectionObject->GpaPageBase,
      &pageCountProcessed);

  if (EFI_ERROR(status))
  {

    //
    // If the protection change was partially made, then undo whatever
    // was done.
    //
    if (pageCountProcessed != 0)
    {
      revertStatus =
        EfiHvpModifySparseGpaPageHostVisibility(
          HV_MAP_GPA_PERMISSIONS_NONE,
          pageCountProcessed,
          protectionObject->GpaPageBase,
          &pageCountProcessed);
      if (EFI_ERROR(revertStatus))
      {

        //
        // This is not allowed to fail - need to fail fast
        //
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
      }
    }

    FreePool(protectionObject);
  }
  else
  {
    InsertTailList(&mHostVisiblePageList, &protectionObject->ListEntry);

    //
    // If zeroing was requested and has not already been performed, then
    // zero the buffer now.
    //
    if (ZeroPages)
    {
      ZeroMem(EfiHvpSharedVa(BaseAddress), ByteCount);
    }

    if (ProtectionHandle != NULL)
    {
      *ProtectionHandle = protectionObject;
    }
  }

  return status;
}

VOID
EFIAPI
EfiHvMakeAddressRangeNotHostVisible(
  IN      EFI_HV_IVM_PROTOCOL *This,
  IN OUT  EFI_HV_PROTECTION_HANDLE *ProtectionHandle
  )
/*++
  Makes a chunk of memory not visible to the host.
  Note: Memory visibility changes for hardware-isolated
      systems may change the contents of the pages.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @param ProtectionHandle Object used to track memory range.

  @returns EFI status.

--*/
{
  EFI_STATUS status;
  EFI_HV_PROTECTION_OBJECT *protectionObject;

  if (ProtectionHandle == NULL || *ProtectionHandle == NULL)
  {
    // fail fast here as this error either indicates a double free or another address range
    // is not made private, which can cause a security issue due to unexpected host visibility.
    FAIL_FAST(EFI_INVALID_PARAMETER, "Invalid protection handle");
  }

  protectionObject = (EFI_HV_PROTECTION_OBJECT *)(*ProtectionHandle);

  RemoveEntryList(&protectionObject->ListEntry);

  status =
    EfiHvpModifySparseGpaPageHostVisibility(
      HV_MAP_GPA_PERMISSIONS_NONE,
      protectionObject->NumberOfPages,
      protectionObject->GpaPageBase,
      NULL);
  if (EFI_ERROR(status))
  {

    //
    // This is not allowed to fail - need to fail fast
    //
    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
  }

  FreePool(protectionObject);
  *ProtectionHandle = NULL;
}
