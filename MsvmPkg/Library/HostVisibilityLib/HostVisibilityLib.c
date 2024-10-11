/** @file
  This file implements routines to update host visibility of memory.  These
  routines will perform the correct platform-specific sequences when
  hardware isolation is in effect with no paravisor present.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Hv/HvGuest.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Uefi/UefiBaseType.h>
#include <IsolationTypes.h>

UINT64
_sev_pvalidate(
    IN  VOID    *Address,
        UINT32  PageSize,
        UINT32  Validate,
    OUT UINT64  *ErrorCode
    );

UINT64
VispCallSvsm(
    UINT64 CallCode,
    UINT64 Parameter
    );

#define SNP_SUCCESS             0
#define SNP_FAIL_INPUT          1
#define SNP_FAIL_SIZEMISMATCH   6

UINT64
SpecialGhcbCall(
    UINT64 GhcbValue
    );

#pragma warning(disable : 4201)
typedef union _GHCB_MSR
{
    UINT64 AsUINT64;
    struct
    {
        UINT64 GhcbLow : 32;
        UINT64 GhcbHigh : 32;
    };

    struct
    {
        UINT64 GhcbInfo : 12;
        UINT64 GpaPageNumber : 40;
        UINT64 ExtraData : 12;
    };

} GHCB_MSR;
#pragma warning(default : 4201)

#define GHCB_INFO_PAGE_STATE_CHANGE     0x014
#define GHCB_INFO_PAGE_STATE_UPDATED    0x015

#define GHCB_DATA_PAGE_STATE_PRIVATE    0x001
#define GHCB_DATA_PAGE_STATE_SHARED     0x002

typedef struct _SVSM_PVALIDATE {
    UINT8 CallPending;
    UINT8 Reserved1[7];
    UINT16 NumberOfEntries;
    UINT16 NextEntryIndex;
    UINT32 Reserved2;
} SVSM_PVALIDATE, *PSVSM_PVALIDATE;

#define SVSM_PVALIDATE_SIZE_MASK        0x0001
#define SVSM_PVALIDATE_VALIDATE_MASK    0x0004

#define SVSM_CORE_PVALIDATE         0x00000001

#define SVSM_SUCCESS                0
#define SVSM_ERR_INCOMPLETE         0x80000000
#define SVSM_ERR_PVALIDATE          0x80001000
#define SVSM_ERR_PVALIDATE_SIZE_MISMATCH \
    (SVSM_ERR_PVALIDATE + SNP_FAIL_SIZEMISMATCH)

#define TDX_SUCCESS                 0
#define TDX_PAGE_SIZE_MISMATCH      0xC0000B0B
#define TDX_PAGE_ALREADY_ACCEPTED   0x00000B0A
#define TDX_VMCALL_RETRY            1

#define TDX_TDG_STATUS(_status_) ((_status_) >> 32)

#pragma warning(disable : 4201)
typedef union _TDX_ACCEPT_GPA {
    UINT64 AsUINT64;
    struct {
        UINT64 PageSize : 2;
        UINT64 ReservedZ : 10;
        UINT64 GpaPageNumber : 52;
    };
} TDX_ACCEPT_GPA, *PTDX_ACCEPT_GPA;
#pragma warning(default : 4201)

UINT64
_tdx_tdg_mem_page_accept(
    TDX_ACCEPT_GPA AcceptGpa
    );

UINT64
_tdx_vmcall_map_gpa(
                    UINT64  Gpa,
                    UINT64  Size,
    OUT OPTIONAL    UINT64  *FailedGpa
    );


EFI_STATUS
EfiUpdatePageRangeAcceptanceSnp(
    HV_GPA_PAGE_NUMBER  StartingPageNumber,
    UINT64              PageCount,
    BOOLEAN             Accept
    )
/*++

Routine Description:

    This routine updates hardware page acceptance state on an SNP platform
    that runs with no paravisor.

Arguments:

    StartingPageNumber - Supplies the starting GPA page number of the range to
                         change.

    PageCount - Supplies the number of pages to change.

    Accept - Supplies TRUE if the pages are to be accepted, or FALSE if the
             pages are to be unaccepted.

Return Value:

    Note that an error in this call is not recoverable. The caller must take the
    appropriate action to fail fast. This lib can be called from PEI and DXE
    therefore this lib does not perform phase specific fail fast calls.

--*/
{
    UINT64 errorCode;

    while (PageCount != 0)
    {
        //
        // Attempt to validate a 2 MB page if possible.
        //

        if (((StartingPageNumber & (SIZE_2MB - 1)) == 0) &&
            (PageCount >= SIZE_2MB))
        {
            if (_sev_pvalidate(
                (VOID*)(StartingPageNumber * EFI_PAGE_SIZE),
                1,
                Accept,
                &errorCode) != 0)
            {
                errorCode = SNP_FAIL_INPUT;
            }

            if (errorCode == SNP_SUCCESS)
            {
                StartingPageNumber += SIZE_2MB / EFI_PAGE_SIZE;
                PageCount -= SIZE_2MB / EFI_PAGE_SIZE;
                continue;
            }
            else if (errorCode != SNP_FAIL_SIZEMISMATCH)
            {
                return EFI_SECURITY_VIOLATION;
            }
        }

        if (_sev_pvalidate(
            (VOID*)(StartingPageNumber * EFI_PAGE_SIZE),
            0,
            Accept,
            &errorCode) != 0)
        {
            errorCode = SNP_FAIL_INPUT;
        }

        if (errorCode != SNP_SUCCESS)
        {
            return EFI_SECURITY_VIOLATION;
        }

        StartingPageNumber += 1;
        PageCount -= 1;
    }

    return EFI_SUCCESS;
}


