// @file
// DECLSPEC_ALIGN
//
// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
#ifdef __cplusplus
#define DECLSPEC_ALIGN(n) alignas (n)
#else
#ifdef _MSC_VER
#define DECLSPEC_ALIGN(n) __declspec (align (n))
#else
#define DECLSPEC_ALIGN(n) __attribute__ ((aligned (n)))
#endif
#endif
