// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
#include <stddef.h>

UINT8 BitScanForward64 (UINT32 *index, UINT64 value);

void SetBit (void *Bits, size_t Index);
// Not atomic.
// Like Intel bts and Visual C++ _bittestandset64 except:
//   no return value
//   void*; the type does not matter in Visual C++ despite there being 32 and 64 versions.
//   unsigned index
