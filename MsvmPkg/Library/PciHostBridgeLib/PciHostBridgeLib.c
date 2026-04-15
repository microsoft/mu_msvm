/** @file
  Platform PCI Host Bridge Library for Hyper-V Gen2 VMs.

  Reads MCFG + PcieBarApertures from config blob PCDs and returns
  a PCI_ROOT_BRIDGE array for PciHostBridgeDxe.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>
#include <BiosInterface.h>
#include <PciConstants.h>


STATIC
EFI_DEVICE_PATH_PROTOCOL *
CreateRootBridgeDevicePath (
    UINT32  Uid
    )
{
    ACPI_HID_DEVICE_PATH      *AcpiNode;
    EFI_DEVICE_PATH_PROTOCOL  *DevicePath;

    AcpiNode = (ACPI_HID_DEVICE_PATH *)CreateDeviceNode (
                   ACPI_DEVICE_PATH,
                   ACPI_DP,
                   sizeof (ACPI_HID_DEVICE_PATH)
                   );
    if (AcpiNode == NULL) {
        return NULL;
    }

    AcpiNode->HID = PCIE_ROOT_COMPLEX_HID;
    AcpiNode->UID = Uid;

    DevicePath = AppendDevicePathNode (NULL, (EFI_DEVICE_PATH_PROTOCOL *)AcpiNode);
    FreePool (AcpiNode);
    return DevicePath;
}

/**
  Return all the root bridge instances in an array.

  @param[out] Count  The number of root bridge instances.
  @return            All the root bridge instances in an array, or NULL if
                     no root bridges are present.
**/
PCI_ROOT_BRIDGE *
EFIAPI
PciHostBridgeGetRootBridges (
    OUT UINTN  *Count
    )
{
    UINT64                       McfgPtr;
    UINT32                       McfgSize;
    MCFG_TABLE_HEADER            *McfgHdr;
    UINT32                       McfgDataLen;
    UINT32                       McfgEntryCount;
    MCFG_ALLOCATION_ENTRY        *McfgEntries;
    PCIE_BAR_APERTURE_ENTRY      *Apertures;
    UINT32                       ApertureCount;
    PCI_ROOT_BRIDGE              *Bridges;
    UINT32                       i;

    if (Count == NULL) {
        return NULL;
    }

    *Count = 0;

    //
    // Read MCFG from PCD.
    //
    McfgPtr  = PcdGet64 (PcdMcfgPtr);
    McfgSize = PcdGet32 (PcdMcfgSize);

    DEBUG ((DEBUG_VERBOSE, "PCIe: PciHostBridgeLib McfgPtr=0x%lx McfgSize=%u\n", McfgPtr, McfgSize));

    if (McfgPtr == 0 || McfgSize < sizeof (MCFG_TABLE_HEADER)) {
        DEBUG ((DEBUG_INFO, "PCIe: No MCFG table, no root bridges\n"));
        return NULL;
    }

    McfgHdr = (MCFG_TABLE_HEADER *)(UINTN)McfgPtr;

    if (McfgHdr->Header.Length < sizeof (MCFG_TABLE_HEADER) ||
        McfgHdr->Header.Length > McfgSize) {
        DEBUG ((DEBUG_ERROR, "PCIe: Invalid MCFG Length %u (PCD size %u)\n",
                McfgHdr->Header.Length, McfgSize));
        return NULL;
    }

    McfgDataLen = McfgHdr->Header.Length - sizeof (MCFG_TABLE_HEADER);
    McfgEntryCount = McfgDataLen / sizeof (MCFG_ALLOCATION_ENTRY);

    if (McfgEntryCount == 0 || (McfgDataLen % sizeof (MCFG_ALLOCATION_ENTRY)) != 0) {
        DEBUG ((DEBUG_ERROR, "PCIe: Invalid MCFG data (len=%u, entry_size=%u)\n",
                McfgDataLen, (UINT32)sizeof (MCFG_ALLOCATION_ENTRY)));
        return NULL;
    }

    McfgEntries = (MCFG_ALLOCATION_ENTRY *)(McfgHdr + 1);

    //
    // Read PcieBarApertures from PCD.
    //
    Apertures = (PCIE_BAR_APERTURE_ENTRY *)(UINTN)PcdGet64 (PcdPcieBarAperturesPtr);
    ApertureCount = PcdGet32 (PcdPcieBarAperturesSize) / sizeof (PCIE_BAR_APERTURE_ENTRY);

    if (ApertureCount > 0 && Apertures == NULL) {
        DEBUG ((DEBUG_ERROR, "PCIe: ApertureCount > 0 but pointer is NULL\n"));
        ApertureCount = 0;
    }

    DEBUG ((DEBUG_INFO, "PCIe: %u MCFG entries, %u aperture entries\n",
            McfgEntryCount, ApertureCount));

    //
    // Allocate root bridge array, one per aperture entry.
    // PcieBarApertures is the authoritative list of bridges UEFI should
    // enumerate.  MCFG may contain additional bridges for the guest OS.
    //
    if (ApertureCount == 0) {
        DEBUG ((DEBUG_INFO, "PciHostBridgeLib: No apertures, no root bridges for UEFI\n"));
        return NULL;
    }

    Bridges = AllocateZeroPool (ApertureCount * sizeof (PCI_ROOT_BRIDGE));
    if (Bridges == NULL) {
        DEBUG ((DEBUG_ERROR, "PciHostBridgeLib: Failed to allocate root bridges\n"));
        return NULL;
    }

    UINT32 BridgeCount = 0;
    for (i = 0; i < ApertureCount; i++) {
        UINT16  Segment        = Apertures[i].Segment;
        UINT8   StartBus       = Apertures[i].StartBus;
        UINT8   EndBus         = Apertures[i].EndBus;
        UINT64  LowMmioBase    = Apertures[i].LowMmioBase;
        UINT64  LowMmioLength  = Apertures[i].LowMmioLength;
        UINT64  HighMmioBase   = Apertures[i].HighMmioBase;
        UINT64  HighMmioLength = Apertures[i].HighMmioLength;
        UINT32  j;

        //
        // Find the matching MCFG entry to get the ECAM base address
        // and validate that the aperture bus range fits within it.
        //
        BOOLEAN McfgFound = FALSE;
        UINT64  EcamBase  = 0;
        for (j = 0; j < McfgEntryCount; j++) {
            if (McfgEntries[j].PciSegmentGroupNumber == Segment) {
                McfgFound = TRUE;
                EcamBase = McfgEntries[j].BaseAddress
                    + (UINT64)McfgEntries[j].StartBusNumber * PCIE_ECAM_BYTES_PER_BUS;

                if (StartBus < McfgEntries[j].StartBusNumber ||
                    EndBus  > McfgEntries[j].EndBusNumber) {
                    DEBUG ((DEBUG_ERROR,
                            "PCIe: Aperture bus range %u..%u exceeds MCFG range %u..%u for segment %u\n",
                            StartBus, EndBus,
                            McfgEntries[j].StartBusNumber, McfgEntries[j].EndBusNumber,
                            Segment));
                    goto Cleanup;
                }
                break;
            }
        }

        if (!McfgFound) {
            DEBUG ((DEBUG_ERROR,
                    "PCIe: Aperture segment %u has no matching MCFG entry\n",
                    Segment));
            goto Cleanup;
        }

        if (EndBus < StartBus) {
            DEBUG ((DEBUG_ERROR, "PCIe: Invalid bus range for segment %u: Start=%u End=%u\n",
                    Segment, StartBus, EndBus));
            goto Cleanup;
        }

        DEBUG ((DEBUG_INFO,
                "PCIe: Bridge[%u]: Seg=%u Bus=%u..%u EcamBase=%016lx LowMmio=%016lx+%016lx HighMmio=%016lx+%016lx\n",
                BridgeCount, Segment, StartBus, EndBus, EcamBase,
                LowMmioBase, LowMmioLength, HighMmioBase, HighMmioLength));

        Bridges[BridgeCount].Segment              = Segment;
        Bridges[BridgeCount].Supports             = 0;
        Bridges[BridgeCount].Attributes           = 0;
        Bridges[BridgeCount].DmaAbove4G           = TRUE;
        Bridges[BridgeCount].NoExtendedConfigSpace = FALSE;
        Bridges[BridgeCount].AllocationAttributes = EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM |
                                                    EFI_PCI_HOST_BRIDGE_MEM64_DECODE;

        //
        // ResourceAssigned = FALSE: PciHostBridgeDxe will go through the
        // full PCI resource allocation protocol with PciBusDxe.  The ECAM
        // ranges are reserved separately in the loop below.
        //
        Bridges[BridgeCount].ResourceAssigned     = FALSE;

        //
        // Bus aperture.
        //
        Bridges[BridgeCount].Bus.Base        = StartBus;
        Bridges[BridgeCount].Bus.Limit       = EndBus;
        Bridges[BridgeCount].Bus.Translation = 0;

        //
        // I/O aperture — empty (no legacy I/O supported).
        //
        Bridges[BridgeCount].Io.Base        = MAX_UINT64;
        Bridges[BridgeCount].Io.Limit       = 0;
        Bridges[BridgeCount].Io.Translation = 0;

        //
        // Low MMIO aperture (below 4 GB).
        //
        if (LowMmioLength > 0) {
            if (LowMmioBase + LowMmioLength < LowMmioBase) {
                DEBUG ((DEBUG_ERROR,
                        "PCIe: LowMmio overflow: Base=%016lx Length=%016lx\n",
                        LowMmioBase, LowMmioLength));
                goto Cleanup;
            }
            Bridges[BridgeCount].Mem.Base        = LowMmioBase;
            Bridges[BridgeCount].Mem.Limit       = LowMmioBase + LowMmioLength - 1;
            Bridges[BridgeCount].Mem.Translation = 0;
        } else {
            Bridges[BridgeCount].Mem.Base        = MAX_UINT64;
            Bridges[BridgeCount].Mem.Limit       = 0;
            Bridges[BridgeCount].Mem.Translation = 0;
        }

        //
        // High MMIO aperture (above 4 GB).
        //
        if (HighMmioLength > 0) {
            if (HighMmioBase + HighMmioLength < HighMmioBase) {
                DEBUG ((DEBUG_ERROR,
                        "PCIe: HighMmio overflow: Base=%016lx Length=%016lx\n",
                        HighMmioBase, HighMmioLength));
                goto Cleanup;
            }
            Bridges[BridgeCount].MemAbove4G.Base        = HighMmioBase;
            Bridges[BridgeCount].MemAbove4G.Limit       = HighMmioBase + HighMmioLength - 1;
            Bridges[BridgeCount].MemAbove4G.Translation = 0;
        } else {
            Bridges[BridgeCount].MemAbove4G.Base        = MAX_UINT64;
            Bridges[BridgeCount].MemAbove4G.Limit       = 0;
            Bridges[BridgeCount].MemAbove4G.Translation = 0;
        }

        //
        // Prefetchable MMIO — empty (no prefetchable distinction needed).
        //
        Bridges[BridgeCount].PMem.Base        = MAX_UINT64;
        Bridges[BridgeCount].PMem.Limit       = 0;
        Bridges[BridgeCount].PMem.Translation = 0;

        Bridges[BridgeCount].PMemAbove4G.Base        = MAX_UINT64;
        Bridges[BridgeCount].PMemAbove4G.Limit       = 0;
        Bridges[BridgeCount].PMemAbove4G.Translation = 0;

        //
        // Device path — ACPI(PNP0A08, UID from host).
        //
        Bridges[BridgeCount].DevicePath = CreateRootBridgeDevicePath (Apertures[i].Uid);
        if (Bridges[BridgeCount].DevicePath == NULL) {
            DEBUG ((DEBUG_ERROR, "PciHostBridgeLib: Failed to create device path\n"));
            goto Cleanup;
        }

        BridgeCount++;
    }

    if (BridgeCount == 0) {
        goto Cleanup;
    }

    DEBUG ((DEBUG_INFO, "PciHostBridgeLib: Returning %u root bridges\n", BridgeCount));

    *Count = BridgeCount;
    return Bridges;

Cleanup:
    for (i = 0; i < BridgeCount; i++) {
        FreePool (Bridges[i].DevicePath);
    }
    FreePool (Bridges);
    *Count = 0;
    return NULL;
}

/**
  Free the root bridge instances array returned from
  PciHostBridgeGetRootBridges().

  @param[in] Bridges  The root bridge instances array.
  @param[in] Count    The count of the array.
**/
VOID
EFIAPI
PciHostBridgeFreeRootBridges (
    PCI_ROOT_BRIDGE  *Bridges,
    UINTN            Count
    )
{
    UINTN  i;

    if (Bridges == NULL) {
        return;
    }

    for (i = 0; i < Count; i++) {
        FreePool (Bridges[i].DevicePath);
    }

    FreePool (Bridges);
}

/**
  Inform the platform that there has been a resource conflict.

  @param[in] HostBridgeHandle  Handle of the Host Bridge.
  @param[in] Configuration     Pointer to PCI I/O and PCI memory resource
                               descriptors. NULL if no conflict.
**/
VOID
EFIAPI
PciHostBridgeResourceConflict (
    EFI_HANDLE  HostBridgeHandle,
    VOID        *Configuration
    )
{
    //
    // In a virtual machine, resource conflicts should not occur since
    // the VMM controls all resource allocation.  Log and continue.
    //
    DEBUG ((DEBUG_ERROR, "PciHostBridgeLib: Resource conflict detected!\n"));
    ASSERT (FALSE);
}
