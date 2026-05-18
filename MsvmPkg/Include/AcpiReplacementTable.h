/** @file
  Shared definitions for VMM-provided ACPI table replacements passed via HOBs.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Uefi.h>
#include <Library/HobLib.h>
#include <IndustryStandard/Acpi.h>

//
// GUID used to identify ACPI replacement table HOBs.
// Each HOB with this GUID contains an ACPI_REPLACEMENT_TABLE_HOB_DATA
// structure that points to a complete ACPI table (starting with
// EFI_ACPI_DESCRIPTION_HEADER) in persistent memory. When an ACPI table from
// the firmware volume has the same signature as a replacement table HOB, the
// FV table is suppressed and the HOB-referenced table is installed instead.
//
#define ACPI_REPLACEMENT_TABLE_HOB_GUID \
  { 0xa24aef4c, 0xa824, 0x48e9, { 0xb5, 0xb8, 0xbc, 0xd9, 0x6a, 0x59, 0x71, 0xaf } }

extern EFI_GUID gAcpiReplacementTableHobGuid;

//
// HOB payload: pointer to an ACPI table in persistent memory.
//
typedef struct {
    EFI_ACPI_DESCRIPTION_HEADER *Table;
} ACPI_REPLACEMENT_TABLE_HOB_DATA;

//
// Helper: get the ACPI table header from a replacement table HOB.
// HobRaw must point to a valid GUIDed HOB with gAcpiReplacementTableHobGuid.
//
static inline EFI_ACPI_DESCRIPTION_HEADER *
AcpiReplacementTableFromHob(
    IN VOID *HobRaw
    )
{
    EFI_HOB_GUID_TYPE *GuidHob = (EFI_HOB_GUID_TYPE *)HobRaw;
    ACPI_REPLACEMENT_TABLE_HOB_DATA *Data =
        (ACPI_REPLACEMENT_TABLE_HOB_DATA *)GET_GUID_HOB_DATA(GuidHob);
    return Data->Table;
}

//
// Helper: get the first replacement table HOB, returning the raw HOB pointer.
// Returns NULL if none exist. Use with AcpiReplacementTableFromHob() and
// GetNextAcpiReplacementTableHob() to iterate.
//
static inline VOID *
GetFirstAcpiReplacementTableHob(
    VOID
    )
{
    return GetFirstGuidHob(&gAcpiReplacementTableHobGuid);
}

//
// Helper: get the next replacement table HOB after CurrentHob.
// Returns NULL if no more exist.
//
static inline VOID *
GetNextAcpiReplacementTableHob(
    IN VOID *CurrentHob
    )
{
    return GetNextGuidHob(&gAcpiReplacementTableHobGuid, GET_NEXT_HOB(CurrentHob));
}

//
// Helper: find the first replacement table matching a given ACPI signature.
// Returns the table header pointer, or NULL if not found.
//
static inline EFI_ACPI_DESCRIPTION_HEADER *
FindAcpiReplacementTable(
    IN UINT32 Signature
    )
{
    VOID *Hob = GetFirstAcpiReplacementTableHob();
    while (Hob != NULL)
    {
        EFI_ACPI_DESCRIPTION_HEADER *Table = AcpiReplacementTableFromHob(Hob);
        if (Table->Signature == Signature)
        {
            return Table;
        }
        Hob = GetNextAcpiReplacementTableHob(Hob);
    }
    return NULL;
}
