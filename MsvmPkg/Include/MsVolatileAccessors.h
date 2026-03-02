// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once

// See volatile_accessors.h in Windows SDK.
UINT32 ReadAcquire (volatile const UINT32*);
void WriteRelease  (volatile UINT32*, UINT32);
