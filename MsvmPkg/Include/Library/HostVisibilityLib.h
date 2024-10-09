/** @file
  Definitions for functionality provided by host visibility change package.
  These routines will perform the correct platform-specific sequences when
  hardware isolation is in effect with no paravisor present.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Hv/HvGuest.h>

/*++

Routine Description:

    This routine updates hardware page acceptance state on an SNP platform
    that runs with no paravisor.

Arguments:

    IsolationType - Supplies the isolation type of the current platform.

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
EFI_STATUS
EfiUpdatePageRangeAcceptance(
                UINT32              IsolationType,
    IN OPTIONAL VOID                *SvsmCallingArea,
                HV_GPA_PAGE_NUMBER  StartingPageNumber,
                UINT64              PageCount,
                BOOLEAN             Accept
    );


/*++

Routine Description:

    This routine makes a page visible to the host on a hardware isolated
    platform that runs with no paravisor.

Arguments:

    IsolationType - Supplies the isolation type of the current platform.

    SvsmCallingArea - If an SVSM is present, supplies a pointer to the SVSM
                      calling area, otherwise supplies NULL.

    SharedBoundaryGpa - Supplies the shared boundary GPA for the current
                        platform.

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
EFI_STATUS
EfiMakePageRangeHostVisible(
                    UINT32              IsolationType,
    IN OPTIONAL     VOID                *SvsmCallingArea,
                    HV_GPA_PAGE_NUMBER  StartingPageNumber,
                    UINT64              PageCount,
    OUT OPTIONAL    UINT64              *PagesProcessed
    );

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
EFI_STATUS
EfiMakePageRangeHostNotVisible(
                    UINT32              IsolationType,
    IN OPTIONAL     VOID                *SvsmCallingArea,
                    HV_GPA_PAGE_NUMBER  PageNumber,
                    UINT64              PageCount,
    OUT OPTIONAL    UINT64              *PagesProcessed
    );
