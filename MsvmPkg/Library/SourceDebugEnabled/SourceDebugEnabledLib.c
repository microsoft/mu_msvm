/** @file
  This module implements routines that indicate if source debugging is
  runtime enabled for DXE only.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <PiDxe.h>
#include <Base.h>
#include <Library/SourceDebugEnabledLib.h>
#include <Library/HobLib.h>

/**
  Check if source debugging is runtime enabled.

  @InitFlag      Supplies a value that indicates what kind of initialization
                 is being performed. Ignored.

  @return TRUE   Source debugging is enabled.
  @return FALSE  Source debugging is not enabled.

**/
BOOLEAN
EFIAPI
IsSourceDebugEnabled (
  IN UINT32 InitFlag
  )
{
    BOOLEAN debugEnabled = FALSE;

    //
    // There are two ways to figure out if debugging is enabled:
    //      1. Use the PcdDebuggerEnabled set in PEI
    //      2. Use the hob passed that contains if the debugger is enabled
    //
    // We use the hob here since we don't know exactly when this function could
    // be called. If it's called before PCDs are available, early on in DxeCore,
    // the system will die in mysterious ways.
    //
    // This is the same behavior done with the older debug stubs on X64.
    //
    void* hob = GetFirstGuidHob(&gMsvmDebuggerEnabledGuid);
    if (hob != NULL)
    {
        debugEnabled = *((BOOLEAN *)GET_GUID_HOB_DATA(hob));
    }
    else
    {
        // We should always be passing this HOB.
        // ASSERT(FALSE);
    }

    return debugEnabled;
}
