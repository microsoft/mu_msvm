// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once

// intrinsic for NT_ASSERT
//
// This is anathema to its usual use.
// Usually it is inline, preserves all context, uses __annotation to find parameters in pdb.

void NtAssert(void);
