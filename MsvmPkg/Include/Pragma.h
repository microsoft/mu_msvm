// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
#if defined (_MSC_VER) && ! defined (__clang__)
#define PRAGMA(x) __pragma(x)
#else
#define PRAGMA(x) _Pragma(#x)
#endif
