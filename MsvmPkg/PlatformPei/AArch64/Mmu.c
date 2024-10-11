/** @file
  File managing the MMU for ARMv8 architecture

  Copyright (c) 2011-2014, ARM Limited. All rights reserved.
  Copyright (c) 2016, Linaro Limited. All rights reserved.
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  Adapted from ArmPkg\Library\ArmMmuLib\AArch64\ArmMmuLibCore.c

**/

#include <Uefi.h>
#include <Chipset/AArch64.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ArmLib.h>
#include <Library/ArmMmuLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <BiosInterface.h>
#include <Config.h>

#include "Extra.h"

// We use this index definition to define an invalid block entry
#define TT_ATTR_INDX_INVALID    ((UINT32)~0)

STATIC
UINT64
ArmMemoryAttributeToPageAttribute(
    IN ARM_MEMORY_REGION_ATTRIBUTES  Attributes
)
{
    switch (Attributes)
    {
    case ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK:
        return TT_ATTR_INDX_MEMORY_WRITE_BACK | TT_SH_INNER_SHAREABLE;

    case ARM_MEMORY_REGION_ATTRIBUTE_WRITE_THROUGH:
        return TT_ATTR_INDX_MEMORY_WRITE_THROUGH | TT_SH_INNER_SHAREABLE;

        // Uncached and device mappings are treated as outer shareable by default,
    case ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED:
        return TT_ATTR_INDX_MEMORY_NON_CACHEABLE;

    default:
        ASSERT(0);
    case ARM_MEMORY_REGION_ATTRIBUTE_DEVICE:
        if (ArmReadCurrentEL() == AARCH64_EL2)
            return TT_ATTR_INDX_DEVICE_MEMORY | TT_XN_MASK;
        else
            return TT_ATTR_INDX_DEVICE_MEMORY | TT_UXN_MASK | TT_PXN_MASK;
    }
}

#define MIN_T0SZ        16
#define BITS_PER_LEVEL  9

VOID
GetRootTranslationTableInfo(
        UINTN   T0SZ,
    OUT UINTN   *TableLevel,
    OUT UINTN   *TableEntryCount
)
{
    // Get the level of the root table
    if (TableLevel)
    {
        *TableLevel = (T0SZ - MIN_T0SZ) / BITS_PER_LEVEL;
    }

    if (TableEntryCount)
    {
        *TableEntryCount = 1ULL << (BITS_PER_LEVEL - (T0SZ - MIN_T0SZ) % BITS_PER_LEVEL);
    }
}

STATIC
VOID
LookupAddresstoRootTable(
        UINT64  MaxAddress,
    OUT UINTN   *T0SZ,
    OUT UINTN   *TableEntryCount
)
{
    UINTN TopBit;

    // Check the parameters are not NULL
    ASSERT((T0SZ != NULL) && (TableEntryCount != NULL));

    // Look for the highest bit set in MaxAddress
    for (TopBit = 63; TopBit != 0; TopBit--)
    {
        if ((1ULL << TopBit) & MaxAddress)
        {
            // MaxAddress top bit is found
            TopBit = TopBit + 1;
            break;
        }
    }
    ASSERT(TopBit != 0);

    // Calculate T0SZ from the top bit of the MaxAddress
    *T0SZ = 64 - TopBit;

    // Get the Table info from T0SZ
    GetRootTranslationTableInfo(*T0SZ, NULL, TableEntryCount);
}

