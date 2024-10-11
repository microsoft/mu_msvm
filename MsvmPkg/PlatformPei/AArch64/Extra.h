/** @file
  Mmu setup asm interface for ARM64

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

//
// Asm function
//

VOID
EFIAPI
ConfigureCachesAndMmu(
    IN  VOID* TranslationTable,
        UINTN TCR,
        UINTN MAIR
    );