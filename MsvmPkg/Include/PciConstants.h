/** @file
  Common PCIe/MCFG constants shared across MsvmPkg PCI libraries.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>

//
// PCIe ECAM: 32 devices * 8 functions * 4KB config space = 1 MB per bus.
//
#define PCIE_ECAM_BYTES_PER_BUS  (256ULL * 4096)

//
// PNP0A08 — PCI Express Root Complex.
//
#define PCIE_ROOT_COMPLEX_HID  EISA_PNP_ID (0x0A08)

//
// Shorthand for the verbose MCFG types.
//
typedef EFI_ACPI_MEMORY_MAPPED_ENHANCED_CONFIGURATION_SPACE_BASE_ADDRESS_ALLOCATION_STRUCTURE
    MCFG_ALLOCATION_ENTRY;

typedef EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_HEADER
    MCFG_TABLE_HEADER;
