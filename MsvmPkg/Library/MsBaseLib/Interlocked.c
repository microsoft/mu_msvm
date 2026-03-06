// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <Base.h>
#include "MsInterlocked.h"
#include "MsStdAtomic.h"

INT64 InterlockedExchange64(INT64 volatile* ptr, INT64 val)
{
#if EFI_STD_ATOMIC
    return atomic_exchange((INT64 _Atomic volatile*)ptr, val);
#else
    return _InterlockedExchange64(ptr, val);
#endif
}
