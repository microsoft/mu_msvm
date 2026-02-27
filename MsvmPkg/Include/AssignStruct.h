// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
#include "StaticAssertExpression.h"

// Clang issues memcpy calls, which do not link.
// Consider implementing it, that calls CopyMem.
// Assert equal sizes and assignability. Just in case
// someone tries something like mixing char and int.
// If the types are actually structs or unions,
// assignability is identity.
#if defined (__clang__) || defined (__GNUC__) || !defined (_MSC_VER)
#define ASSIGN_STRUCT(dest, src)                \
    ((void)CopyMem (                            \
        (dest)                                  \
        (src),                                  \
        STATIC_ASSERT_EXPRESSION (              \
            sizeof (*(dest)) == sizeof (*(src)) \
            && sizeof (*(dest) = (*(src))),     \
            sizeof (*(src)))))
#else
#define ASSIGN_STRUCT(dest, src)                \
    ((void)STATIC_ASSERT_EXPRESSION (           \
        sizeof (*(dest)) == sizeof (*(src)),    \
        (*(dest) = (*(src)))))
#endif
