/** @file
  Implements EFI_HV_IVM_PROTOCOL DMA pinning support.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "EfiHvInternal.h"

#define HV_GPA_PAGE_RANGE_MAX_PAGE_COUNT 0x800

STATIC_ASSERT(HV_GPA_PAGE_PINNING_MAX_RANGE_COUNT <= WINHVP_MAX_REPS_PER_HYPERCALL,
              "The GPA page pinning max range count must fit in a rep hypercall");

STATIC
UINT32
EfiHvCountGpaPageRangePages(
  IN  PHV_GPA_PAGE_RANGE GpaRangeList,
  IN  UINT32             RangeCount
  )
{
  UINT32 i;
  UINT32 pageCount;

  pageCount = 0;
  for (i = 0; i < RangeCount; i++)
  {
    pageCount += (UINT32)GpaRangeList[i].AdditionalPages + 1;
  }

  return pageCount;
}

STATIC
VOID
EfiHvBuildGpaPageRangeList(
  OUT PHV_GPA_PAGE_RANGE GpaRangeList,
  IN  UINT32             MaxRangeCount,
  IN  HV_GPA_PAGE_NUMBER GpaPageBase,
  IN  UINT32             PageCount,
  OUT UINT32             *RangeCount,
  OUT UINT32             *RangePageCount
  )
{
  UINT32 pagesInRange;

  *RangeCount = 0;
  *RangePageCount = 0;

  while ((*RangeCount < MaxRangeCount) && (PageCount != 0))
  {
    pagesInRange = MIN(PageCount, HV_GPA_PAGE_RANGE_MAX_PAGE_COUNT);

    GpaRangeList[*RangeCount].AdditionalPages = pagesInRange - 1;
    GpaRangeList[*RangeCount].LargePage = 0;
    GpaRangeList[*RangeCount].BasePfn = GpaPageBase + *RangePageCount;

    *RangeCount += 1;
    *RangePageCount += pagesInRange;
    PageCount -= pagesInRange;
  }
}

EFI_STATUS
EfiHvpPinUnpinGpaPageRanges(
  IN              BOOLEAN             Pin,
  IN              UINT32              PageCount,
  IN              HV_GPA_PAGE_NUMBER  GpaPageBase,
  OUT OPTIONAL    UINT32              *PageCountProcessed
  )
/*++
  Handles the HvCallPinGpaPageRanges/HvCallUnpinGpaPageRanges hypercalls.

  @param Pin TRUE to pin, FALSE to unpin.

  @param PageCount The number of pages to pin/unpin.

  @param GpaPageBase Supplies the address of the first target GPA. The
              remaining pages will be modified sequentially from this GPA.

  @param PageCountProcessed If present, the number of pages that are successfully processed
                will be returned in this.

  @returns EFI status.

--*/
{
  PHV_INPUT_GPA_PAGE_PINNING pInputBuffer;
  HV_STATUS hvStatus;
  EFI_STATUS status;
  EFI_TPL oldTpl;
  UINT32 possibleRepsPerCall;
  UINT32 pagesProcessedThisCall;
  UINT32 rangePageCount;
  UINT32 rangesInCurrentCall;
  UINT32 rangesProcessedThisCall;
  UINT32 totalPageCountProcessed;

  if (PageCountProcessed != NULL)
  {
    *PageCountProcessed = 0;
  }

  if (PageCount == 0)
  {
    DEBUG((DEBUG_ERROR, "%a: 0 page count\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  pInputBuffer = (PHV_INPUT_GPA_PAGE_PINNING)mHvPages->HypercallInputPage;
  possibleRepsPerCall = HV_GPA_PAGE_PINNING_MAX_RANGE_COUNT;

  totalPageCountProcessed = 0;
  oldTpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);

  for (;;)
  {
    if (PageCount == 0)
    {
      FAIL_FAST(EFI_BAD_BUFFER_SIZE, "Invalid GPA page pinning page count");
    }

    rangesProcessedThisCall = 0;

    ZeroMem(pInputBuffer, HV_PAGE_SIZE);

    EfiHvBuildGpaPageRangeList(
      pInputBuffer->GpaRangeList,
      possibleRepsPerCall,
      GpaPageBase + totalPageCountProcessed,
      PageCount,
      &rangesInCurrentCall,
      &rangePageCount);

    if ((rangesInCurrentCall == 0) ||
      (rangesInCurrentCall > WINHVP_MAX_REPS_PER_HYPERCALL) ||
      (rangePageCount > PageCount))
    {
      FAIL_FAST(EFI_BAD_BUFFER_SIZE, "Invalid GPA page pinning batch");
    }

    hvStatus =
      HvHypercallIssue(
        mBypassOnly ? &mHvBypassContext : &mHvContext,
        Pin ? HvCallPinGpaPageRanges : HvCallUnpinGpaPageRanges,
        FALSE, // not fast
        rangesInCurrentCall,
        EfiHvpBasePa((UINTN)pInputBuffer),
        0, // no output
        &rangesProcessedThisCall);
    status = EfiHvConvertStatus(hvStatus);

    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(rangesProcessedThisCall <= rangesInCurrentCall);
    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE((status != EFI_SUCCESS) || (rangesProcessedThisCall == rangesInCurrentCall));

    pagesProcessedThisCall =
      EfiHvCountGpaPageRangePages(
        pInputBuffer->GpaRangeList,
        rangesProcessedThisCall);
    totalPageCountProcessed += pagesProcessedThisCall;

    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(pagesProcessedThisCall <= PageCount);
    PageCount -= pagesProcessedThisCall;

    if ((status != EFI_SUCCESS) || (PageCount == 0))
    {
      break;
    }
  }

  gBS->RestoreTPL(oldTpl);

  if (PageCountProcessed != NULL)
  {
    *PageCountProcessed = totalPageCountProcessed;
  }

  return status;
}

EFI_STATUS
EfiHvpTrackPinnedGpaPageRange(
  IN  HV_GPA_PAGE_NUMBER  RequestGpaPageBase,
  IN  UINT32              RequestPageCount,
  IN  HV_GPA_PAGE_NUMBER  PinnedGpaPageBase,
  IN  UINT32              PinnedPageCount
  )
{
  EFI_HV_PIN_OBJECT *pinObject;

  if (PinnedPageCount == 0)
  {
    FAIL_FAST(EFI_INVALID_PARAMETER, "Invalid tracked pin range");
  }

  pinObject = AllocatePool(sizeof(*pinObject));
  if (pinObject == NULL)
  {
    return EFI_OUT_OF_RESOURCES;
  }

  pinObject->RequestGpaPageBase = RequestGpaPageBase;
  pinObject->RequestNumberOfPages = RequestPageCount;
  pinObject->GpaPageBase = PinnedGpaPageBase;
  pinObject->NumberOfPages = PinnedPageCount;
  InsertTailList(&mPinnedPageList, &pinObject->ListEntry);

  return EFI_SUCCESS;
}

UINT32
EfiHvpUnpinTrackedGpaPageRanges(
  IN  HV_GPA_PAGE_NUMBER  RequestGpaPageBase,
  IN  UINT32              RequestPageCount
  )
{
  LIST_ENTRY *entry;
  LIST_ENTRY *nextEntry;
  EFI_HV_PIN_OBJECT *pinObject;
  EFI_STATUS status;
  UINT32 rangesUnpinned;

  rangesUnpinned = 0;
  for (entry = GetFirstNode(&mPinnedPageList);
     !IsNull(&mPinnedPageList, entry);
     entry = nextEntry)
  {
    nextEntry = GetNextNode(&mPinnedPageList, entry);
    pinObject = BASE_CR(entry, EFI_HV_PIN_OBJECT, ListEntry);
    if ((pinObject->RequestGpaPageBase != RequestGpaPageBase) ||
      (pinObject->RequestNumberOfPages != RequestPageCount))
    {
      continue;
    }

    RemoveEntryList(&pinObject->ListEntry);

    status =
      EfiHvpPinUnpinGpaPageRanges(
        FALSE,
        pinObject->NumberOfPages,
        pinObject->GpaPageBase,
        NULL);
    if (EFI_ERROR(status))
    {
      FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    FreePool(pinObject);
    rangesUnpinned += 1;
  }

  return rangesUnpinned;
}

VOID
EfiHvpGetGpaRangeBounds(
  IN  CONST HV_GPA_PAGE_RANGE *Range,
  OUT UINT64                  *BaseGpaPage,
  OUT UINT64                  *EndGpaPage
  )
{
  UINT64 pageCount;

  if (Range->LargePage)
  {
    *BaseGpaPage = Range->BaseLargePfn * EFI_SIZE_TO_PAGES(SIZE_2MB);
    if (Range->PageSize == HV_GPA_PAGE_RANGE_PAGE_SIZE_1GB)
    {
      pageCount = (Range->AdditionalPages + 1) * EFI_SIZE_TO_PAGES(SIZE_1GB);
    }
    else
    {
      pageCount = (Range->AdditionalPages + 1) * EFI_SIZE_TO_PAGES(SIZE_2MB);
    }
  }
  else
  {
    *BaseGpaPage = Range->BasePfn;
    pageCount = Range->AdditionalPages + 1;
  }

  *EndGpaPage = *BaseGpaPage + pageCount;
}

EFI_STATUS
EfiHvpQueryAlwaysPinnedSubranges(
  IN  HV_GPA_PAGE_NUMBER                  GpaPageBase,
  IN  UINT32                              PageCount,
  OUT PHV_OUTPUT_QUERY_GPA_RANGE_SUBRANGES *Subranges
  )
{
  PHV_INPUT_QUERY_GPA_RANGE input;
  PHV_OUTPUT_QUERY_GPA_RANGE output;
  HV_STATUS hvStatus;
  EFI_STATUS status;

  input = (PHV_INPUT_QUERY_GPA_RANGE)mHvPages->HypercallInputPage;
  output = (PHV_OUTPUT_QUERY_GPA_RANGE)mHvPages->HypercallOutputPage;

  ZeroMem(input, HV_PAGE_SIZE);
  ZeroMem(output, HV_PAGE_SIZE);

  input->InfoClass = HvQueryGpaRangeAlwaysPinnedSubranges;
  input->StartGpn = GpaPageBase;
  input->PageCount = PageCount;

  hvStatus =
    HvHypercallIssue(
      mUseBypassContext ? &mHvBypassContext : &mHvContext,
      HvCallQueryInformationGpaRange,
      FALSE,
      0,
      EfiHvpBasePa((UINTN)input),
      EfiHvpBasePa((UINTN)output),
      NULL);
  status = EfiHvConvertStatus(hvStatus);
  if (EFI_ERROR(status))
  {
    DEBUG((DEBUG_ERROR,
         "%a: HvCallQueryInformationGpaRange failed for GPN 0x%lx pages %u: %r\n",
         __func__,
         GpaPageBase,
         PageCount,
         status));
    return status;
  }

  *Subranges = &output->AlwaysPinnedSubranges;
  return EFI_SUCCESS;
}

EFI_STATUS
EfiHvpPinAndTrackGpaPageRange(
  IN      HV_GPA_PAGE_NUMBER  RequestGpaPageBase,
  IN      UINT32              RequestPageCount,
  IN      HV_GPA_PAGE_NUMBER  GpaPageBase,
  IN      UINT32              PageCount,
  IN OUT  BOOLEAN             *PinApplied
  )
{
  UINT32 pageCountProcessed;
  EFI_STATUS status;

  if (PageCount == 0)
  {
    FAIL_FAST(EFI_INVALID_PARAMETER, "Invalid GPA page pinning range");
  }

  pageCountProcessed = 0;
  status =
    EfiHvpPinUnpinGpaPageRanges(
      TRUE,
      PageCount,
      GpaPageBase,
      &pageCountProcessed);
  if (EFI_ERROR(status))
  {
    DEBUG((DEBUG_ERROR,
         "%a: failed to pin GPN 0x%lx pages %u: %r (processed %u)\n",
         __func__,
         GpaPageBase,
         PageCount,
         status,
         pageCountProcessed));
    if (pageCountProcessed != 0)
    {
      if (EFI_ERROR(EfiHvpPinUnpinGpaPageRanges(FALSE, pageCountProcessed, GpaPageBase, NULL)))
      {
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
      }
    }

    return status;
  }

  status = EfiHvpTrackPinnedGpaPageRange(
         RequestGpaPageBase,
         RequestPageCount,
         GpaPageBase,
         PageCount);
  if (EFI_ERROR(status))
  {
    if (EFI_ERROR(EfiHvpPinUnpinGpaPageRanges(FALSE, PageCount, GpaPageBase, NULL)))
    {
      FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    return status;
  }

  *PinApplied = TRUE;
  return EFI_SUCCESS;
}

EFI_STATUS
EfiHvpPinGpaPageRangeSkippingAlwaysPinnedPages(
  IN      HV_GPA_PAGE_NUMBER  RequestGpaPageBase,
  IN      UINT32              RequestPageCount,
  IN      HV_GPA_PAGE_NUMBER  GpaPageBase,
  IN      UINT32              PageCount,
  IN OUT  BOOLEAN             *PinApplied
  )
{
  PHV_OUTPUT_QUERY_GPA_RANGE_SUBRANGES subranges;
  UINT64 currentGpaPage;
  UINT64 endGpaPage;
  UINT64 subrangeBase;
  UINT64 subrangeEnd;
  UINT32 maxSubrangeCount;
  UINT32 firstHalfPageCount;
  UINT32 gapPageCount;
  UINT16 index;
  EFI_STATUS status;

  if (PageCount == 0)
  {
    FAIL_FAST(EFI_INVALID_PARAMETER, "Invalid GPA page pinning range");
  }

  status = EfiHvpQueryAlwaysPinnedSubranges(GpaPageBase, PageCount, &subranges);
  if (EFI_ERROR(status))
  {
    return status;
  }

  maxSubrangeCount =
    (HV_PAGE_SIZE - sizeof(HV_OUTPUT_QUERY_GPA_RANGE_SUBRANGES)) /
    sizeof(HV_GPA_PAGE_RANGE);
  if (subranges->SubrangeCount > maxSubrangeCount)
  {
    return EFI_BAD_BUFFER_SIZE;
  }

  if ((subranges->SubrangeCount == maxSubrangeCount) && (PageCount > 1))
  {
    firstHalfPageCount = PageCount / 2;
    status =
      EfiHvpPinGpaPageRangeSkippingAlwaysPinnedPages(
        RequestGpaPageBase,
        RequestPageCount,
        GpaPageBase,
        firstHalfPageCount,
        PinApplied);
    if (EFI_ERROR(status))
    {
      return status;
    }

    return EfiHvpPinGpaPageRangeSkippingAlwaysPinnedPages(
           RequestGpaPageBase,
           RequestPageCount,
           GpaPageBase + firstHalfPageCount,
           PageCount - firstHalfPageCount,
           PinApplied);
  }

  if (subranges->SubrangeCount != 0)
  {
    DEBUG((DEBUG_INFO,
         "%a: GPN 0x%lx pages %u has %u always-pinned subranges; pinning gaps only\n",
         __func__,
         GpaPageBase,
         PageCount,
         subranges->SubrangeCount));
  }

  currentGpaPage = GpaPageBase;
  endGpaPage = GpaPageBase + PageCount;
  for (index = 0; index < subranges->SubrangeCount; index += 1)
  {
    EfiHvpGetGpaRangeBounds(&subranges->SubrangeList[index], &subrangeBase, &subrangeEnd);
    DEBUG((DEBUG_INFO,
         "%a: always-pinned subrange GPN 0x%lx-0x%lx\n",
         __func__,
         subrangeBase,
         subrangeEnd));
    if ((subrangeEnd <= currentGpaPage) || (subrangeBase >= endGpaPage))
    {
      continue;
    }

    if (subrangeBase > currentGpaPage)
    {
      gapPageCount = (UINT32)(subrangeBase - currentGpaPage);
      DEBUG((DEBUG_INFO,
           "%a: pinning gap GPN 0x%lx pages %u before always-pinned GPN 0x%lx-0x%lx\n",
           __func__,
           currentGpaPage,
           gapPageCount,
           subrangeBase,
           subrangeEnd));
      status = EfiHvpPinAndTrackGpaPageRange(
             RequestGpaPageBase,
             RequestPageCount,
             currentGpaPage,
             gapPageCount,
             PinApplied);
      if (EFI_ERROR(status))
      {
        return status;
      }
    }

    currentGpaPage = MIN(subrangeEnd, endGpaPage);
  }

  if (currentGpaPage < endGpaPage)
  {
    gapPageCount = (UINT32)(endGpaPage - currentGpaPage);
    if (subranges->SubrangeCount != 0)
    {
      DEBUG((DEBUG_INFO,
           "%a: pinning trailing gap GPN 0x%lx pages %u\n",
           __func__,
           currentGpaPage,
           gapPageCount));
    }

    status = EfiHvpPinAndTrackGpaPageRange(
           RequestGpaPageBase,
           RequestPageCount,
           currentGpaPage,
           gapPageCount,
           PinApplied);
  }

  return status;
}

//
// EFI_HV_IVM_PROTOCOL pinning entry points.
//

EFI_STATUS
EFIAPI
EfiHvPinAddressRange(
  IN              EFI_HV_IVM_PROTOCOL *This,
  IN              VOID                *BaseAddress,
  IN              UINT32              ByteCount,
  OUT OPTIONAL    BOOLEAN             *PinApplied
  )
/*++
  Pins a chunk of guest physical memory for host DMA.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @param BaseAddress Base address of memory range.

  @param ByteCount Size of memory block in bytes.

  @param PinApplied If present, receives TRUE when the range was pinned and must be unpinned.

  @returns EFI status.

--*/
{
  UINT32 pageCount;
  UINT64 gpaPageBase;
  BOOLEAN pinApplied;
  EFI_STATUS status;

  if (PinApplied != NULL)
  {
    *PinApplied = FALSE;
  }

  if ((((UINTN)BaseAddress & (EFI_PAGE_SIZE - 1)) != 0) ||
    ((ByteCount & (EFI_PAGE_SIZE - 1)) != 0) ||
    (ByteCount == 0))
  {
    status = EFI_INVALID_PARAMETER;
    DEBUG((DEBUG_ERROR, "--- %a: incorrect alignment or size - %r \n", __func__, status));
    return status;
  }

  pageCount = ByteCount / EFI_PAGE_SIZE;
  gpaPageBase = (UINTN)BaseAddress / EFI_PAGE_SIZE;
  pinApplied = FALSE;

  status =
    EfiHvpPinGpaPageRangeSkippingAlwaysPinnedPages(
      gpaPageBase,
      pageCount,
      gpaPageBase,
      pageCount,
      &pinApplied);
  if (EFI_ERROR(status))
  {
    EfiHvpUnpinTrackedGpaPageRanges(gpaPageBase, pageCount);
    return status;
  }

  if (PinApplied != NULL)
  {
    *PinApplied = pinApplied;
  }

  return EFI_SUCCESS;
}

VOID
EFIAPI
EfiHvUnpinAddressRange(
  IN  EFI_HV_IVM_PROTOCOL *This,
  IN  VOID                *BaseAddress,
  IN  UINT32              ByteCount
  )
/*++
  Unpins a chunk of guest physical memory previously pinned for host DMA.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @param BaseAddress Base address of memory range.

  @param ByteCount Size of memory block in bytes.

--*/
{
  UINT64 gpaPageBase;
  UINT32 pageCount;
  UINT32 rangesUnpinned;

  if ((((UINTN)BaseAddress & (EFI_PAGE_SIZE - 1)) != 0) ||
    ((ByteCount & (EFI_PAGE_SIZE - 1)) != 0) ||
    (ByteCount == 0))
  {
    FAIL_FAST(EFI_INVALID_PARAMETER, "Invalid pin range");
  }

  gpaPageBase = (UINTN)BaseAddress / EFI_PAGE_SIZE;
  pageCount = ByteCount / EFI_PAGE_SIZE;

  rangesUnpinned = EfiHvpUnpinTrackedGpaPageRanges(gpaPageBase, pageCount);
  if (rangesUnpinned == 0)
  {
    FAIL_FAST(EFI_INVALID_PARAMETER, "Unpin of untracked range");
  }
}
