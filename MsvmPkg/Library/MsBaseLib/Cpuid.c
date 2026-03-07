// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifdef MDE_CPU_X64
#include <Base.h>
#include <Library/BaseLib.h>
#include "MsCpuid.h"

void MsCpuid (unsigned info[4], int type)
{
    AsmCpuid (type, /*Eax*/&info[0], /*Ebx*/&info[1], /*Ecx*/&info[2], /*Edx*/&info[3]);
}

#endif
