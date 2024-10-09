/** @file
  This is the Hyper-V "Platform" PEI Module. It initializes in preparation
  for running other PEI Modules and eventually running DXE Core.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <BiosInterface.h>
#include <Platform.h>
#include <Config.h>
#include <Hob.h>
#include <Hv.h>
#include <Guid/MemoryTypeInformation.h>
#include <IndustryStandard/Acpi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/DeviceStateLib.h>

#if defined(MDE_CPU_AARCH64)
#include <Mmu.h>
#endif
#if defined (MDE_CPU_X64)
#include <Library/MtrrLib.h>
#endif
#include <Library/PeiServicesLib.h>
#include <Library/ResourcePublicationLib.h>
#include <Ppi/MasterBootMode.h>
#include <IsolationTypes.h>

#if defined (MDE_CPU_X64)
#define INITPEIMEMORY InitPeiMemoryIntel
#elif defined(MDE_CPU_AARCH64)
#define INITPEIMEMORY InitPeiMemoryArm
#else
#error Unsupported Architecture
#endif


#if defined(MDE_CPU_X64)

//
// Initial data for Memory Type Information HOB.
//
static EFI_MEMORY_TYPE_INFORMATION MsvmDefaultMemoryTypeInformation[] =
{
    { EfiACPIMemoryNVS,       0x004 },
    { EfiACPIReclaimMemory,   0x032 },
    { EfiReservedMemoryType,  0x000 },
    { EfiRuntimeServicesData, 0x055 },
    { EfiRuntimeServicesCode, 0x055 },
    { EfiBootServicesCode,    0x64A },
    { EfiBootServicesData,    0xBDC },
    { EfiMaxMemoryType,       0x000 }
};

//
// Initial data for Memory Type Information HOB for TDX guests. TDX guests use
// 5 (4 for page tables and 1 for the MP wake up structure) pages of EfiACPIMemoryNVS.
//
static EFI_MEMORY_TYPE_INFORMATION MsvmDefaultMemoryTypeInformationTdxGuest[] =
{
    { EfiACPIMemoryNVS,       0x008 },
    { EfiACPIReclaimMemory,   0x032 },
    { EfiReservedMemoryType,  0x004 },
    { EfiRuntimeServicesData, 0x055 },
    { EfiRuntimeServicesCode, 0x055 },
    { EfiBootServicesCode,    0x64A },
    { EfiBootServicesData,    0xBDC },
    { EfiMaxMemoryType,       0x000 }
};

//
// Initial data for Memory Type Information HOB for hibernate enabled VMs.
// Because a memory map change across hibernate/resume can be fatal, we add
// additional buffer in the calculations (EfiBootServicesData recommendation
// is doubled/rounded), based on BmMisc.c memory type allocation prints.
// This accounts for 4 SCSI drives and 2 NICs present during UEFI.
//
static EFI_MEMORY_TYPE_INFORMATION MsvmMemoryTypeInformationHibernateEnabled[] =
{
    { EfiACPIMemoryNVS,       0x0004 },
    { EfiACPIReclaimMemory,   0x0032 },
    { EfiReservedMemoryType,  0x0004 },
    { EfiRuntimeServicesData, 0x0054 },
    { EfiRuntimeServicesCode, 0x0030 },
    { EfiBootServicesCode,    0x0554 },
    { EfiBootServicesData,    0x21BE },
    { EfiMaxMemoryType,       0x0000 }
};


#elif defined(MDE_CPU_AARCH64)

//
// Initial data for Memory Type Information HOB.
//
static EFI_MEMORY_TYPE_INFORMATION MsvmDefaultMemoryTypeInformation[] =
{
    { EfiACPIMemoryNVS,       0x000 },
    { EfiACPIReclaimMemory,   0x026 },
    { EfiReservedMemoryType,  0x000 },
    { EfiRuntimeServicesData, 0x104 },
    { EfiRuntimeServicesCode, 0x4B0 },
    { EfiBootServicesCode,    0x584 },
    { EfiBootServicesData,    0xD2F },
    { EfiMaxMemoryType,       0x000 }
};

//
// Initial data for Memory Type Information HOB for hibernate enabled VMs.
// Because a memory map change across hibernate/resume can be fatal, we add
// additional buffer in the calculations (EfiBootServicesData recommendation
// is doubled/rounded), based on BmMisc.c memory type allocation prints.
// This accounts for 4 SCSI drives and 2 NICs present during UEFI.
//
static EFI_MEMORY_TYPE_INFORMATION MsvmMemoryTypeInformationHibernateEnabled[] =
{
    { EfiACPIMemoryNVS,       0x0000 },
    { EfiACPIReclaimMemory,   0x0026 },
    { EfiReservedMemoryType,  0x0000 },
    { EfiRuntimeServicesData, 0x0104 },
    { EfiRuntimeServicesCode, 0x04B0 },
    { EfiBootServicesCode,    0x0584 },
    { EfiBootServicesData,    0x2000 },
    { EfiMaxMemoryType,       0x0000 }
};

#endif

//
// Boot Mode PPI.
//
static EFI_PEI_PPI_DESCRIPTOR MsvmBootModePpiDescriptor[] =
{
    {
        EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST,
        &gEfiPeiMasterBootModePpiGuid,
        NULL
    }
};

//
// Read/write Bios Device helper functions.
//
// N.B. Don't use the common library as PEI should not use mutable global
// variables, which only work in our environment because the whole UEFI image is
// located in read/write system memory. In the case of MMIO, the address space
// is identity mapped throughout PEI and does not change.
//
static
VOID
WriteBiosDevice(
    IN UINT32 AddressRegisterValue,
    IN UINT32 DataRegisterValue
    )
{
    UINTN biosBaseAddress = PcdGet32(PcdBiosBaseAddress);
#if defined(MDE_CPU_AARCH64)
    MmioWrite32(biosBaseAddress, AddressRegisterValue);
    MmioWrite32(biosBaseAddress + 4, DataRegisterValue);
#elif defined(MDE_CPU_X64)
    IoWrite32(biosBaseAddress, AddressRegisterValue);
    IoWrite32(biosBaseAddress + 4, DataRegisterValue);
#endif
}

static
UINT32
ReadBiosDevice(
    IN UINT32 AddressRegisterValue
    )
{
    UINTN biosBaseAddress = PcdGet32(PcdBiosBaseAddress);
#if defined(MDE_CPU_AARCH64)
    MmioWrite32(biosBaseAddress, AddressRegisterValue);
    return MmioRead32(biosBaseAddress + 4);
#elif defined(MDE_CPU_X64)
    IoWrite32(biosBaseAddress, AddressRegisterValue);
    return IoRead32(biosBaseAddress + 4);
#endif
}

#if defined (MDE_CPU_X64)
UINTN
GetPageTableSize(
    IN CONST UINT8 PhysicalAddressWidth
    )
/*++

Routine Description:

    Calculates the page table size.

Arguments:

    PhysicalAddressWidth - The number of bits in the address width.

Return Value:

    The page table size.

--*/
{
    BOOLEAN pcdUse1GPageTable;
    BOOLEAN page1GSupport;
    UINT32  regEax;
    UINT32  regEdx;
    UINT32  pml4Entries;
    UINT32  pdpEntries;
    UINTN   totalPages;

    DEBUG((DEBUG_VERBOSE, ">>> GetPageTableSize(%d)\n", PhysicalAddressWidth));

    //
    // The code below is based on CreateIdentityMappingPageTables() in
    // "MdeModulePkg/Core/DxeIplPeim/X64/VirtualMemory.c".
    //
    page1GSupport = FALSE;

    pcdUse1GPageTable = PcdGetBool(PcdUse1GPageTable);
    DEBUG((DEBUG_VERBOSE, "PcdUse1GPageTable is %a\n", pcdUse1GPageTable ? "TRUE" : "FALSE" ));
    if (pcdUse1GPageTable)
    {
        AsmCpuid(0x80000000, &regEax, NULL, NULL, NULL);
        if (regEax >= 0x80000001)
        {
            AsmCpuid(0x80000001, NULL, NULL, NULL, &regEdx);
            if ((regEdx & BIT26) != 0)
            {
                page1GSupport = TRUE;
            }
        }
    }
    DEBUG((DEBUG_VERBOSE, "page1GSupport is %a\n", page1GSupport ? "TRUE" : "FALSE"));

    if (PhysicalAddressWidth <= 39)
    {
        pml4Entries = 1;
        pdpEntries = 1 << (PhysicalAddressWidth - 30);
        ASSERT(pdpEntries <= 0x200);
    }
    else
    {
        pml4Entries = 1 << (PhysicalAddressWidth - 39);
        ASSERT(pml4Entries <= 0x200);
        pdpEntries = 512;
    }

    totalPages = page1GSupport ? pml4Entries + 1 :
                               (pdpEntries + 1) * pml4Entries + 1;
    ASSERT(totalPages <= 0x40201);

    DEBUG((DEBUG_VERBOSE, "<<< GetPageTableSize returning %ull\n",
        (UINTN)EFI_PAGES_TO_SIZE(totalPages)));

    return (UINTN)(EFI_PAGES_TO_SIZE(totalPages));
}
#endif