EFI_STATUS
EfiUpdatePageRangeAcceptanceSnpSvsm(
    IN  VOID                *SvsmCallingArea,
        HV_GPA_PAGE_NUMBER  StartingPageNumber,
        UINT64              PageCount,
        BOOLEAN             Accept
    )
/*++

Routine Description:

    This routine updates hardware page acceptance state on an SNP platform
    that uses an SVSM.

Arguments:

    SvsmCallingArea - Supplies a pointer to the SVSM calling area.

    StartingPageNumber - Supplies the starting GPA page number of the range to
                         change.

    PageCount - Supplies the number of pages to change.

    Accept - Supplies TRUE if the pages are to be accepted, or FALSE if the
             pages are to be unaccepted.

Return Value:

    Note that an error in this call is not recoverable. The caller must take the
    appropriate action to fail fast. This lib can be called from PEI and DXE
    therefore this lib does not perform phase specific fail fast calls.

--*/
{
    UINT64 errorCode;
    UINT32 index;
    HV_GPA_PAGE_NUMBER largePageSize;
    UINT32 maximumEntries;
    UINT32 numberOfEntries;
    UINT64* pageArray;
    UINT32 pageIndex;
    HV_GPA_PAGE_NUMBER pageNumber;
    UINT64 pagesRemaining;
    PSVSM_PVALIDATE pvalidate;

    //
    // Locate the SVSM calling area.  This page will be used as the parameter page.
    //

    pvalidate = SvsmCallingArea;

    maximumEntries = (EFI_PAGE_SIZE - ((UINTN)pvalidate & (EFI_PAGE_SIZE - 1))) / sizeof(UINTN);
    numberOfEntries = sizeof(SVSM_PVALIDATE) / sizeof(UINTN);
    if (maximumEntries <= numberOfEntries)
    {
        return EFI_SECURITY_VIOLATION;
    }
    maximumEntries -= numberOfEntries;
    pageArray = (UINT64*)(pvalidate + 1);

    largePageSize = SIZE_2MB / HV_PAGE_SIZE;

    while (PageCount != 0)
    {
        //
        // Fill the parameter page with as many entries as will fit.
        //

        errorCode = SVSM_SUCCESS;
        pageIndex = 0;
        pageNumber = StartingPageNumber;
        pagesRemaining = PageCount;
        numberOfEntries = 0;
        while ((pagesRemaining != 0) && (numberOfEntries < maximumEntries))
        {
            pageArray[numberOfEntries] = pageNumber * EFI_PAGE_SIZE;
            pageArray[numberOfEntries] |= SVSM_PVALIDATE_VALIDATE_MASK;

            //
            // Insert a large page entry if possible, but only if the last
            // call did not fail with a size mismatch error.
            //

            if (((pageNumber & (largePageSize - 1)) == 0) &&
                (pagesRemaining >= largePageSize) &&
                (errorCode != SVSM_ERR_PVALIDATE_SIZE_MISMATCH))
            {
                pageArray[numberOfEntries] |= SVSM_PVALIDATE_SIZE_MASK;
                pageNumber += largePageSize;
                pagesRemaining -= largePageSize;
            }
            else
            {
                pageNumber += 1;
                pagesRemaining -= 1;
                errorCode = SVSM_SUCCESS;
            }

            numberOfEntries += 1;
        }

        //
        // Call the SVSM to process as many pages as possible.
        //

        pvalidate->NumberOfEntries = (UINT16)numberOfEntries;
        pvalidate->NextEntryIndex = 0;
        pvalidate->CallPending = 1;
        while (pvalidate->CallPending)
        {
            errorCode = VispCallSvsm(SVSM_CORE_PVALIDATE,
                                     (UINTN)&pvalidate->NumberOfEntries);
        }

        //
        // If the call failed and the failure was not due to a size mismatch,
        // then fail immediately.
        //

        if ((errorCode != SVSM_SUCCESS) &&
            (errorCode != SVSM_ERR_INCOMPLETE) &&
            (errorCode != SVSM_ERR_PVALIDATE_SIZE_MISMATCH))
        {
            return EFI_SECURITY_VIOLATION;
        }

        //
        // Consume as many entries as were successful.
        //

        for (index = 0; index < pvalidate->NextEntryIndex; index += 1)
        {
            if (pageArray[index] & SVSM_PVALIDATE_SIZE_MASK)
            {
                ASSERT(((pageArray[index] / EFI_PAGE_SIZE) & (largePageSize - 1)) == 0);
                ASSERT(PageCount >= largePageSize);
                StartingPageNumber += largePageSize;
                PageCount -= largePageSize;
            }
            else
            {
                StartingPageNumber += 1;
                PageCount -= 1;
            }
        }
    }

    return EFI_SUCCESS;
}


