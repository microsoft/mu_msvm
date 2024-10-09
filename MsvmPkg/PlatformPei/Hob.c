/** @file
  Hob-building functionality.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/HobLib.h>
#include <Library/DebugLib.h>
#include <BiosInterface.h>
#include <Platform.h>
#include <Config.h>
#include <Hob.h>
#include <Hv.h>
#include <Library/HostVisibilityLib.h>
#include <IsolationTypes.h>
#include <Library/CrashDumpAgentLib.h>


#define BASIC_FLAGS                                     \
    (EFI_RESOURCE_ATTRIBUTE_PRESENT |                   \
     EFI_RESOURCE_ATTRIBUTE_INITIALIZED)

#define STANDARD_FLAGS                                  \
    (BASIC_FLAGS |                                      \
     EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE |               \
     EFI_RESOURCE_ATTRIBUTE_TESTED)

#define MEMORY_FLAGS                                    \
    (STANDARD_FLAGS |                                   \
     EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |         \
     EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |   \
     EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE)

#define PERSISTENT_MEMORY_FLAGS                         \
    (MEMORY_FLAGS |                                     \
     EFI_RESOURCE_ATTRIBUTE_PERSISTENT)

#define SP_MEMORY_FLAGS                                 \
    (MEMORY_FLAGS |                                     \
     EFI_RESOURCE_ATTRIBUTE_SPECIAL_PURPOSE)

static
VOID
HobpAcceptRamPages(
    IN OUT  PPLATFORM_INIT_CONTEXT  Context,
            HV_GPA_PAGE_NUMBER      GpaPageBase,
            UINT64                  PageCount
    )
/*++

Routine Description:

    Accepts a range of RAM GPA pages on hardware isolated platforms that require
    such acceptance.

Arguments:

    Context - The platform init context.

    GpaPageBase - Supplies the address of the first target GPA to accept. The
        remaining pages will be modified sequentially from this GPA.

    PageCount - The number of pages to modify.

Return Value:

    None.

--*/
{
    UINT32 configBlobSize = PcdGet32(PcdConfigBlobSize);
    UINT64 configBlobBase = (UINT64)Context->StartOfConfigBlob;
    HV_GPA_PAGE_NUMBER configBlobPageLimit =
        ((configBlobBase + configBlobSize - 1) / EFI_PAGE_SIZE) + 1;

    //
    // No acceptance is required unless this is a hardware isolated platform
    // with no paravisor.
    //

    if (!IsHardwareIsolatedNoParavisor())
    {
        return;
    }

    //
    // The region from 0 to the end of the config blob is expected to be pre-accepted, so exclude
    // that from the range.
    //
    if (GpaPageBase < configBlobPageLimit)
    {
        if (GpaPageBase + PageCount > configBlobPageLimit)
        {
            PageCount -= configBlobPageLimit - GpaPageBase;
            GpaPageBase = configBlobPageLimit;
        }
        else
        {
            //
            // The region is entirely pre-accepted - there is nothing to do.
            //
            return;
        }
    }

    //
    // Accept pages as required by the architecture.
    //

#if defined(MDE_CPU_X64)
    if (IsHardwareIsolated())
    {
        PEI_FAIL_FAST_IF_FAILED(EfiUpdatePageRangeAcceptance(
            GetIsolationType(),
            (VOID*)PcdGet64(PcdSvsmCallingArea),
            GpaPageBase,
            PageCount,
            TRUE));
    }
#endif
}


void
HobAddMmioRange(
    EFI_PHYSICAL_ADDRESS    BaseAddress,
    UINT64                  Size
    )
/*++

Routine Description:

    Adds an mmio range hob to the current hob list.

Arguments:

    BaseAddress - Base address of the mmio range.

    Size - Size of the mmio range.

Return Value:

    None.

--*/
{
    BuildResourceDescriptorHob(EFI_RESOURCE_MEMORY_MAPPED_IO,
                               STANDARD_FLAGS,
                               BaseAddress,
                               Size);
    DEBUG((DEBUG_VERBOSE,
           "HOB Start % 17lx End %17lx %s\n",
           BaseAddress,
           BaseAddress + Size - 1,
           L"MMIO"));
}


