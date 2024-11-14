/** @file
  Gets configuration values and exports them as globals and PCDs.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Platform.h>
#include <BiosInterface.h>
#include <AcpiTables.h>
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/Tpm20.h>
#if defined(MDE_CPU_AARCH64)
#include <Library/ArmLib.h>
#endif
#include <Library/CrashDumpAgentLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/ResourcePublicationLib.h>
#include <IsolationTypes.h>
#include "Hv.h"
#include "Config.h"
#include <Guid/DxeMemoryProtectionSettings.h>
#include <UefiConstants.h>
#include <Hob.h>

//
// Values and type used with CPUID to get the physical address width.
//
#define CPUID_FUNCTION_EXTENDED_MAX_FUNCTION        0x80000000
#define CPUID_FUNCTION_EXTENDED_ADDRESS_SPACE_SIZES 0x80000008

typedef union _CPUID_ADDRESS_SPACE_SIZES
{
#pragma warning(disable : 4201)
    struct
    {
        UINT8 PhysicalAddressBits;
        UINT8 VirtualAddressBits;
        UINT16 Reserved;
    };
#pragma warning(default : 4201)

    UINT32 Value;
} CPUID_ADDRESS_SPACE_SIZES;

UINT8
GetPhysicalAddressWidth(
    IN CONST EFI_PEI_SERVICES**  PeiServices
    )
/*++

Routine Description:

    Gets the number of bits in the CPU address width.

Arguments:

    PeiServices - An indirect pointer to the PEI Services Table.

Return Value:

    The number of bits in the address width.

--*/
{
    UINT8 maximumAddressWidth = 48;
    UINT8 minimumAddressWidth = 36;
    UINT8 physicalAddressWidth = 0;

#if defined(MDE_CPU_X64)

    UINT32 maximumFunction;
    CPUID_ADDRESS_SPACE_SIZES addressSpaceSizes;

    if (mIsolationType == UefiIsolationTypeTdx)
    {
        // The shared GPA bit position defines the physical address width.
        physicalAddressWidth = (UINT8)mSharedGpaBit + 1;
        DEBUG((DEBUG_VERBOSE, "TDX PhysicalAddressWidth %d\n", physicalAddressWidth));
        return physicalAddressWidth;
    }

    //
    // Query with CPUID
    //
    AsmCpuid(CPUID_FUNCTION_EXTENDED_MAX_FUNCTION, &maximumFunction, NULL, NULL, NULL);
    if (maximumFunction >= CPUID_FUNCTION_EXTENDED_ADDRESS_SPACE_SIZES)
    {
        AsmCpuid(CPUID_FUNCTION_EXTENDED_ADDRESS_SPACE_SIZES,
                 &addressSpaceSizes.Value,
                 NULL,
                 NULL,
                 NULL);

        physicalAddressWidth = addressSpaceSizes.PhysicalAddressBits;
    }
    else
    {
        // It is highly unlikely that the CPUID leaf doesn't exist.
        // Regardless just use the minimum as the default.
        DEBUG((DEBUG_WARN, "Can't query CPUID so defaulting address width to %u bits\n",
            minimumAddressWidth));
        physicalAddressWidth = minimumAddressWidth;
    }

    if ((mIsolationType == UefiIsolationTypeSnp) && (mSharedGpaBit != 0))
    {
        // Ensure the address width is at least wide enough to hold the shared GPA bit.
        minimumAddressWidth = (UINT8)mSharedGpaBit + 1;
    }

#elif defined(MDE_CPU_AARCH64)

    // Read system register ID_AA64MMFR0_EL1
    // ID_AA64MMFR0_EL1.PARange is bits[3:0]
    // Valid values for ARMv8.1 PARange are 0 thru 6 which mean the following address widths.
    static UINT8 aw[7] = { 32, 36, 40, 42, 44, 48, 52 };
    UINT64 regValue = (UINT64)ArmReadIdMmfr0();
    DEBUG((DEBUG_VERBOSE, "ArmReadIdMmfr0 %lx PARange %lx\n", regValue, regValue & 0xF));
    if ((regValue & 0xF) < 7)
    {
        physicalAddressWidth = aw[regValue & 0xF];
    }
    else
    {
        // It is highly unlikely for the register to have an invalid value.
        // Regardless just use 36 as the default.
        DEBUG((DEBUG_WARN, "Invalid D_AA64MMFR0_EL1.PARange so defaulting address width to %u bits\n",
            minimumAddressWidth));
        physicalAddressWidth = minimumAddressWidth;
    }

#endif

    if (physicalAddressWidth < minimumAddressWidth)
    {
        DEBUG((DEBUG_WARN, "Increasing address width from %u to %u\n",
            physicalAddressWidth, minimumAddressWidth));
        physicalAddressWidth = minimumAddressWidth;
    }

    if (physicalAddressWidth > maximumAddressWidth)
    {
        DEBUG((DEBUG_WARN, "Reducing address width from %u to %u\n",
            physicalAddressWidth, maximumAddressWidth));
        physicalAddressWidth = maximumAddressWidth;
    }

    DEBUG((DEBUG_VERBOSE, "PhysicalAddressWidth %d\n", physicalAddressWidth));

    return physicalAddressWidth;
}


VOID
DebugDumpMadt(
    IN VOID* Madt
)
{
#if !defined(MDEPKG_NDEBUG)
    EFI_ACPI_DESCRIPTION_HEADER  *acpiHdr = (EFI_ACPI_DESCRIPTION_HEADER*)Madt;
    EFI_ACPI_6_2_IO_APIC_STRUCTURE* madtIoApic;
    EFI_ACPI_6_2_LOCAL_APIC_NMI_STRUCTURE* madtApicNmi;
    EFI_ACPI_6_2_INTERRUPT_SOURCE_OVERRIDE_STRUCTURE* madtOverride;
    EFI_ACPI_6_2_PROCESSOR_LOCAL_APIC_STRUCTURE* madtApic;
    EFI_ACPI_6_2_PROCESSOR_LOCAL_X2APIC_STRUCTURE* madtX2Apic;
    EFI_ACPI_6_2_GIC_DISTRIBUTOR_STRUCTURE* madtGicd;
    EFI_ACPI_6_2_GIC_STRUCTURE* madtGicc;

    UINT8 *cursor;

    //
    // Debug dump the MADT entries.
    //
    DEBUG((DEBUG_VERBOSE, "--- MADT data @ %x\n", acpiHdr));
    DEBUG((DEBUG_VERBOSE, "    Header Signature %x\n", acpiHdr->Signature));
    DEBUG((DEBUG_VERBOSE, "    Length %x\n", acpiHdr->Length));

    cursor = (UINT8 *)acpiHdr;
    cursor += sizeof(EFI_ACPI_6_2_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER);

    do
    {
        switch (*cursor)  // UINT8 sized Type always at start of struct
        {

        case EFI_ACPI_6_2_IO_APIC:
            madtIoApic = (EFI_ACPI_6_2_IO_APIC_STRUCTURE *)cursor;

            DEBUG((DEBUG_VERBOSE, "    IOAPIC Type %x Len %02x IoApicId %02x IoApicAddress %02x\n",
                madtIoApic->Type, madtIoApic->Length, madtIoApic->IoApicId, madtIoApic->IoApicAddress));

            cursor += madtIoApic->Length;
            break;

        case EFI_ACPI_6_2_LOCAL_APIC_NMI:
            madtApicNmi = (EFI_ACPI_6_2_LOCAL_APIC_NMI_STRUCTURE *)cursor;

            DEBUG((DEBUG_VERBOSE, "    APIC NMI Type %x Len %02x Flags %02x AcpiProcessorUid %02x LocalApicLint %x\n",
                madtApicNmi->Type, madtApicNmi->Length, madtApicNmi->Flags, madtApicNmi->AcpiProcessorUid,
                madtApicNmi->LocalApicLint));

            cursor += madtApicNmi->Length;
            break;

        case EFI_ACPI_6_2_INTERRUPT_SOURCE_OVERRIDE:
            madtOverride = (EFI_ACPI_6_2_INTERRUPT_SOURCE_OVERRIDE_STRUCTURE *)cursor;

            DEBUG((DEBUG_VERBOSE, "    Interrupt Source Override Type %x Len %02x Flags %02x Source %02x GlobalSystemInterrupt %x\n",
                madtOverride->Type, madtOverride->Length, madtOverride->Flags, madtOverride->Source,
                madtOverride->GlobalSystemInterrupt));

            cursor += madtOverride->Length;
            break;

        case EFI_ACPI_6_2_PROCESSOR_LOCAL_APIC:
            madtApic = (EFI_ACPI_6_2_PROCESSOR_LOCAL_APIC_STRUCTURE *)cursor;

            DEBUG((DEBUG_VERBOSE, "    APIC Type %x Len %02x Flags %02x ApicId %02x\n",
                madtApic->Type, madtApic->Length, madtApic->Flags, madtApic->ApicId));

            cursor += madtApic->Length;
            break;

        case EFI_ACPI_6_2_PROCESSOR_LOCAL_X2APIC:
            madtX2Apic = (EFI_ACPI_6_2_PROCESSOR_LOCAL_X2APIC_STRUCTURE *)cursor;

            DEBUG((DEBUG_VERBOSE, "   X2APIC Type %x Len %02x Flags %02x X2ApicId %02x\n",
                madtX2Apic->Type, madtX2Apic->Length, madtX2Apic->Flags, madtX2Apic->X2ApicId));

            cursor += madtX2Apic->Length;
            break;

        case EFI_ACPI_6_2_GICD:
            madtGicd = (EFI_ACPI_6_2_GIC_DISTRIBUTOR_STRUCTURE *)cursor;

            DEBUG((DEBUG_VERBOSE, "   GICD Type %x Len %02x GicId %02x PhysicalBaseAddress %02x\n",
                madtGicd->Type, madtGicd->Length, madtGicd->GicId, madtGicd->PhysicalBaseAddress));

            cursor += madtGicd->Length;
            break;

        case EFI_ACPI_6_2_GIC:
            madtGicc = (EFI_ACPI_6_2_GIC_STRUCTURE *)cursor;

            DEBUG((DEBUG_VERBOSE, "   GICD Type %x Len %02x Flags %02x AcpiProcessorUid %02x\n",
                madtGicc->Type, madtGicc->Length, madtGicc->Flags, madtGicc->AcpiProcessorUid));

            cursor += madtGicc->Length;
            break;

        default:
            madtApic = (EFI_ACPI_6_2_PROCESSOR_LOCAL_APIC_STRUCTURE *)cursor;

            DEBUG((DEBUG_VERBOSE, "    APIC Type %x Len %02x Flags %02x ApicId %02x\n",
                madtApic->Type, madtApic->Length, madtApic->Flags, madtApic->ApicId));

            cursor += madtApic->Length;
            break;
        }
    } while (cursor < ((UINT8 *)acpiHdr + acpiHdr->Length));

#endif
}