#if defined(MDE_CPU_AARCH64)
VOID
InitPeiMemoryArm(
    IN OUT  PPLATFORM_INIT_CONTEXT  Context,
            UINT64                  Base,
            UINT64                  Length,
    OUT     UINT64                  *AllocatedBase,
    OUT     UINT64                  *AllocatedLength
)
/*++

Routine Description:

    Utility function to initalize PEI system memory.

Arguments:

    Context - The platform init context.

    Base - The base address of the preferred allocation range.

    Length - The size in bytes of the preferred allocation range.

    AllocatedBase - Returns the base of the reserved allocation, consisting of the firmware image.

    AllocatedLength - Returns the size of the reserved allocation.

Return Value:

    None.

--*/
{
    EFI_STATUS status;

    //
    // Establish PEI memory first so we can create HOBs in the formal PEI heap.
    // Subtract the size used by the config blob, which starts at the beginning
    // of system memory.
    //
    status = PublishSystemMemory(Base, Length);
    ASSERT_EFI_ERROR(status);

    //
    // Mark the firmware image as allocated, allowing it to be reclaimed by
    // the guest OS later.
    //
    *AllocatedBase = 0;
    *AllocatedLength = PcdGet32(PcdFdSize);
}
#endif

#if defined (MDE_CPU_X64)
VOID
InitPeiMemoryIntel(
    IN OUT  PPLATFORM_INIT_CONTEXT  Context,
            UINT64                  Base,
            UINT64                  Length,
    OUT     UINT64                  *AllocatedBase,
    OUT     UINT64                  *AllocatedLength
)
/*++

Routine Description:

    Utility function to initalize PEI system memory.  This also creates
    special memory ranges below 1 MB.

Arguments:

    Context - The platform init context.

    Base - The base address of the preferred allocation range.

    Length - The size in bytes of the preferred allocation range.

    AllocatedBase - Returns the base of the reserved allocation, consisting of the firmware image and additional misc
        pages.

    AllocatedLength - Returns the size of the reserved allocation.

Return Value:

    None.

--*/
{
    EFI_STATUS status;
    UINT64 pageTableSize;
    UINT64 peiSize;
    DEBUG((DEBUG_VERBOSE, ">>> InitPeiMemoryIntel \n"));

    //
    // Establish PEI memory first so we can create HOBs in the formal PEI heap.
    // This first memory range is, by design, the memory below the MMIO range below 4GB.
    // Try to include a page table on x64 that can be large when cpu address width is large
    //
    // Insufficient room for a large page table is not fatal as the DXE page table creation
    // code has been updated to fall back to a smaller table.  This will still permit really
    // small VMs on machines with lots of address bits.
    //
    pageTableSize = GetPageTableSize(Context->PhysicalAddressWidth);
    peiSize = MIN(Length, (pageTableSize + SIZE_64MB));
    DEBUG((DEBUG_VERBOSE, "InitPeiMemoryIntel: peiBase %lx peiSize %lx\n", Base, peiSize));
    status = PublishSystemMemory(Base, peiSize);
    ASSERT_EFI_ERROR(status);

    //
    // The sub 1MB region of the address space is special in that we have to account for
    // two cases within it.
    //
    // 1) Even though the host actually puts memory between GPA 640K and 768K
    //    it can't be declared as existing. Linux fails to boot if memory
    //    is declared there. This happens to be the PCAT legacy VGA MMIO range.
    //
    // 2) The memory between 768K and 1MB exists but can't be declared as
    //    regular system memory.  At least one Windows boot driver (Intel
    //    iaStorAV) attempts to access this area with MmMapIoSpace. If this
    //    memory is marked system memory that can apparently trigger a bugcheck.
    //    Therefore this slice is marked reserved. It exists but shouldn't
    //    really be used.
    //
    //
    //           top +---------------------------------------------------
    //               | System Memory
    // 1MB  0x100000 +---------------------------------------------------
    //               | Reserved Memory - legacy device ROM & BIOS
    // 768KB 0xC0000 +---------------------------------------------------
    //                 Empty           - legacy VGA MMIO
    // 640KB 0xA0000 +---------------------------------------------------
    //               | System Memory
    //           0x0 +---------------------------------------------------
    //

    //
    // Declare System Memory from 0 to 640K.
    //
    HobAddMemoryRange(Context, 0, SIZE_512KB + SIZE_128KB);

    //
    // Skip the range from 640K to 768K (legacy VGA MMIO range) by not
    // declaring anything in that range.
    //

    //
    // Declare Reserved Memory from 768K to 1MB.
    //
    HobAddReservedMemoryRange(BASE_512KB + SIZE_256KB, SIZE_256KB);

    //
    // Mark the region occupied by the firmware, along with the page tables, GDT
    // entries, and free RW pages as allocated, which should allow it to be
    // reclaimed by the guest OS.
    //
    *AllocatedBase = PcdGet64(PcdFdBaseAddress);
    *AllocatedLength =
        PcdGet32(PcdFdSize) +
        SIZE_4KB * MISC_PAGE_COUNT_TOTAL;

    DEBUG((DEBUG_VERBOSE, "<<< InitPeiMemoryIntel\n"));
}
#endif