void
HobAddMemoryRange(
    IN OUT  PPLATFORM_INIT_CONTEXT  Context,
            EFI_PHYSICAL_ADDRESS    BaseAddress,
            UINT64                  Size
    )
/*++

Routine Description:

    Adds a memory range hob to the current hob list.

Arguments:

    Context - The platform init context.

    BaseAddress - Base address of the memory range.

    Size - Size of the memory range.

Return Value:

    None.

--*/
{
    ASSERT((BaseAddress % EFI_PAGE_SIZE) == 0);
    ASSERT((Size % EFI_PAGE_SIZE) == 0);

    HobpAcceptRamPages(Context, BaseAddress / EFI_PAGE_SIZE, Size / EFI_PAGE_SIZE);

    BuildResourceDescriptorHob(EFI_RESOURCE_SYSTEM_MEMORY,
                               MEMORY_FLAGS,
                               BaseAddress,
                               Size);
    DEBUG((DEBUG_VERBOSE,
           "HOB Start % 17lx End %17lx %s\n",
           BaseAddress,
           BaseAddress + Size - 1,
           L"Memory"));
}


void
HobAddPersistentMemoryRange(
    EFI_PHYSICAL_ADDRESS BaseAddress,
    UINT64               Size
    )
/*++

Routine Description:

    Adds a persistent memory range hob to the current hob list.

Arguments:

    BaseAddress - Base address of the memory range.

    Size - Size of the memory range.

Return Value:

    None.

--*/
{
    BuildResourceDescriptorHob(EFI_RESOURCE_SYSTEM_MEMORY,
                               PERSISTENT_MEMORY_FLAGS,
                               BaseAddress,
                               Size);
    DEBUG((DEBUG_VERBOSE,
           "HOB Start % 17lx End %17lx %s\n",
           BaseAddress,
           BaseAddress + Size - 1,
           L"Memory"));
}

void
HobAddSpecificPurposeMemoryRange(
    EFI_PHYSICAL_ADDRESS BaseAddress,
    UINT64               Size
    )
/*++

Routine Description:

    Adds a specific purpose memory range hob to the current hob list.

Arguments:

    BaseAddress - Base address of the reserved memory range.

    Size - Size of the reserved memory range.

Return Value:

    None.

--*/
{
    BuildResourceDescriptorHob(EFI_RESOURCE_SYSTEM_MEMORY,
                               SP_MEMORY_FLAGS,
                               BaseAddress,
                               Size);
    DEBUG((DEBUG_VERBOSE,
           "HOB Start % 17lx End %17lx %s\n",
           BaseAddress,
           BaseAddress + Size - 1,
           L"Specific Purpose Memory"));
}


void
HobAddReservedMemoryRange(
    EFI_PHYSICAL_ADDRESS BaseAddress,
    UINT64               Size
    )
/*++

Routine Description:

    Adds a reserved memory range hob to the current hob list.

Arguments:

    BaseAddress - Base address of the reserved memory range.

    Size - Size of the reserved memory range.

Return Value:

    None.

--*/
{
    BuildResourceDescriptorHob(EFI_RESOURCE_MEMORY_RESERVED,
                               STANDARD_FLAGS,
                               BaseAddress,
                               Size);
    DEBUG((DEBUG_VERBOSE,
           "HOB Start % 17lx End %17lx %s\n",
           BaseAddress,
           BaseAddress + Size - 1,
           L"Reserved Memory"));
}


void
HobAddUntestedMemoryRange(
    IN OUT  PPLATFORM_INIT_CONTEXT  Context,
            EFI_PHYSICAL_ADDRESS    BaseAddress,
            UINT64                  Size
    )
