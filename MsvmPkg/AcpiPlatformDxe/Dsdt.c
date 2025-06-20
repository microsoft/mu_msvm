/** @file
  This module is responsible for runtime initialization of the DSDT acpi table.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include "AcpiPlatform.h"

#pragma pack(1)

typedef struct _DSDT_AML_DATA
{
    UINT32 Mmio1Start;
    UINT32 Mmio1Length;
    UINT32 Mmio2StartMb;
    UINT32 Mmio2LengthMb;
    UINT64 GenerationIdAddress;
    UINT32 ProcessorCount;
    UINT32 NvdimmBufferAddress;
    UINT8  SerialControllerEnabled;
    UINT8  TpmEnabled;
    UINT8  OempEnabled;
    UINT8  HibernateEnabled;
    UINT8  PmemEnabled;
    UINT8  VirtualBatteryEnabled;
    UINT8  SgxMemoryEnabled;
    UINT8  ProcIdleEnabled;
    UINT8  CxlMemoryEnabled;
    UINT16 NvdimmCount;
} DSDT_AML_DATA;

typedef struct _DSDT_AML_DESCRIPTOR
{
    UINT64 Signature;
    UINT32 PhysicalAddress;
} DSDT_AML_DESCRIPTOR;

#define DSDT_AML_DESCRIPTOR_SIGNATURE 0x0c00534f4942805bui64
#define NVDIMM_IO_BUFFER_SIZE 4096

#pragma pack()

EFI_STATUS
DsdtAllocateAmlData(
    OUT UINT32 *AmlDataAddress
    )
/*++

Routine Description:

    Allocates and initializes the data structure that is used to pass
    data between the UEFI firmware and the AML ACPI code running as part of
    the DSDT table.

    This routine also allocates the storage used for the VM generation ID
    feature, since this storage is pointed to by the DSDT.

Arguments:

    AllocatedAmlData - Returns a pointer to the allocated data structure.

Return Value:

    EFI_STATUS.

--*/
{
    DSDT_AML_DATA *data;
    EFI_PHYSICAL_ADDRESS dataPages;
    EFI_PHYSICAL_ADDRESS nvdimmBuffer;
    VOID *generationId;
    EFI_STATUS status;

    generationId = NULL;
    dataPages = 0;
    nvdimmBuffer = 0;

    //
    // Allocate a page for the AML data in runtime services below 4GB. This
    // is necessary because the DSDT uses a 32-bit physical address to
    // find the data.
    //
    dataPages = (EFI_PHYSICAL_ADDRESS)(UINT32)-1;
    status = gBS->AllocatePages(AllocateMaxAddress,
                                EfiRuntimeServicesData,
                                EFI_SIZE_TO_PAGES(sizeof(*data)),
                                &dataPages);

    if (EFI_ERROR(status))
    {
        DEBUG((DEBUG_ERROR, "%a: Failed to allocate memory for DSDT AML data.\n", __FUNCTION__));
        dataPages = 0;
        goto Cleanup;
    }

    data = (DSDT_AML_DATA *)(UINTN)dataPages;
    ZeroMem(data, sizeof(*data));

    data->Mmio1Start = (UINT32)(PcdGet64(PcdLowMmioGapBasePageNumber) * SIZE_4KB);
    data->Mmio1Length = (UINT32)(PcdGet64(PcdLowMmioGapSizeInPages) * SIZE_4KB);
    data->Mmio2StartMb = (UINT32)(PcdGet64(PcdHighMmioGapBasePageNumber) * SIZE_4KB / SIZE_1MB);
    data->Mmio2LengthMb = (UINT32)(PcdGet64(PcdHighMmioGapSizeInPages) * SIZE_4KB / SIZE_1MB);

    //
    // Allocate space for the generation ID and inform both
    // the worker process and DSDT of its address.
    //
    generationId = AllocateRuntimeZeroPool(BiosInterfaceGenerationIdSize);
    if (generationId == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    if (!mHardwareIsolatedNoParavisor)
    {
        SetGenerationIdAddress((UINTN)generationId);
    }

    data->GenerationIdAddress = (UINTN)generationId;

    //
    // Inform DSDT of other dynamic configuration.
    //
    data->ProcessorCount = PcdGet32(PcdProcessorCount);
    data->SerialControllerEnabled = PcdGetBool(PcdSerialControllersEnabled);
    data->TpmEnabled = PcdGetBool(PcdTpmEnabled);
    data->OempEnabled = PcdGetBool(PcdLoadOempTable);
    data->HibernateEnabled = PcdGetBool(PcdHibernateEnabled);
    data->PmemEnabled = mHardwareIsolatedNoParavisor ? 0 : (GetNfitSize() > 0);
    DEBUG((DEBUG_ERROR, "%a: PmemEnabled %d\n", __FUNCTION__, data->PmemEnabled));
    data->VirtualBatteryEnabled = PcdGetBool(PcdVirtualBatteryEnabled);
    data->SgxMemoryEnabled = PcdGetBool(PcdSgxMemoryEnabled);
    data->ProcIdleEnabled = PcdGetBool(PcdProcIdleEnabled);
    data->CxlMemoryEnabled = PcdGetBool(PcdCxlMemoryEnabled);
    data->NvdimmCount = PcdGet16(PcdNvdimmCount);

    DEBUG((DEBUG_VERBOSE, "--- %a: Mmio1Start               0x%lx\n", __FUNCTION__, data->Mmio1Start));
    DEBUG((DEBUG_VERBOSE, "--- %a: Mmio1Length              0x%lx\n", __FUNCTION__, data->Mmio1Start));
    DEBUG((DEBUG_VERBOSE, "--- %a: Mmio2StartMb             0x%lx\n", __FUNCTION__, data->Mmio2StartMb));
    DEBUG((DEBUG_VERBOSE, "--- %a: Mmio2LengthMb            0x%lx\n", __FUNCTION__, data->Mmio2LengthMb));
    DEBUG((DEBUG_VERBOSE, "--- %a: ProcessorCount           0x%lx\n", __FUNCTION__, data->ProcessorCount));
    DEBUG((DEBUG_VERBOSE, "--- %a: SerialControllerEnabled  0x%x\n", __FUNCTION__, data->SerialControllerEnabled));
    DEBUG((DEBUG_VERBOSE, "--- %a: HibernateEnabled         0x%x\n", __FUNCTION__, data->HibernateEnabled));
    DEBUG((DEBUG_VERBOSE, "--- %a: PmemEnabled              0x%x\n", __FUNCTION__, data->PmemEnabled));
    DEBUG((DEBUG_VERBOSE, "--- %a: VirtualBatteryEnabled    0x%x\n", __FUNCTION__, data->VirtualBatteryEnabled));
    DEBUG((DEBUG_VERBOSE, "--- %a: SgxMemoryEnabled         0x%x\n", __FUNCTION__, data->SgxMemoryEnabled));
    DEBUG((DEBUG_VERBOSE, "--- %a: ProcIdleEnabled          0x%x\n", __FUNCTION__, data->ProcIdleEnabled));
    DEBUG((DEBUG_VERBOSE, "--- %a: CxlMemoryEnabled         0x%x\n", __FUNCTION__, data->CxlMemoryEnabled));
    DEBUG((DEBUG_VERBOSE, "--- %a: NvdimmCount              0x%x\n", __FUNCTION__, data->NvdimmCount));

    //
    // Allocate space for the NVDIMM IO Buffer if VPMEM is enabled.
    //
    if (data->PmemEnabled) {
        DEBUG((DEBUG_VERBOSE, "%a: Allocating NVDIMM IO Buffer\n", __FUNCTION__));
        nvdimmBuffer = (EFI_PHYSICAL_ADDRESS)(UINT32)-1;
        DEBUG((DEBUG_VERBOSE, "%a: NVDIMM IO Buffer size: 0x%x pages\n", __FUNCTION__, 
               EFI_SIZE_TO_PAGES(NVDIMM_IO_BUFFER_SIZE)));
        status = gBS->AllocatePages(AllocateMaxAddress,
                                    EfiRuntimeServicesData,
                                    EFI_SIZE_TO_PAGES(NVDIMM_IO_BUFFER_SIZE),
                                    &nvdimmBuffer);
        if (EFI_ERROR(status))
        {
            DEBUG((DEBUG_ERROR, "%a: Failed to allocate memory for NVDIMM IO Buffer.\n", __FUNCTION__));
            nvdimmBuffer = 0;
            goto Cleanup;
        }

        ZeroMem((VOID*)nvdimmBuffer, NVDIMM_IO_BUFFER_SIZE);
        SetVpmemACPIBuffer((UINT32)nvdimmBuffer);
    }

    data->NvdimmBufferAddress = (UINT32)nvdimmBuffer;
    *AmlDataAddress = (UINT32)dataPages;
    DEBUG((DEBUG_ERROR, "%a: AmlDataAddress: 0x%x\n", __FUNCTION__, *AmlDataAddress));
    DEBUG((DEBUG_ERROR, "%a: NvdimmBufferAddress: 0x%x\n", __FUNCTION__, data->NvdimmBufferAddress));
    status = EFI_SUCCESS;

Cleanup:

    if (EFI_ERROR(status))
    {
        if (generationId != NULL)
        {
            FreePool(generationId);
        }

        if (dataPages != 0)
        {
            gBS->FreePages(dataPages, EFI_SIZE_TO_PAGES(sizeof(*data)));
        }

        if (nvdimmBuffer != 0)
        {
            gBS->FreePages(nvdimmBuffer, EFI_SIZE_TO_PAGES(NVDIMM_IO_BUFFER_SIZE));
        }
    }

    DEBUG((DEBUG_VERBOSE, "<<< %a: status %r\n", __FUNCTION__, status));

    return status;
}

//
// Entry point
//

EFI_STATUS
DsdtInitializeTable(
    IN OUT  EFI_ACPI_DESCRIPTION_HEADER* Dsdt
    )
/*++

Routine Description:

    Initializes the Dsdt table.

Arguments:

    Dsdt - The Dsdt Table, expressed as an EFI_ACPI_DESCRIPTION_HEADER*.

Return Value:

    EFI_SUCCESS

--*/
{
    UINT32 amlData;
    UINT8 *data;
    UINT32 tableIndex;
    DSDT_AML_DESCRIPTOR *descriptor;
    EFI_STATUS status;

    //
    // Allocate the AML data that's used to share information with the
    // DSDT table.
    //

    status = DsdtAllocateAmlData(&amlData);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // The AML data must be pointed to by the DSDT directly via an
    // OperationRegion labeled "BIOS". Find the position in the DSDT file
    // where this operation region is described, then overwrite the 32-bit
    // address that is already present with the physical address of the
    // newly allocated data.
    //

    status = EFI_NOT_FOUND;
    data = (UINT8 *)(Dsdt + 1);
    for (tableIndex = 0;
         tableIndex + sizeof(*descriptor) < Dsdt->Length;
         tableIndex += 1)
    {
        descriptor = (DSDT_AML_DESCRIPTOR *)&data[tableIndex];
        if (descriptor->Signature == DSDT_AML_DESCRIPTOR_SIGNATURE)
        {
            descriptor->PhysicalAddress = amlData;
            status = EFI_SUCCESS;
            break;
        }
    }

Cleanup:
    return status;
}