VOID
DebugDumpSrat(
    IN VOID* Srat
    )
{
#if !defined(MDEPKG_NDEBUG)
    EFI_ACPI_DESCRIPTION_HEADER  *acpiHdr = (EFI_ACPI_DESCRIPTION_HEADER*) Srat;
    EFI_ACPI_6_2_PROCESSOR_LOCAL_APIC_SAPIC_AFFINITY_STRUCTURE *sratApic;
    EFI_ACPI_6_2_PROCESSOR_LOCAL_X2APIC_AFFINITY_STRUCTURE *sratX2Apic;
    EFI_ACPI_6_2_GICC_AFFINITY_STRUCTURE *sratGicc;
    EFI_ACPI_6_2_MEMORY_AFFINITY_STRUCTURE *sratMem;
    UINTN base, size;
    UINT8 *cursor;

    //
    // Debug dump the SRAT entries.
    //
    DEBUG((DEBUG_VERBOSE, "--- SRAT data @ %x\n", acpiHdr));
    DEBUG((DEBUG_VERBOSE, "    Header Signature %x\n", acpiHdr->Signature));
    DEBUG((DEBUG_VERBOSE, "    Length %x\n", acpiHdr->Length));

    cursor = (UINT8 *)acpiHdr;
    cursor += sizeof(EFI_ACPI_6_2_SYSTEM_RESOURCE_AFFINITY_TABLE_HEADER);

    do
    {
        switch(*cursor)  // UINT8 sized Type always at start of struct
        {
            case EFI_ACPI_6_2_PROCESSOR_LOCAL_APIC_SAPIC_AFFINITY:
                sratApic = (EFI_ACPI_6_2_PROCESSOR_LOCAL_APIC_SAPIC_AFFINITY_STRUCTURE *)cursor;

                DEBUG((DEBUG_VERBOSE, "    APIC Type %x Len %02x Flags %02x ApicId %02x Dom %x\n",
                    sratApic->Type, sratApic->Length, sratApic->Flags, sratApic->ApicId,
                    sratApic->ProximityDomain7To0));

                cursor += sratApic->Length;
                break;

            case EFI_ACPI_6_2_PROCESSOR_LOCAL_X2APIC_AFFINITY:
                sratX2Apic = (EFI_ACPI_6_2_PROCESSOR_LOCAL_X2APIC_AFFINITY_STRUCTURE *)cursor;

                DEBUG((DEBUG_VERBOSE, "   X2APIC Type %x Len %02x Flags %02x X2ApicId %02x Dom %x\n",
                    sratX2Apic->Type, sratX2Apic->Length, sratX2Apic->Flags, sratX2Apic->X2ApicId,
                    sratX2Apic->ProximityDomain));

                cursor += sratX2Apic->Length;
                break;

            case EFI_ACPI_6_2_GICC_AFFINITY:
                sratGicc = (EFI_ACPI_6_2_GICC_AFFINITY_STRUCTURE *)cursor;

                DEBUG((DEBUG_VERBOSE, "   GICC Type %x Len %02x Flags %02x ProcessorUid %02x Dom %x\n",
                    sratGicc->Type, sratGicc->Length, sratGicc->Flags, sratGicc->AcpiProcessorUid,
                    sratGicc->ProximityDomain));

                cursor += sratGicc->Length;
                break;

            case EFI_ACPI_6_2_MEMORY_AFFINITY:
                sratMem = (EFI_ACPI_6_2_MEMORY_AFFINITY_STRUCTURE *)cursor;

                base = (((UINT64)sratMem->AddressBaseHigh) << 32) |
                        (UINT64)sratMem->AddressBaseLow;
                size = (((UINT64)sratMem->LengthHigh) << 32) |
                        (UINT64)sratMem->LengthLow;

                DEBUG((DEBUG_VERBOSE, "    MEM  Type %x Len %02x Flags %02x Base % 14lx Len % 14lx "
                    "Dom %x\n",
                    sratMem->Type, sratMem->Length, sratMem->Flags, base, size,
                    sratMem->ProximityDomain));

                cursor += sratMem->Length;
                break;

            default:
                sratMem = (EFI_ACPI_6_2_MEMORY_AFFINITY_STRUCTURE *)cursor;

                DEBUG((DEBUG_VERBOSE, "    *Skipping* Type %x\n", sratMem->Type));

                cursor += sratMem->Length;
                break;
        }
    } while (cursor < ((UINT8 *)acpiHdr + acpiHdr->Length));
#endif
}


VOID
DebugDumpSlit(
    IN VOID* Slit
    )
{
#if !defined(MDEPKG_NDEBUG)
    EFI_ACPI_DESCRIPTION_HEADER  *acpiHdr = (EFI_ACPI_DESCRIPTION_HEADER*) Slit;
    UINT8 *cursor;

    DEBUG((DEBUG_VERBOSE, "--- SLIT data @ %x\n", acpiHdr));
    DEBUG((DEBUG_VERBOSE, "    Header Signature %x\n", acpiHdr->Signature));
    DEBUG((DEBUG_VERBOSE, "    Length %x\n", acpiHdr->Length));

    cursor = (UINT8*)acpiHdr;
    cursor += sizeof(EFI_ACPI_DESCRIPTION_HEADER);

    UINT64 localityCount = *(UINT64*)cursor;
    DEBUG((DEBUG_VERBOSE, "    Number of Localities: %x\n", localityCount));

    cursor += sizeof(UINT64);

    DEBUG((DEBUG_VERBOSE, "    Entries:\n"));
    DEBUG((DEBUG_VERBOSE, "    "));
    for (UINT64 i = 0;i < localityCount;i += 1)
    {
        DEBUG((DEBUG_VERBOSE, "    [%d]", i));
    }
    DEBUG((DEBUG_VERBOSE, "\n"));

    for (UINT64 i = 0;i < localityCount;i += 1)
    {
        DEBUG((DEBUG_VERBOSE, "    [%d]", i));
        for (UINT64 j = 0;j < localityCount;j += 1)
        {
            DEBUG((DEBUG_VERBOSE, "    %d", *cursor));
            cursor += 1;
        }
        DEBUG((DEBUG_VERBOSE, "\n"));
    }
#endif
}

VOID
DebugDumpHmat(
    IN VOID* Hmat
    )
{
#if !defined(MDEPKG_NDEBUG)
    EFI_ACPI_DESCRIPTION_HEADER  *acpiHdr = (EFI_ACPI_DESCRIPTION_HEADER*) Hmat;
    UINT32 hmatEntryCount;
    UINT8 *cursor;
    UINT8 *hmatEnd;
    EFI_ACPI_6_5_HMAT_STRUCTURE_HEADER *hmatEntryHeader;
    EFI_ACPI_6_5_HMAT_STRUCTURE_MEMORY_PROXIMITY_DOMAIN_ATTRIBUTES * msar;
    EFI_ACPI_6_5_HMAT_STRUCTURE_SYSTEM_LOCALITY_LATENCY_AND_BANDWIDTH_INFO * sllbi;
    EFI_ACPI_6_5_HMAT_STRUCTURE_MEMORY_SIDE_CACHE_INFO * msci;

    DEBUG((DEBUG_VERBOSE, "--- Hmat data @ %x\n", acpiHdr));
    DEBUG((DEBUG_VERBOSE, "    Header Signature %x\n", acpiHdr->Signature));
    DEBUG((DEBUG_VERBOSE, "    Length %x\n", acpiHdr->Length));

    hmatEntryCount = 0;
    cursor = (UINT8*)acpiHdr;
    hmatEnd = cursor + acpiHdr->Length;
    cursor += sizeof(EFI_ACPI_6_5_HETEROGENEOUS_MEMORY_ATTRIBUTE_TABLE_HEADER);

    while (cursor < hmatEnd)
    {
        hmatEntryHeader = (EFI_ACPI_6_5_HMAT_STRUCTURE_HEADER*)cursor;
        ++hmatEntryCount;

        switch (hmatEntryHeader->Type)
        {
        case EFI_ACPI_6_5_HMAT_TYPE_MEMORY_PROXIMITY_DOMAIN_ATTRIBUTES:
            msar  = (EFI_ACPI_6_5_HMAT_STRUCTURE_MEMORY_PROXIMITY_DOMAIN_ATTRIBUTES*)cursor;
            DEBUG((DEBUG_VERBOSE, "    MSAR\n"));
            DEBUG((DEBUG_VERBOSE, "     InitiatorProximityDomainValid %x\n", (UINT32)msar->Flags.InitiatorProximityDomainValid));
            DEBUG((DEBUG_VERBOSE, "     InitiatorProximityDomain %x\n", msar->InitiatorProximityDomain));
            DEBUG((DEBUG_VERBOSE, "     MemoryProximityDomain %x\n", msar->MemoryProximityDomain));
            break;

        case EFI_ACPI_6_5_HMAT_TYPE_SYSTEM_LOCALITY_LATENCY_AND_BANDWIDTH_INFO:
            sllbi = (EFI_ACPI_6_5_HMAT_STRUCTURE_SYSTEM_LOCALITY_LATENCY_AND_BANDWIDTH_INFO *)cursor;
            DEBUG((DEBUG_VERBOSE, "    SLLBI\n"));
            DEBUG((DEBUG_VERBOSE, "     MemoryHierarchy %x\n", (UINT32)sllbi->Flags.MemoryHierarchy));
            DEBUG((DEBUG_VERBOSE, "     AccessAttributes %x\n", (UINT32)sllbi->Flags.AccessAttributes));
            DEBUG((DEBUG_VERBOSE, "     MinTransferSize %x\n", (UINT32)sllbi->MinTransferSize));
            DEBUG((DEBUG_VERBOSE, "     NumberOfInitiatorProximityDomains %x\n", sllbi->NumberOfInitiatorProximityDomains));
            DEBUG((DEBUG_VERBOSE, "     NumberOfTargetProximityDomains %x\n", sllbi->NumberOfTargetProximityDomains));
            break;

        case EFI_ACPI_6_5_HMAT_TYPE_MEMORY_SIDE_CACHE_INFO:
            msci = (EFI_ACPI_6_5_HMAT_STRUCTURE_MEMORY_SIDE_CACHE_INFO *)cursor;
            DEBUG((DEBUG_VERBOSE, "    MSCI\n"));
            DEBUG((DEBUG_VERBOSE, "      MemoryProximityDomain %x\n", msci->MemoryProximityDomain));
            DEBUG((DEBUG_VERBOSE, "      TotalCacheLevels %x\n", (UINT32)msci->CacheAttributes.TotalCacheLevels));
            DEBUG((DEBUG_VERBOSE, "      CacheLevel %x\n", (UINT32)msci->CacheAttributes.CacheLevel));
            DEBUG((DEBUG_VERBOSE, "      CacheAssociativity %x\n", (UINT32)msci->CacheAttributes.CacheAssociativity));
            DEBUG((DEBUG_VERBOSE, "      WritePolicy %x\n", (UINT32)msci->CacheAttributes.WritePolicy));
            DEBUG((DEBUG_VERBOSE, "      CacheLineSize %x\n", (UINT32)msci->CacheAttributes.CacheLineSize));
            break;

        default:
            DEBUG((DEBUG_VERBOSE, "Uknown HMAT entry type %x\n", (UINT32)hmatEntryHeader->Type));
            break;
        }

        cursor += hmatEntryHeader->Length;
    }

    DEBUG((DEBUG_VERBOSE, "    HmatEntryCount %x\n", hmatEntryCount));
#endif
}

