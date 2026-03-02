// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent

// Visual C++ stdatomic.h is unusable.
// https://developercommunity.visualstudio.com/t/stdatomich-does-not-work-C11/11051100
#if defined (__clang__) || defined (__GNUC__) || !defined (_MSC_VER)
#include <stdatomic.h>
#define EFI_STD_ATOMIC 1
#else
#define EFI_STD_ATOMIC 0
#endif
