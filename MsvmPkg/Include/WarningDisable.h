// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
#include "Pragma.h"
#define CLANG_WARNING_DISABLE(x) CLANG_PRAGMA(clang diagnostic ignored #x)
#define GCC_WARNING_DISABLE(x) GCC_PRAGMA(GCC diagnostic ignored #x)
#define MS_WARNING_DISABLE(x) MS_PRAGMA(warning(disable:x))