EFI_STATUS
EfiUpdatePageRangeAcceptanceTdx(
    HV_GPA_PAGE_NUMBER  StartingPageNumber,
    UINT64              PageCount
    )
/*++

Routine Description:

    This routine updates hardware page acceptance state on a TDX platform that
    runs with no paravisor.

Arguments:

    StartingPageNumber - Supplies the starting GPA page number of the range to
                         change.

    PageCount - Supplies the number of pages to change.

Return Value:

    EFI_STATUS.

    Note that an error in this call is not recoverable. The caller must take the
    appropriate action to fail fast. This lib can be called from PEI and DXE
    therefore this lib does not perform phase specific fail fast calls.

--*/
{
    TDX_ACCEPT_GPA acceptGpa;
    HV_GPA_PAGE_NUMBER largePageSize;
    UINT64 errorCode;

    acceptGpa.AsUINT64 = 0;
    acceptGpa.GpaPageNumber = StartingPageNumber;

    largePageSize = SIZE_2MB / HV_PAGE_SIZE;

    while (PageCount != 0)
    {
        //
        // Attempt to validate a 2 MB page if possible.
        //

        if (((acceptGpa.GpaPageNumber & (largePageSize - 1)) == 0) &&
            (PageCount >= largePageSize))
        {
            acceptGpa.PageSize = 1;
            errorCode = _tdx_tdg_mem_page_accept(acceptGpa);
            if (TDX_TDG_STATUS(errorCode) == TDX_SUCCESS)
            {
                acceptGpa.GpaPageNumber += largePageSize;
                PageCount -= largePageSize;
                continue;
            }
            else if (TDX_TDG_STATUS(errorCode) != TDX_PAGE_SIZE_MISMATCH)
            {
                DEBUG((DEBUG_VERBOSE,
                       "Failed to accept (large) page at 0x%lx errorCode 0x%lx\n",
                       acceptGpa.GpaPageNumber,
                       errorCode));

                return EFI_SECURITY_VIOLATION;
            }
        }

        acceptGpa.PageSize = 0;
        errorCode = _tdx_tdg_mem_page_accept(acceptGpa);
        if (TDX_TDG_STATUS(errorCode) != TDX_SUCCESS)
        {
            DEBUG((DEBUG_VERBOSE,
                   "Failed to accept (small) page at 0x%lx errorCode 0x%lx\n",
                   acceptGpa.GpaPageNumber,
                   errorCode));

            return EFI_SECURITY_VIOLATION;
        }

        acceptGpa.GpaPageNumber += 1;
        PageCount -= 1;
    }

    return EFI_SUCCESS;
}