STATIC
UINT64*
GetBlockEntryListFromAddress(
    IN      UINT64  *RootTable,
            UINT64  TCR,
            UINT64  RegionStart,
    OUT     UINTN   *TableLevel,
    IN OUT  UINT64  *BlockEntrySize,
    OUT     UINT64  **LastBlockEntry
)
{
    UINTN   RootTableLevel;
    UINTN   RootTableEntryCount;
    UINT64 *TranslationTable;
    UINT64 *BlockEntry;
    UINT64 *SubTableBlockEntry;
    UINT64  BlockEntryAddress;
    UINTN   BaseAddressAlignment;
    UINTN   PageLevel;
    UINTN   Index;
    UINTN   IndexLevel;
    UINTN   T0SZ;
    UINT64  Attributes;
    UINT64  TableAttributes;

    // Initialize variable
    BlockEntry = NULL;

    // Ensure the parameters are valid
    if (!(TableLevel && BlockEntrySize && LastBlockEntry))
    {
        ASSERT_EFI_ERROR(EFI_INVALID_PARAMETER);
        return NULL;
    }

    // Ensure the Region is aligned on 4KB boundary
    if ((RegionStart & (SIZE_4KB - 1)) != 0)
    {
        ASSERT_EFI_ERROR(EFI_INVALID_PARAMETER);
        return NULL;
    }

    // Ensure the required size is aligned on 4KB boundary and not 0
    if ((*BlockEntrySize & (SIZE_4KB - 1)) != 0 || *BlockEntrySize == 0)
    {
        ASSERT_EFI_ERROR(EFI_INVALID_PARAMETER);
        return NULL;
    }

    T0SZ = TCR & TCR_T0SZ_MASK;
    // Get the Table info from T0SZ
    GetRootTranslationTableInfo(T0SZ, &RootTableLevel, &RootTableEntryCount);

    // If the start address is 0x0 then we use the size of the region to identify the alignment
    if (RegionStart == 0)
    {
        // Identify the highest possible alignment for the Region Size
        BaseAddressAlignment = LowBitSet64(*BlockEntrySize);
    }
    else
    {
        // Identify the highest possible alignment for the Base Address
        BaseAddressAlignment = LowBitSet64(RegionStart);
    }

    // Identify the Page Level the RegionStart must belong to. Note that PageLevel
    // should be at least 1 since block translations are not supported at level 0
    PageLevel = MAX(3 - ((BaseAddressAlignment - 12) / 9), 1);

    // If the required size is smaller than the current block size then we need to go to the page below.
    // The PageLevel was calculated on the Base Address alignment but did not take in account the alignment
    // of the allocation size
    while (*BlockEntrySize < TT_BLOCK_ENTRY_SIZE_AT_LEVEL(PageLevel))
    {
        // It does not fit so we need to go a page level above
        PageLevel++;
    }

    //
    // Get the Table Descriptor for the corresponding PageLevel. We need to decompose RegionStart to get appropriate entries
    //

    TranslationTable = RootTable;
    for (IndexLevel = RootTableLevel; IndexLevel <= PageLevel; IndexLevel++)
    {
        BlockEntry = (UINT64*)TT_GET_ENTRY_FOR_ADDRESS(TranslationTable, IndexLevel, RegionStart);

        if ((IndexLevel != 3) && ((*BlockEntry & TT_TYPE_MASK) == TT_TYPE_TABLE_ENTRY))
        {
            // Go to the next table
            TranslationTable = (UINT64*)(*BlockEntry & TT_ADDRESS_MASK_DESCRIPTION_TABLE);

            // If we are at the last level then update the last level to next level
            if (IndexLevel == PageLevel)
            {
                // Enter the next level
                PageLevel++;
            }
        }
        else if ((*BlockEntry & TT_TYPE_MASK) == TT_TYPE_BLOCK_ENTRY)
        {
            // If we are not at the last level then we need to split this BlockEntry
            if (IndexLevel != PageLevel)
            {
                // Retrieve the attributes from the block entry
                Attributes = *BlockEntry & TT_ATTRIBUTES_MASK;

                // Convert the block entry attributes into Table descriptor attributes
                TableAttributes = TT_TABLE_AP_NO_PERMISSION;
                if (Attributes & TT_NS)
                {
                    TableAttributes = TT_TABLE_NS;
                }

                // Get the address corresponding at this entry
                BlockEntryAddress = RegionStart;
                BlockEntryAddress = BlockEntryAddress >> TT_ADDRESS_OFFSET_AT_LEVEL(IndexLevel);
                // Shift back to right to set zero before the effective address
                BlockEntryAddress = BlockEntryAddress << TT_ADDRESS_OFFSET_AT_LEVEL(IndexLevel);

                // Set the correct entry type for the next page level
                if ((IndexLevel + 1) == 3)
                {
                    Attributes |= TT_TYPE_BLOCK_ENTRY_LEVEL3;
                }
                else
                {
                    Attributes |= TT_TYPE_BLOCK_ENTRY;
                }

                // Create a new translation table
                TranslationTable = AllocatePages(1);
                //DEBUG((DEBUG_VERBOSE, "ConfigureMmu using page @ %p\n", TranslationTable));
                if (TranslationTable == NULL)
                {
                    return NULL;
                }

                // Populate the newly created lower level table
                SubTableBlockEntry = TranslationTable;
                for (Index = 0; Index < TT_ENTRY_COUNT; Index++)
                {
                    *SubTableBlockEntry = Attributes | (BlockEntryAddress + (Index << TT_ADDRESS_OFFSET_AT_LEVEL(IndexLevel + 1)));
                    SubTableBlockEntry++;
                }

                // Fill the BlockEntry with the new TranslationTable
                *BlockEntry = ((UINTN)TranslationTable & TT_ADDRESS_MASK_DESCRIPTION_TABLE) | TableAttributes | TT_TYPE_TABLE_ENTRY;
            }
        }
        else
        {
            if (IndexLevel != PageLevel)
            {
                //
                // Case when we have an Invalid Entry and we are at a page level above of the one targetted.
                //

                // Create a new translation table
                TranslationTable = AllocatePages(1);
                //DEBUG((DEBUG_VERBOSE, "ConfigureMmu using page @ %p\n", TranslationTable));
                if (TranslationTable == NULL)
                {
                    return NULL;
                }

                ZeroMem(TranslationTable, TT_ENTRY_COUNT * sizeof(UINT64));

                // Fill the new BlockEntry with the TranslationTable
                *BlockEntry = ((UINTN)TranslationTable & TT_ADDRESS_MASK_DESCRIPTION_TABLE) | TT_TYPE_TABLE_ENTRY;
            }
        }
    }

    // Expose the found PageLevel to the caller
    *TableLevel = PageLevel;

    // Now, we have the Table Level we can get the Block Size associated to this table
    *BlockEntrySize = TT_BLOCK_ENTRY_SIZE_AT_LEVEL(PageLevel);

    // The last block of the root table depends on the number of entry in this table,
    // otherwise it is always the (TT_ENTRY_COUNT - 1)th entry in the table.
    *LastBlockEntry = TT_LAST_BLOCK_ADDRESS(TranslationTable,
        (PageLevel == RootTableLevel) ? RootTableEntryCount : TT_ENTRY_COUNT);

    return BlockEntry;
}