VOID
DebugDumpMemoryMap(
    IN  VOID*   MemMap,
        UINT32  MemMapSize,
        BOOLEAN LegacyMemoryMap
    )
{
    //
    // Debug dump the Memory Map entries.
    //
#if !defined(MDEPKG_NDEBUG)
    DEBUG((DEBUG_VERBOSE, "--- Memory Map data @ %x Length %x\n", MemMap, MemMapSize));
    if (!LegacyMemoryMap)
    {
        PVM_MEMORY_RANGE_V5 range = (PVM_MEMORY_RANGE_V5)MemMap;
        do
        {
            DEBUG((DEBUG_VERBOSE, "    Base % 14lx Len % 14lx Flags % 8x\n",
                range->BaseAddress, range->Length, range->Flags));
            range++;
        } while ((UINT8*)range < ((UINT8*)MemMap + MemMapSize));
    }
    else
    {
        PVM_MEMORY_RANGE range = (PVM_MEMORY_RANGE)MemMap;
        do
        {
            DEBUG((DEBUG_VERBOSE, "    Base % 14lx Len % 14lx\n", range->BaseAddress, range->Length));
            range++;
        } while ((UINT8*)range < ((UINT8*)MemMap + MemMapSize));
    }
#endif
}

UEFI_CONFIG_HEADER*
GetStartOfConfigBlob(
    VOID
    )
/*++

Routine Description:

    Return the start of the config blob, past the firmware and any additional
    data.

Arguments:

    None.

Return Value:

    A pointer to the start of the config blob.

--*/
{
#if defined(MDE_CPU_X64)
    //
    // On X64, the config blob starts after the end of the firmware, and after
    // some misc. pages (including space for the pagetables and GDT entries).
    //
    UINT64 configBlobBase =
        PcdGet64(PcdFdBaseAddress) +
        PcdGet32(PcdFdSize) +
        SIZE_4KB * MISC_PAGE_COUNT_TOTAL;
#elif defined(MDE_CPU_AARCH64)
    //
    // On AARCH64, the config blob starts after the end of the firmware, and
    // after the pagetables and stack/heap, at the start of system ram.
    //
    UINT64 configBlobBase = PcdGet64(PcdSystemMemoryBaseAddress);
#endif

    DEBUG((DEBUG_VERBOSE, "ConfigBlobBase: 0x%lx\n", configBlobBase));

    return (UEFI_CONFIG_HEADER*)(UINTN)configBlobBase;
}

