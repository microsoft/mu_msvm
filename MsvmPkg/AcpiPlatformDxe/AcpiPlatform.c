/** @file
  This file contains routines to locate ACPI tables in the firmware volume,
  update them appropriately, and install them via the AcpiTable protocol.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Protocol/AcpiTable.h>
#include <Protocol/FirmwareVolume2.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/IoLib.h>
#include <Guid/Acpi.h>
#include <AcpiPlatform.h>
#include <IsolationTypes.h>

typedef EFI_STATUS (*INIT_ROUTINE)(EFI_ACPI_DESCRIPTION_HEADER*);

typedef struct _INIT_TABLE_ENTRY
{
    UINT32 signature;
    INIT_ROUTINE initRoutine;
} INIT_TABLE_ENTRY;

//
// The list of tables that need to be updated at runtime. All other tables
// are installed without modification.
//

INIT_TABLE_ENTRY AcpiInitTable[] =
{
    { VM_ACPI_ENTROPY_TABLE_SIGNATURE, Oem0InitializeTable },
    { EFI_ACPI_6_2_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE, DsdtInitializeTable },
    { EFI_ACPI_6_2_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_SIGNATURE, SpcrInitializeTable },
    { EFI_ACPI_6_2_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE, FacpInitializeTable },
    { EFI_ACPI_6_2_WATCHDOG_ACTION_TABLE_SIGNATURE, WdatInitializeTable },
};

#define NUM_TABLE_ENTRIES (sizeof(AcpiInitTable) / sizeof(INIT_TABLE_ENTRY))

BOOLEAN mHardwareIsolatedNoParavisor = FALSE;

EFI_STATUS
RuntimeInitializeTableIfNecessary(
    IN OUT  EFI_ACPI_DESCRIPTION_HEADER* Table
    )
/*++

Routine Description:

    Performs any runtime initialization required by a given ACPI table.

Arguments:

    Table - The ACPI table which may require runtime initialization.

Return Value:

    An appropriate EFI_STATUS value.

--*/
{
    INIT_TABLE_ENTRY* entry;
    UINT32 index;

    for (index = 0; index < NUM_TABLE_ENTRIES; index++)
    {
        entry = &AcpiInitTable[index];
        if (entry->signature == Table->Signature)
        {
            return entry->initRoutine(Table);
        }
    }

    return EFI_SUCCESS;
}


EFI_STATUS
LocateFvInstanceWithTables(
    OUT EFI_FIRMWARE_VOLUME2_PROTOCOL** Instance
    )
/*++

Routine Description:

    Locates the first instance of a protocol.  If the protocol requested is an
    FV protocol, then it will return the first FV that contains the ACPI table
    storage file.

Arguments:

    Instance - Return pointer to the first instance of the protocol

Return Value:

    An appropriate EFI_STATUS value.

--*/
{
    EFI_STATUS                      status;
    EFI_HANDLE*                     handleBuffer;
    UINTN                           numberOfHandles;
    EFI_FV_FILETYPE                 fileType;
    UINT32                          fvStatus;
    EFI_FV_FILE_ATTRIBUTES          attributes;
    UINTN                           size;
    UINTN                           index;
    EFI_FIRMWARE_VOLUME2_PROTOCOL*  fvInstance;

    fvStatus = 0;

    //
    // Locate protocol.
    //
    status = gBS->LocateHandleBuffer(ByProtocol,
                                     &gEfiFirmwareVolume2ProtocolGuid,
                                     NULL,
                                     &numberOfHandles,
                                     &handleBuffer);

    if (EFI_ERROR(status))
    {
        //
        // Defined errors at this time are not found and out of resources.
        //
        return status;
    }

    //
    // Looking for FV with ACPI storage file
    //
    status = EFI_NOT_FOUND;
    for (index = 0; index < numberOfHandles; index++)
    {
        //
        // Get the protocol on this handle
        // This should not fail because of LocatehandleBuffer
        //
        status = gBS->HandleProtocol(handleBuffer[index],
                                     &gEfiFirmwareVolume2ProtocolGuid,
                                     (VOID **)&fvInstance);

        ASSERT_EFI_ERROR(status);

        //
        // See if it has the ACPI storage file
        //

        status = fvInstance->ReadFile(fvInstance,
                                      (EFI_GUID *)PcdGetPtr(PcdAcpiTableStorageFile),
                                      NULL,
                                      &size,
                                      &fileType,
                                      &attributes,
                                      &fvStatus);

        if (!EFI_ERROR(status))
        {
            *Instance = fvInstance;
            break;
        }
        else if (status != EFI_NOT_FOUND)
        {
            break;
        }
    }

    //
    // Our exit status is determined by the success of the previous operations
    // If the protocol was found, Instance already points to it.
    //
    gBS->FreePool(handleBuffer);
    return status;
}


