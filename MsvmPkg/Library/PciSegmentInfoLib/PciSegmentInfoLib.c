/** @file
  Platform PCI Segment Info Library for Hyper-V Gen2 VMs.

  Reads MCFG from the ACPI replacement table HOB and provides segment-to-ECAM-base
  mapping for BasePciSegmentLibSegmentInfo.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PciSegmentInfoLib.h>
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>
#include <AcpiReplacementTable.h>
#include <PciConstants.h>

STATIC
PCI_SEGMENT_INFO  *mSegmentInfo = NULL;
STATIC UINTN              mSegmentCount = 0;

/**
  Return an array of PCI_SEGMENT_INFO describing each PCIe segment.

  @param[out] Count  Number of entries returned.
  @return            Pointer to a cached array of PCI_SEGMENT_INFO.
**/
PCI_SEGMENT_INFO *
EFIAPI
GetPciSegmentInfo (
    OUT UINTN  *Count
    )
{
    UINT32                       DataLen;
    UINT32                       EntryCount;
    MCFG_ALLOCATION_ENTRY        *Entries;
    UINT32                       i;

    if (mSegmentInfo != NULL) {
        *Count = mSegmentCount;
        return mSegmentInfo;
    }

    //
    // Find MCFG table from HOB list.
    //
    EFI_PEI_HOB_POINTERS          hob;
    MCFG_TABLE_HEADER            *McfgHdr = NULL;

    hob.Raw = GetFirstGuidHob(&gAcpiReplacementTableHobGuid);
    while (hob.Raw != NULL)
    {
        ACPI_REPLACEMENT_TABLE_HOB_DATA *hobData = (ACPI_REPLACEMENT_TABLE_HOB_DATA *)GET_GUID_HOB_DATA(hob.Guid);
        EFI_ACPI_DESCRIPTION_HEADER *acpiHdr = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)hobData->TableAddress;
        if (acpiHdr->Signature == EFI_ACPI_6_2_PCI_EXPRESS_MEMORY_MAPPED_CONFIGURATION_SPACE_BASE_ADDRESS_DESCRIPTION_TABLE_SIGNATURE)
        {
            McfgHdr = (MCFG_TABLE_HEADER *)acpiHdr;
            break;
        }
        hob.Raw = GetNextGuidHob(&gAcpiReplacementTableHobGuid, GET_NEXT_HOB(hob));
    }

    if (McfgHdr == NULL || McfgHdr->Header.Length < sizeof(MCFG_TABLE_HEADER)) {
        DEBUG ((DEBUG_INFO, "PCIe: PciSegmentInfoLib: No MCFG table in HOBs\n"));
        *Count = 0;
        return NULL;
    }

    DataLen = McfgHdr->Header.Length - sizeof (MCFG_TABLE_HEADER);
    EntryCount = DataLen / sizeof (MCFG_ALLOCATION_ENTRY);

    if (EntryCount == 0 || (DataLen % sizeof (MCFG_ALLOCATION_ENTRY)) != 0) {
        DEBUG ((DEBUG_ERROR, "PCIe: PciSegmentInfoLib: Invalid MCFG data (len=%u, entry_size=%u)\n",
                DataLen, (UINT32)sizeof (MCFG_ALLOCATION_ENTRY)));
        *Count = 0;
        return NULL;
    }

    DEBUG ((DEBUG_INFO, "PCIe: PciSegmentInfoLib: %u segments from MCFG\n", EntryCount));

    mSegmentInfo = AllocateZeroPool (EntryCount * sizeof (PCI_SEGMENT_INFO));
    if (mSegmentInfo == NULL) {
        DEBUG ((DEBUG_ERROR, "PCIe: PciSegmentInfoLib: Failed to allocate segment info\n"));
        *Count = 0;
        return NULL;
    }

    Entries = (MCFG_ALLOCATION_ENTRY *)(McfgHdr + 1);

    for (i = 0; i < EntryCount; i++) {
        mSegmentInfo[i].SegmentNumber  = Entries[i].PciSegmentGroupNumber;
        mSegmentInfo[i].BaseAddress    = Entries[i].BaseAddress;
        mSegmentInfo[i].StartBusNumber = Entries[i].StartBusNumber;
        mSegmentInfo[i].EndBusNumber   = Entries[i].EndBusNumber;

        DEBUG ((DEBUG_INFO, "PCIe:   SegmentInfo[%u]: Seg=%u ECAM=%016lx Bus=%u..%u\n",
                i, Entries[i].PciSegmentGroupNumber, Entries[i].BaseAddress,
                Entries[i].StartBusNumber, Entries[i].EndBusNumber));
    }

    mSegmentCount = EntryCount;
    *Count = mSegmentCount;
    return mSegmentInfo;
}