EFI_STATUS
EfiUpdatePageRangeAcceptance(
                UINT32              IsolationType,
    IN OPTIONAL VOID                *SvsmCallingArea,
                HV_GPA_PAGE_NUMBER  StartingPageNumber,
                UINT64              PageCount,
                BOOLEAN             Accept
    )
/*++

Routine Description:

    This routine updates hardware page acceptance state on an SNP platform
    that runs with no paravisor.

Arguments:

    IsolationType - Supplies the isolation type of the current platform.

    SvsmCallingArea - If an SVSM is present, supplies a pointer to the SVSM
                      calling area, otherwise supplies NULL.

    StartingPageNumber - Supplies the starting GPA page number of the range to
                         change.

    PageCount - Supplies the number of pages to change.

    Accept - Supplies TRUE if the pages are to be accepted, or FALSE if the
             pages are to be unaccepted.

Return Value:

    EFI_STATUS.

    Note that an error in this call is not recoverable. The caller must take the
    appropriate action to fail fast. This lib can be called from PEI and DXE
    therefore this lib does not perform phase specific fail fast calls.

--*/
{
    if (IsolationType == UefiIsolationTypeTdx)
    {
        //
        // No action is required when acceptance is being revoked.
        //

        if (Accept)
        {
            return EfiUpdatePageRangeAcceptanceTdx(StartingPageNumber, PageCount);
        }
    }
    else
    {
        ASSERT(IsolationType == UefiIsolationTypeSnp);

        if (SvsmCallingArea != NULL)
        {
            return EfiUpdatePageRangeAcceptanceSnpSvsm(SvsmCallingArea,
                                                       StartingPageNumber,
                                                       PageCount,
                                                       Accept);
        }
        else
        {
            return EfiUpdatePageRangeAcceptanceSnp(StartingPageNumber,
                                                   PageCount,
                                                   Accept);
        }
    }

    return EFI_SUCCESS;
}


EFI_STATUS
VispPvalidateSinglePage(
    IN OPTIONAL VOID                *SvsmCallingArea,
                HV_GPA_PAGE_NUMBER  PageNumber,
                BOOLEAN             Validate
    )
/*++

Routine Description:

    This routine executes PVALIDATE for a single page, either directly or via
    a call to the SVSM.

Arguments:

    SvsmCallingArea - If an SVSM is present, supplies a pointer to the SVSM
                      calling area, otherwise supplies NULL.

    PageNumber - Supplies the GPA page number to process.

    Validate - Supplies the validation argument for PVALIDATE.

Return Value:

    EFI_STATUS.

--*/
{
    UINT64 errorCode;
    UINT64* pageArray;
    SVSM_PVALIDATE *pvalidate;

    if (SvsmCallingArea != NULL)
    {
        //
        // Locate the SVSM calling area.  This page will be used as the parameter page.
        //

        pvalidate = SvsmCallingArea;
        pageArray = (UINT64*)(pvalidate + 1);
        *pageArray = PageNumber * EFI_PAGE_SIZE;
        if (Validate)
        {
            *pageArray |= SVSM_PVALIDATE_VALIDATE_MASK;
        }

        pvalidate->NumberOfEntries = 1;
        pvalidate->NextEntryIndex = 0;
        pvalidate->CallPending = 1;
        while (pvalidate->CallPending)
        {
            errorCode = VispCallSvsm(SVSM_CORE_PVALIDATE,
                                     (UINTN)&pvalidate->NumberOfEntries);
        }

        if (errorCode != SVSM_SUCCESS)
        {
            return EFI_SECURITY_VIOLATION;
        }
    }
    else
    {
        if (_sev_pvalidate((VOID*)(PageNumber * EFI_PAGE_SIZE), 0, 0, &errorCode) != 0)
        {
            return EFI_SECURITY_VIOLATION;
        }

        if (errorCode != 0)
        {
            return EFI_SECURITY_VIOLATION;
        }
    }

    return EFI_SUCCESS;
}