EFI_STATUS
AcpiInstallMadtTable(
    IN OUT  EFI_ACPI_TABLE_PROTOCOL *AcpiTable
    )
/*++

Routine Description:

    Retrieves the MADT table from the worker process and installs it.

Arguments:

    AcpiTable - A pointer to the ACPI table protocol.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_ACPI_DESCRIPTION_HEADER *table;
    UINTN tableHandle;
    UINT32 madtSize;

#if defined(MDE_CPU_X64)

    UINT32 isolationType;
    UINT8 *updatedMadtTable = NULL;
    UINT32 updatedMadtSize;
    EFI_ACPI_6_4_MULTIPROCESSOR_WAKEUP_STRUCTURE *mpWakeUpStruct;
    EFI_PHYSICAL_ADDRESS apMailboxAddress = 0;

#endif


    //
    // Get the MADT from the config blob parsed in PEI.
    //
    madtSize = PcdGet32(PcdMadtSize);
    table = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN) PcdGet64(PcdMadtPtr);

    ASSERT(table->Length == madtSize);

#if defined(MDE_CPU_X64)

    isolationType = GetIsolationType();

    //
    // Add the wake up structure and update the table if there are APs present for TDX.
    //
    if (isolationType == UefiIsolationTypeTdx && !IsParavisorPresent())
    {
        if (PcdGet32(PcdProcessorCount) > 1)
        {
            DEBUG((EFI_D_INFO, "Original Madt length : 0x%x\n", madtSize));

            //
            // Allocate memory for the new table which includes the wake up structure.
            //
            updatedMadtSize = madtSize + sizeof(EFI_ACPI_6_4_MULTIPROCESSOR_WAKEUP_STRUCTURE);
            status = gBS->AllocatePool(EfiACPIReclaimMemory, updatedMadtSize, (VOID**)&updatedMadtTable);
            if (EFI_ERROR(status))
            {
                DEBUG((EFI_D_ERROR, "%a: Failed to allocate memory for the new MADT table.\n", __FUNCTION__));
                goto Cleanup;
            }

            status = gBS->AllocatePages(AllocateAnyPages,
                                        EfiACPIMemoryNVS,
                                        EFI_SIZE_TO_PAGES(SIZE_4KB),
                                        (EFI_PHYSICAL_ADDRESS*)&apMailboxAddress);
            if (EFI_ERROR(status))
            {
                DEBUG((EFI_D_ERROR, "%a: Failed to allocate memory for the new MADT wake up structure.\n", __FUNCTION__));
                goto Cleanup;
            }

            //
            // Copy the original table over and update the header fields and the wake up structure in it.
            //
            CopyMem(updatedMadtTable, (UINT8 *)table, table->Length);

            table                = (EFI_ACPI_DESCRIPTION_HEADER *)updatedMadtTable;
            table->Length        = updatedMadtSize;

            mpWakeUpStruct                  = (EFI_ACPI_6_4_MULTIPROCESSOR_WAKEUP_STRUCTURE *)(updatedMadtTable + madtSize);
            mpWakeUpStruct->Type            = EFI_ACPI_6_4_MULTIPROCESSOR_WAKEUP;
            mpWakeUpStruct->Length          = sizeof (EFI_ACPI_6_4_MULTIPROCESSOR_WAKEUP_STRUCTURE);
            mpWakeUpStruct->MailBoxVersion  = 0;
            mpWakeUpStruct->Reserved        = 0;
            mpWakeUpStruct->MailBoxAddress  = apMailboxAddress;

            table->Checksum     = CalculateCheckSum8((UINT8*)&table, updatedMadtSize);


            status = PcdSet64S(PcdAcpiMadtMpMailBoxAddress, (UINT64) apMailboxAddress);
            if (EFI_ERROR(status))
            {
                DEBUG((EFI_D_ERROR, "%a: Failed to set the mailbox address PCD.\n", __FUNCTION__));
                goto Cleanup;
            }
        }
    }

#endif

    //
    // Install it into the published tables.
    //
    status = AcpiTable->InstallAcpiTable(AcpiTable,
                                         table,
                                         table->Length,
                                         &tableHandle);

#if defined(MDE_CPU_X64)

Cleanup:

    //
    // Cleanup memory allocated for the new MADT table
    //
    if (updatedMadtTable != NULL)
    {
        gBS->FreePool(updatedMadtTable);
        updatedMadtTable = NULL;
    }

    if (EFI_ERROR(status))
    {
        if (apMailboxAddress != 0)
        {
            gBS->FreePages(apMailboxAddress, EFI_SIZE_TO_PAGES(SIZE_4KB));
            apMailboxAddress = 0;
        }
    }

#endif

    return status;
}


EFI_STATUS
AcpiInstallSratTable(
    IN OUT  EFI_ACPI_TABLE_PROTOCOL *AcpiTable
    )
/*++

Routine Description:

    Retrieves the SRAT table from the worker process and installs it.

Arguments:

    AcpiTable - A pointer to the ACPI table protocol.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_ACPI_DESCRIPTION_HEADER *table;
    UINTN tableHandle;
    UINT32 sratSize;

    //
    // Get the SRAT from the config blob parsed in PEI.
    //
    sratSize = PcdGet32(PcdSratSize);
    table = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN) PcdGet64(PcdSratPtr);

    ASSERT(table->Length == sratSize);

    //
    // Install it into the published tables.
    //
    status = AcpiTable->InstallAcpiTable(AcpiTable,
                                         table,
                                         table->Length,
                                         &tableHandle);

    return status;
}


EFI_STATUS
AcpiInstallHmatTable(
    IN OUT  EFI_ACPI_TABLE_PROTOCOL *AcpiTable
    )
/*++

Routine Description:

    Retrieves the HMAT table from the worker process and installs it.

Arguments:

    AcpiTable - A pointer to the ACPI table protocol.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_ACPI_DESCRIPTION_HEADER *table;
    UINTN tableHandle;
    UINT32 tableSize;

    tableSize = PcdGet32(PcdHmatSize);
    table = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN) PcdGet64(PcdHmatPtr);

    if (tableSize == 0)
    {
        ASSERT(table == NULL);
        DEBUG((EFI_D_INFO, "HMAT not installed.\n"));
        return EFI_SUCCESS;
    }

    ASSERT(table->Length == tableSize);

    status = AcpiTable->InstallAcpiTable(AcpiTable,
                                         table,
                                         table->Length,
                                         &tableHandle);

    return status;
}


EFI_STATUS
AcpiInstallPpttTable(
    IN OUT  EFI_ACPI_TABLE_PROTOCOL *AcpiTable
    )
/*++

Routine Description:

    Retrieves the PPTT table from the worker process and installs it.

Arguments:

    AcpiTable - A pointer to the ACPI table protocol.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_ACPI_DESCRIPTION_HEADER *table;
    UINTN tableHandle;
    UINT32 tableSize;

    tableSize = PcdGet32(PcdPpttSize);
    table = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN) PcdGet64(PcdPpttPtr);

    if (tableSize == 0)
    {
        ASSERT(table == NULL);
        DEBUG((EFI_D_INFO, "PPTT not installed.\n"));
        return EFI_SUCCESS;
    }

    ASSERT(table->Length == tableSize);

    status = AcpiTable->InstallAcpiTable(AcpiTable,
                                         table,
                                         table->Length,
                                         &tableHandle);

    return status;
}


EFI_STATUS
AcpiInstallSlitTable(
    IN OUT  EFI_ACPI_TABLE_PROTOCOL *AcpiTable
    )
/*++

Routine Description:

    Retrieves the SLIT table from the worker process and installs it.

Arguments:

    AcpiTable - A pointer to the ACPI table protocol.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_ACPI_DESCRIPTION_HEADER *table;
    UINTN tableHandle;
    UINT32 slitSize;

    //
    // Get the SLIT from the config blob parsed in PEI.
    //
    slitSize = PcdGet32(PcdSlitSize);
    table = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN) PcdGet64(PcdSlitPtr);

    if (slitSize == 0)
    {
        ASSERT(table == NULL);
        DEBUG((EFI_D_INFO, "SLIT not installed.\n"));
        return EFI_SUCCESS;
    }

    ASSERT(table->Length == slitSize);

    //
    // Install it into the published tables.
    //
    status = AcpiTable->InstallAcpiTable(AcpiTable,
                                         table,
                                         table->Length,
                                         &tableHandle);

    return status;
}


EFI_STATUS
AcpiInstallNfitTable(
    IN OUT  EFI_ACPI_TABLE_PROTOCOL *AcpiTable
    )
{
    EFI_STATUS status;
    EFI_ACPI_DESCRIPTION_HEADER *table = NULL;
    EFI_PHYSICAL_ADDRESS buffer;
    UINT32 nfitSize;
    UINTN tableHandle;

    //
    // Hardware isolated VMs with no paravisor have no PMEM today.
    //
    if (mHardwareIsolatedNoParavisor)
    {
        return EFI_SUCCESS;
    }

    //
    // Get the size of the NFIT
    //
    nfitSize = GetNfitSize();
    DEBUG((EFI_D_ERROR, "%a: NFIT size: 0x%x\n", __FUNCTION__, nfitSize));

    //
    // Size of 0 means no NFIT
    // We dynamically determine the NFIT size since we need to support
    // hot-add of devices.
    //
    if (nfitSize == 0)
    {
        DEBUG((EFI_D_ERROR, "%a: NFIT size is 0, no nfit\n", __FUNCTION__));
        return EFI_SUCCESS;
    }

    #define BELOW_4GB (0xFFFFFFFFULL)
    buffer = BELOW_4GB;
    status = gBS->AllocatePages(AllocateMaxAddress,
                                EfiBootServicesData,
                                EFI_SIZE_TO_PAGES(nfitSize),
                                &buffer);
    table = (void*) buffer;
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "%a: Failed to allocate memory for NFIT table.\n", __FUNCTION__));
        table = NULL;
        goto Cleanup;
    }

    //
    // Notify the vPMEM vdev to populate the NFIT table
    //
    DEBUG((EFI_D_ERROR, "%a: Calling GetNfit to populate the NFIT table.\n", __FUNCTION__));
    GetNfit((UINT64)table);

    //
    // Install the NFIT table
    //
    status = AcpiTable->InstallAcpiTable(AcpiTable,
                                         table,
                                         table->Length,
                                         &tableHandle);
    DEBUG((EFI_D_ERROR, "%a: NFIT table installed at 0x%p, status: %r\n",
           __FUNCTION__, table, status));                                         

    //
    // Cleanup memory allocated for the NFIT table
    //
    if (table != NULL)
    {
        gBS->FreePages(buffer, EFI_SIZE_TO_PAGES(nfitSize));
        table = NULL;
    }

Cleanup:

    return status;
}


EFI_STATUS
AcpiInstallConfigStructTable(
    IN OUT  EFI_ACPI_TABLE_PROTOCOL *AcpiTable
    )
/*++

Routine Description:

    Retrieves the config struct table if present and installs it.

Arguments:

    AcpiTable - A pointer to the ACPI table protocol.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_ACPI_DESCRIPTION_HEADER *table;
    UINTN tableHandle;
    UINT32 tableSize;

    //
    // Get the table from the config blob parsed in PEI. It may not be present.
    //
    tableSize = PcdGet32(PcdAcpiTableSize);

    if (tableSize == 0)
    {
        return EFI_SUCCESS;
    }

    table = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN) PcdGet64(PcdAcpiTablePtr);

    ASSERT(table->Length == tableSize);

    //
    // Install it into the published tables.
    //
    status = AcpiTable->InstallAcpiTable(AcpiTable,
                                         table,
                                         table->Length,
                                         &tableHandle);

    return status;
}


#if defined(MDE_CPU_X64)
EFI_STATUS
AcpiInstallAsptTable(
    IN OUT  EFI_ACPI_TABLE_PROTOCOL *AcpiTable
    )
/*++

Routine Description:

    Retrieves the ASPT table from the worker process and installs it.

Arguments:

    AcpiTable - A pointer to the ACPI table protocol.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_ACPI_DESCRIPTION_HEADER *table;
    UINTN tableHandle;
    UINT32 asptSize;

    //
    // Get the ASPT from the config blob parsed in PEI.
    //
    asptSize = PcdGet32(PcdAsptSize);
    table = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN) PcdGet64(PcdAsptPtr);

    if (asptSize == 0)
    {
        //
        // The ASPT will not be provided if no compatible AMD Secure Processor
        // is enabled.
        //
        return EFI_SUCCESS;
    }

    ASSERT(table->Length == asptSize);

    //
    // Install it into the published tables.
    //
    status = AcpiTable->InstallAcpiTable(AcpiTable,
                                         table,
                                         table->Length,
                                         &tableHandle);

    return status;
}
#endif


EFI_STATUS
AcpiInstallMcfgTable(
    EFI_ACPI_TABLE_PROTOCOL *AcpiTable
    )
/*++

Routine Description:

    Retrieves the MCFG table from the worker process and installs it.

Arguments:

    AcpiTable - A pointer to the ACPI table protocol.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_ACPI_DESCRIPTION_HEADER *table;
    UINTN tableHandle;
    UINT32 tableSize;

    //
    // Get the table from the config blob parsed in PEI. It may not be present.
    //
    tableSize = PcdGet32(PcdMcfgSize);

    if (tableSize == 0)
    {
        return EFI_SUCCESS;
    }

    table = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN) PcdGet64(PcdMcfgPtr);

    if (table == NULL)
    {
        return EFI_NOT_FOUND;
    }

    ASSERT(table->Length == tableSize);

    //
    // Install it into the published tables.
    //
    status = AcpiTable->InstallAcpiTable(AcpiTable,
                                         table,
                                         table->Length,
                                         &tableHandle);

    return status;
}


EFI_STATUS
AcpiInstallSsdtTable(
    EFI_ACPI_TABLE_PROTOCOL *AcpiTable
    )
/*++

Routine Description:

    Retrieves the SSDT table from the worker process and installs it.

Arguments:

    AcpiTable - A pointer to the ACPI table protocol.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_ACPI_DESCRIPTION_HEADER *table;
    UINTN tableHandle;
    UINT32 tableSize;

    //
    // Get the table from the config blob parsed in PEI. It may not be present.
    //
    tableSize = PcdGet32(PcdSsdtSize);

    if (tableSize == 0)
    {
        return EFI_SUCCESS;
    }

    table = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN) PcdGet64(PcdSsdtPtr);

    if (table == NULL)
    {
        return EFI_NOT_FOUND;
    }

    ASSERT(table->Length == tableSize);

    //
    // Install it into the published tables.
    //
    status = AcpiTable->InstallAcpiTable(AcpiTable,
                                         table,
                                         table->Length,
                                         &tableHandle);

    return status;
}


EFI_STATUS
AcpiInstallIortTable(
    EFI_ACPI_TABLE_PROTOCOL *AcpiTable
    )
/*++

Routine Description:

    Retrieves the IORT table from the worker process and installs it.

Arguments:

    AcpiTable - A pointer to the ACPI table protocol.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_ACPI_DESCRIPTION_HEADER *table;
    UINTN tableHandle;
    UINT32 tableSize;

    //
    // Get the table from the config blob parsed in PEI. It may not be present.
    //
    tableSize = PcdGet32(PcdIortSize);

    if (tableSize == 0)
    {
        return EFI_SUCCESS;
    }

    table = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN) PcdGet64(PcdIortPtr);

    if (table == NULL)
    {
        return EFI_NOT_FOUND;
    }

    ASSERT(table->Length == tableSize);

    //
    // Install it into the published tables.
    //
    status = AcpiTable->InstallAcpiTable(AcpiTable,
                                         table,
                                         table->Length,
                                         &tableHandle);

    return status;
}


EFI_STATUS
EFIAPI
AcpiPlatformInitializeAcpiTables(
    IN  EFI_HANDLE        ImageHandle,
    IN  EFI_SYSTEM_TABLE* SystemTable
    )
/*++

Routine Description:

    Entry point of the ACPI platform driver.

Arguments:

    ImageHandle - Driver Image Handle.

    SystemTable - EFI System Table.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_ACPI_TABLE_PROTOCOL*        acpiTable;
    EFI_FIRMWARE_VOLUME2_PROTOCOL*  fwVol;
    INTN                            instance;
    EFI_ACPI_DESCRIPTION_HEADER*    currentTable;
    UINT32                          fvStatus;
    UINTN                           size;
    EFI_STATUS                      status;
    UINTN                           tableHandle;

    mHardwareIsolatedNoParavisor = IsHardwareIsolatedNoParavisor();

    //
    // Find the AcpiTable protocol.
    //
    status = gBS->LocateProtocol(&gEfiAcpiTableProtocolGuid, NULL, (VOID**) &acpiTable);

    ASSERT_EFI_ERROR(status);

    //
    // Locate the firmware volume protocol.
    //
    status = LocateFvInstanceWithTables(&fwVol);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Read tables from the storage file.
    //
    for (instance = 0; ; instance += 1)
    {
        currentTable = NULL;
        status = fwVol->ReadSection(fwVol,
                                    (EFI_GUID*) PcdGetPtr(PcdAcpiTableStorageFile),
                                    EFI_SECTION_RAW,
                                    instance,
                                    (VOID **)&currentTable,
                                    &size,
                                    &fvStatus);

        if (EFI_ERROR(status))
        {
            if (status == EFI_NOT_FOUND)
            {
                break;
            }

            goto Cleanup;
        }

        //
        // Add the table.
        //
        ASSERT(size >= currentTable->Length);

        status = RuntimeInitializeTableIfNecessary(currentTable);

        if (EFI_ERROR(status))
        {
            //
            // If the table init routine returned EFI_UNSUPPORTED then
            // don't install this table and continue normally.
            //
            if (status == EFI_UNSUPPORTED)
            {
                status = EFI_SUCCESS;
            }
        }
        else
        {
            //
            // Install the table.
            //
            status = acpiTable->InstallAcpiTable(acpiTable,
                                                 currentTable,
                                                 currentTable->Length,
                                                 &tableHandle);
        }

        //
        // Free memory allocated by ReadSection.
        //
        gBS->FreePool(currentTable);

        if (EFI_ERROR(status))
        {
            goto Cleanup;
        }
    }

    //
    // Add the MADT table.
    //
    status = AcpiInstallMadtTable(acpiTable);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Add the SRAT table.
    //
    status = AcpiInstallSratTable(acpiTable);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Add the PPTT table.
    //
    status = AcpiInstallPpttTable(acpiTable);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Add the SLIT table.
    //
    status = AcpiInstallSlitTable(acpiTable);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Add the NFIT table.
    //
    status = AcpiInstallNfitTable(acpiTable);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

#if defined(MDE_CPU_X64)
    //
    // Add the ASPT table.
    //
    status = AcpiInstallAsptTable(acpiTable);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }
#endif

    //
    // Add the dynamic config struct table if present.
    //
    status = AcpiInstallConfigStructTable(acpiTable);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Add the MCFG table if present.
    //
    status = AcpiInstallMcfgTable(acpiTable);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Add the SSDT table if present.
    //
    status = AcpiInstallSsdtTable(acpiTable);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Install the IORT table if present
    //
    status = AcpiInstallIortTable(acpiTable);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Add the HMAT table if present.
    //
    status = AcpiInstallHmatTable(acpiTable);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = EFI_SUCCESS;

Cleanup:
    return status;
}
