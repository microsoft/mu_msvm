// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <Library/BaseLib.h>
#include "MsBarrier.h"
#include "MsStdAtomic.h"

// CompilerBarrier is only a barrier for memory accesses.
// It does not impede all optimizations.
void CompilerBarrier (void)
{
#if EFI_STD_ATOMIC
    atomic_signal_fence (memory_order_seq_cst);
#elif _MSC_VER
    _ReadWriteBarrier ();
#else
#error unknown target
#endif
}

void MemoryBarrier (void)
{
    // UEFI Upstream MemoryFence varies greatly.
    //
    // ARM:  Correct. dmb.
    // Gcc:  Very close to correct.
    // Msvc: Does nothing, broken.
    //
    CompilerBarrier ();
#if EFI_STD_ATOMIC
    atomic_thread_fence (memory_order_seq_cst);
#elif _M_AMD64
    __faststorefence ();
#elif _M_ARM64
    MemoryFence ();
#else
#error unknown target
#endif
}