EFI_STATUS
EfiMakePageRangeHostVisibleSnp(
    IN OPTIONAL     VOID                *SvsmCallingArea,
                    HV_GPA_PAGE_NUMBER  StartingPageNumber,
                    UINT64              PageCount,
    OUT OPTIONAL    UINT64              *PagesProcessed
    )
/*++

Routine Description:

    This routine makes a range of pages visible to the host on an SNP platform
    that runs with no paravisor.

Arguments:

    SvsmCallingArea - If an SVSM is present, supplies a pointer to the SVSM
                      calling area, otherwise supplies NULL.

    StartingPageNumber - Supplies the starting GPA page number of the range to
                         make visible.

    PageCount - Supplies the number of pages to make visible.

    PagesProcessed - Supplies an optional pointer to a page that should
                     receive the number of pages that were successfully
                     processed.

Return Value:

    EFI_STATUS.

    Note that an error in this call is not recoverable. The caller must take the
    appropriate action to fail fast. This lib can be called from PEI and DXE
    therefore this lib does not perform phase specific fail fast calls.

--*/
{
    GHCB_MSR ghcbMsr;
    EFI_STATUS status;

    if (PagesProcessed != NULL)
    {
        *PagesProcessed = 0;
    }

    while (PageCount != 0)
    {
        //
        // Ensure this page is no longer a valid private address.
        //

        status = VispPvalidateSinglePage(SvsmCallingArea, StartingPageNumber, FALSE);
        if (EFI_ERROR(status))
        {
            return status;
        }

        //
        // Request a page conversion via the GHCB register protocol.
        //

        ghcbMsr.AsUINT64 = 0;
        ghcbMsr.GhcbInfo = GHCB_INFO_PAGE_STATE_CHANGE;
        ghcbMsr.GpaPageNumber = StartingPageNumber;
        ghcbMsr.ExtraData = GHCB_DATA_PAGE_STATE_SHARED;

        ghcbMsr.AsUINT64 = SpecialGhcbCall(ghcbMsr.AsUINT64);

        if (ghcbMsr.AsUINT64 != GHCB_INFO_PAGE_STATE_UPDATED)
        {
            //
            // Restore this page to an accepted state since the visibility was not
            // modified.
            //

            VispPvalidateSinglePage(SvsmCallingArea, StartingPageNumber, TRUE);
            return EFI_SECURITY_VIOLATION;
        }

        if (PagesProcessed != NULL)
        {
            *PagesProcessed += 1;
        }

        StartingPageNumber += 1;
        PageCount -= 1;
    }

    return EFI_SUCCESS;
}


EFI_STATUS
EfiChangePageRangeHostVisibilityTdx(
                    HV_GPA  StartingGpa,
                    UINT64  PageCount,
                    BOOLEAN Visible,
    OUT OPTIONAL    UINT64  *PagesProcessed
    )
