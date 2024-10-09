/** @file
  Mmu setup for ARM64

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

//
// Functions
//
EFI_STATUS
EFIAPI
ConfigureMmu(
    UINT64  MaxAddress
    );