VOID
InitializeMemoryMap(
    IN OUT PPLATFORM_INIT_CONTEXT Context
    )
/*++

Routine Description:

    Initializes the memory map of the vm by creating appropriate HOBs and
    triggering the MTRRs to be initialized.

Arguments:

    Context - The platform init context.

Return Value:

    None.

--*/
{
    UINT64 allocationBase;
    UINT32 configBlobSize = PcdGet32(PcdConfigBlobSize);
    UINT64 configBlobBase = (UINT64) GetStartOfConfigBlob();
    BOOLEAN legacyMemoryMap = PcdGetBool(PcdLegacyMemoryMap);
    UINT32 memMapSize = PcdGet32(PcdMemoryMapSize);
    VOID* memMap = (VOID*)(UINTN) PcdGet64(PcdMemoryMapPtr);
    UINT32 pass;
    UINT64 peiBase;
    UINT64 peiLength;
    UINT64 preallocatedBase;
    UINT64 preallocatedLength;
    PVM_MEMORY_RANGE range;
    UINT64 rangeBase;
    UINT32 rangeFlags;
    UINT64 rangeLength;
    PVM_MEMORY_RANGE_V5 rangeV5;
    BOOLEAN suppressBiosDevice;
    UINT64 truncateSize;
#if defined (MDE_CPU_X64)
    BOOLEAN hostEmulatorsWhenHardwareIsolated = PcdGetBool(PcdHostEmulatorsWhenHardwareIsolated);
#endif

    //
    // Locate the top of the config blob, rounded to a page boundary.  This
    // will represent the minimum usable allocation address for PEI.
    //
    if (configBlobSize % SIZE_4KB != 0)
    {
        configBlobSize = ((configBlobSize / SIZE_4KB) + 1) * SIZE_4KB;
    }

    allocationBase = configBlobBase + configBlobSize;

    //
    // If this is a hardware isolated VM with no paravisor, then skip all
    // communication with the BiosDevice.
    //

    suppressBiosDevice = FALSE;

#if defined(MDE_CPU_X64)
    if (IsHardwareIsolatedNoParavisor() && !hostEmulatorsWhenHardwareIsolated)
    {
        suppressBiosDevice = TRUE;
    }
#endif

    //
    // Prepare to identify the largest range that could be used for holding
    // PEI allocations.
    //

    peiBase = 0;
    peiLength = 0;

    //
    // Make two passes over the memory map to determine configuration.  In the
    // first pass, determine which memory block has the greatest amount of
    // free memory; this will be used for PEI allocations.  In the second
    // pass, create HOBs for memory regions.
    //
    rangeV5 = NULL;
    UINT32 hobCount = 0;
    UINT32 spCount = 0;
    for (pass = 0; pass < 2; pass += 1)
    {
        ASSERT(memMap != NULL);
        range = (PVM_MEMORY_RANGE)memMap;
        do
        {
            if (legacyMemoryMap)
            {
                //
                // Used by legacy Hyper-V (VM version 8.0)
                //
                // A memory map range contains only base address and length.
                //
                DEBUG((DEBUG_VERBOSE, "Range BaseAddress %lx \n", range->BaseAddress));
                DEBUG((DEBUG_VERBOSE, "Range Length      %lx \n", range->Length));

                rangeBase = range->BaseAddress;
                rangeLength = range->Length;
                rangeFlags = 0;

                range += 1;
            }
            else
            {
                //
                // A memory map range now contains base address, length, and attribute flags.
                // The reserved bit allows for support of Intel SGX memory.
                //
                rangeV5 = (PVM_MEMORY_RANGE_V5)range;

                DEBUG((DEBUG_VERBOSE, "BaseAddress %lx \n", rangeV5->BaseAddress));
                DEBUG((DEBUG_VERBOSE, "Length      %lx \n", rangeV5->Length));
                DEBUG((DEBUG_VERBOSE, "Flags       %x \n", rangeV5->Flags));

                rangeBase = rangeV5->BaseAddress;
                rangeLength = rangeV5->Length;
                rangeFlags = rangeV5->Flags;

                range = (PVM_MEMORY_RANGE)(rangeV5 + 1);
            }

#if defined (MDE_CPU_X64)
            //
            // Exclude everything below 1 MB; those ranges will be configured at the end of pass 0.
            //
            if (rangeBase < BASE_1MB)
            {
                truncateSize = BASE_1MB - rangeBase;
                rangeBase = BASE_1MB;
                if (rangeLength < truncateSize)
                {
                    rangeLength = 0;
                }
                else
                {
                    rangeLength -= truncateSize;
                }
            }
#endif

            //
            // Ignore any memory below the top of the config blob.  This is
            // handled specially at the end of pass 0.
            //
            if (rangeBase < allocationBase)
            {
                truncateSize = allocationBase - rangeBase;
                rangeBase = allocationBase;
                if (rangeLength < truncateSize)
                {
                    rangeLength = 0;
                }
                else
                {
                    rangeLength -= truncateSize;
                }
            }

            if (pass == 0)
            {
                //
                // Ignore any memory above 4 GB as a candiate for PEI memory.
                //
                if (rangeBase >= 0x100000000)
                {
                    rangeLength = 0;
                }
                else if (rangeBase + rangeLength > 0x100000000)
                {
                    rangeLength = 0x100000000 - rangeBase;
                }

                //
                // Capture the largest sized block as a candidate for PEI
                // allocations.
                //
                if (rangeLength > peiLength)
                {
                    peiBase = rangeBase;
                    peiLength = rangeLength;
                }
                hobCount++;
            }
            else
            {
                //
                // Pass 1: Create a HOB describing this region.
                //
                if (rangeFlags & VM_MEMORY_RANGE_FLAG_PLATFORM_RESERVED)
                {
                    HobAddReservedMemoryRange(rangeBase, rangeLength);
                }
                else if (rangeFlags & VM_MEMORY_RANGE_FLAG_PERSISTENT_MEMORY)
                {
                    HobAddPersistentMemoryRange(rangeBase, rangeLength);
                }
                else if (rangeFlags & VM_MEMORY_RANGE_FLAG_SPECIFIC_PURPOSE)
                {
                    HobAddSpecificPurposeMemoryRange(rangeBase, rangeLength);
                    spCount++;
                }
                else
                {
#if defined (MDE_CPU_X64)
                    // On X64, system memory above 4GB can cause UEFI drivers
                    // to explode in bad ways due to UINT32 casts. Just mark
                    // regions above 4GB as untested, and use the null memory
                    // test later in BDS to mark them as tested.
                    if (rangeBase >= 0x100000000)
                    {
                        HobAddUntestedMemoryRange(Context, rangeBase, rangeLength);
                    }
                    else
                    {
                        HobAddMemoryRange(Context, rangeBase, rangeLength);
                    }
#else
                    // On other architectures, just add the memory range like normal.
                    HobAddMemoryRange(Context, rangeBase, rangeLength);
#endif
                }
            }
        } while ((UINT8*)range < ((UINT8*)memMap + memMapSize));

        if (pass == 0)
        {
            //
            // Now that the preferred allocation block has been chosen,
            // configure PEI allocations and any initial memory ranges.
            //
            INITPEIMEMORY(Context, peiBase, peiLength, &preallocatedBase, &preallocatedLength);

            //
            // Create a memory range for the preallocated region and the
            // config blob and mark both as allocated.
            //
            HobAddMemoryRange(Context, preallocatedBase, preallocatedLength);
            HobAddAllocatedMemoryRange(preallocatedBase, preallocatedLength);

            HobAddMemoryRange(Context, configBlobBase, configBlobSize);
            HobAddAllocatedMemoryRange(configBlobBase, configBlobSize);
        }
    }

#if defined (MDE_CPU_X64)
    //
    // Initialize the fixed MTRR for low memory.
    // The variable MTRRs are set later in this function with a trigger to
    // the BiosDevice.
    //
    // N.B. This call also has the effect of enabling MTRRs. The default
    // MTRR type remains uncached.
    //
    MtrrSetMemoryAttribute(0, SIZE_512KB + SIZE_128KB, CacheWriteBack);
#endif

    //
    // Low and high MMIO range
    //
#if defined (MDE_CPU_X64)
    HobAddMmioRange(
        PcdGet64(PcdLowMmioGapBasePageNumber) * SIZE_4KB,
        PcdGet64(PcdLowMmioGapSizeInPages) * SIZE_4KB
        );
#elif defined(MDE_CPU_AARCH64)
    //
    // For ARM64 we are still using the BiosDevice for runtime services.
    // However the registers are now in MMIO space instead of IO space. Therefore the
    // addresses need to be translated after the guest calls SetVirtualAddressMap.
    // To have the address range included with the guest's call to SetVirtualAddressMap
    // the range has to be declared as DXE runtime memory. That has to be done in DXE phase
    // by a driver so the range can't be declared as MMIO here.  Therefore leave that page
    // out of this early general platform declaration.
    //
    UINT64 GapBase = PcdGet32(PcdBiosBaseAddress);
    UINT64 GapSize = SIZE_4KB;
    UINT64 FirstRangeBase = PcdGet64(PcdLowMmioGapBasePageNumber) * SIZE_4KB;
    UINT64 FirstRangeSize = GapBase - FirstRangeBase;
    UINT64 SecondRangeBase = FirstRangeBase + FirstRangeSize + GapSize;
    UINT64 SecondRangeSize = (PcdGet64(PcdLowMmioGapSizeInPages) * SIZE_4KB) -
                             (FirstRangeSize + GapSize);

    HobAddMmioRange(
        FirstRangeBase,
        FirstRangeSize
        );

    HobAddMmioRange(
        SecondRangeBase,
        SecondRangeSize
        );
#endif
    HobAddMmioRange(
        PcdGet64(PcdHighMmioGapBasePageNumber) * SIZE_4KB,
        PcdGet64(PcdHighMmioGapSizeInPages) * SIZE_4KB
        );

    //
    // Memory Type Information HOB
    //
#if defined(MDE_CPU_X64)
    if (IsHardwareIsolatedNoParavisor() && GetIsolationType() == UefiIsolationTypeTdx)
    {
        HobAddGuidData(
            &gEfiMemoryTypeInformationGuid,
            MsvmDefaultMemoryTypeInformationTdxGuest,
            sizeof(MsvmDefaultMemoryTypeInformationTdxGuest)
            );
    }
    else
#endif

    if (PcdGetBool(PcdHibernateEnabled))
    {
        HobAddGuidData(
                &gEfiMemoryTypeInformationGuid,
                MsvmMemoryTypeInformationHibernateEnabled,
                sizeof(MsvmMemoryTypeInformationHibernateEnabled)
                );
    }
    else
    {
        HobAddGuidData(
                &gEfiMemoryTypeInformationGuid,
                MsvmDefaultMemoryTypeInformation,
                sizeof(MsvmDefaultMemoryTypeInformation)
                );
    }

    //
    // Add CPU HOB with resultant address width and 16-bits of IO space.
    //
    HobAddCpu(Context->PhysicalAddressWidth, 16);

#if defined (MDE_CPU_X64)
    //
    // Tell the BiosDevice to set up the variable MTRRs.
    //
    if (!suppressBiosDevice && !hostEmulatorsWhenHardwareIsolated)
    {
        //
        // Setting MTRRs for virtual processors is not supported for
        // hardware isolated systems.
        //
        WriteBiosDevice(BiosConfigBootFinalize, Context->PhysicalAddressWidth);
    }
#endif

#if defined(MDE_CPU_AARCH64)
    //
    // Configure the MMU.
    //
    ConfigureMmu(
        (1ULL << Context->PhysicalAddressWidth) - 1
        );
#endif

    DEBUG((DEBUG_VERBOSE, "<<< InitializeMemoryMap\n"));
}


