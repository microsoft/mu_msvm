/** @file
  This file implements helper routines to facilitate Isolation checks.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <IsolationTypes.h>

UINT32
GetIsolationType(
    )
{
    return PcdGet32(PcdIsolationArchitecture);
}

BOOLEAN
IsParavisorPresent(
    )
{
#if defined(MDE_CPU_AARCH64)
    return FALSE;
#else
    return PcdGetBool(PcdIsolationParavisorPresent);
#endif
}

BOOLEAN
IsIsolatedEx(
    UINT32 IsolationType
    )
{
    return (IsolationType != UefiIsolationTypeNone);
}

BOOLEAN
IsIsolated(
    )
{
    return IsIsolatedEx(GetIsolationType());
}

BOOLEAN
IsHardwareIsolatedEx(
    UINT32 IsolationType
    )
{
#if defined(MDE_CPU_AARCH64)
    return FALSE;
#else
    return (IsolationType >= UefiIsolationTypeSnp);
#endif
}

BOOLEAN
IsHardwareIsolated(
    )
{
#if defined(MDE_CPU_AARCH64)
    return FALSE;
#else
    return IsHardwareIsolatedEx(GetIsolationType());
#endif
}

BOOLEAN
IsSoftwareIsolatedEx(
    UINT32 IsolationType
    )
{
    return (IsolationType == UefiIsolationTypeVbs);
}

BOOLEAN
IsSoftwareIsolated(
    )
{
    return IsSoftwareIsolatedEx(GetIsolationType());
}

BOOLEAN
IsHardwareIsolatedNoParavisorEx(
    UINT32 IsolationType,
    BOOLEAN IsParavisorPresent
    )
{
#if defined(MDE_CPU_AARCH64)
    return FALSE;
#else
    return (IsHardwareIsolatedEx(IsolationType) && !IsParavisorPresent);
#endif
}

BOOLEAN
IsHardwareIsolatedNoParavisor(
    )
{
#if defined(MDE_CPU_AARCH64)
    return FALSE;
#else
    return IsHardwareIsolatedNoParavisorEx(GetIsolationType(), IsParavisorPresent());
#endif
}