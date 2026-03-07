// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#include "MsBit.h"

UINT8 BitScanForward64 (UINT32 *Index, UINT64 Value)
{
    // There are no bits set.
    if (Value == 0)
        return 0;

    // There is at least one bit set.

// https://gcc.gnu.org/onlinedocs/gcc/Bit-Operation-Builtins.html
#if defined (__clang__) || defined (__GNUC__)
    *Index = __builtin_ctzll (Value);
#elif defined (_MSC_VER)
    unsigned long temp = 0;
    _BitScanForward64 (&temp, Value);
    *Index = temp;
#else
    UINT32 i = 0;
    UINT64 const one = 1;
    UINT32 const limit = (sizeof (Value) * 8);
    for (i = 0; i < limit && !(Value & (one << i)); ++i) ;
    *Index = i;
#endif
    return 1;
}

void SetBit (void *Bits, size_t Index)
// Not atomic.
// Like Intel bts and Visual C++ _bittestandset64 except:
//   no return value
//   void*; the type does not matter in Visual C++ despite there being 32 and 64 versions.
//   unsigned index
{
    *((Index >> 3) + (UINT8*)Bits) |= (1 << (Index & 7));
}
