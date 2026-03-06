// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <Base.h>
#include "MsStdAtomic.h"
#include "MsVolatileAccessors.h"

UINT32 ReadAcquire (UINT32 const volatile *ptr)
{
#if EFI_STD_ATOMIC
    return atomic_load_explicit ((UINT32 _Atomic const volatile*)ptr, memory_order_acquire);
#elif _M_ARM64 || _M_ARM64EC
    return __ldar32 (ptr);
#elif _M_AMD64
    return *ptr;
#else
#error unknown target
#endif
}

void WriteRelease (volatile UINT32 *ptr, UINT32 value)
{
#if EFI_STD_ATOMIC
    atomic_store_explicit ((UINT32 _Atomic volatile*)ptr, value, memory_order_release);
#elif _M_ARM64 || _M_ARM64EC
    __stlr32 (ptr, value);
#elif _M_AMD64
    *ptr = value;
#else
#error unknown target
#endif
}
