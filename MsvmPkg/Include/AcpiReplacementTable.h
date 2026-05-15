/** @file
  Shared definitions for VMM-provided ACPI table replacements passed via HOBs.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Uefi.h>

//
// GUID used to identify ACPI replacement table HOBs.
// Each HOB with this GUID contains a complete ACPI table starting with
// EFI_ACPI_DESCRIPTION_HEADER. When an ACPI table from the firmware volume
// has the same signature as a replacement table HOB, the FV table is
// suppressed and the HOB-provided table is installed instead.
//
#define ACPI_REPLACEMENT_TABLE_HOB_GUID \
  { 0xa24aef4c, 0xa824, 0x48e9, { 0xb5, 0xb8, 0xbc, 0xd9, 0x6a, 0x59, 0x71, 0xaf } }

extern EFI_GUID gAcpiReplacementTableHobGuid;