/*++

Routine Description:

    This routine changes the host visibility of a range of pages on a TDX
    platform.

Arguments:

    StartingGpa - Supplies the starting GPA of the range to make visible. This refers to the base
        (private) GPA alias for the page.

    PageCount - Supplies the number of pages to make visible.

    Visible - TRUE if the pages are to be made host visible, FALSE otherwise.

    PagesProcessed - Supplies an optional pointer to a page that should
                     receive the number of pages that were successfully
                     processed.

Return Value:

    EFI_STATUS.

    Note that an error in this call is not recoverable. The caller must take the
    appropriate action to fail fast. This lib can be called from PEI and DXE
    therefore this lib does not perform phase specific fail fast calls.

--*/
{
    EFI_STATUS acceptStatus;
    HV_GPA nextGpa;
    UINT64 pagesProcessed;
    EFI_STATUS status;
    UINT64 errorCode;
    HV_GPA sharedGpaBoundary;

    //
    // Request a page conversion via the MapPage GHCI call.
    //

    sharedGpaBoundary = PcdGet64(PcdIsolationSharedGpaBoundary);
    ASSERT(StartingGpa < sharedGpaBoundary);

    if (Visible)
    {
        StartingGpa += sharedGpaBoundary;
    }

    status = EFI_SUCCESS;
    pagesProcessed = 0;
    nextGpa = StartingGpa;
    while (pagesProcessed < PageCount)
    {
        errorCode = _tdx_vmcall_map_gpa(
            nextGpa,
            (PageCount - pagesProcessed) * HV_PAGE_SIZE,
            &nextGpa);

        if (TDX_TDG_STATUS(errorCode) != TDX_SUCCESS)
        {
            DEBUG((DEBUG_VERBOSE,
                   "MapPage GHCI call failed at GPA = 0x%lx with error code 0x%lx",
                   nextGpa,
                   errorCode));
            status = EFI_SECURITY_VIOLATION;
        }
        else if (errorCode == TDX_SUCCESS)
        {
            pagesProcessed = PageCount;
            break;
        }
        else
        {
            ASSERT(errorCode == TDX_VMCALL_RETRY);
        }

        //
        // If the count of pages processed is not reasonable, then proceed as
        // if the call failed entirely.
        //

        pagesProcessed = (nextGpa - StartingGpa) / HV_PAGE_SIZE;
        if ((pagesProcessed > PageCount) || (nextGpa & EFI_PAGE_MASK))
        {
            pagesProcessed = 0;
            status = EFI_SECURITY_VIOLATION;
        }

        if (EFI_ERROR(status))
        {
            break;
        }
    }

    //
    // If pages are being made private, then reaccept any pages that were
    // successfully processed.
    //

    if ((pagesProcessed != 0) && !Visible)
    {
        acceptStatus = EfiUpdatePageRangeAcceptanceTdx(
            StartingGpa / HV_PAGE_SIZE,
            pagesProcessed);

        if (EFI_ERROR(acceptStatus))
        {
            status = acceptStatus;
        }
    }

    if (PagesProcessed != NULL)
    {
        *PagesProcessed = pagesProcessed;
    }

    return status;
}


EFI_STATUS
EfiMakePageRangeHostVisible(
                    UINT32              IsolationType,
    IN OPTIONAL     VOID                *SvsmCallingArea,
                    HV_GPA_PAGE_NUMBER  StartingPageNumber,
                    UINT64              PageCount,
    OUT OPTIONAL    UINT64              *PagesProcessed
    )
/*++

Routine Description:

    This routine makes a page visible to the host on a hardware isolated
    platform that runs with no paravisor.

Arguments:

    IsolationType - Supplies the isolation type of the current platform.

    SvsmCallingArea - If an SVSM is present, supplies a pointer to the SVSM
                      calling area, otherwise supplies NULL.

    StartingPageNumber - Supplies the starting GPA page number of the range to
                         make visible.

    PageCount - Supplies the number of pages to make visible.

    PagesProcessed - Supplies an optional pointer to a page that should
                     receive the number of pages that were successfully
                     processed.

Return Value:

    EFI_STATUS.

    Note that an error in this call is not recoverable. The caller must take the
    appropriate action to fail fast. This lib can be called from PEI and DXE
    therefore this lib does not perform phase specific fail fast calls.

--*/
{
    switch (IsolationType)
    {
    case UefiIsolationTypeSnp:
        return EfiMakePageRangeHostVisibleSnp(
            SvsmCallingArea,
            StartingPageNumber,
            PageCount,
            PagesProcessed);

    case UefiIsolationTypeTdx:
        return EfiChangePageRangeHostVisibilityTdx(
            StartingPageNumber * HV_PAGE_SIZE,
            PageCount,
            TRUE,
            PagesProcessed);
    }

    return EFI_INVALID_PARAMETER;
}


