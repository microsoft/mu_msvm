// @file
// DECLSPEC_CACHEALIGN
//   AMD64: DECLSPEC_ALIGN (64)
//   ARM64: DECLSPEC_ALIGN (128)
//
// There are varying ways to chose these values.
// These are chosen for layout compatibility with Windows.
// See also C++: std::hardware_destructive_interference_size
//               std::hardware_constructive_interference_size
//
// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
#include "DeclspecAlign.h"
#if defined (MDE_CPU_X64)
#define DECLSPEC_CACHEALIGN DECLSPEC_ALIGN (64)
#elif defined (MDE_CPU_AARCH64)
#define DECLSPEC_CACHEALIGN DECLSPEC_ALIGN (128)
#else
#error unknown target
#endif
