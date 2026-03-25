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
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>
#include <BiosInterface.h>

//
// Local typedef for readability.
//
typedef EFI_ACPI_MEMORY_MAPPED_ENHANCED_CONFIGURATION_SPACE_BASE_ADDRESS_ALLOCATION_STRUCTURE
    MCFG_ALLOCATION_ENTRY;

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

    AcpiNode->HID = EISA_PNP_ID (0x0A08);
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
    EFI_ACPI_DESCRIPTION_HEADER  *McfgHdr;
    UINT32                       McfgReservedSize;
    UINT32                       McfgDataLen;
    UINT32                       McfgEntryCount;
    MCFG_ALLOCATION_ENTRY        *McfgEntries;
    PCIE_BAR_APERTURE_ENTRY      *Apertures;
    UINT32                       ApertureCount;
    PCI_ROOT_BRIDGE              *Bridges;
    UINT32                       i;

    *Count = 0;

    //
    // Read MCFG from PCD.
    //
    McfgPtr  = PcdGet64 (PcdMcfgPtr);
    McfgSize = PcdGet32 (PcdMcfgSize);

    DEBUG ((DEBUG_ERROR, "ePCI: PciHostBridgeLib McfgPtr=0x%lx McfgSize=%u\n", McfgPtr, McfgSize));

    if (McfgPtr == 0 || McfgSize < sizeof (EFI_ACPI_DESCRIPTION_HEADER)) {
        DEBUG ((DEBUG_ERROR, "ePCI: No MCFG table, no root bridges\n"));
        return NULL;
    }

    McfgHdr = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)McfgPtr;
    McfgReservedSize = 8;

    if (McfgHdr->Length < sizeof (EFI_ACPI_DESCRIPTION_HEADER) + McfgReservedSize ||
        McfgHdr->Length > McfgSize) {
        DEBUG ((DEBUG_ERROR, "ePCI: Invalid MCFG Length %u (PCD size %u)\n",
                McfgHdr->Length, McfgSize));
        return NULL;
    }

    McfgDataLen = McfgHdr->Length - sizeof (EFI_ACPI_DESCRIPTION_HEADER) - McfgReservedSize;
    McfgEntryCount = McfgDataLen / sizeof (MCFG_ALLOCATION_ENTRY);

    if (McfgEntryCount == 0) {
        DEBUG ((DEBUG_ERROR, "ePCI: MCFG has 0 entries\n"));
        return NULL;
    }

    McfgEntries = (MCFG_ALLOCATION_ENTRY *)((UINT8 *)McfgHdr
                      + sizeof (EFI_ACPI_DESCRIPTION_HEADER) + McfgReservedSize);

    //
    // Read PcieBarApertures from PCD.
    //
    Apertures = (PCIE_BAR_APERTURE_ENTRY *)(UINTN)PcdGet64 (PcdPcieBarAperturesPtr);
    ApertureCount = PcdGet32 (PcdPcieBarAperturesSize) / sizeof (PCIE_BAR_APERTURE_ENTRY);

    if (ApertureCount > 0 && Apertures == NULL) {
        DEBUG ((DEBUG_ERROR, "ePCI: ApertureCount > 0 but pointer is NULL\n"));
        ApertureCount = 0;
    }

    DEBUG ((DEBUG_ERROR, "ePCI: %u MCFG entries, %u aperture entries\n",
            McfgEntryCount, ApertureCount));

    //
    // Allocate root bridge array.
    //
    Bridges = AllocateZeroPool (McfgEntryCount * sizeof (PCI_ROOT_BRIDGE));
    if (Bridges == NULL) {
        DEBUG ((DEBUG_ERROR, "PciHostBridgeLib: Failed to allocate root bridges\n"));
        return NULL;
    }

    

    for (i = 0; i < McfgEntryCount; i++) {
        UINT16  Segment  = McfgEntries[i].PciSegmentGroupNumber;
        UINT8   StartBus = McfgEntries[i].StartBusNumber;
        UINT8   EndBus   = McfgEntries[i].EndBusNumber;
        UINT64  LowMmioBase   = 0;
        UINT64  LowMmioLength = 0;
        UINT64  HighMmioBase   = 0;
        UINT64  HighMmioLength = 0;
        UINT32  j;

        //
        // Find matching aperture entry by segment number.
        //
        for (j = 0; j < ApertureCount; j++) {
            if (Apertures[j].Segment == Segment) {
                LowMmioBase    = Apertures[j].LowMmioBase;
                LowMmioLength  = Apertures[j].LowMmioLength;
                HighMmioBase   = Apertures[j].HighMmioBase;
                HighMmioLength = Apertures[j].HighMmioLength;
                break;
            }
        }

        DEBUG ((DEBUG_ERROR,
                "ePCI: Bridge[%u]: Seg=%u Bus=%u..%u EcamBase=%016lx LowMmio=%016lx+%016lx HighMmio=%016lx+%016lx\n",
                i, Segment, StartBus, EndBus,
                McfgEntries[i].BaseAddress,
                LowMmioBase, LowMmioLength, HighMmioBase, HighMmioLength));

        Bridges[i].Segment              = Segment;
        Bridges[i].Supports             = 0;
        Bridges[i].Attributes           = 0;
        Bridges[i].DmaAbove4G           = TRUE;
        Bridges[i].NoExtendedConfigSpace = FALSE;
        Bridges[i].ResourceAssigned     = FALSE;
        Bridges[i].AllocationAttributes = EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM |
                                          EFI_PCI_HOST_BRIDGE_MEM64_DECODE;

        //
        // Bus aperture.
        //
        Bridges[i].Bus.Base        = StartBus;
        Bridges[i].Bus.Limit       = EndBus;
        Bridges[i].Bus.Translation = 0;

        //
        // I/O aperture — empty (no legacy I/O for Gen2 ePCI).
        //
        Bridges[i].Io.Base        = MAX_UINT64;
        Bridges[i].Io.Limit       = 0;
        Bridges[i].Io.Translation = 0;

        //
        // Low MMIO aperture (below 4 GB).
        //
        if (LowMmioLength > 0) {
            Bridges[i].Mem.Base        = LowMmioBase;
            Bridges[i].Mem.Limit       = LowMmioBase + LowMmioLength - 1;
            Bridges[i].Mem.Translation = 0;
        } else {
            Bridges[i].Mem.Base        = MAX_UINT64;
            Bridges[i].Mem.Limit       = 0;
            Bridges[i].Mem.Translation = 0;
        }

        //
        // High MMIO aperture (above 4 GB).
        //
        if (HighMmioLength > 0) {
            Bridges[i].MemAbove4G.Base        = HighMmioBase;
            Bridges[i].MemAbove4G.Limit       = HighMmioBase + HighMmioLength - 1;
            Bridges[i].MemAbove4G.Translation = 0;
        } else {
            Bridges[i].MemAbove4G.Base        = MAX_UINT64;
            Bridges[i].MemAbove4G.Limit       = 0;
            Bridges[i].MemAbove4G.Translation = 0;
        }

        //
        // Prefetchable MMIO — empty (no prefetchable distinction needed).
        //
        Bridges[i].PMem.Base        = MAX_UINT64;
        Bridges[i].PMem.Limit       = 0;
        Bridges[i].PMem.Translation = 0;

        Bridges[i].PMemAbove4G.Base        = MAX_UINT64;
        Bridges[i].PMemAbove4G.Limit       = 0;
        Bridges[i].PMemAbove4G.Translation = 0;

        //
        // Device path — ACPI(PNP0A08, UID=Segment).
        //
        Bridges[i].DevicePath = CreateRootBridgeDevicePath (Segment);
        if (Bridges[i].DevicePath == NULL) {
            DEBUG ((DEBUG_ERROR, "PciHostBridgeLib: Failed to create device path\n"));
            for (UINT32 k = 0; k < i; k++) {
                FreePool (Bridges[k].DevicePath);
            }
            FreePool (Bridges);
            return NULL;
        }
    }

    DEBUG ((DEBUG_INFO, "PciHostBridgeLib: Returning %u root bridges\n", McfgEntryCount));

    //
    // Reserve the ECAM MMIO range in GCD so that other DXE drivers
    // (e.g., VideoDxe) cannot allocate MMIO space that overlaps ECAM.
    // PlatformPei declared these ranges as MMIO HOBs, but HOBs only
    // add the range to GCD — they don't reserve it.  Without this
    // reservation, gDS->AllocateMemorySpace(AnySearchBottomUp, MMIO)
    // will hand out ECAM addresses for unrelated MMIO allocations.
    //
    for (i = 0; i < McfgEntryCount; i++) {
        UINT64 EcamBase = McfgEntries[i].BaseAddress
            + (UINT64)McfgEntries[i].StartBusNumber * 256 * 4096;
        UINT64 EcamSize =
            (UINT64)(McfgEntries[i].EndBusNumber - McfgEntries[i].StartBusNumber + 1)
            * 256 * 4096;

        EFI_STATUS ReserveStatus = gDS->AllocateMemorySpace (
            EfiGcdAllocateAddress,
            EfiGcdMemoryTypeMemoryMappedIo,
            0,
            EcamSize,
            &EcamBase,
            gImageHandle,
            NULL
            );
        if (EFI_ERROR (ReserveStatus)) {
            DEBUG ((DEBUG_WARN,
                    "PciHostBridgeLib: Failed to reserve ECAM range %016lx+%016lx: %r\n",
                    EcamBase, EcamSize, ReserveStatus));
        } else {
            DEBUG ((DEBUG_INFO,
                    "PciHostBridgeLib: Reserved ECAM range %016lx+%016lx\n",
                    EcamBase, EcamSize));
        }
    }

    *Count = McfgEntryCount;
    return Bridges;
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