STATIC
EFI_STATUS
UpdateRegionMapping(
    IN  UINT64  *RootTable,
        UINT64  TCR,
        UINT64  RegionStart,
        UINT64  RegionLength,
        UINT64  Attributes,
        UINT64  BlockEntryMask
)
{
    UINT32  Type;
    UINT64  *BlockEntry;
    UINT64  *LastBlockEntry;
    UINT64  BlockEntrySize;
    UINTN   TableLevel;

    // Ensure the Length is aligned on 4KB boundary
    if ((RegionLength == 0) || ((RegionLength & (SIZE_4KB - 1)) != 0))
    {
        ASSERT_EFI_ERROR(EFI_INVALID_PARAMETER);
        return EFI_INVALID_PARAMETER;
    }

    do
    {
        // Get the first Block Entry that matches the Virtual Address and also the information on the Table Descriptor
        // such as the the size of the Block Entry and the address of the last BlockEntry of the Table Descriptor
        BlockEntrySize = RegionLength;
        BlockEntry = GetBlockEntryListFromAddress(RootTable, TCR, RegionStart, &TableLevel, &BlockEntrySize, &LastBlockEntry);
        if (BlockEntry == NULL)
        {
            // GetBlockEntryListFromAddress() return NULL when it fails to allocate new pages from the Translation Tables
            return EFI_OUT_OF_RESOURCES;
        }

        if (TableLevel != 3)
        {
            Type = TT_TYPE_BLOCK_ENTRY;
        }
        else
        {
            Type = TT_TYPE_BLOCK_ENTRY_LEVEL3;
        }

        do
        {
            // Fill the Block Entry with attribute and output block address
            *BlockEntry &= BlockEntryMask;
            *BlockEntry |= (RegionStart & TT_ADDRESS_MASK_BLOCK_ENTRY) | Attributes | Type;

            // Go to the next BlockEntry
            RegionStart += BlockEntrySize;
            RegionLength -= BlockEntrySize;
            BlockEntry++;

            // Break the inner loop when next block is a table
            // Rerun GetBlockEntryListFromAddress to avoid page table memory leak
            if (TableLevel != 3 &&
                (*BlockEntry & TT_TYPE_MASK) == TT_TYPE_TABLE_ENTRY)
            {
                break;
            }
        } while ((RegionLength >= BlockEntrySize) && (BlockEntry <= LastBlockEntry));
    } while (RegionLength != 0);

    return EFI_SUCCESS;
}

