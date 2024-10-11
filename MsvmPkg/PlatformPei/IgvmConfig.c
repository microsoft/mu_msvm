/** @file
  Gets configuration values from IGVM file format parameters and exports
  them as globals and PCDs.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <IndustryStandard/Acpi.h>
#include <PiPei.h>
#include <Platform.h>
#include <BiosInterface.h>
#include <Hv/HvGuest.h>
#if defined(MDE_CPU_AARCH64)
#include <Library/ArmLib.h>
#endif
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/ResourcePublicationLib.h>
#include <Hv.h>
#include <Config.h>
#include <IsolationTypes.h>
#include <UefiConstants.h>

typedef struct _IGVM_VHS_MEMORY_MAP_ENTRY {
    UINT64 StartingGpaPageNumber;
    UINT64 NumberOfPages;
    UINT16 Type;
    UINT16 Flags;
    UINT32 Reserved;
} IGVM_VHS_MEMORY_MAP_ENTRY;

enum IGVM_VHS_MEMORY_MAP_ENTRY_TYPES
{
    IGVM_VHF_MEMORY_MAP_ENTRY_TYPE_MEMORY            = 0x0,
    IGVM_VHF_MEMORY_MAP_ENTRY_TYPE_PLATFORM_RESERVED = 0x1,
    IGVM_VHF_MEMORY_MAP_ENTRY_TYPE_PERSISTENT        = 0x2,
    IGVM_VHF_MEMORY_MAP_ENTRY_TYPE_VTL2_PROTECTABLE  = 0x3,
};

VOID*
GetIgvmData(
    IN  VOID    *ParameterAreaBase,
        UINT32  PageOffset
    )
/*++

Routine Description:

    Obtain the base of an IGVM parameter block.

Arguments:

    ParameterAreaBase - Supplies the base address of the parameter area.

    PageOffset - Supplies the page offset of the desired parameter block.

Return Value:

    The address of the desired parameter block.

--*/
{
    return (UINT8*)ParameterAreaBase + (PageOffset * EFI_PAGE_SIZE);
}


EFI_STATUS
ParseIgvmMemoryMap(
    IN  UEFI_IGVM_PARAMETER_INFO    *ParameterInfo,
        UINT64                      SvsmBase,
        UINT64                      SvsmSize
    )
