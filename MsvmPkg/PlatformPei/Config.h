/** @file
  Loader configuration related data and functions.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

//
// Functions
//
UEFI_CONFIG_HEADER*
GetStartOfConfigBlob(
    VOID
    );

EFI_STATUS
GetConfiguration(
    IN CONST EFI_PEI_SERVICES** PeiServices,
    OUT UINT8* PhysicalAddressWidth
    );

EFI_STATUS
GetIgvmConfigInfo(
    VOID
    );

VOID
ConfigSetProcessorInfo(
    UEFI_CONFIG_PROCESSOR_INFORMATION *ProcessorInfo
    );

VOID
ConfigSetUefiConfigFlags(
    UEFI_CONFIG_FLAGS *ConfigFlags
    );
