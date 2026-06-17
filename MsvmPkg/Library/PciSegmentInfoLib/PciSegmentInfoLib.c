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
    MCFG_TABLE_HEADER *McfgHdr = (MCFG_TABLE_HEADER *)FindAcpiReplacementTable(
        EFI_ACPI_6_2_PCI_EXPRESS_MEMORY_MAPPED_CONFIGURATION_SPACE_BASE_ADDRESS_DESCRIPTION_TABLE_SIGNATURE);

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

    DEBUG ((DEBUG_INFO, "PCIe: PciSegmentInfoLib: %u MCFG entries\n", EntryCount));

    Entries = (MCFG_ALLOCATION_ENTRY *)(McfgHdr + 1);

    //
    // Coalesce MCFG entries that share a PCI Segment Group Number into a single
    // PCI_SEGMENT_INFO.  The upstream consumer (PciSegmentLibCommon.c) expects
    // exactly one entry per segment and matches by segment number alone, and
    // PCI_SEGMENT_INFO can hold only one ECAM base per segment.
    //
    // The PCI Firmware Spec (§4.1.1, §4.1.2 Table 4-3) DOES permit multiple
    // same-segment host bridges with distinct, discontinuous ECAM base
    // addresses.  We do not support that here: this platform's VMM always lays
    // same-segment host bridges over a single contiguous ECAM, so every
    // same-segment entry carries the same bus-0-relative BaseAddress.  We assert
    // that invariant and fail loudly if it is ever violated, rather than
    // silently dropping a distinct base we cannot represent.
    //
    // Allocate up to EntryCount slots (upper bound on unique segments).
    //
    mSegmentInfo = AllocateZeroPool (EntryCount * sizeof (PCI_SEGMENT_INFO));
    if (mSegmentInfo == NULL) {
        DEBUG ((DEBUG_ERROR, "PCIe: PciSegmentInfoLib: Failed to allocate segment info\n"));
        *Count = 0;
        return NULL;
    }

    //
    // Build coalesced entries in a single pass.
    //
    UINT32 SegIdx = 0;
    for (i = 0; i < EntryCount; i++) {
        UINT32  k;
        BOOLEAN Found = FALSE;

        //
        // Check if we already have an entry for this segment.
        //
        for (k = 0; k < SegIdx; k++) {
            if (mSegmentInfo[k].SegmentNumber == Entries[i].PciSegmentGroupNumber) {
                Found = TRUE;
                //
                // Validate that the BaseAddress matches the existing entry.
                //
                if (mSegmentInfo[k].BaseAddress != Entries[i].BaseAddress) {
                    DEBUG ((DEBUG_ERROR,
                            "PCIe: PciSegmentInfoLib: Segment %u has conflicting ECAM bases: "
                            "%016lx vs %016lx\n",
                            Entries[i].PciSegmentGroupNumber,
                            mSegmentInfo[k].BaseAddress,
                            Entries[i].BaseAddress));
                    ASSERT (FALSE);
                    FreePool (mSegmentInfo);
                    mSegmentInfo = NULL;
                    *Count = 0;
                    return NULL;
                }
                //
                // Widen the bus range to encompass this entry.
                //
                if (Entries[i].StartBusNumber < mSegmentInfo[k].StartBusNumber) {
                    mSegmentInfo[k].StartBusNumber = Entries[i].StartBusNumber;
                }
                if (Entries[i].EndBusNumber > mSegmentInfo[k].EndBusNumber) {
                    mSegmentInfo[k].EndBusNumber = Entries[i].EndBusNumber;
                }
                break;
            }
        }

        if (!Found) {
            mSegmentInfo[SegIdx].SegmentNumber  = Entries[i].PciSegmentGroupNumber;
            mSegmentInfo[SegIdx].BaseAddress    = Entries[i].BaseAddress;
            mSegmentInfo[SegIdx].StartBusNumber = Entries[i].StartBusNumber;
            mSegmentInfo[SegIdx].EndBusNumber   = Entries[i].EndBusNumber;
            SegIdx++;
        }
    }

    for (i = 0; i < SegIdx; i++) {
        DEBUG ((DEBUG_INFO, "PCIe:   SegmentInfo[%u]: Seg=%u ECAM=%016lx Bus=%u..%u\n",
                i, mSegmentInfo[i].SegmentNumber, mSegmentInfo[i].BaseAddress,
                mSegmentInfo[i].StartBusNumber, mSegmentInfo[i].EndBusNumber));
    }

    mSegmentCount = SegIdx;
    *Count = mSegmentCount;
    return mSegmentInfo;
}