VOID
InitializeWatchdog()
/*++

Routine Description:

    Initializes and starts the watchdog timer

    Note that until the Watchdog DXE driver is loaded, there is no entity
    to reset the watchdog count. This should not be an issue since the initial
    watchdog count is in minutes and the DXE driver should load within milliseconds

Arguments:

    None.

Return Value:

    None.

--*/
{
    UINT32 hwResolution;

    hwResolution = ReadBiosDevice(BiosConfigWatchdogResolution);

    if ((hwResolution != 0) && (hwResolution != BIOS_WATCHDOG_NOT_ENABLED))
    {
        //
        // Use one-shot mode and the default count for the watchdog device.
        // Directly program the watchdog registers since the WatchdogTimerLib
        // is only available for DXE drivers
        //
        WriteBiosDevice(BiosConfigWatchdogConfig, BIOS_WATCHDOG_RUNNING | BIOS_WATCHDOG_ONE_SHOT);
    }

}

VOID
InitializeDeviceState()
/*++

Routine Description:

    Initializes any device state needed during PEI initialization

Arguments:

    None.

Return Value:

    None.

--*/
{
    DEVICE_STATE deviceState = 0;

    if (PcdGetBool(PcdDebuggerEnabled))
    {
        DEBUG((DEBUG_INFO, "Debugger enabled\n"));
        deviceState |= DEVICE_STATE_SOURCE_DEBUG_ENABLED;
    }

#if defined(DEBUG_PLATFORM)
    deviceState |= DEVICE_STATE_DEVELOPMENT_BUILD_ENABLED;
#endif

    // Secure boot state requires NVRAM access and will be set in
    // early DXE phase via PlatformDeviceStateHelperInit

    AddDeviceState(deviceState);
}