VOID
DebugDumpUefiConfigStruct(
    IN UEFI_CONFIG_HEADER* Header
    )
{
#if !defined(MDEPKG_NDEBUG)
    DEBUG((DEBUG_VERBOSE, "Header Type: 0x%x \tHeader Length: 0x%x\n", Header->Type, Header->Length));

    switch(Header->Type)
    {
        case UefiConfigStructureCount:
            UEFI_CONFIG_STRUCTURE_COUNT *count = (UEFI_CONFIG_STRUCTURE_COUNT*) Header;
            DEBUG((DEBUG_VERBOSE, "\tTotalStructureCount: %u\n", count->TotalStructureCount));
            break;

        case UefiConfigBiosInformation:
            UEFI_CONFIG_BIOS_INFORMATION *biosInfo = (UEFI_CONFIG_BIOS_INFORMATION*) Header;
            DEBUG((DEBUG_VERBOSE, "\tBiosSizePages: 0x%x\n\tLegacyMemoryMap:%u\n", biosInfo->BiosSizePages, biosInfo->Flags.LegacyMemoryMap));
            break;

        case UefiConfigMadt:
            UEFI_CONFIG_MADT * madt = (UEFI_CONFIG_MADT*)Header;
            DebugDumpMadt(madt->Madt);
            break;

        case UefiConfigSrat:
            UEFI_CONFIG_SRAT *srat = (UEFI_CONFIG_SRAT*) Header;
            DebugDumpSrat(srat->Srat);
            break;

        case UefiConfigSlit:
            UEFI_CONFIG_SLIT *slit = (UEFI_CONFIG_SLIT*)Header;
            DebugDumpSlit(slit->Slit);
            break;

        case UefiConfigPptt:
            DEBUG((DEBUG_VERBOSE, "\tPPTT table found.\n"));
            break;

        case UefiConfigHmat:
            UEFI_CONFIG_HMAT *hmat = (UEFI_CONFIG_HMAT*)Header;
            DebugDumpHmat(hmat->Hmat);
            break;

        case UefiConfigMemoryMap:
            UEFI_CONFIG_MEMORY_MAP *memMap = (UEFI_CONFIG_MEMORY_MAP*) Header;
            DebugDumpMemoryMap(memMap->MemoryMap, Header->Length - sizeof(UEFI_CONFIG_HEADER), PcdGetBool(PcdLegacyMemoryMap));
            break;

        case UefiConfigEntropy:
            DEBUG((DEBUG_VERBOSE, "\tEntropy table found.\n"));
            break;

        case UefiConfigBiosGuid:
            UEFI_CONFIG_BIOS_GUID *biosGuid = (UEFI_CONFIG_BIOS_GUID*) Header;
            DEBUG((DEBUG_VERBOSE, "\tBiosGuid: %g\n", (EFI_GUID*) biosGuid->BiosGuid));
            break;

        case UefiConfigSmbiosSystemManufacturer:
            UEFI_CONFIG_SMBIOS_SYSTEM_MANUFACTURER *systemManufacturer = (UEFI_CONFIG_SMBIOS_SYSTEM_MANUFACTURER*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios System Manufacturer: %a\n", systemManufacturer->SystemManufacturer));
            break;

        case UefiConfigSmbiosSystemProductName:
            UEFI_CONFIG_SMBIOS_SYSTEM_PRODUCT_NAME *systemProductName = (UEFI_CONFIG_SMBIOS_SYSTEM_PRODUCT_NAME*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios System Product Name: %a\n", systemProductName->SystemProductName));
            break;

        case UefiConfigSmbiosSystemVersion:
            UEFI_CONFIG_SMBIOS_SYSTEM_VERSION *systemVersion = (UEFI_CONFIG_SMBIOS_SYSTEM_VERSION*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios System Version: %a\n", systemVersion->SystemVersion));
            break;

        case UefiConfigSmbiosSystemSerialNumber:
            UEFI_CONFIG_SMBIOS_SYSTEM_SERIAL_NUMBER *systemSerialNumber = (UEFI_CONFIG_SMBIOS_SYSTEM_SERIAL_NUMBER*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios System Serial Number: %a\n", systemSerialNumber->SystemSerialNumber));
            break;

        case UefiConfigSmbiosSystemSKUNumber:
            UEFI_CONFIG_SMBIOS_SYSTEM_SKU_NUMBER *systemSKUNumber = (UEFI_CONFIG_SMBIOS_SYSTEM_SKU_NUMBER*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios System SKU Number: %a\n", systemSKUNumber->SystemSKUNumber));
            break;

        case UefiConfigSmbiosSystemFamily:
            UEFI_CONFIG_SMBIOS_SYSTEM_FAMILY *systemFamily = (UEFI_CONFIG_SMBIOS_SYSTEM_FAMILY*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios System Family: %a\n", systemFamily->SystemFamily));
            break;

        case UefiConfigSmbiosBaseSerialNumber:
            UEFI_CONFIG_SMBIOS_BASE_SERIAL_NUMBER *baseSerialNumber = (UEFI_CONFIG_SMBIOS_BASE_SERIAL_NUMBER*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios Base Serial Number: %a\n", baseSerialNumber->BaseSerialNumber));
            break;

        case UefiConfigSmbiosChassisSerialNumber:
            UEFI_CONFIG_SMBIOS_CHASSIS_SERIAL_NUMBER *chassisSerialNumber = (UEFI_CONFIG_SMBIOS_CHASSIS_SERIAL_NUMBER*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios Chassis Serial Number: %a\n", chassisSerialNumber->ChassisSerialNumber));
            break;

        case UefiConfigSmbiosChassisAssetTag:
            UEFI_CONFIG_SMBIOS_CHASSIS_ASSET_TAG *chassisAssetTag = (UEFI_CONFIG_SMBIOS_CHASSIS_ASSET_TAG*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios Chassis Asset Tag: %a\n", chassisAssetTag->ChassisAssetTag));
            break;

        case UefiConfigSmbiosBiosLockString:
            UEFI_CONFIG_SMBIOS_BIOS_LOCK_STRING *biosLockString = (UEFI_CONFIG_SMBIOS_BIOS_LOCK_STRING*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios Bios Lock String: %a\n", biosLockString->BiosLockString));
            break;

        case UefiConfigSmbiosMemoryDeviceSerialNumber:
            UEFI_CONFIG_SMBIOS_MEMORY_DEVICE_SERIAL_NUMBER *memoryDeviceSerialNumber = (UEFI_CONFIG_SMBIOS_MEMORY_DEVICE_SERIAL_NUMBER*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios Memory Device Serial Number: %a\n", memoryDeviceSerialNumber->MemoryDeviceSerialNumber));
            break;

        case UefiConfigSmbios31ProcessorInformation:
            UEFI_CONFIG_SMBIOS_3_1_PROCESSOR_INFORMATION *procInfo = (UEFI_CONFIG_SMBIOS_3_1_PROCESSOR_INFORMATION*) Header;
            DEBUG((DEBUG_VERBOSE, "\tProcessorType: %u\n", procInfo->ProcessorType));
            DEBUG((DEBUG_VERBOSE, "\tProcessorID: 0x%x\n", procInfo->ProcessorID));
            DEBUG((DEBUG_VERBOSE, "\tVoltage: %u\n", procInfo->Voltage));
            DEBUG((DEBUG_VERBOSE, "\tExternalClock: 0x%x\n", procInfo->ExternalClock));
            DEBUG((DEBUG_VERBOSE, "\tMaxSpeed: 0x%x\n", procInfo->MaxSpeed));
            DEBUG((DEBUG_VERBOSE, "\tCurrentSpeed: 0x%x\n", procInfo->CurrentSpeed));
            DEBUG((DEBUG_VERBOSE, "\tStatus: 0x%x\n", procInfo->Status));
            DEBUG((DEBUG_VERBOSE, "\tProcessorUpgrade: 0x%x\n", procInfo->ProcessorUpgrade));
            DEBUG((DEBUG_VERBOSE, "\tProcessorCharacteristics: 0x%x\n", procInfo->ProcessorCharacteristics));
            DEBUG((DEBUG_VERBOSE, "\tProcessorFamily2: %u\n", procInfo->ProcessorFamily2));
            break;

        case UefiConfigSmbiosSocketDesignation:
            UEFI_CONFIG_SMBIOS_SOCKET_DESIGNATION *socketDesignation = (UEFI_CONFIG_SMBIOS_SOCKET_DESIGNATION*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios Socket Designation: %a\n", socketDesignation->SocketDesignation));
            break;

        case UefiConfigSmbiosProcessorManufacturer:
            UEFI_CONFIG_SMBIOS_PROCESSOR_MANUFACTURER *processorManufacturer = (UEFI_CONFIG_SMBIOS_PROCESSOR_MANUFACTURER*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios Processor Manufacturer: %a\n", processorManufacturer->ProcessorManufacturer));
            break;

        case UefiConfigSmbiosProcessorVersion:
            UEFI_CONFIG_SMBIOS_PROCESSOR_VERSION *processorVersion = (UEFI_CONFIG_SMBIOS_PROCESSOR_VERSION*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios Processor Version: %a\n", processorVersion->ProcessorVersion));
            break;

        case UefiConfigSmbiosProcessorSerialNumber:
            UEFI_CONFIG_SMBIOS_PROCESSOR_SERIAL_NUMBER *processorSerialNumber = (UEFI_CONFIG_SMBIOS_PROCESSOR_SERIAL_NUMBER*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios Processor Serial Number: %a\n", processorSerialNumber->ProcessorSerialNumber));
            break;

        case UefiConfigSmbiosProcessorAssetTag:
            UEFI_CONFIG_SMBIOS_PROCESSOR_ASSET_TAG *processorAssetTag = (UEFI_CONFIG_SMBIOS_PROCESSOR_ASSET_TAG*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios Processor Asset Tag: %a\n", processorAssetTag->ProcessorAssetTag));
            break;

        case UefiConfigSmbiosProcessorPartNumber:
            UEFI_CONFIG_SMBIOS_PROCESSOR_PART_NUMBER *processorPartNumber = (UEFI_CONFIG_SMBIOS_PROCESSOR_PART_NUMBER*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSmbios Processor Part Number: %a\n", processorPartNumber->ProcessorPartNumber));
            break;

        case UefiConfigFlags:
            UEFI_CONFIG_FLAGS *flags = (UEFI_CONFIG_FLAGS*) Header;
            DEBUG((DEBUG_VERBOSE, "\tSerialControllersEnabled: %u\n", flags->Flags.SerialControllersEnabled));
            DEBUG((DEBUG_VERBOSE, "\tPauseAfterBootFailure: %u\n", flags->Flags.PauseAfterBootFailure));
            DEBUG((DEBUG_VERBOSE, "\tPxeIpV6: %u\n", flags->Flags.PxeIpV6));
            DEBUG((DEBUG_VERBOSE, "\tDhcp6DuidTypeLlt: %u\n", flags->Flags.Dhcp6DuidTypeLlt));
            DEBUG((DEBUG_VERBOSE, "\tDebuggerEnabled: %u\n", flags->Flags.DebuggerEnabled));
            DEBUG((DEBUG_VERBOSE, "\tLoadOempTable: %u\n", flags->Flags.LoadOempTable));
            DEBUG((DEBUG_VERBOSE, "\tTpmEnabled: %u\n", flags->Flags.TpmEnabled));
            DEBUG((DEBUG_VERBOSE, "\tHibernateEnabled: %u\n", flags->Flags.HibernateEnabled));
            DEBUG((DEBUG_VERBOSE, "\tConsoleMode: %u\n", flags->Flags.ConsoleMode));
            DEBUG((DEBUG_VERBOSE, "\tMemoryAttributesTableEnabled: %u\n", flags->Flags.MemoryAttributesTableEnabled));
            DEBUG((DEBUG_VERBOSE, "\tVirtualBatteryEnabled: %u\n", flags->Flags.VirtualBatteryEnabled));
            DEBUG((DEBUG_VERBOSE, "\tSgxMemoryEnabled: %u\n", flags->Flags.SgxMemoryEnabled));
            DEBUG((DEBUG_VERBOSE, "\tIsVmbfsBoot: %u\n", flags->Flags.IsVmbfsBoot));
            DEBUG((DEBUG_VERBOSE, "\tMeasureAdditionalPcrs: %u\n", flags->Flags.MeasureAdditionalPcrs));
            DEBUG((DEBUG_VERBOSE, "\tShutdownAfterBootFailure: %u\n", flags->Flags.DisableFrontpage));
            DEBUG((DEBUG_VERBOSE, "\tDefaultBootAlwaysAttempt: %u\n", flags->Flags.DefaultBootAlwaysAttempt));
            DEBUG((DEBUG_VERBOSE, "\tLowPowerS0IdleEnabled: %u\n", flags->Flags.LowPowerS0IdleEnabled));
            DEBUG((DEBUG_VERBOSE, "\tVpciBootEnabled: %u\n", flags->Flags.VpciBootEnabled));
            DEBUG((DEBUG_VERBOSE, "\tProcIdleEnabled: %u\n", flags->Flags.ProcIdleEnabled));
            DEBUG((DEBUG_VERBOSE, "\tCxlMemoryEnabled: %u\n", flags->Flags.CxlMemoryEnabled));
            DEBUG((DEBUG_VERBOSE, "\tDisableSha384Pcr: %u\n", flags->Flags.DisableSha384Pcr));
            DEBUG((DEBUG_VERBOSE, "\tMediaPresentEnabledByDefault: %u\n", flags->Flags.MediaPresentEnabledByDefault));
            DEBUG((DEBUG_VERBOSE, "\tMemoryProtectionMode: %u\n", flags->Flags.MemoryProtectionMode));
            DEBUG((DEBUG_VERBOSE, "\tWatchdogEnabled: %u\n", flags->Flags.WatchdogEnabled));
            DEBUG((DEBUG_VERBOSE, "\tTpmLocalityRegsEnabled: %u\n", flags->Flags.TpmLocalityRegsEnabled));
            break;

        case UefiConfigProcessorInformation:
            UEFI_CONFIG_PROCESSOR_INFORMATION *processorInfo = (UEFI_CONFIG_PROCESSOR_INFORMATION*) Header;
            DEBUG((DEBUG_VERBOSE, "\tProcessor Count: %u\n\tProcessorsPerVirtualSocket: %u\n",
                processorInfo->ProcessorCount,
                processorInfo->ProcessorsPerVirtualSocket));
            break;

        case UefiConfigMmioRanges:
            UEFI_CONFIG_MMIO_RANGES *mmioRanges = (UEFI_CONFIG_MMIO_RANGES*) Header;
            DEBUG((DEBUG_VERBOSE, "\tMmio Ranges:\n"));
            DEBUG((DEBUG_VERBOSE, "\tStart:0x%17lx Size:0x%x\n", mmioRanges->Ranges[0].MmioPageNumberStart, mmioRanges->Ranges[0].MmioSizeInPages));
            DEBUG((DEBUG_VERBOSE, "\tStart:0x%17lx Size:0x%x\n", mmioRanges->Ranges[1].MmioPageNumberStart, mmioRanges->Ranges[1].MmioSizeInPages));
            break;

        case UefiConfigAcpiTable:
            UEFI_CONFIG_ACPI_TABLE *acpi = (UEFI_CONFIG_ACPI_TABLE*) Header;
            UINT64 acpiTableSize = acpi->Header.Length - sizeof(UEFI_CONFIG_HEADER);
            EFI_ACPI_DESCRIPTION_HEADER* acpiHeader = (EFI_ACPI_DESCRIPTION_HEADER*) acpi->AcpiTableData;
            DEBUG((DEBUG_VERBOSE, "\tAcpi Data Size:0x%x\n", acpiTableSize));
            DEBUG((DEBUG_VERBOSE, "\tAcpi Header Size:0x%x\n", acpiHeader->Length));
            DEBUG((DEBUG_VERBOSE, "\tAcpi Header Signature:0x%x\n", acpiHeader->Signature));
            break;

        case UefiConfigNvdimmCount:
            UEFI_CONFIG_NVDIMM_COUNT *cfg = (UEFI_CONFIG_NVDIMM_COUNT*) Header;
            DEBUG((DEBUG_VERBOSE, "\tNVDIMM Count:0x%lx\n", cfg->Count));
            break;

        case UefiConfigVpciInstanceFilter:
            UEFI_CONFIG_VPCI_INSTANCE_FILTER *filter = (UEFI_CONFIG_VPCI_INSTANCE_FILTER*) Header;
            DEBUG((DEBUG_VERBOSE, "\tVpci instance filter:%g\n", (EFI_GUID*) filter->InstanceGuid));
            break;

        case UefiConfigMcfg:
            DEBUG((DEBUG_VERBOSE, "\tMCFG table found.\n"));
            break;

        case UefiConfigSsdt:
            DEBUG((DEBUG_VERBOSE, "\tSSDT table found.\n"));
            break;

        case UefiConfigIort:
            DEBUG((DEBUG_VERBOSE, "\tIORT table found.\n"));
            break;

        default:
            DEBUG((DEBUG_VERBOSE, "\t!!! Unrecognized config structure type !!!\n"));
            break;
    }
#endif
}

VOID
GetSmbiosStructureStringLength(
        UINT32  HeaderLength,
    IN  UINT8*  String,
    OUT UINT32* StringLength
    )
/*++

Routine Description:

    Get the length of an SMBIOS string config structure, including the null
    terminator. Will truncate strings if no null terminator is found.

Arguments:

    HeaderLength - The length in the config structure header.

    String - A pointer to the start of the string data past the config header.

    StringLength - Returns the length of the string, including null terminator.

Return Value:

    None

--*/
{
    *StringLength = 0;
    UINT64 remainingStructureSize = HeaderLength - sizeof(UEFI_CONFIG_HEADER);

    UINTN length = AsciiStrnLenS((CHAR8*) String, remainingStructureSize);

    if (length == remainingStructureSize)
    {
        //
        // No NULL found, truncate by adding one at the end.
        //
        String[length - 1] = 0;
        *StringLength = (UINT32)length;

        DEBUG((DEBUG_VERBOSE, "SMBIOS String Structure had no null terminator, truncating to size 0x%x. Truncated string:%a", length, String));
    }
    else
    {
        //
        // Add one to the length for the null character.
        //
        *StringLength = (UINT32) (length + 1);
    }
}

VOID
ConfigSetProcessorInfo(
    IN UEFI_CONFIG_PROCESSOR_INFORMATION *ProcessorInfo
    )
{
    if (ProcessorInfo->ProcessorCount == 0)
    {
        DEBUG((DEBUG_ERROR, "Processors count was 0.\n"));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    if (ProcessorInfo->ProcessorsPerVirtualSocket == 0)
    {
        DEBUG((DEBUG_ERROR, "Processors per virtual socket was 0.\n"));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    if (ProcessorInfo->ThreadsPerProcessor == 0)
    {
        DEBUG((DEBUG_ERROR, "Threads per processor was 0.\n"));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    // Ignore the return value. We do not want to fail fast for these errors.
    PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdProcessorCount, ProcessorInfo->ProcessorCount));
    PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdProcessorsPerVirtualSocket, ProcessorInfo->ProcessorsPerVirtualSocket));
    PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdThreadsPerProcessor, ProcessorInfo->ThreadsPerProcessor));
}


VOID
ConfigSetUefiConfigFlags(
    IN UEFI_CONFIG_FLAGS *ConfigFlags
    )
{
    DXE_MEMORY_PROTECTION_SETTINGS memoryProtectionSettings;

    // Ignore the return value. We do not want to fail fast for these errors.
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdSerialControllersEnabled, (UINT8) ConfigFlags->Flags.SerialControllersEnabled));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdPauseAfterBootFailure, (UINT8) ConfigFlags->Flags.PauseAfterBootFailure));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdPxeIpV6, (UINT8) ConfigFlags->Flags.PxeIpV6));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdDebuggerEnabled, (UINT8) ConfigFlags->Flags.DebuggerEnabled));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdLoadOempTable, (UINT8) ConfigFlags->Flags.LoadOempTable));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdTpmEnabled, (UINT8) ConfigFlags->Flags.TpmEnabled));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdHibernateEnabled, (UINT8) ConfigFlags->Flags.HibernateEnabled));
    PEI_FAIL_FAST_IF_FAILED(PcdSet8S(PcdConsoleMode, (UINT8) ConfigFlags->Flags.ConsoleMode));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdMemoryAttributesTableEnabled, (UINT8) ConfigFlags->Flags.MemoryAttributesTableEnabled));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdVirtualBatteryEnabled, (UINT8) ConfigFlags->Flags.VirtualBatteryEnabled));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdSgxMemoryEnabled, (UINT8) ConfigFlags->Flags.SgxMemoryEnabled));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdIsVmbfsBoot, (UINT8) ConfigFlags->Flags.IsVmbfsBoot));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdDisableFrontpage, (UINT8) ConfigFlags->Flags.DisableFrontpage));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdDefaultBootAlwaysAttempt, (UINT8) ConfigFlags->Flags.DefaultBootAlwaysAttempt));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdLowPowerS0IdleEnabled, (UINT8)ConfigFlags->Flags.LowPowerS0IdleEnabled));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdVpciBootEnabled, (UINT8)ConfigFlags->Flags.VpciBootEnabled));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdProcIdleEnabled, (UINT8) ConfigFlags->Flags.ProcIdleEnabled));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdCxlMemoryEnabled, (UINT8) ConfigFlags->Flags.CxlMemoryEnabled));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdEnableIMCWhenIsolated, (UINT8) ConfigFlags->Flags.EnableIMCWhenIsolated));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdMediaPresentEnabledByDefault, (UINT8) ConfigFlags->Flags.MediaPresentEnabledByDefault));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdWatchdogEnabled, (UINT8) ConfigFlags->Flags.WatchdogEnabled));
    PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdTpmLocalityRegsEnabled, (UINT8) ConfigFlags->Flags.TpmLocalityRegsEnabled));

    //
    // If memory protections are enabled, configure the value into the HOB.
    //
    if (ConfigFlags->Flags.MemoryProtectionMode == ConfigLibMemoryProtectionModeDisabled)
    {
        memoryProtectionSettings = (DXE_MEMORY_PROTECTION_SETTINGS) DXE_MEMORY_PROTECTION_SETTINGS_OFF;
    }
    else if (ConfigFlags->Flags.MemoryProtectionMode == ConfigLibMemoryProtectionModeDefault)
    {
        memoryProtectionSettings = (DXE_MEMORY_PROTECTION_SETTINGS) DXE_MEMORY_PROTECTION_SETTINGS_SHIP_MODE;
    }
    else if (ConfigFlags->Flags.MemoryProtectionMode == ConfigLibMemoryProtectionModeStrict)
    {
        memoryProtectionSettings = (DXE_MEMORY_PROTECTION_SETTINGS) DXE_MEMORY_PROTECTION_SETTINGS_DEBUG;
        memoryProtectionSettings.ImageProtectionPolicy.Fields.RaiseErrorIfProtectionFails = 0;
    }
    else if (ConfigFlags->Flags.MemoryProtectionMode == ConfigLibMemoryProtectionModeRelaxed)
    {
        memoryProtectionSettings = (DXE_MEMORY_PROTECTION_SETTINGS) DXE_MEMORY_PROTECTION_SETTINGS_SHIP_MODE;
        memoryProtectionSettings.ImageProtectionPolicy.Fields.RaiseErrorIfProtectionFails = 0;

        // Linux has some known loader limitations. The following checks needs to be relaxed for Linux
        // to boot successfully. For more details on these individual fields, look at DxeMemoryProtectionSettings.h
        memoryProtectionSettings.NullPointerDetectionPolicy.Fields.DisableReadyToBoot = 1;
        memoryProtectionSettings.NxProtectionPolicy.Fields.EfiLoaderData = 0;
        memoryProtectionSettings.NxProtectionPolicy.Fields.EfiBootServicesData = 0;
        memoryProtectionSettings.NxProtectionPolicy.Fields.EfiConventionalMemory = 0;
    }

    // for architectures like ARM64, the UEFI spec declares that 64k runtime granularity must be supported
    // and the heap guard system does not support 64k guard pages, so we cannot guard these regions
    if (RUNTIME_PAGE_ALLOCATION_GRANULARITY != EFI_PAGE_SIZE) {
        memoryProtectionSettings.HeapGuardPageType.Fields.EfiACPIMemoryNVS = 0;
        memoryProtectionSettings.HeapGuardPageType.Fields.EfiReservedMemoryType = 0;
        memoryProtectionSettings.HeapGuardPageType.Fields.EfiRuntimeServicesCode = 0;
        memoryProtectionSettings.HeapGuardPageType.Fields.EfiRuntimeServicesData = 0;
        memoryProtectionSettings.HeapGuardPoolType.Fields.EfiACPIMemoryNVS = 0;
        memoryProtectionSettings.HeapGuardPoolType.Fields.EfiReservedMemoryType = 0;
        memoryProtectionSettings.HeapGuardPoolType.Fields.EfiRuntimeServicesCode = 0;
        memoryProtectionSettings.HeapGuardPoolType.Fields.EfiRuntimeServicesData = 0;
    }

    HobAddGuidData(&gDxeMemoryProtectionSettingsGuid,
        &memoryProtectionSettings,
        sizeof(memoryProtectionSettings));

    // AARCH64 vTPM support does not require measured boot backwards-compatibility modes
