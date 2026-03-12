// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
#include "MinMaxType.h"
#include "StaticAssertExpression.h"
// If value is unsigned and type is signed, this always fails.
#define CAST_CONSTANT(value, type) \
  STATIC_ASSERT_EXPRESSION(value >= MIN_TYPE(type) && value <= MAX_TYPE(type), (type)(value))