EFI_STATUS
EFIAPI
InitializePlatform(
    IN          EFI_PEI_FILE_HANDLE FileHandle,
    IN CONST    EFI_PEI_SERVICES**  PeiServices
  )
/*++

Routine Description:

    Entry point of the Platform PEIM.  Initializes the platform.

Arguments:

    FileHandle - Handle of the file being invoked.

    PeiServices  An indirect pointer to the PEI Services Table.

Return Value:

    EFI_SUCCESS on success, otherwise an error status.

--*/
{
    PLATFORM_INIT_CONTEXT context;
    EFI_STATUS status;

    DEBUG((DEBUG_VERBOSE, ">>> *** Platform PEIM InitializePlatform@%p\n", InitializePlatform));

    ZeroMem(&context, sizeof(context));

    //
    // Determine whether this system is running isolated in order to determine
    // the correct mechanism for loading the configuration.
    //
    HvDetectIsolation();

    //
    // Get the configuration from the loader.
    //
    status = GetConfiguration(PeiServices, &context.PhysicalAddressWidth);
    if (EFI_ERROR(status))
    {
        ASSERT(FALSE);
        return status;
    }

    context.StartOfConfigBlob = GetStartOfConfigBlob();

    //
    // DxeBdLib.c InitializeDebugAgent is called very early on in DXE Core,
    // before any drivers are dispatched. Thus, we need to send this boolean
    // flag via a HOB since the Pcd module isn't yet available.
    //
    BOOLEAN debuggerEnabled = PcdGetBool(PcdDebuggerEnabled);
    HobAddGuidData(&gMsvmDebuggerEnabledGuid,
        &debuggerEnabled,
        sizeof(BOOLEAN));

    //
    // Set the boot mode and installs the boot mode tag PPI.
    //
    status = PeiServicesSetBootMode(BOOT_WITH_FULL_CONFIGURATION);
    ASSERT_EFI_ERROR(status);

    status = PeiServicesInstallPpi(MsvmBootModePpiDescriptor);
    ASSERT_EFI_ERROR(status);

    //
    // Init memory map before publishing any other HOBs.
    //
    InitializeMemoryMap(&context);

    //
    // Publish the FV HOB.
    //
    DEBUG((DEBUG_VERBOSE, "--- InitializePlatform FV Base %p Size %lx\n",
        PcdGet64(PcdFvBaseAddress), PcdGet32(PcdFvSize)));
    HobAddFvMemoryRange(PcdGet64(PcdFvBaseAddress), PcdGet32(PcdFvSize));

    HobAddFvMemoryRange(PcdGet64(PcdDxeFvBaseAddress), PcdGet32(PcdDxeFvSize));

    if (!IsHardwareIsolatedNoParavisor())
    {
        //
        // Init the watchdog
        //
        InitializeWatchdog();
    }


    //
    // Initialize device state before we finish.
    //
    InitializeDeviceState();

    DEBUG((DEBUG_VERBOSE, "<<< *** Platform PEIM InitializePlatform@%p\n", InitializePlatform));

    return EFI_SUCCESS;
}

