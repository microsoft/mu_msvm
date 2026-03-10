// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
// PRAGMA has the advantage over #pragma that it is not a preprocessing directive.
// It can occur in expressions.
//
// Pragmas never change the meaning of a program.
// It should work with them all removed.
// MSC does support standard _Pragma but so far __pragma feels easier to use.
#if defined (_MSC_VER) && !defined (__clang__)
#define PRAGMA(x) __pragma(x)
#else
#define PRAGMA(x) _Pragma(#x)
#endif

#define CLANG_PRAGMA(x)
#define GCC_PRAGMA(x)
#define MS_PRAGMA(x)

#if defined (__clang__)
#undef CLANG_PRAGMA
#define CLANG_PRAGMA PRAGMA
#elif defined (__GNUC__)
#undef GCC_PRAGMA
#define GCC_PRAGMA PRAGMA
#elif defined (_MSC_VER)
#undef MS_PRAGMA
#define MS_PRAGMA PRAGMA
#endif
