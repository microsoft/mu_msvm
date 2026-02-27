// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
// inline is complicated.
// Minor:
//   Msvc __forceinline inline warns 4141. inline __forceinline does not.
// Significant:
//   Msvc __forceinline is extern, but unused functions are discarded and not guaranteed linkable,
//   unless extern __forceinline, which would seem redundant, but in this case, "extern" means "keep".
//   __forceinline is therefore *usually* like __attribute__((always_inline)) static inline,
//   unless it is extern.
//   It would be mostly constructive to #define __forceinline static inline __forceinline
//   but this breaks the ability to say extern __forceinline.
//   Therefore __forceinline can be made usually portable, but extern __forceinline cannot.
//   Use extern ALWAYS_INLINE instead, or sometimes ALWAYS_INLINE suffices.
//
// The story is really more complicated.
// If compilation options are consistent, then ultimately, all code should be moved out of headers,
//   then inline becomes useless, and only extern __forceinline or __attribute__((always_inline)) useful.
// However code in headers is also useful to inherit its callers compiler flags.
#if defined (__clang__) || defined (__GNUC__)
#define ALWAYS_INLINE __attribute__ ((always_inline))
#define __forceinline __attribute__ ((always_inline)) static inline
#define NO_INLINE     __attribute__ ((noinline))
#elif defined (_MSC_VER)
#pragma warning (disable:4141)
#define ALWAYS_INLINE __forceinline
#define NO_INLINE __declspec (noinline)
#else
#error unknown compiler
#endif
