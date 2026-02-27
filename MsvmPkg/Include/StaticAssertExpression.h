// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once

// Compile iff 'expr' is true, yielding 'value'.
// see also: Common\MU_TIANO\SecurityPkg\DeviceSecurity\SpdmLib\libspdm\os_stub\mbedtlslib\mbedtls\library\common.h
#define STATIC_ASSERT_EXPRESSION(expr, value) \
    (sizeof (char [(expr) ? 1 : -1]), (value))
