/** @file
  Shared definitions for RAM disk configuration HOBs passed from PEI to DXE.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Uefi.h>
#include <Library/HobLib.h>

//
// GUID used to identify RAM disk configuration HOBs.
// Each HOB with this GUID contains a RAM_DISK_CONFIG_HOB_DATA structure
// describing a RAM disk region that should be registered via
// EFI_RAM_DISK_PROTOCOL during DXE.
//
#define MSVM_RAM_DISK_CONFIG_HOB_GUID \
  { 0xa41d6c49, 0xaf34, 0x4e34, { 0xa0, 0xb3, 0x12, 0x9a, 0x48, 0x66, 0x70, 0xcd } }

extern EFI_GUID gMsvmRamDiskConfigHobGuid;

//
// HOB payload: describes a RAM disk region to register.
//
typedef struct {
    EFI_PHYSICAL_ADDRESS RamDiskGpa;
    UINT64               RamDiskSize;
} RAM_DISK_CONFIG_HOB_DATA;