EFI_STATUS
EfiMakePageRangeHostNotVisibleSnp(
    IN OPTIONAL     VOID                *SvsmCallingArea,
                    HV_GPA_PAGE_NUMBER  StartingPageNumber,
                    UINT64              PageCount,
    OUT OPTIONAL    UINT64              *PagesProcessed
    )
/*++

Routine Description:

    This routine makes a range of pages private to the guest (not visible to
    the host) on an SNP platform that runs with no paravisor.

Arguments:

    SvsmCallingArea - If an SVSM is present, supplies a pointer to the SVSM
                      calling area, otherwise supplies NULL.

    StartingPageNumber - Supplies the starting GPA page number of the range to
                         make private.

    PageCount - Supplies the number of pages to make private.

    PagesProcessed - Supplies an optional pointer to a page that should
                     receive the number of pages that were successfully
                     processed.

Return Value:

    EFI_STATUS.

    Note that an error in this call is not recoverable. The caller must take the
    appropriate action to fail fast. This lib can be called from PEI and DXE
    therefore this lib does not perform phase specific fail fast calls.

--*/
{
    GHCB_MSR ghcbMsr;
    EFI_STATUS status;

    if (PagesProcessed != NULL)
    {
        *PagesProcessed = 0;
    }

    while (PageCount != 0)
    {
        //
        // Request a page conversion via the GHCB register protocol.
        //

        ghcbMsr.AsUINT64 = 0;
        ghcbMsr.GhcbInfo = GHCB_INFO_PAGE_STATE_CHANGE;
        ghcbMsr.GpaPageNumber = StartingPageNumber;
        ghcbMsr.ExtraData = GHCB_DATA_PAGE_STATE_SHARED;

        ghcbMsr.AsUINT64 = SpecialGhcbCall(ghcbMsr.AsUINT64);

        if (ghcbMsr.AsUINT64 != GHCB_INFO_PAGE_STATE_UPDATED)
        {
            return EFI_SECURITY_VIOLATION;
        }

        //
        // Validate this page to make it accessible again.
        //

        status = VispPvalidateSinglePage(SvsmCallingArea, StartingPageNumber, TRUE);
        if (EFI_ERROR(status))
        {
            return status;
        }

        if (PagesProcessed != NULL)
        {
            *PagesProcessed += 1;
        }

        StartingPageNumber += 1;
        PageCount -= 1;
    }

    return EFI_SUCCESS;
}


EFI_STATUS
EfiMakePageRangeHostNotVisible(
                    UINT32              IsolationType,
    IN OPTIONAL     VOID                *SvsmCallingArea,
                    HV_GPA_PAGE_NUMBER  StartingPageNumber,
                    UINT64              PageCount,
    OUT OPTIONAL    UINT64              *PagesProcessed
    )
/*++

Routine Description:

    This routine makes a page private to the guest (not visible to the host)
    on a hardware isolated platform that runs with no paravisor.

Arguments:

Arguments:

    IsolationType - Supplies the isolation type of the current platform.

    SvsmCallingArea - If an SVSM is present, supplies a pointer to the SVSM
                      calling area, otherwise supplies NULL.

    StartingPageNumber - Supplies the starting GPA page number of the range to
                         make not visible.

    PageCount - Supplies the number of pages to make not visible.

    PagesProcessed - Supplies an optional pointer to a page that should
                     receive the number of pages that were successfully
                     processed.

Return Value:

    EFI_STATUS.

    Note that an error in this call is not recoverable. The caller must take the
    appropriate action to fail fast. This lib can be called from PEI and DXE
    therefore this lib does not perform phase specific fail fast calls.

--*/
{
    switch (IsolationType)
    {
    case UefiIsolationTypeSnp:
        return EfiMakePageRangeHostNotVisibleSnp(
            SvsmCallingArea,
            StartingPageNumber,
            PageCount,
            PagesProcessed);

    case UefiIsolationTypeTdx:
        return EfiChangePageRangeHostVisibilityTdx(
            StartingPageNumber * HV_PAGE_SIZE,
            PageCount,
            FALSE,
            PagesProcessed);
    }

    return EFI_INVALID_PARAMETER;
}
