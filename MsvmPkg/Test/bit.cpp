// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <stdint.h>
#if !(defined (__clang__) || defined (__GNUC__))
#include "intrin.h"
#endif
#include "../Library/MsBaseLib/Barrier.c"
#include "../Library/MsBaseLib/Bit.c"
#include "../Library/MsBaseLib/VolatileAccessors.c"

// Slow portable version that works with any compiler for witness testing.
// Only for test purposes.
uint8_t SlowPortable_BitScanForward64 (uint32_t *pindex, uint64_t value)
{
    uint32_t index = 0;
    uint64_t const one = 1;
    uint32_t const limit = (sizeof (value) * 8);
    for (index = 0; (index < limit) && !(value & (one << index)); ++index) ;
    *pindex = index;
    return index != limit;
}

#include <string.h>
#include <random>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>

void
Test_SetBit (
    void   *clean,
    void   *portable,
    void   *msvc,
    uint64_t size,
    uint64_t index
    )
// This function "witness tests" the portable function SetBit
// with what it replaces, Visual C++ _bittestandset64.
{
    assert (memcmp (portable, clean, size) == 0);
    assert (memcmp (msvc, clean, size) == 0);
#ifdef _MSC_VER
    SetBit (portable, index);
    _bittestandset64 ((int64_t*)msvc, index);
    assert (memcmp (portable, msvc, size) == 0);
    assert (memcmp (portable, clean, size));
    assert (memcmp (msvc, clean, size));
#endif
}

void
Test_BitScan (uint64_t value)
{
    uint32_t fast_index{};
    uint32_t slowPortable_index{};

    uint8_t fast_result = BitScanForward64 (&fast_index, value);
    uint8_t slowPortable_result = SlowPortable_BitScanForward64 (&slowPortable_index, value);

    assert (fast_result == slowPortable_result);
    assert (!fast_result || (fast_index == slowPortable_index));
}

int main ()
{
    std::random_device random_device;
    std::mt19937 twist (random_device ());
    std::uniform_int_distribution<uint64_t> random;

    for (uint64_t i = 0; i < 1000; ++i)
    {
        int64_t data1 [1000]{};
        int64_t data2 [1000]{};
        int64_t data3 [1000]{};
        Test_SetBit (data1, data2, data3, sizeof (data1), i);
    }

    for (uint64_t i = 0; i < 100000000; ++i)
        Test_BitScan (random (twist));

    printf ("success\n");

    return 0;
}