/*++

Routine Description:

    Parses the memory map in IGVM format to construct a memory map suitable
    for consumption by the rest of UEFI.

Arguments:

    ParameterInfo - Supplies a pointer to the parameter information block.

    SvsmBase - Supplies the base of any SVSM region.

    SvsmSize - Supplies the size of any SVSM region.

Return Value:

    EFI_STATUS.

--*/
{
    UINT64 basePage;
    UINT32 index;
    UINT32 maximumIndex;
    UINT32 maximumRange;
    IGVM_VHS_MEMORY_MAP_ENTRY *memoryMap;
    UINT64 nextPage;
    UINT64 pageCount;
    PVM_MEMORY_RANGE_V5 range;
    UINT32 rangeIndex;
    UINT32 rangeFlags;
    UINT64 reservedBase;
    UINT64 reservedEnd;
    UINT64 svsmEnd;
    VOID* uefiMemoryMap;
    EFI_STATUS Status;

    memoryMap = GetIgvmData(ParameterInfo, ParameterInfo->MemoryMapOffset);
    maximumIndex = (ParameterInfo->MemoryMapPageCount * EFI_PAGE_SIZE) /
                   sizeof(IGVM_VHS_MEMORY_MAP_ENTRY);

    //
    // Make sure any SVSM region is sane.
    //

    svsmEnd = SvsmBase + SvsmSize;
    if (svsmEnd < SvsmBase)
    {
        return EFI_DEVICE_ERROR;
    }
    SvsmBase /= SIZE_4KB;
    SvsmSize /= SIZE_4KB;
    svsmEnd /= SIZE_4KB;

    //
    // Convert the memory map to the format expected by UEFI.
    //

    uefiMemoryMap = GetIgvmData(ParameterInfo, ParameterInfo->UefiMemoryMapOffset);

    range = uefiMemoryMap;
    maximumRange = (ParameterInfo->UefiMemoryMapPageCount * EFI_PAGE_SIZE) /
                   sizeof(VM_MEMORY_RANGE_V5);

    Status = PcdSetBoolS(PcdLegacyMemoryMap, FALSE);
    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to set the PCD PcdLegacyMemoryMap::0x%x \n", Status));
        return Status;
    }

    //
    // Determine the address of the next reserved area.
    //

    reservedBase = ParameterInfo->VpContextPageNumber;
    reservedEnd = reservedBase + 1;
    if ((SvsmSize != 0) && (SvsmBase < reservedBase))
    {
        reservedBase = SvsmBase;
        reservedEnd = svsmEnd;
    }

    nextPage = 0;
    index = 0;
    rangeIndex = 0;

    while ((index < maximumIndex) && (rangeIndex < maximumRange))
    {
        basePage = memoryMap[index].StartingGpaPageNumber;
        pageCount = memoryMap[index].NumberOfPages;
        if (pageCount == 0)
        {
            break;
        }

        if (basePage < nextPage)
        {
            return EFI_DEVICE_ERROR;
        }

        nextPage = basePage + pageCount;
        if (nextPage <= basePage)
        {
            return EFI_DEVICE_ERROR;
        }

        rangeFlags = 0;
        switch (memoryMap[index].Type)
        {
        case IGVM_VHF_MEMORY_MAP_ENTRY_TYPE_MEMORY:
            break;

        case IGVM_VHF_MEMORY_MAP_ENTRY_TYPE_PLATFORM_RESERVED:
            rangeFlags |= VM_MEMORY_RANGE_FLAG_PLATFORM_RESERVED;
            break;

        default:
            return EFI_DEVICE_ERROR;
        }

        //
        // Determine whether this range can be consumed in its entirety.  It
        // must be split if it crosses the VP context page or the SVSM region.
        //

        if ((rangeFlags & VM_MEMORY_RANGE_FLAG_PLATFORM_RESERVED) == 0)
        {
            //
            // Ensure that the location of the next reserved range is correct.
            //

            if (basePage >= reservedEnd)
            {
                reservedEnd = 0;
                reservedBase = reservedEnd - 1;
                if (basePage <= ParameterInfo->VpContextPageNumber)
                {
                    reservedBase = ParameterInfo->VpContextPageNumber;
                    reservedEnd = reservedBase + 1;
                }
                if ((SvsmSize != 0) &&
                    (SvsmBase < reservedBase) &&
                    (basePage < svsmEnd))
                {
                    reservedBase = SvsmBase;
                    reservedEnd = svsmEnd;
                }

                if (reservedEnd == 0)
                {
                    reservedEnd -= 1;
                    reservedBase = reservedEnd - 1;
                }
            }

            //
            // Check for overlap with any reserved range.
            //

            if ((basePage < reservedEnd) && (nextPage > reservedBase))
            {
                if (basePage < reservedBase)
                {
                    //
                    // Generate a free range to describe that portion that
                    // lies before the reserved range, and split the current
                    // range so it can be processed again in the next pass.
                    //

                    memoryMap[index].StartingGpaPageNumber = reservedBase;
                    memoryMap[index].NumberOfPages = nextPage - reservedBase;
                    pageCount = reservedBase - basePage;
                    nextPage = basePage;
                }
                else
                {
                    //
                    // Generate a reserved range to describe that portion that
                    // overlaps the reserved range.  If the current range lies
                    // entirely within the reserved range, then move past it,
                    // otherwise truncate it so the remainder can be processed
                    // again in the next pass.
                    //

                    rangeFlags = VM_MEMORY_RANGE_FLAG_PLATFORM_RESERVED;
                    if (nextPage < reservedEnd)
                    {
                        index += 1;
                    }
                    else
                    {
                        pageCount = reservedEnd - basePage;
                        memoryMap[index].StartingGpaPageNumber = reservedEnd;
                        memoryMap[index].NumberOfPages = nextPage - reservedEnd;
                        nextPage = reservedEnd;
                    }
                }
            }
            else
            {
                //
                // This range does not overlap the reserved range, so consume
                // it in its entirety.
                //

                index += 1;
            }
        }
        else
        {
            index += 1;
        }

        range->BaseAddress = basePage * SIZE_4KB;
        range->Length = pageCount * SIZE_4KB;
        range->Flags = rangeFlags;
        range->Reserved = 0;

        range += 1;
    }

    Status = PcdSet64S(PcdMemoryMapPtr, (UINT64)uefiMemoryMap);
    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to set the PCD PcdMemoryMapPtr::0x%x \n", Status));
        return Status;
    }
    Status = PcdSet32S(PcdMemoryMapSize, (UINT32)((UINT64)range - (UINT64)uefiMemoryMap));
    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to set the PCD PcdMemoryMapSize::0x%x \n", Status));
        return Status;
    }

    return EFI_SUCCESS;
}


VOID
ParseIgvmCommandLine(
    IN UEFI_IGVM_PARAMETER_INFO *ParameterInfo
    )
/*++

Routine Description:

    Parses the command line in IGVM format to determine additional parameters
    (e.g. debug parameters).

Arguments:

    ParameterInfo - Supplies a pointer to the parameter information block.

Return Value:

    None.

--*/
{
    unsigned char *commandString;
    UINT32 maximumSize;
    UINT32 size;

    //
    // Verify command line is within parameter page.
    //

    commandString = GetIgvmData(ParameterInfo, ParameterInfo->CommandLineOffset);
    size = 0;
    maximumSize = ParameterInfo->CommandLinePageCount * EFI_PAGE_SIZE;

    while (commandString[size] != '\0')
    {
        size++;

        //
        // No null terminator found, can't be valid.
        //

        if (size >= maximumSize)
        {
            return;
        }
    }
}


EFI_STATUS
GetIgvmConfigInfo(
    VOID
    )