#if defined(MDE_CPU_X64)
    //
    // For Hyper-V VM version 9.0 and above, MeasureAdditionalPcrs will be TRUE.
    // When TRUE, we will perform a more "standard" measured boot
    //
    if (ConfigFlags->Flags.MeasureAdditionalPcrs)
    {
        PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(TcgMeasureBootStringsInPcr4, TRUE));
        PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdExcludeFvMainFromMeasurements, FALSE));
    }

    //
    // For Hyper-V VM versions below 9.3, DisableSha384Pcr will be TRUE.
    // When TRUE, we remove SHA-384 from the PCR hash mask.
    //
    if (ConfigFlags->Flags.DisableSha384Pcr)
    {
        PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdTpm2HashMask, (PcdGet32(PcdTpm2HashMask) & ~HASH_ALG_SHA384)));
    }
#endif

    if (ConfigFlags->Flags.Dhcp6DuidTypeLlt)
    {
        // Set to Dhcp6DuidTypeLlt
        PEI_FAIL_FAST_IF_FAILED(PcdSet8S(PcdDhcp6UidType, 1));
    }
}


EFI_STATUS
VerifyStructureLength(
    IN UEFI_CONFIG_HEADER* Header
    )
/*++

Routine Description:

    Verify that the config structure of a given type is the correct length.

Arguments:

    Header - A pointer to the header of a config structure.

Return Value:

    EFI_SUCCESS on success. EFI_INVALID_PARAMETER if the Header length is invalid.

--*/
{
    //
    // All structures must be aligned to 8 bytes, as AARCH64 does not allow
    // unaligned access like X64.
    // The size must be as big as the header size.
    //
    if (Header->Length % 8 != 0)
    {
        DEBUG((DEBUG_ERROR, "Structure Type 0x%x was length 0x%x, not aligned to 8 bytes.\n",
            Header->Type,
            Header->Length));
        return EFI_INVALID_PARAMETER;
    }

    if (Header->Length <= sizeof(UEFI_CONFIG_HEADER))
    {
        DEBUG((DEBUG_ERROR, "Structure Type 0x%x was length 0x%x, and smaller than the header size and information.\n",
            Header->Type,
            Header->Length));
        return EFI_INVALID_PARAMETER;
    }

    //
    // Size of 0 means the structure has a variable length, and will
    // be verified later on.
    //
    static const UINT32 StructureLengthTable[] =
    {
        sizeof(UEFI_CONFIG_STRUCTURE_COUNT), //UefiConfigStructureCount
        sizeof(UEFI_CONFIG_BIOS_INFORMATION), //UefiConfigBiosInformation
        0, //UefiConfigSrat
        0, //UefiConfigMemoryMap
        sizeof(UEFI_CONFIG_ENTROPY), //UefiConfigEntropy
        sizeof(UEFI_CONFIG_BIOS_GUID), //UefiConfigBiosGuid
        0, //UefiConfigSmbiosSystemSerialNumber
        0, //UefiConfigSmbiosBaseSerialNumber
        0, //UefiConfigSmbiosChassisSerialNumber
        0, //UefiConfigSmbiosChassisAssetTag
        0, //UefiConfigSmbiosBiosLockString
        sizeof(UEFI_CONFIG_SMBIOS_3_1_PROCESSOR_INFORMATION), //UefiConfigSmbios31ProcessorInformation
        0, //UefiConfigSmbiosSocketDesignation
        0, //UefiConfigSmbiosProcessorManufacturer
        0, //UefiConfigSmbiosProcessorVersion
        0, //UefiConfigSmbiosProcessorSerialNumber
        0, //UefiConfigSmbiosProcessorAssetTag
        0, //UefiConfigSmbiosProcessorPartNumber
        sizeof(UEFI_CONFIG_FLAGS), //UefiConfigFlags
        sizeof(UEFI_CONFIG_PROCESSOR_INFORMATION), //UefiConfigProcessorInformation
        0, //UefiConfigMmioRanges
        0, //UefiConfigAARCH64MPIDR - not used
        0, //UefiConfigAcpiTable
        sizeof(UEFI_CONFIG_NVDIMM_COUNT), //UefiConfigNvdimmCount
        0, //UefiConfigMadt
        sizeof(UEFI_CONFIG_VPCI_INSTANCE_FILTER), //UefiConfigVpciInstanceFilter
        0, //UefiConfigSmbiosSystemManufacturer
        0, //UefiConfigSmbiosSystemProductName
        0, //UefiConfigSmbiosSystemVersion
        0, //UefiConfigSmbiosSystemSKUNumber
        0, //UefiConfigSmbiosSystemFamily
        0, //UefiConfigSmbiosMemoryDeviceSerialNumber
        0, //UefiConfigSlit
        0, //UefiConfigAspt
        0, //UefiConfigPptt
        0, //UefiConfigGic
        0, //UefiConfigMcfg
        0, //UefiConfigSsdt
        0, //UefiConfigHmat
        0, //UefiConfigIort
    };

    //
    // If this is a type that is not currently parsed, ignore it.
    //
    if (Header->Type >= (sizeof(StructureLengthTable)/sizeof(StructureLengthTable[0])))
    {
        return EFI_SUCCESS;
    }

    //
    // Otherwise, check structure length via lookup table. Nonzero values
    // must match.
    //
    UINT32 expectedLength = StructureLengthTable[Header->Type];
    if (expectedLength != 0 && Header->Length != expectedLength)
    {
        DEBUG((DEBUG_ERROR, "Structure Type 0x%x was length 0x%x, expected Length %x\n",
            Header->Type,
            Header->Length,
            expectedLength));
        return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
GetUefiConfigInfo(
    VOID
    )
/*++

Routine Description:

    Get and parse the config blob that contains information from the Bios VDEV.
    Note that the information received and parsed here can come from the host
    and cannot be trusted. Validate the config information before using it.

Arguments:

    None.

Return Value:

    EFI_SUCCESS on success. Error if the config blob is malformed.

--*/
{
    UINT32 stringLength = 0;
    UEFI_CONFIG_HEADER *header = NULL;
    UEFI_CONFIG_STRUCTURE_COUNT *configCount = NULL;
    UINT32 calculatedConfigSize = 0;

    //
    // Tracking to see if the config blob has all the required structures.
    //
#pragma warning(disable : 4201)
#if defined(MDE_CPU_X64)
    static const UINT64 AllStructuresFound = 0x1FF;
    union {
        struct {
            UINT64 UefiConfigBiosInformation:1;
            UINT64 UefiConfigMadt:1;
            UINT64 UefiConfigSrat:1;
            UINT64 UefiConfigMemoryMap:1;
            UINT64 UefiConfigEntropy:1;
            UINT64 UefiConfigBiosGuid:1;
            UINT64 UefiConfigFlags:1;
            UINT64 UefiConfigProcessorInformation:1;
            UINT64 UefiConfigMmioRanges:1;
            UINT64 Reserved:55;
        };

        UINT64 AsUINT64;
    } requiredStructures;
#elif defined(MDE_CPU_AARCH64)
    static const UINT64 AllStructuresFound = 0xFF;
    union {
        struct {
            UINT64 UefiConfigBiosInformation:1;
            UINT64 UefiConfigSrat:1;
            UINT64 UefiConfigMemoryMap:1;
            UINT64 UefiConfigEntropy:1;
            UINT64 UefiConfigBiosGuid:1;
            UINT64 UefiConfigFlags:1;
            UINT64 UefiConfigProcessorInformation:1;
            UINT64 UefiConfigMmioRanges:1;
            UINT64 Reserved:56;
        };

        UINT64 AsUINT64;
    } requiredStructures;
#endif
#pragma warning(default : 4201)

    requiredStructures.AsUINT64 = 0;

    header = GetStartOfConfigBlob();

    //
    // Read the first structure, which must be the structure describing the
    // total number of structures.
    //
    DebugDumpUefiConfigStruct(header);
    configCount = (UEFI_CONFIG_STRUCTURE_COUNT*) header;

    //
    // Sanity checks to make sure that the header is correct.
    //
    if (header->Type != UefiConfigStructureCount ||
        configCount->TotalStructureCount <= 1)
    {
        DEBUG((DEBUG_ERROR, "*** Malformed Header (Structure count) \n"));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    if (EFI_ERROR(VerifyStructureLength(header)))
    {
        DEBUG((DEBUG_ERROR, "*** Malformed Header Length (Structure count) \n"));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdConfigBlobSize, configCount->TotalConfigBlobSize));

    //
    // Advance past initial header to other structures.
    //
    calculatedConfigSize += header->Length;
    header = (UEFI_CONFIG_HEADER*) ((UINT64) header + header->Length);

    //
    // Loop through the remaining structures.
    //
    for (UINT64 i = 1; i < configCount->TotalStructureCount; i++)
    {
        if (EFI_ERROR(VerifyStructureLength(header)))
        {
            DEBUG((DEBUG_ERROR, "*** Malformed Header Length\n"));
            FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
        }

        if (calculatedConfigSize > configCount->TotalConfigBlobSize)
        {
            DEBUG((DEBUG_ERROR, "Config offset of 0x%x is greater than the actual size of 0x%x\n",
                calculatedConfigSize,
                configCount->TotalConfigBlobSize));
            FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
        }

        DebugDumpUefiConfigStruct(header);

        switch(header->Type)
        {
            case UefiConfigBiosInformation:
                UEFI_CONFIG_BIOS_INFORMATION *biosInfo = (UEFI_CONFIG_BIOS_INFORMATION*) header;
                PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdLegacyMemoryMap, (UINT8)biosInfo->Flags.LegacyMemoryMap));
                requiredStructures.UefiConfigBiosInformation = 1;
                break;

            case UefiConfigMadt:
                UEFI_CONFIG_MADT * madtStructure = (UEFI_CONFIG_MADT*)header;
                EFI_ACPI_DESCRIPTION_HEADER *madtHdr = (EFI_ACPI_DESCRIPTION_HEADER*)madtStructure->Madt;

                if (madtStructure->Header.Length < (sizeof(UEFI_CONFIG_HEADER) + sizeof(EFI_ACPI_DESCRIPTION_HEADER)) ||
                    madtHdr->Signature != EFI_ACPI_6_2_MULTIPLE_APIC_DESCRIPTION_TABLE_SIGNATURE ||
                    madtHdr->Length >(madtStructure->Header.Length - sizeof(UEFI_CONFIG_HEADER)))
                {
                    DEBUG((DEBUG_ERROR, "*** Malformed MADT\n"));
                    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
                }

                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdMadtPtr, (UINT64)madtStructure->Madt));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdMadtSize, madtHdr->Length));
