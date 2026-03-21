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

//
// Local typedef for readability.
//
typedef EFI_ACPI_MEMORY_MAPPED_ENHANCED_CONFIGURATION_SPACE_BASE_ADDRESS_ALLOCATION_STRUCTURE
    MCFG_ALLOCATION_ENTRY;

STATIC PCI_SEGMENT_INFO  *mSegmentInfo = NULL;
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
    EFI_ACPI_DESCRIPTION_HEADER  *McfgHdr;
    UINT32                       McfgReservedSize;
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

    if (McfgPtr == 0 || McfgSize < sizeof (EFI_ACPI_DESCRIPTION_HEADER)) {
        *Count = 0;
        return NULL;
    }

    McfgHdr = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)McfgPtr;

    //
    // MCFG layout: ACPI header + 8 bytes reserved + array of allocation entries.
    //
    McfgReservedSize = 8;

    if (McfgHdr->Length < sizeof (EFI_ACPI_DESCRIPTION_HEADER) + McfgReservedSize ||
        McfgHdr->Length > McfgSize) {
        DEBUG ((DEBUG_ERROR, "PciSegmentInfoLib: Invalid MCFG Length %u (PCD size %u)\n",
                McfgHdr->Length, McfgSize));
        *Count = 0;
        return NULL;
    }

    DataLen = McfgHdr->Length - sizeof (EFI_ACPI_DESCRIPTION_HEADER) - McfgReservedSize;
    EntryCount = DataLen / sizeof (MCFG_ALLOCATION_ENTRY);

    DEBUG ((DEBUG_INFO, "PciSegmentInfoLib: %u segments from MCFG\n", EntryCount));

    mSegmentInfo = AllocateZeroPool (EntryCount * sizeof (PCI_SEGMENT_INFO));
    if (mSegmentInfo == NULL) {
        DEBUG ((DEBUG_ERROR, "PciSegmentInfoLib: Failed to allocate segment info\n"));
        *Count = 0;
        return NULL;
    }

    Entries = (MCFG_ALLOCATION_ENTRY *)((UINT8 *)McfgHdr
                  + sizeof (EFI_ACPI_DESCRIPTION_HEADER) + McfgReservedSize);

    for (i = 0; i < EntryCount; i++) {
        mSegmentInfo[i].SegmentNumber  = Entries[i].PciSegmentGroupNumber;
        mSegmentInfo[i].BaseAddress    = Entries[i].BaseAddress;
        mSegmentInfo[i].StartBusNumber = Entries[i].StartBusNumber;
        mSegmentInfo[i].EndBusNumber   = Entries[i].EndBusNumber;

        DEBUG ((DEBUG_INFO, "  Segment[%u]: Seg=%u ECAM=%016lx Bus=%u..%u\n",
                i, Entries[i].PciSegmentGroupNumber, Entries[i].BaseAddress,
                Entries[i].StartBusNumber, Entries[i].EndBusNumber));
    }

    mSegmentCount = EntryCount;
    *Count = mSegmentCount;
    return mSegmentInfo;
}
