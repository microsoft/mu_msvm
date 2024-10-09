/** @file
  Hob-building functionality.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <PiPei.h>

void
HobAddMmioRange(
    EFI_PHYSICAL_ADDRESS BaseAddress,
    UINT64               Size
    );


void
HobAddMemoryRange(
    IN OUT  PPLATFORM_INIT_CONTEXT  Context,
            EFI_PHYSICAL_ADDRESS       BaseAddress,
            UINT64                     Size
    );


void
HobAddPersistentMemoryRange(
    EFI_PHYSICAL_ADDRESS BaseAddress,
    UINT64               Size
    );

void
HobAddSpecificPurposeMemoryRange(
    EFI_PHYSICAL_ADDRESS BaseAddress,
    UINT64               Size
    );

void
HobAddReservedMemoryRange(
    EFI_PHYSICAL_ADDRESS BaseAddress,
    UINT64               Size
    );


void
HobAddUntestedMemoryRange(
    IN OUT  PPLATFORM_INIT_CONTEXT  Context,
            EFI_PHYSICAL_ADDRESS BaseAddress,
            UINT64               Size
    );


void
HobAddAllocatedMemoryRange(
    EFI_PHYSICAL_ADDRESS BaseAddress,
    UINT64               Size
    );


void
HobAddFvMemoryRange(
    EFI_PHYSICAL_ADDRESS BaseAddress,
    UINT64               Size
    );


void
HobAddIoRange(
    EFI_PHYSICAL_ADDRESS BaseAddress,
    UINT64               Size
    );


void
HobAddCpu(
    UINT8 SizeOfMemorySpace,
    UINT8 SizeOfIoSpace
    );


void
HobAddGuidData(
    IN  EFI_GUID* Guid,
    IN  VOID*     Data,
        UINTN     DataSize
  );