#if defined(MDE_CPU_X64)
                requiredStructures.UefiConfigMadt = 1;
#endif
                break;

            case UefiConfigSrat:
                UEFI_CONFIG_SRAT *sratStructure = (UEFI_CONFIG_SRAT*) header;
                EFI_ACPI_DESCRIPTION_HEADER *sratHdr = (EFI_ACPI_DESCRIPTION_HEADER*) sratStructure->Srat;

                //
                // NOTE: Because ARM GICC affinity structures are not aligned to 8 bytes,
                // this structure may be padded. Thus, the table described by the ACPI header
                // just needs to be less than the overall length.
                //
                if (sratStructure->Header.Length < (sizeof(UEFI_CONFIG_HEADER) + sizeof(EFI_ACPI_DESCRIPTION_HEADER)) ||
                    sratHdr->Signature != EFI_ACPI_6_2_SYSTEM_RESOURCE_AFFINITY_TABLE_SIGNATURE ||
                    sratHdr->Length > (sratStructure->Header.Length - sizeof(UEFI_CONFIG_HEADER)))
                {
                    DEBUG((DEBUG_ERROR, "*** Malformed SRAT\n"));
                    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
                }

                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSratPtr, (UINT64)sratStructure->Srat));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSratSize, sratHdr->Length));
                requiredStructures.UefiConfigSrat = 1;
                break;

            case UefiConfigSlit:
                UEFI_CONFIG_SLIT *slitStructure = (UEFI_CONFIG_SLIT*) header;
                EFI_ACPI_DESCRIPTION_HEADER *slitHdr = (EFI_ACPI_DESCRIPTION_HEADER*) slitStructure->Slit;

                if (slitStructure->Header.Length < (sizeof(UEFI_CONFIG_HEADER) + sizeof(EFI_ACPI_DESCRIPTION_HEADER)) ||
                    slitHdr->Signature != EFI_ACPI_6_2_SYSTEM_LOCALITY_INFORMATION_TABLE_SIGNATURE ||
                    slitHdr->Length > (slitStructure->Header.Length - sizeof(UEFI_CONFIG_HEADER)))
                {
                    DEBUG((DEBUG_ERROR, "*** Malformed SLIT\n"));
                    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
                }

                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSlitPtr, (UINT64)slitStructure->Slit));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSlitSize, slitHdr->Length));
                break;

            case UefiConfigPptt:
                UEFI_CONFIG_PPTT *ppttStructure = (UEFI_CONFIG_PPTT*) header;
                EFI_ACPI_DESCRIPTION_HEADER *ppttHdr = (EFI_ACPI_DESCRIPTION_HEADER*) ppttStructure->Pptt;

                if (ppttStructure->Header.Length < (sizeof(UEFI_CONFIG_HEADER) + sizeof(EFI_ACPI_DESCRIPTION_HEADER)) ||
                    ppttHdr->Signature != EFI_ACPI_6_2_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_STRUCTURE_SIGNATURE ||
                    ppttHdr->Length > (ppttStructure->Header.Length - sizeof(UEFI_CONFIG_HEADER)))
                {
                    DEBUG((DEBUG_ERROR, "*** Malformed PPTT\n"));
                    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
                }

                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdPpttPtr, (UINT64)ppttStructure->Pptt));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdPpttSize, ppttHdr->Length));
                break;

            case UefiConfigHmat:
                UEFI_CONFIG_HMAT *hmatStructure = (UEFI_CONFIG_HMAT*) header;
                EFI_ACPI_DESCRIPTION_HEADER *hmatHdr = (EFI_ACPI_DESCRIPTION_HEADER*) hmatStructure->Hmat;

                if (hmatStructure->Header.Length < (sizeof(UEFI_CONFIG_HEADER) + sizeof(EFI_ACPI_DESCRIPTION_HEADER)) ||
                    hmatHdr->Signature != EFI_ACPI_6_5_HETEROGENEOUS_MEMORY_ATTRIBUTE_TABLE_SIGNATURE ||
                    hmatHdr->Length > (hmatStructure->Header.Length - sizeof(UEFI_CONFIG_HEADER)))
                {
                    DEBUG((DEBUG_ERROR, "*** Malformed HMAT\n"));
                    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
                }

                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdHmatPtr, (UINT64)hmatStructure->Hmat));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdHmatSize, hmatHdr->Length));
                break;

            case UefiConfigMemoryMap:
                UEFI_CONFIG_MEMORY_MAP *memoryMapStructure = (UEFI_CONFIG_MEMORY_MAP*) header;
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdMemoryMapPtr, (UINT64) memoryMapStructure->MemoryMap));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdMemoryMapSize, header->Length - sizeof(UEFI_CONFIG_HEADER)));
                requiredStructures.UefiConfigMemoryMap = 1;
                break;

            case UefiConfigEntropy:
                UEFI_CONFIG_ENTROPY *entropy = (UEFI_CONFIG_ENTROPY*) header;
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdEntropyPtr, (UINT64) entropy->Entropy));
                requiredStructures.UefiConfigEntropy = 1;
                break;

            case UefiConfigBiosGuid:
                UEFI_CONFIG_BIOS_GUID *biosGuid = (UEFI_CONFIG_BIOS_GUID*) header;
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdBiosGuidPtr, (UINT64) biosGuid->BiosGuid));
                requiredStructures.UefiConfigBiosGuid = 1;
                break;

            case UefiConfigSmbiosSystemManufacturer:
                UEFI_CONFIG_SMBIOS_SYSTEM_MANUFACTURER *systemManufacturer = (UEFI_CONFIG_SMBIOS_SYSTEM_MANUFACTURER*) header;
                GetSmbiosStructureStringLength(header->Length, systemManufacturer->SystemManufacturer, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosSystemManufacturerStr, (UINT64)systemManufacturer->SystemManufacturer));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosSystemManufacturerSize, stringLength));

                break;

            case UefiConfigSmbiosSystemProductName:
                UEFI_CONFIG_SMBIOS_SYSTEM_PRODUCT_NAME *systemProductName = (UEFI_CONFIG_SMBIOS_SYSTEM_PRODUCT_NAME*) header;
                GetSmbiosStructureStringLength(header->Length, systemProductName->SystemProductName, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosSystemProductNameStr, (UINT64)systemProductName->SystemProductName));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosSystemProductNameSize, stringLength));

                break;

            case UefiConfigSmbiosSystemVersion:
                UEFI_CONFIG_SMBIOS_SYSTEM_VERSION *systemVersion = (UEFI_CONFIG_SMBIOS_SYSTEM_VERSION*) header;
                GetSmbiosStructureStringLength(header->Length, systemVersion->SystemVersion, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosSystemVersionStr, (UINT64)systemVersion->SystemVersion));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosSystemVersionSize, stringLength));

                break;

            case UefiConfigSmbiosSystemSerialNumber:
                UEFI_CONFIG_SMBIOS_SYSTEM_SERIAL_NUMBER *systemSerialNumber = (UEFI_CONFIG_SMBIOS_SYSTEM_SERIAL_NUMBER*) header;
                GetSmbiosStructureStringLength(header->Length, systemSerialNumber->SystemSerialNumber, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosSystemSerialNumberStr, (UINT64)systemSerialNumber->SystemSerialNumber));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosSystemSerialNumberSize, stringLength));

                break;

            case UefiConfigSmbiosSystemSKUNumber:
                UEFI_CONFIG_SMBIOS_SYSTEM_SKU_NUMBER *systemSKUNumber = (UEFI_CONFIG_SMBIOS_SYSTEM_SKU_NUMBER*) header;
                GetSmbiosStructureStringLength(header->Length, systemSKUNumber->SystemSKUNumber, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosSystemSKUNumberStr, (UINT64)systemSKUNumber->SystemSKUNumber));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosSystemSKUNumberSize, stringLength));

                break;

            case UefiConfigSmbiosSystemFamily:
                UEFI_CONFIG_SMBIOS_SYSTEM_FAMILY *systemFamily = (UEFI_CONFIG_SMBIOS_SYSTEM_FAMILY*) header;
                GetSmbiosStructureStringLength(header->Length, systemFamily->SystemFamily, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosSystemFamilyStr, (UINT64)systemFamily->SystemFamily));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosSystemFamilySize, stringLength));

                break;

            case UefiConfigSmbiosBaseSerialNumber:
                UEFI_CONFIG_SMBIOS_BASE_SERIAL_NUMBER *baseSerialNumber = (UEFI_CONFIG_SMBIOS_BASE_SERIAL_NUMBER*) header;
                GetSmbiosStructureStringLength(header->Length, baseSerialNumber->BaseSerialNumber, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosBaseSerialNumberStr, (UINT64)baseSerialNumber->BaseSerialNumber));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosBaseSerialNumberSize, stringLength));

                break;

            case UefiConfigSmbiosChassisSerialNumber:
                UEFI_CONFIG_SMBIOS_CHASSIS_SERIAL_NUMBER *chassisSerialNumber = (UEFI_CONFIG_SMBIOS_CHASSIS_SERIAL_NUMBER*) header;
                GetSmbiosStructureStringLength(header->Length, chassisSerialNumber->ChassisSerialNumber, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosChassisSerialNumberStr, (UINT64)chassisSerialNumber->ChassisSerialNumber));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosChassisSerialNumberSize, stringLength));

                break;

            case UefiConfigSmbiosChassisAssetTag:
                UEFI_CONFIG_SMBIOS_CHASSIS_ASSET_TAG *chassisAssetTag = (UEFI_CONFIG_SMBIOS_CHASSIS_ASSET_TAG*) header;
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosChassisAssetTagStr, (UINT64) chassisAssetTag->ChassisAssetTag));
                GetSmbiosStructureStringLength(header->Length, chassisAssetTag->ChassisAssetTag, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosChassisAssetTagSize, stringLength));

                break;

            case UefiConfigSmbiosBiosLockString:
                UEFI_CONFIG_SMBIOS_BIOS_LOCK_STRING *biosLockString = (UEFI_CONFIG_SMBIOS_BIOS_LOCK_STRING*) header;
                GetSmbiosStructureStringLength(header->Length, biosLockString->BiosLockString, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosBiosLockStringStr, (UINT64)biosLockString->BiosLockString));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosBiosLockStringSize, stringLength));

                break;

            case UefiConfigSmbiosMemoryDeviceSerialNumber:
                UEFI_CONFIG_SMBIOS_MEMORY_DEVICE_SERIAL_NUMBER *memoryDeviceSerialNumber = (UEFI_CONFIG_SMBIOS_MEMORY_DEVICE_SERIAL_NUMBER*) header;
                GetSmbiosStructureStringLength(header->Length, memoryDeviceSerialNumber->MemoryDeviceSerialNumber, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosMemoryDeviceSerialNumberStr, (UINT64)memoryDeviceSerialNumber->MemoryDeviceSerialNumber));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosMemoryDeviceSerialNumberSize, stringLength));

                break;

            case UefiConfigSmbios31ProcessorInformation:
                UEFI_CONFIG_SMBIOS_3_1_PROCESSOR_INFORMATION *procInfo = (UEFI_CONFIG_SMBIOS_3_1_PROCESSOR_INFORMATION*) header;
                PEI_FAIL_FAST_IF_FAILED(PcdSet8S(PcdSmbiosProcessorType, procInfo->ProcessorType));
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosProcessorID, procInfo->ProcessorID));
                PEI_FAIL_FAST_IF_FAILED(PcdSet8S(PcdSmbiosProcessorVoltage, procInfo->Voltage));
                PEI_FAIL_FAST_IF_FAILED(PcdSet16S(PcdSmbiosProcessorExternalClock, procInfo->ExternalClock));
                PEI_FAIL_FAST_IF_FAILED(PcdSet16S(PcdSmbiosProcessorMaxSpeed, procInfo->MaxSpeed));
                PEI_FAIL_FAST_IF_FAILED(PcdSet16S(PcdSmbiosProcessorCurrentSpeed, procInfo->CurrentSpeed));
                PEI_FAIL_FAST_IF_FAILED(PcdSet8S(PcdSmbiosProcessorStatus, procInfo->Status));
                PEI_FAIL_FAST_IF_FAILED(PcdSet8S(PcdSmbiosProcessorUpgrade, procInfo->ProcessorUpgrade));
                PEI_FAIL_FAST_IF_FAILED(PcdSet16S(PcdSmbiosProcessorCharacteristics, procInfo->ProcessorCharacteristics));
                PEI_FAIL_FAST_IF_FAILED(PcdSet16S(PcdSmbiosProcessorFamily2, procInfo->ProcessorFamily2));
                break;

            case UefiConfigSmbiosSocketDesignation:
                UEFI_CONFIG_SMBIOS_SOCKET_DESIGNATION *socketDesignation = (UEFI_CONFIG_SMBIOS_SOCKET_DESIGNATION*) header;
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosProcessorSocketDesignationStr, (UINT64) socketDesignation->SocketDesignation));
                GetSmbiosStructureStringLength(header->Length, socketDesignation->SocketDesignation, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosProcessorSocketDesignationSize, stringLength));

                break;

            case UefiConfigSmbiosProcessorManufacturer:
                UEFI_CONFIG_SMBIOS_PROCESSOR_MANUFACTURER *processorManufacturer = (UEFI_CONFIG_SMBIOS_PROCESSOR_MANUFACTURER*) header;
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosProcessorManufacturerStr, (UINT64) processorManufacturer->ProcessorManufacturer));
                GetSmbiosStructureStringLength(header->Length, processorManufacturer->ProcessorManufacturer, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosProcessorManufacturerSize, stringLength));

                break;

            case UefiConfigSmbiosProcessorVersion:
                UEFI_CONFIG_SMBIOS_PROCESSOR_VERSION *processorVersion = (UEFI_CONFIG_SMBIOS_PROCESSOR_VERSION*) header;
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosProcessorVersionStr, (UINT64) processorVersion->ProcessorVersion));
                GetSmbiosStructureStringLength(header->Length, processorVersion->ProcessorVersion, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosProcessorVersionSize, stringLength));

                break;

            case UefiConfigSmbiosProcessorSerialNumber:
                UEFI_CONFIG_SMBIOS_PROCESSOR_SERIAL_NUMBER *processorSerialNumber = (UEFI_CONFIG_SMBIOS_PROCESSOR_SERIAL_NUMBER*) header;
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosProcessorSerialNumberStr, (UINT64) processorSerialNumber->ProcessorSerialNumber));
                GetSmbiosStructureStringLength(header->Length, processorSerialNumber->ProcessorSerialNumber, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosProcessorSerialNumberSize, stringLength));

                break;

            case UefiConfigSmbiosProcessorAssetTag:
                UEFI_CONFIG_SMBIOS_PROCESSOR_ASSET_TAG *processorAssetTag = (UEFI_CONFIG_SMBIOS_PROCESSOR_ASSET_TAG*) header;
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosProcessorAssetTagStr, (UINT64) processorAssetTag->ProcessorAssetTag));
                GetSmbiosStructureStringLength(header->Length, processorAssetTag->ProcessorAssetTag, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosProcessorAssetTagSize, stringLength));

                break;

            case UefiConfigSmbiosProcessorPartNumber:
                UEFI_CONFIG_SMBIOS_PROCESSOR_PART_NUMBER *processorPartNumber = (UEFI_CONFIG_SMBIOS_PROCESSOR_PART_NUMBER*) header;
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSmbiosProcessorAssetTagStr, (UINT64) processorPartNumber->ProcessorPartNumber));
                GetSmbiosStructureStringLength(header->Length, processorPartNumber->ProcessorPartNumber, &stringLength);
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSmbiosProcessorAssetTagSize, stringLength));

                break;

            case UefiConfigFlags:
                UEFI_CONFIG_FLAGS *flags = (UEFI_CONFIG_FLAGS*) header;
                ConfigSetUefiConfigFlags(flags);
                requiredStructures.UefiConfigFlags = 1;
                break;

            case UefiConfigProcessorInformation:
                UEFI_CONFIG_PROCESSOR_INFORMATION *processorInfo = (UEFI_CONFIG_PROCESSOR_INFORMATION*) header;
                ConfigSetProcessorInfo(processorInfo);
                requiredStructures.UefiConfigProcessorInformation = 1;
                break;

            case UefiConfigMmioRanges:
                UINT64 lowGap, highGap;
                UEFI_CONFIG_MMIO_RANGES *mmioRanges = (UEFI_CONFIG_MMIO_RANGES*) header;

                //
                // Size must be exactly two MMIO entries.
                //
                if (header->Length != (sizeof(UEFI_CONFIG_HEADER) + sizeof(UEFI_CONFIG_MMIO) * 2))
                {
                    DEBUG((DEBUG_ERROR, "***Malformed MMIO range structure\n"));
                    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
                }

                //
                // Figure out which entry is the low gap, and which is the high.
                //
                if (mmioRanges->Ranges[0].MmioPageNumberStart < mmioRanges->Ranges[1].MmioPageNumberStart)
                {
                    lowGap = 0;
                    highGap = 1;
                }
                else
                {
                    lowGap = 1;
                    highGap = 0;
                }

                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdLowMmioGapBasePageNumber, mmioRanges->Ranges[lowGap].MmioPageNumberStart));
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdLowMmioGapSizeInPages, mmioRanges->Ranges[lowGap].MmioSizeInPages));
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdHighMmioGapBasePageNumber, mmioRanges->Ranges[highGap].MmioPageNumberStart));
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdHighMmioGapSizeInPages, mmioRanges->Ranges[highGap].MmioSizeInPages));
                requiredStructures.UefiConfigMmioRanges = 1;
                break;

            case UefiConfigAcpiTable:
                UEFI_CONFIG_ACPI_TABLE *acpiTable = (UEFI_CONFIG_ACPI_TABLE*) header;
                EFI_ACPI_DESCRIPTION_HEADER *acpiHeader = (EFI_ACPI_DESCRIPTION_HEADER*) acpiTable->AcpiTableData;

                //
                // Verify ACPI table header is completely within the config structure.
                //
                if (acpiTable->Header.Length < (sizeof(UEFI_CONFIG_HEADER) + sizeof(EFI_ACPI_DESCRIPTION_HEADER)) ||
                    acpiHeader->Length > (acpiTable->Header.Length - sizeof(UEFI_CONFIG_HEADER)))
                {
                    DEBUG((DEBUG_ERROR, "***ACPI table is not contained within config structure size.\n"));
                    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
                }

                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdAcpiTablePtr, (UINT64) acpiTable->AcpiTableData));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdAcpiTableSize, acpiHeader->Length));
                break;

            case UefiConfigNvdimmCount:
                UEFI_CONFIG_NVDIMM_COUNT *cfg = (UEFI_CONFIG_NVDIMM_COUNT*) header;
                PEI_FAIL_FAST_IF_FAILED(PcdSet16S(PcdNvdimmCount, cfg->Count));
                break;

            case UefiConfigVpciInstanceFilter:
                UEFI_CONFIG_VPCI_INSTANCE_FILTER *filter = (UEFI_CONFIG_VPCI_INSTANCE_FILTER*) header;
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdVpciInstanceFilterGuidPtr, (UINT64) filter->InstanceGuid));
                break;