/*++

Routine Description:

    Adds an untested memory range hob to the current hob list.

Arguments:

    BaseAddress - Base address of the untested memory range.

    Size - Size of the untested memory range.

Return Value:

    None.

--*/
{
    ASSERT((BaseAddress % EFI_PAGE_SIZE) == 0);
    ASSERT((Size % EFI_PAGE_SIZE) == 0);

    HobpAcceptRamPages(Context, BaseAddress / EFI_PAGE_SIZE, Size / EFI_PAGE_SIZE);

    BuildResourceDescriptorHob(EFI_RESOURCE_SYSTEM_MEMORY,
                               MEMORY_FLAGS & ~EFI_RESOURCE_ATTRIBUTE_TESTED,
                               BaseAddress,
                               Size);
    DEBUG((DEBUG_VERBOSE,
           "HOB Start % 17lx End %17lx %s\n",
           BaseAddress,
           BaseAddress + Size - 1,
           L"Untested Memory"));
}


void
HobAddAllocatedMemoryRange(
    EFI_PHYSICAL_ADDRESS BaseAddress,
    UINT64               Size
    )
/*++

Routine Description:

    Adds a allocated memory range hob to the current hob list.

Arguments:

    BaseAddress - Base address of the fv memory range.

    Size - Size of the untested fv range.

Return Value:

    None.

--*/
{
    BuildMemoryAllocationHob(BaseAddress, Size, EfiBootServicesData);
    DEBUG((DEBUG_VERBOSE,
           "HOB Start % 17lx End %17lx %s\n",
           BaseAddress,
           BaseAddress + Size - 1,
           L"Allocated Memory"));
}


void
HobAddFvMemoryRange(
    EFI_PHYSICAL_ADDRESS BaseAddress,
    UINT64               Size
    )
/*++

Routine Description:

    Adds a fv memory range hob to the current hob list.

Arguments:

    BaseAddress - Base address of the fv memory range.

    Size - Size of the untested fv range.

Return Value:

    None.

--*/
{
    BuildFvHob(BaseAddress, Size);
    DEBUG((DEBUG_VERBOSE,
           "HOB Start % 17lx End %17lx %s\n",
           BaseAddress,
           BaseAddress + Size - 1,
           L"Firmware Volume"));
}

void
HobAddIoRange(
    EFI_PHYSICAL_ADDRESS BaseAddress,
    UINT64               Size
    )
/*++

Routine Description:

    Adds an io port range to the current hob.

Arguments:

    BaseAddress - Base address of the io port range.

    Size - Size of the io port range.

Return Value:

    None.

--*/
{
    BuildResourceDescriptorHob(EFI_RESOURCE_IO, BASIC_FLAGS, BaseAddress, Size);
    DEBUG((DEBUG_VERBOSE,
           "HOB Start % 17lx End %17lx %s\n",
           BaseAddress,
           BaseAddress + Size - 1,
           L"IO Ports"));
}


void
HobAddCpu(
    UINT8 SizeOfMemorySpace,
    UINT8 SizeOfIoSpace
    )
/*++

Routine Description:

    Adds a cpu hob to the current hob list.

Arguments:

    SizeOfMemorySpace - The size of the cpu's memory space.

    SizeOfIoSpace - The size of the cpu's io space.

Return Value:

    None.

--*/
{
    BuildCpuHob(SizeOfMemorySpace, SizeOfIoSpace);
    DEBUG((DEBUG_VERBOSE, "HOB MemWidth %d IOWidth %d Cpu\n", SizeOfMemorySpace, SizeOfIoSpace));
}


void
HobAddGuidData(
    IN  EFI_GUID* Guid,
    IN  VOID*     Data,
        UINTN     DataSize
  )
/*++

Routine Description:

    Adds a guid data hob to the current hob list.  This appears to just
    association some blob of data with a given guid.

Arguments:

    Guid - The guid to which the data will be associated data.

    Data - The data associated with the guid.

    DataSize - The size of Data.

Return Value:

    None.

--*/
{
    BuildGuidDataHob(Guid, Data, DataSize);
    DEBUG((DEBUG_VERBOSE, "HOB Base % 17lx Size %17lx GUID Data\n", Data, DataSize));
}