STATIC
EFI_STATUS
FillTranslationTable(
    IN  UINT64                        *RootTable,
        UINT64                        TCR,
    IN  ARM_MEMORY_REGION_DESCRIPTOR  *MemoryRegion
)
{
    return UpdateRegionMapping(
        RootTable,
        TCR,
        MemoryRegion->VirtualBase,
        MemoryRegion->Length,
        ArmMemoryAttributeToPageAttribute(MemoryRegion->Attributes) | TT_AF,
        0
    );
}

EFI_STATUS
EFIAPI
ConfigureMmu(
    UINT64  MaxAddress
)
{
    VOID*                         TranslationTable;
    UINT32                        TranslationTableAttribute;
    UINTN                         T0SZ;
    UINTN                         RootTableEntryCount;
    UINT64                        TCR;
    EFI_STATUS                    Status;
    ARM_MEMORY_REGION_DESCRIPTOR* MemoryTable;
    UINT64                        lowMmioBaseAddress = PcdGet64(PcdLowMmioGapBasePageNumber) * SIZE_4KB;
    UINT64                        lowMmioSize = PcdGet64(PcdLowMmioGapSizeInPages) * SIZE_4KB;
    UINT64                        highMmioBaseAddress = PcdGet64(PcdHighMmioGapBasePageNumber) * SIZE_4KB;
    UINT64                        highMmioSize = PcdGet64(PcdHighMmioGapSizeInPages) * SIZE_4KB;
#define MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS 6
    ARM_MEMORY_REGION_DESCRIPTOR virtualMemoryTable[MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS];

    DEBUG((DEBUG_VERBOSE, "ConfigureMmu(0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx)\n",
        MaxAddress, lowMmioBaseAddress, lowMmioSize, highMmioBaseAddress, highMmioSize));

    //
    // Fill table that drives the mmu setup functions.
    //
    // From zero to beginning of low MMIO gap.
    virtualMemoryTable[0].PhysicalBase = 0;
    virtualMemoryTable[0].VirtualBase = virtualMemoryTable[0].PhysicalBase;
    virtualMemoryTable[0].Length = (SIZE_4GB - lowMmioSize);
    virtualMemoryTable[0].Attributes = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;

    // First MMIO gap.
    virtualMemoryTable[1].PhysicalBase = virtualMemoryTable[0].PhysicalBase + virtualMemoryTable[0].Length;
    virtualMemoryTable[1].VirtualBase = virtualMemoryTable[1].PhysicalBase;
    virtualMemoryTable[1].Length = lowMmioSize;
    virtualMemoryTable[1].Attributes = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

    // From 4GB to beginning of high MMIO gap.
    virtualMemoryTable[2].PhysicalBase = virtualMemoryTable[1].PhysicalBase + virtualMemoryTable[1].Length;
    virtualMemoryTable[2].VirtualBase = virtualMemoryTable[2].PhysicalBase;
    virtualMemoryTable[2].Length = highMmioBaseAddress - virtualMemoryTable[2].PhysicalBase;
    virtualMemoryTable[2].Attributes = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;

    // Second MMIO gap.
    virtualMemoryTable[3].PhysicalBase = virtualMemoryTable[2].PhysicalBase + virtualMemoryTable[2].Length;
    virtualMemoryTable[3].VirtualBase = virtualMemoryTable[3].PhysicalBase;
    virtualMemoryTable[3].Length = highMmioSize;
    virtualMemoryTable[3].Attributes = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

    // To top of address space.
    virtualMemoryTable[4].PhysicalBase = virtualMemoryTable[3].PhysicalBase + virtualMemoryTable[3].Length;
    virtualMemoryTable[4].VirtualBase = virtualMemoryTable[4].PhysicalBase;
    virtualMemoryTable[4].Length = MaxAddress + 1 - virtualMemoryTable[4].PhysicalBase;
    virtualMemoryTable[4].Attributes = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;

    virtualMemoryTable[5].PhysicalBase = 0;
    virtualMemoryTable[5].VirtualBase = 0;
    virtualMemoryTable[5].Length = 0;
    virtualMemoryTable[5].Attributes = 0;

    // Lookup the Table Level to get the information
    LookupAddresstoRootTable(MaxAddress, &T0SZ, &RootTableEntryCount);

    //
    // Calculate new TCR value
    //
    // Ideally we will be running at EL2, but should support EL1 as well.
    // UEFI should not run at EL3.
    if (ArmReadCurrentEL() == AARCH64_EL2)
    {
        //Note: Bits 23 and 31 are reserved(RES1) bits in TCR_EL2
        TCR = T0SZ | (1UL << 31) | (1UL << 23) | TCR_TG0_4KB;

        // Set the Physical Address Size using MaxAddress
        if (MaxAddress < SIZE_4GB)
        {
            TCR |= TCR_PS_4GB;
        }
        else if (MaxAddress < SIZE_64GB)
        {
            TCR |= TCR_PS_64GB;
        }
        else if (MaxAddress < SIZE_1TB)
        {
            TCR |= TCR_PS_1TB;
        }
        else if (MaxAddress < SIZE_4TB)
        {
            TCR |= TCR_PS_4TB;
        }
        else if (MaxAddress < SIZE_16TB)
        {
            TCR |= TCR_PS_16TB;
        }
        else if (MaxAddress < SIZE_256TB)
        {
            TCR |= TCR_PS_256TB;
        }
        else
        {
            DEBUG((EFI_D_ERROR, "ArmConfigureMmu: The MaxAddress 0x%lX is not supported by this MMU configuration.\n", MaxAddress));
            ASSERT(0); // Bigger than 48-bit memory space are not supported
            return EFI_UNSUPPORTED;
        }
    }
    else if (ArmReadCurrentEL() == AARCH64_EL1)
    {
        // Due to Cortex-A57 erratum #822227 we must set TG1[1] == 1, regardless of EPD1.
        TCR = T0SZ | TCR_TG0_4KB | TCR_TG1_4KB | TCR_EPD1;

        // Set the Physical Address Size using MaxAddress
        if (MaxAddress < SIZE_4GB)
        {
            TCR |= TCR_IPS_4GB;
        }
        else if (MaxAddress < SIZE_64GB)
        {
            TCR |= TCR_IPS_64GB;
        }
        else if (MaxAddress < SIZE_1TB)
        {
            TCR |= TCR_IPS_1TB;
        }
        else if (MaxAddress < SIZE_4TB)
        {
            TCR |= TCR_IPS_4TB;
        }
        else if (MaxAddress < SIZE_16TB)
        {
            TCR |= TCR_IPS_16TB;
        }
        else if (MaxAddress < SIZE_256TB)
        {
            TCR |= TCR_IPS_256TB;
        }
        else
        {
            DEBUG((EFI_D_ERROR, "ArmConfigureMmu: The MaxAddress 0x%lX is not supported by this MMU configuration.\n", MaxAddress));
            ASSERT(0); // Bigger than 48-bit memory space are not supported
            return EFI_UNSUPPORTED;
        }
    }
    else
    {
        ASSERT(0); // UEFI is only expected to run at EL2 and EL1, not EL3.
        return EFI_UNSUPPORTED;
    }

    //
    // Translation table walks are always cache coherent on ARMv8-A, so cache
    // maintenance on page tables is never needed. Since there is a risk of
    // loss of coherency when using mismatched attributes, and given that memory
    // is mapped cacheable except for extraordinary cases (such as non-coherent
    // DMA), have the page table walker perform cached accesses as well, and
    // assert below that that matches the attributes we use for CPU accesses to
    // the region.
    //
    TCR |= TCR_SH_INNER_SHAREABLE |
        TCR_RGN_OUTER_WRITE_BACK_ALLOC |
        TCR_RGN_INNER_WRITE_BACK_ALLOC;



    // Allocate page for root translation table
    TranslationTable = AllocatePages(1);
    //DEBUG((DEBUG_VERBOSE, "ConfigureMmu using page @ %p\n", TranslationTable));
    if (TranslationTable == NULL)
    {
        return EFI_OUT_OF_RESOURCES;
    }

    ZeroMem(TranslationTable, RootTableEntryCount * sizeof(UINT64));

    TranslationTableAttribute = TT_ATTR_INDX_INVALID;
    MemoryTable = virtualMemoryTable;
    while (MemoryTable->Length != 0)
    {

        DEBUG_CODE_BEGIN();
        // Find the memory attribute for the Translation Table
        if ((UINTN)TranslationTable >= MemoryTable->PhysicalBase &&
            (UINTN)TranslationTable + EFI_PAGE_SIZE <= MemoryTable->PhysicalBase +
            MemoryTable->Length)
        {
            TranslationTableAttribute = MemoryTable->Attributes;
        }
        DEBUG_CODE_END();

        Status = FillTranslationTable(TranslationTable, TCR, MemoryTable);
        if (EFI_ERROR(Status))
        {
            goto FREE_TRANSLATION_TABLE;
        }
        MemoryTable++;
    }

    ASSERT(TranslationTableAttribute == ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK);

    ConfigureCachesAndMmu(
        TranslationTable,
        TCR,
        MAIR_ATTR(TT_ATTR_INDX_DEVICE_MEMORY, MAIR_ATTR_DEVICE_MEMORY) |                      // mapped to EFI_MEMORY_UC
        MAIR_ATTR(TT_ATTR_INDX_MEMORY_NON_CACHEABLE, MAIR_ATTR_NORMAL_MEMORY_NON_CACHEABLE) | // mapped to EFI_MEMORY_WC
        MAIR_ATTR(TT_ATTR_INDX_MEMORY_WRITE_THROUGH, MAIR_ATTR_NORMAL_MEMORY_WRITE_THROUGH) | // mapped to EFI_MEMORY_WT
        MAIR_ATTR(TT_ATTR_INDX_MEMORY_WRITE_BACK, MAIR_ATTR_NORMAL_MEMORY_WRITE_BACK));

    return EFI_SUCCESS;

FREE_TRANSLATION_TABLE:
    FreePages(TranslationTable, 1);
    return Status;
}