#if defined(MDE_CPU_X64)
            case UefiConfigAspt:
                UEFI_CONFIG_AMD_ASPT *asptStructure = (UEFI_CONFIG_AMD_ASPT*) header;
                EFI_ACPI_DESCRIPTION_HEADER *asptHdr = (EFI_ACPI_DESCRIPTION_HEADER*) asptStructure->Aspt;

                if (asptStructure->Header.Length < (sizeof(UEFI_CONFIG_HEADER) + sizeof(EFI_ACPI_DESCRIPTION_HEADER)) ||
                    asptHdr->Signature != AMD_ACPI_ASPT_TABLE_SIGNATURE ||
                    asptHdr->Length > (asptStructure->Header.Length - sizeof(UEFI_CONFIG_HEADER)))
                {
                    DEBUG((DEBUG_ERROR, "***Malformed ASPT\n"));
                    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
                }

                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdAsptPtr, (UINT64)asptStructure->Aspt));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdAsptSize, asptHdr->Length));
                break;
#endif

#if defined(MDE_CPU_AARCH64)
            case UefiConfigGic:
                UEFI_CONFIG_GIC *gicConfig = (UEFI_CONFIG_GIC*) header;
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdGicDistributorBase, gicConfig->GicDistributorBase));
                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdGicRedistributorsBase, gicConfig->GicRedistributorsBase));
                break;
