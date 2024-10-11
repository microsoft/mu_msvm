
/** @file
  This file contains architecture specific functions for debugging a failure.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/CrashLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>

#include <Hv/HvGuestMsr.h>

/**
  Called when a fatal error is detected and the system cannot continue.
  It is not expected that this function returns.

  @param  ErrorCode     Bugcheck code
  @param  Param1        Bugcheck code specific parameter.
  @param  Param2        Bugcheck code specific parameter.
  @param  Param3        Bugcheck code specific parameter.
  @param  Param4        Bugcheck code specific parameter.

**/
VOID
FailFast(
    IN  UINTN               ErrorCode,
    IN  UINTN               Param1,
    IN  UINTN               Param2,
    IN  UINTN               MessageBuffer,
    IN  UINTN               MessageLength
    )
{
    DEBUG((EFI_D_ERROR, "FailFast invoked.\n"));
    ASSERT(FALSE);

    ReportCrash(ErrorCode, Param1, Param2, MessageBuffer, MessageLength);

    ResetAfterCrash(ErrorCode, Param1, Param2);
}

VOID
FailFastFromMacro(
    IN  UINTN               ErrorCode,
    IN  CONST CHAR8 *       Component,
    IN  UINTN               Line,
    IN  CONST CHAR8 *       Description
    )
{
    CHAR8 buffer[HV_CRASH_MAXIMUM_MESSAGE_SIZE];

    //
    // Write the crash message.
    //
    UINTN bytes = AsciiSPrint(buffer,
                               HV_CRASH_MAXIMUM_MESSAGE_SIZE,
                               "MsvmPkg FAIL_FAST\nDESCRIPTION: %a\nERROR: %d\nCOMPONENT: %a\nLINE: %d\n",
                               Description,
                               ErrorCode,
                               Component,
                               Line);

    DEBUG((EFI_D_ERROR, "\n%a\n", buffer));

    FailFast(ErrorCode, 0, Line, (UINTN)&buffer, bytes);
}