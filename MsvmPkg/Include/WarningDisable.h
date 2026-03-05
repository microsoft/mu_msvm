// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
#include "Pragma.h"

#define CLANG_WARNING_DISABLE(x)
#define GCC_WARNING_DISABLE(x)
#define MS_WARNING_DISABLE(x)

#if defined (__clang__)
#undef CLANG_WARNING_DISABLE
#define CLANG_WARNING_DISABLE(x) PRAGMA(clang diagnostic ignored #x)
#elif defined (__GNUC__)
#undef GCC_WARNING_DISABLE
#define GCC_WARNING_DISABLE(x) PRAGMA(GCC diagnostic ignored #x)
#elif defined (_MSC_VER)
#undef MS_WARNING_DISABLE
#define MS_WARNING_DISABLE(x) PRAGMA(warning(disable:x))
#endif