#endif
            case UefiConfigMcfg:
                UEFI_CONFIG_MCFG *mcfgStructure = (UEFI_CONFIG_MCFG*) header;
                EFI_ACPI_DESCRIPTION_HEADER *mcfgHdr = (EFI_ACPI_DESCRIPTION_HEADER*) mcfgStructure->Mcfg;

                if (mcfgStructure->Header.Length < (sizeof(UEFI_CONFIG_HEADER) + sizeof(EFI_ACPI_DESCRIPTION_HEADER)) ||
                    mcfgHdr->Signature != EFI_ACPI_6_2_PCI_EXPRESS_MEMORY_MAPPED_CONFIGURATION_SPACE_BASE_ADDRESS_DESCRIPTION_TABLE_SIGNATURE ||
                    mcfgHdr->Length > (mcfgStructure->Header.Length - sizeof(UEFI_CONFIG_HEADER)))
                {
                    DEBUG((DEBUG_ERROR, "*** Malformed MCFG\n"));
                    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
                }

                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdMcfgPtr, (UINT64)mcfgStructure->Mcfg));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdMcfgSize, mcfgHdr->Length));
                break;

            case UefiConfigSsdt:
                UEFI_CONFIG_SSDT *ssdtStructure = (UEFI_CONFIG_SSDT*) header;
                EFI_ACPI_DESCRIPTION_HEADER *ssdtHdr = (EFI_ACPI_DESCRIPTION_HEADER*) ssdtStructure->Ssdt;

                if (ssdtStructure->Header.Length < (sizeof(UEFI_CONFIG_HEADER) + sizeof(EFI_ACPI_DESCRIPTION_HEADER)) ||
                    ssdtHdr->Signature != EFI_ACPI_6_2_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE ||
                    ssdtHdr->Length > (ssdtStructure->Header.Length - sizeof(UEFI_CONFIG_HEADER)))
                {
                    DEBUG((DEBUG_ERROR, "*** Malformed SSDT\n"));
                    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
                }

                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSsdtPtr, (UINT64)ssdtStructure->Ssdt));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSsdtSize, ssdtHdr->Length));
                break;

            case UefiConfigIort:
                UEFI_CONFIG_IORT *iortStructure = (UEFI_CONFIG_IORT*) header;
                EFI_ACPI_DESCRIPTION_HEADER *iortHdr = (EFI_ACPI_DESCRIPTION_HEADER*) iortStructure->Iort;

                if (iortStructure->Header.Length < (sizeof(UEFI_CONFIG_HEADER) + sizeof(EFI_ACPI_DESCRIPTION_HEADER)) ||
                    iortHdr->Signature != EFI_ACPI_6_2_IO_REMAPPING_TABLE_SIGNATURE ||
                    iortHdr->Length > (iortStructure->Header.Length - sizeof(UEFI_CONFIG_HEADER)))
                {
                    DEBUG((DEBUG_ERROR, "*** Malformed IORT\n"));
                    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
                }

                PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdIortPtr, (UINT64)iortStructure->Iort));
                PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdIortSize, iortHdr->Length));
                break;
        }

        calculatedConfigSize += header->Length;
        header = (UEFI_CONFIG_HEADER*) ((UINT64) header + header->Length);
    }

    if (requiredStructures.AsUINT64 != AllStructuresFound)
    {
        DEBUG((DEBUG_ERROR, "Missing required structures, found structures: 0x%x\n", requiredStructures.AsUINT64));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    if (configCount->TotalConfigBlobSize != calculatedConfigSize)
    {
        DEBUG((DEBUG_ERROR, "Reported config size of 0x%x did not match actual size of 0x%x\n", configCount->TotalConfigBlobSize, calculatedConfigSize));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    return EFI_SUCCESS;
}

EFI_STATUS
GetConfiguration(
    IN CONST    EFI_PEI_SERVICES**  PeiServices,
    OUT         UINT8*              PhysicalAddressWidth
    )
/*++

Routine Description:

    Gets the configuraton from the loader.

Arguments:

    PeiServices  An indirect pointer to the PEI Services Table.

    PhysicalAddressWidth - Returns the number of bits in the address width.

Return Value:

    n/a

--*/
{
    EFI_STATUS status;

    //
    // If this is a hardware-isolated VM running without a paravisor, then no
    // config blob is present.  Instead, the parameters were inserted in IGVM
    // format and must be parsed as such.
    //
    if (IsHardwareIsolatedNoParavisor())
    {
        status = GetIgvmConfigInfo();
    }
    else
    {
        status = GetUefiConfigInfo();
    }

    //
    // The config blob was not well formed, do not proceed.
    //
    if (EFI_ERROR(status))
    {
        ASSERT(FALSE);
        return status;
    }

    //
    // Get the address width.
    //
    *PhysicalAddressWidth = GetPhysicalAddressWidth(PeiServices);

    return EFI_SUCCESS;
}