/*++

Routine Description:

    Get and parse the config information in IGVM format.

Arguments:

    None.

Return Value:

    EFI_SUCCESS on success. Error if the config information is corrupt.

--*/
{
    UEFI_CONFIG_FLAGS configFlags;
    UEFI_IGVM_PARAMETER_INFO *parameterInfo;
    UEFI_CONFIG_PROCESSOR_INFORMATION processorInfo;
    VOID* secretsPage;
    EFI_STATUS status;
    UINT64 svsmBase;
    UINT64 svsmSize;

    //
    // Locate the parameter layout description at the base of the parameter
    // area.
    //

    parameterInfo = (UEFI_IGVM_PARAMETER_INFO *)GetStartOfConfigBlob();

    //
    // Capture the total size of config information.
    //

    PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdConfigBlobSize, parameterInfo->ParameterPageCount * EFI_PAGE_SIZE));

    if (parameterInfo->UefiIgvmConfigurationFlags & UEFI_IGVM_CONFIGURATION_ENABLE_HOST_EMULATORS)
    {
        PEI_FAIL_FAST_IF_FAILED(PcdSetBoolS(PcdHostEmulatorsWhenHardwareIsolated, TRUE));
    }

    //
    // TODO: use parameters for this
    // Assume a single processor until VPR/VPS information can be configured
    // in the IGVM file.
    //

    processorInfo.ProcessorCount = 1;
    processorInfo.ProcessorsPerVirtualSocket = 1;
    processorInfo.ThreadsPerProcessor = 1;
    ConfigSetProcessorInfo(&processorInfo);

    //
    // Update the processor count.
    //
    UEFI_IGVM_LOADER_BLOCK *loaderBlock = (UEFI_IGVM_LOADER_BLOCK*)GetIgvmData(parameterInfo, parameterInfo->LoaderBlockOffset);
    if (loaderBlock->NumberOfProcessors == 0 || loaderBlock->NumberOfProcessors > HV_MAXIMUM_PROCESSORS)
    {
        DEBUG((DEBUG_ERROR, "Invalid processor count %u.\n", loaderBlock->NumberOfProcessors));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }
    PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdProcessorCount, loaderBlock->NumberOfProcessors));

    //
    // Enable ACPI tables
    //
    if (parameterInfo->MadtPageCount == 0)
    {
        DEBUG((DEBUG_ERROR, "MadtPageCount was 0.\n"));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    EFI_ACPI_DESCRIPTION_HEADER *madtHdr = (EFI_ACPI_DESCRIPTION_HEADER*)GetIgvmData(parameterInfo, parameterInfo->MadtOffset);

    if (madtHdr->Signature != EFI_ACPI_6_2_MULTIPLE_APIC_DESCRIPTION_TABLE_SIGNATURE ||
        madtHdr->Length > (parameterInfo->MadtPageCount * EFI_PAGE_SIZE))
    {
        DEBUG((DEBUG_ERROR, "*** Malformed MADT\n"));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdMadtPtr, (UINT64)madtHdr));
    PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdMadtSize, madtHdr->Length));

    if (parameterInfo->SratPageCount == 0)
    {
        DEBUG((DEBUG_ERROR, "SratPageCount was 0.\n"));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    EFI_ACPI_DESCRIPTION_HEADER *sratHdr = (EFI_ACPI_DESCRIPTION_HEADER*)GetIgvmData(parameterInfo, parameterInfo->SratOffset);

    if (sratHdr->Signature != EFI_ACPI_6_2_SYSTEM_RESOURCE_AFFINITY_TABLE_SIGNATURE ||
        sratHdr->Length > (parameterInfo->SratPageCount * EFI_PAGE_SIZE))
    {
        DEBUG((DEBUG_ERROR, "*** Malformed SRAT\n"));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    PEI_FAIL_FAST_IF_FAILED(PcdSet64S(PcdSratPtr, (UINT64)sratHdr));
    PEI_FAIL_FAST_IF_FAILED(PcdSet32S(PcdSratSize, sratHdr->Length));

    //
    // Parse the command line to obtain debug parameters.
    //

    ParseIgvmCommandLine(parameterInfo);

    //
    // Build a config structure with a statically defined configuration.
    //

    ZeroMem(&configFlags, sizeof(configFlags));
    configFlags.Flags.MeasureAdditionalPcrs = 1;
    configFlags.Flags.DefaultBootAlwaysAttempt = 1;
    configFlags.Flags.VpciBootEnabled = 1;
    configFlags.Flags.MemoryProtectionMode = ConfigLibMemoryProtectionModeDefault;

    ConfigSetUefiConfigFlags(&configFlags);

    //
    // If a secrets page is present, then check to see whether an SVSM is
    // present.
    //

    if (parameterInfo->SecretsPageOffset != 0)
    {
        secretsPage = GetIgvmData(parameterInfo, parameterInfo->SecretsPageOffset);
        HvDetectSvsm(secretsPage, &svsmBase, &svsmSize);
    }

    //
    // Convert the memory map to UEFI format.
    //

    status = ParseIgvmMemoryMap(parameterInfo, svsmBase, svsmSize);
    if (EFI_ERROR(status))
    {
        return status;
    }

    return EFI_SUCCESS;
}
