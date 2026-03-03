// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once

// This is a lightweight compiler barrier that inhibits
// compiler optimizations only, of memory accesses only.
// It generates no code, except call/ret which can be optimized away.
void CompilerBarrier (void);

// A full memory barrier, for all loads and all stores.
// This includes CompilerBarrier, and again, does not inhibit optimizing non-memory accesses.
void MemoryBarrier (void);
