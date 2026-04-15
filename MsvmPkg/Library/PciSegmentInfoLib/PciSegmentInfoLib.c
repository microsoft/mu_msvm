/** @file
  Platform PCI Segment Info Library for Hyper-V Gen2 VMs.

  Reads MCFG from config blob PCD and provides segment-to-ECAM-base
  mapping for BasePciSegmentLibSegmentInfo.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PciSegmentInfoLib.h>
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>
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
    UINT64                       McfgPtr;
    UINT32                       McfgSize;
    MCFG_TABLE_HEADER            *McfgHdr;
    UINT32                       DataLen;
    UINT32                       EntryCount;
    MCFG_ALLOCATION_ENTRY        *Entries;
    UINT32                       i;

    if (mSegmentInfo != NULL) {
        *Count = mSegmentCount;
        return mSegmentInfo;
    }

    //
    // Parse MCFG table from PCD (same data PlatformPei extracted from config blob).
    //
    McfgPtr  = PcdGet64 (PcdMcfgPtr);
    McfgSize = PcdGet32 (PcdMcfgSize);

    if (McfgPtr == 0 || McfgSize < sizeof (MCFG_TABLE_HEADER)) {
        DEBUG ((DEBUG_INFO, "PCIe: PciSegmentInfoLib: No MCFG table\n"));
        *Count = 0;
        return NULL;
    }

    McfgHdr = (MCFG_TABLE_HEADER *)(UINTN)McfgPtr;

    if (McfgHdr->Header.Length < sizeof (MCFG_TABLE_HEADER) ||
        McfgHdr->Header.Length > McfgSize) {
        DEBUG ((DEBUG_ERROR, "PCIe: PciSegmentInfoLib: Invalid MCFG Length %u (PCD size %u)\n",
                McfgHdr->Header.Length, McfgSize));
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
