/** @file

    This file contains the code to fail fast.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

#define FAIL_FAST(EfiStatus, Description) \
    FailFastFromMacro(EfiStatus, __FILE__, __LINE__, Description);

#define FAIL_FAST_INITIALIZATION_FAILURE(EfiStatus) \
    FAIL_FAST(EfiStatus, "Critical initialization failure");

#define FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR() \
    FAIL_FAST(EFI_SECURITY_VIOLATION, "Unexpected host behavior");

#define FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(Condition) \
    if (!(Condition)) { FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR() } \

VOID
ReportCrash(
    IN  UINTN              Param0,
    IN  UINTN              Param1,
    IN  UINTN              Param2,
    IN  UINTN              MessageBuffer,
    IN  UINTN              MessageLength
    );

VOID
ResetAfterCrash(
    IN  UINTN              ErrorCode,
    IN  UINTN              Param1,
    IN  UINTN              Param2
    );

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
    IN  UINTN              ErrorCode,
    IN  UINTN              Param1,
    IN  UINTN              Param2,
    IN  UINTN              MessageBuffer,
    IN  UINTN              MessageLength
    );

VOID
FailFastFromMacro(
    IN  UINTN              ErrorCode,
    IN  CONST CHAR8 *      Component,
    IN  UINTN              Line,
    IN  CONST CHAR8 *      Description
    );