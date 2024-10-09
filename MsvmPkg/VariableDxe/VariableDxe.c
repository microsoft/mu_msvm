/** @file
  EFI Variable Services

  Derivative of:
    SecurityPkg\VariableAuthenticated\RuntimDxe\Variable.c
    SecurityPkg\VariableAuthenticated\RuntimDxe\VariableDxe.c

  Copyright (c) 2006 - 2012, Intel Corporation. All rights reserved.
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseMemoryLib.h>
#include <Library/Baselib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Guid/GlobalVariable.h>
#include <Guid/VariableFormat.h>
#include <Guid/ImageAuthentication.h>
#include <Protocol/Variable.h>
#include <Protocol/VariableWrite.h>
#include <Guid/EventGroup.h>
#include <NvramVariableDxe.h>

//
// Size of RAM used for maintaining the variables.
//
#define STORE_MAIN_SIZE    (128 * 1024)

//
// Size of RAM used as scratch area for variable updates.
//
#define STORE_SCRATCH_SIZE (sizeof(VARIABLE_HEADER) + \
                            EFI_MAX_VARIABLE_NAME_SIZE +  \
                            EFI_MAX_VARIABLE_DATA_SIZE)

//
// Some useful defines.
//
#define EFI_VARIABLE_ATTRIBUTES_MASK (EFI_VARIABLE_NON_VOLATILE | \
                                          EFI_VARIABLE_BOOTSERVICE_ACCESS | \
                                          EFI_VARIABLE_RUNTIME_ACCESS | \
                                          EFI_VARIABLE_HARDWARE_ERROR_RECORD | \
                                          EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS | \
                                          EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS | \
                                          EFI_VARIABLE_APPEND_WRITE)

#define AUTHINFO2_SIZE(VarAuth2) ((OFFSET_OF (EFI_VARIABLE_AUTHENTICATION_2, AuthInfo)) + \
                    (UINTN) ((EFI_VARIABLE_AUTHENTICATION_2 *) (VarAuth2))->AuthInfo.Hdr.dwLength)

#define OFFSET_OF_AUTHINFO2_CERT_DATA ((OFFSET_OF (EFI_VARIABLE_AUTHENTICATION_2, AuthInfo)) + \
                    (OFFSET_OF (WIN_CERTIFICATE_UEFI_GUID, CertData)))

//
// Defines for the guid and variables associated with the private HyperV namespace.
//
#define HYPERV_PRIVATE_NAMESPACE \
  { 0x610b9e98, 0xc6f6, 0x47f8, 0x8b, 0x47, 0x2d, 0x2d, 0xa0, 0xd5, 0x2a, 0x91 }

//
// The attribute for this volative variable is BS.
//
#define OSLOADER_INDICATIONS_NAME     L"OsLoaderIndications"



//
// Module variables.
//


//
// Variable protocol handle.
//
EFI_HANDLE          mHandle = NULL;

//
// Pointer to the volatile variable store.  This is runtime memory.
//
static VOID*        mVariableStore = NULL;

//
// Offset to the free area in the volatile variable store.
//
static UINTN        mStoreFreeOffset = 0;

//
// Scratch buffer for SetVariable append write.
//
static VOID*        mAppendBuffer = NULL;

//
// Events this driver handles.
//
static EFI_EVENT    mVirtualAddressChangeEvent = NULL;
static EFI_EVENT    mExitBootServicesEvent = NULL;


//
// Private routines
//


BOOLEAN
IsValidVariableHeader(
    IN VARIABLE_HEADER* Variable
    )
/*++

Routine Description:

    This code checks if variable header is valid or not.

Arguments:

    Variable - Pointer to the Variable Header.

Returns:

    TRUE if the variable header is valid.

--*/
{
    return (Variable != NULL) &&
           (Variable->StartId == VARIABLE_DATA) &&
           (Variable->NameSize <= EFI_MAX_VARIABLE_NAME_SIZE) &&
           (Variable->DataSize <= EFI_MAX_VARIABLE_DATA_SIZE);
}


CHAR16*
GetVariableNamePtr(
    IN VARIABLE_HEADER* Variable
    )
/*++

Routine Description:

    Returns a pointer to the Name string in a variable structure.

Arguments:

    Variable - Pointer to the Variable Header.

Returns:

    Pointer to the Name string.

--*/
{
    return (CHAR16 *)(Variable + 1);
}


UINT8*
GetVariableDataPtr(
    IN VARIABLE_HEADER* Variable
    )
/*++

Routine Description:

    Returns a pointer to the data in a variable structure.

Arguments:

    Variable - Pointer to the variable header.

Returns:

    Pointer to the variable data.

--*/
{
    //
    // Accounts for the padding of the Name string that
    // puts the data buffer on an appropriate boundary.
    //
    return (UINT8 *)((UINTN)GetVariableNamePtr(Variable) +
                            Variable->NameSize +
                            GET_PAD_SIZE(Variable->NameSize));
}


VARIABLE_HEADER*
GetNextVariablePtr(
    IN VARIABLE_HEADER* Variable
    )
/*++

Routine Description:

   Returns the pointer to the next variable header.

Arguments:

    Variable - Pointer to the variable header.

Returns:

    Pointer to next variable header.

--*/
{
    if (!IsValidVariableHeader(Variable))
    {
        return NULL;
    }

     //
    // Accounts for the padding of Data that
    // puts the next variable structure on an appropriate boundary.
    //
    return (VARIABLE_HEADER *)((UINTN)GetVariableDataPtr(Variable) +
                                      Variable->DataSize +
                                      GET_PAD_SIZE(Variable->DataSize));
}


UINTN
NameSizeOfVariable(
    IN VARIABLE_HEADER* Variable
    )
/*++

Routine Description:

    Returns the size in bytes of a variable's Name string including
    the null terminator.

Arguments:

    Variable - Pointer to the variable header.

Returns:

    Size of the Name string or 0 if the variable structure is invalid.

--*/
{
    if (Variable->State == (UINT8) (-1) ||
        Variable->DataSize == (UINT32) (-1) ||
        Variable->NameSize == (UINT32) (-1) ||
        Variable->Attributes == (UINT32) (-1))
    {
        return 0;
    }
    return (UINTN)Variable->NameSize;
}


UINTN
DataSizeOfVariable(
    IN VARIABLE_HEADER* Variable
    )
/*++

Routine Description:

    Returns the size in bytes of a variable's data.

Arguments:

    Variable - Pointer to the variable header.

Returns:

    Size of the Data or 0 if the variable structure is invalid.

--*/
{
    if (Variable->State == (UINT8)  (-1) ||
        Variable->DataSize == (UINT32) (-1) ||
        Variable->NameSize == (UINT32) (-1) ||
        Variable->Attributes == (UINT32) (-1))
    {
        return 0;
    }
    return (UINTN) Variable->DataSize;
}


VARIABLE_HEADER*
GetStartPointer(
    IN VOID* Store
    )
/*++

Routine Description:

    Returns the pointer to the first variable header in the store.

Arguments:

    Store - Pointer to the variable store memory.

Returns:

    Pointer to first variable header.

--*/
{
  return (VARIABLE_HEADER *)(Store);
}


VARIABLE_HEADER*
GetEndPointer(
    IN VOID* Store
    )
/*++

Routine Description:

    Returns the pointer to the end of the variable store.

Arguments:

    VarStoreHeader - Pointer to the variable store header.

Returns:

    Pointer to the end of the variable store memory.

--*/
{
    return (VARIABLE_HEADER *)((UINTN)Store + STORE_MAIN_SIZE);
}


VOID
Reclaim (
    )
/*++

Routine Description:

    Reclaims deleted variable space.

Arguments:

    None.

Returns:

    None.

--*/
{
    VARIABLE_HEADER*  currentVariable;
    VARIABLE_HEADER*  nextVariable;

#if !defined(MDEPKG_NDEBUG)
    NvramDebugLog("Reclaim start - offset 0x08%x", mStoreFreeOffset);
#endif

    currentVariable = GetStartPointer(mVariableStore);
    while (IsValidVariableHeader(currentVariable))
    {
        nextVariable = GetNextVariablePtr(currentVariable);
        if (currentVariable->State != VAR_ADDED &&
            currentVariable->State != (VAR_IN_DELETED_TRANSITION & VAR_ADDED))
        {
            CopyMem((VOID*)currentVariable,
                    (VOID*)nextVariable,
                    (UINTN)GetEndPointer(mVariableStore) - (UINTN)nextVariable);
            SetMem((VOID*)((UINTN)GetStartPointer(mVariableStore) + mStoreFreeOffset),
                    STORE_MAIN_SIZE - mStoreFreeOffset,
                    0xff);
            mStoreFreeOffset -= ((UINTN)nextVariable - (UINTN)currentVariable);
        }
        else
        {
            currentVariable = GetNextVariablePtr(currentVariable);
        }
    }
#if !defined(MDEPKG_NDEBUG)
    NvramDebugLog("Reclaim stop  - offset 0x08%x", mStoreFreeOffset);
#endif
}


EFI_STATUS
FindVariable(
    IN  CHAR16*           VariableName,
    IN  EFI_GUID*         VendorGuid,
    OUT VARIABLE_HEADER** Variable
  )
/*++

Routine Description:

    Finds a variable in the volatile store.

Arguments:

    VariableName - Name of the variable to be found.

    VendorGuid - Vendor GUID to be found.

    Variable - Receives a pointer to a variable header or NULL if not found.

Returns:

    EFI_INVALID_PARAMETER       - Invalid parameter.

    EFI_SUCCESS                 - Found the specified variable.

    EFI_NOT_FOUND               - Not found.

--*/
{
    VARIABLE_HEADER* CurrPtr;
    VARIABLE_HEADER* EndPtr;
    VARIABLE_HEADER* deletedVariable;

    //
    // Find the variable by enumerating through the variable structures.
    //
    for(
        deletedVariable = NULL,
        CurrPtr = GetStartPointer(mVariableStore),
        EndPtr = GetEndPointer(mVariableStore);
        (CurrPtr < EndPtr) && IsValidVariableHeader(CurrPtr);
        CurrPtr = GetNextVariablePtr(CurrPtr)
       )
    {
        if (CurrPtr->State == VAR_ADDED ||
            CurrPtr->State == (VAR_IN_DELETED_TRANSITION & VAR_ADDED))
        {
            if (VariableName[0] == 0)
            {
                //
                // Looking for first variable.
                //
                if (CurrPtr->State == (VAR_IN_DELETED_TRANSITION & VAR_ADDED))
                {
                    deletedVariable = CurrPtr;
                }
                else
                {
                    *Variable = CurrPtr;
                    return EFI_SUCCESS;
                }
            }
            else
            {
                //
                // Looking for a specific variable.
                //
                if (CompareGuid(VendorGuid, &CurrPtr->VendorGuid))
                {
                    ASSERT(NameSizeOfVariable(CurrPtr) != 0);
                    if (CompareMem(VariableName,
                                   (void *)GetVariableNamePtr(CurrPtr),
                                   NameSizeOfVariable(CurrPtr)) == 0)
                    {
                        if (CurrPtr->State ==
                            (VAR_IN_DELETED_TRANSITION & VAR_ADDED))
                        {
                            deletedVariable = CurrPtr;
                        }
                        else
                        {
                            *Variable = CurrPtr;
                            return EFI_SUCCESS;
                        }
                    }
                }
            }
        }
    }

    //
    // Nothing found or VAR_IN_DELETED_TRANSITION found.
    //
    *Variable =  deletedVariable;

    return (*Variable == NULL) ? EFI_NOT_FOUND : EFI_SUCCESS;
}


EFI_STATUS
UpdateVariable (
  IN            CHAR16*          VariableName,
  IN            EFI_GUID*        VendorGuid,
  IN            VOID*            Data,
  IN            UINTN            DataSize,
  IN            UINT32           Attributes,
  IN OPTIONAL   VARIABLE_HEADER* Variable
  )
/*++

Routine Description:

    Updates the volatile store with variable information

Arguments:

    VariableName - A Null-terminated string that is the name of the vendor's variable.

    VendorGuid - A unique identifier for the vendor.

    Attributes - Attributes bitmask to set for the variable.

    DataSize - The size in bytes of the Data buffer. Unless the
               EFI_VARIABLE_APPEND_WRITE or EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS
               attribute is set, a size of zero causes the variable to be deleted.
               When the EFI_VARIABLE_APPEND_WRITE attribute is set, then a SetVariable call
               with a DataSize of zero will not cause any change to the variable value (the
               timestamp associated with the variable my be updated).

    Data - The contents for the variable.

    Variable - Variable header pointer that contains existing variable information.

Returns:


--*/
{
    VARIABLE_HEADER* newVariable;
    UINTN newVariableSize;
    UINTN scratchSize;
    UINTN scratchDataSize;
    UINTN nameOffset;
    UINTN nameSize;
    UINTN dataOffset;

    //
    // Checks that apply only to volatile store regardless of existing variable or not.
    //
    // Neither authentication scheme supported for volatile variables.
    //
    if ((Attributes & (EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS |
                       EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS)) != 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Reject non-volatile.
    //
    if ((Attributes & EFI_VARIABLE_NON_VOLATILE) != 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    if (Variable != NULL)
    {
        //
        // This is an update or delete of an existing variable.
        //
        if (EfiAtRuntime())
        {
            //
            // Volatile variable are read-only at runtime by definition.
            //
            return EFI_WRITE_PROTECTED;
        }

        //
        // Setting a variable with no access attributes or zero DataSize
        // causes it to be deleted.  However if the EFI_VARIABLE_APPEND_WRITE
        // is set a DataSize of zero will not delete the variable.
        //
        if ((((Attributes & EFI_VARIABLE_APPEND_WRITE) == 0) && (DataSize == 0)) ||
            ((Attributes & (EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS)) == 0))
        {
            Variable->State &= VAR_DELETED;
            return EFI_SUCCESS;
        }

        //
        // If the same data has been passed in and not APPEND_WRITE then
        // return to the caller immediately.
        //
        if (DataSizeOfVariable(Variable) == DataSize &&
            (CompareMem(Data, GetVariableDataPtr(Variable), DataSize) == 0) &&
            ((Attributes & EFI_VARIABLE_APPEND_WRITE) == 0))
        {
            return EFI_SUCCESS;
        }

        //
        // EFI_VARIABLE_APPEND_WRITE means append new data to existing
        //
        if ((Attributes & EFI_VARIABLE_APPEND_WRITE) != 0)
        {
            //
            // Check if new size will be too big.
            //
            if ((Variable->DataSize + DataSize) > EFI_MAX_VARIABLE_DATA_SIZE)
            {
                return EFI_OUT_OF_RESOURCES;
            }

            //
            // Copy existing data to scratch buffer.
            //
            GetVariableDataPtr(Variable);

            dataOffset = sizeof(VARIABLE_HEADER) + Variable->NameSize +
                            GET_PAD_SIZE(Variable->NameSize);
            CopyMem(mAppendBuffer,
                    (UINT8*)((UINTN)Variable + dataOffset),
                    Variable->DataSize);

            //
            // Append the new data on the end.
            //
            CopyMem((UINT8*)((UINTN)mAppendBuffer + Variable->DataSize),
                        Data,
                        DataSize);

            //
            // Override the Data ptr and DataSize to the combined data.
            //
            Data = mAppendBuffer;
            DataSize = Variable->DataSize + DataSize;
        }

        //
        // Mark the existing variable in deleted transition.
        //
        Variable->State &= VAR_IN_DELETED_TRANSITION;
    }
    else
    {
        //
        // Not an existing variable. This is a new variable.
        //

        //
        // DataSize of zero and APPEND_WRITE set is a no-op for non-existing variable.
        //
        if ((DataSize == 0) && ((Attributes & EFI_VARIABLE_APPEND_WRITE) != 0))
        {
            return EFI_SUCCESS;
        }

        //
        // Setting a data variable with zero DataSize or no access attributes means to delete it.
        // There is no variable to delete.
        //
        if (DataSize == 0 ||
           (Attributes & (EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS)) == 0)
        {
            return EFI_NOT_FOUND;
        }

        //
        // Volatile variable cannot be created at Runtime.
        //
        if (EfiAtRuntime())
        {
            return EFI_INVALID_PARAMETER;
        }
    }

    //
    // Now update variable or create new variable.
    //
    // Use scratch data area at the end as temporary storage for the new/updated variable.
    //
    newVariable = GetEndPointer(mVariableStore);
    scratchSize = STORE_SCRATCH_SIZE;
    scratchDataSize = scratchSize - sizeof(VARIABLE_HEADER) -
                      StrSize(VariableName) - GET_PAD_SIZE(StrSize(VariableName));

    SetMem(newVariable, scratchSize, 0xff);

    newVariable->StartId  = VARIABLE_DATA;
    //
    // Intentionally not setting State to VAR_ADDED;
    //
    newVariable->Reserved = 0;

    //
    // Don't store the APPEND_WRITE bit.
    //
    newVariable->Attributes  = Attributes & (~EFI_VARIABLE_APPEND_WRITE);

    //
    // Copy name, data, and GUID.
    //
    nameOffset = sizeof(VARIABLE_HEADER);
    nameSize = StrSize(VariableName);
    CopyMem((UINT8 *)((UINTN)newVariable + nameOffset),  VariableName, nameSize);
    dataOffset = nameOffset + nameSize + GET_PAD_SIZE(nameSize);
    CopyMem((UINT8 *)((UINTN)newVariable + dataOffset),  Data, DataSize);
    CopyMem (&newVariable->VendorGuid, VendorGuid, sizeof(EFI_GUID));

    //
    // There will be pad bytes after Data, the nextVariable->NameSize and
    // nextVariable->DataSize should not include pad size so that variable
    // service can get actual size in GetVariable.
    //
    newVariable->NameSize = (UINT32)nameSize;
    newVariable->DataSize = (UINT32)DataSize;

    //
    // The actual size of the variable in storage should include pad size.
    //
    newVariableSize = dataOffset + DataSize + GET_PAD_SIZE(DataSize);

    //
    // Reclaim space if necessary.
    //
    if ((UINT32)(newVariableSize + mStoreFreeOffset) > STORE_MAIN_SIZE)
    {
        Reclaim();

        //
        // If still not enough space, return out of resources.
        //
        if ((UINT32)(newVariableSize + mStoreFreeOffset) > STORE_MAIN_SIZE)
        {
            return EFI_OUT_OF_RESOURCES;
        }
    }

    //
    // Now set the new variable state to VAR_ADDED.
    //
    newVariable->State = VAR_ADDED;

    //
    // Copy the new variable to the free space in the store.
    //
    CopyMem((VOID*)((UINTN)mVariableStore + mStoreFreeOffset), (VOID*)newVariable, newVariableSize);
    mStoreFreeOffset += newVariableSize;

    //
    // Mark the old variable as deleted.
    //
    if (Variable != NULL)
    {
        Variable->State &= VAR_DELETED;
    }

    return EFI_SUCCESS;
}


BOOLEAN
IsReadOnlyVariable(
    IN  CHAR16   *VariableName,
    IN  EFI_GUID *VendorGuid
    )
/*++

Routine Description:

    Given a Variable Name and Vendor Guid returns a boolean
    indicating if that variable is read-only.

Arguments:

    VariableName - A Null-terminated string that is the name of the vendor's variable.

    VendorGuid - A unique identifier for the vendor.

Returns:

    TRUE if variable is read-only. FALSE otherwise.

--*/
{
    if (CompareGuid(VendorGuid, &gEfiGlobalVariableGuid))
    {
        if ((StrCmp(VariableName, EFI_SETUP_MODE_NAME) == 0) ||
            (StrCmp(VariableName, EFI_SIGNATURE_SUPPORT_NAME) == 0) ||
            (StrCmp(VariableName, EFI_SECURE_BOOT_MODE_NAME) == 0) ||
            (StrCmp(VariableName, EFI_DB_DEFAULT_VARIABLE_NAME) == 0))
        {
            return TRUE;
        }
    }
    return FALSE;
}


EFI_STATUS
VariableInitialize(
    )
/*++

Routine Description:

    Initialize the volatile variable store.

Arguments:

    None.

Returns:

    EFI_STATUS.

--*/
{
    EFI_STATUS status = EFI_SUCCESS;

    mVariableStore = NULL;
    mAppendBuffer = NULL;

    //
    // Allocate memory for volatile store.
    //
    mVariableStore =
        (VARIABLE_STORE_HEADER*)AllocateRuntimePool(STORE_MAIN_SIZE + STORE_SCRATCH_SIZE);
    if (mVariableStore == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    //
    // Allocate memory for append write scratch buffer.
    //
    mAppendBuffer  = AllocateRuntimePool(EFI_MAX_VARIABLE_DATA_SIZE);
    if (mAppendBuffer == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    //
    // Init the memory store.
    //
    SetMem(mVariableStore, STORE_MAIN_SIZE + STORE_SCRATCH_SIZE, 0xff);
    mStoreFreeOffset = (UINTN)GetStartPointer(mVariableStore) - (UINTN)mVariableStore;

Cleanup:

    if (EFI_ERROR(status))
    {
        if (mVariableStore != NULL)
        {
            FreePool(mVariableStore);
        }
        if (mAppendBuffer != NULL)
        {
            FreePool(mAppendBuffer);
        }
    }
    return status;
}


EFI_STATUS
VariableCommonInitialize(
    )
/*++

Routine Description:

    Initializes both the non-volatile and volatile variable stores.

Arguments:

    None.

Returns:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;

    status = NvramInitialize();
    if (EFI_ERROR(status))
    {

        ASSERT_EFI_ERROR(status);
        return status;
    }
    status = VariableInitialize();
    if (EFI_ERROR(status))
    {

        ASSERT_EFI_ERROR(status);
        return status;
    }

    return EFI_SUCCESS;
}


//
// Event callbacks
//


VOID
EFIAPI
ExitBootServicesHandler(
    IN  EFI_EVENT Event,
    IN  void*     Context
    )
/*++

Routine Description:

    Called when the Exit Boot Services event is signalled.

Arguments:

    Event - Event.

    Context - Context.

Returns:

--*/
{
    EFI_GUID hyperVGuid = HYPERV_PRIVATE_NAMESPACE;
    EFI_STATUS status;
    UINT32 supportedIndications;
    VARIABLE_HEADER* variable;
    BOOLEAN vsmAware;

    vsmAware = FALSE;

    //
    // Fetch necessary state for exitbootservices from volatile store.
    //
    status = FindVariable(OSLOADER_INDICATIONS_NAME,
                          &hyperVGuid,
                          &variable);

    if ((status == EFI_SUCCESS) &&
        (variable != NULL) &&
        (DataSizeOfVariable(variable) == sizeof (UINT32)))
    {
        supportedIndications = *(UINT32*)GetVariableDataPtr(variable);
        vsmAware = (supportedIndications & 1);
    }

    //
    // Signal the NVRAM store.
    //
    NvramExitBootServicesHandler(vsmAware);
}


VOID
EFIAPI
VirtualAddressChangeHandler(
    IN  EFI_EVENT Event,
    IN  void*     Context
    )
/*++

Routine Description:

    Called when the Virtual Address Change event is signalled.

Arguments:

    Event - Event.

    Context - Context.

Returns:

--*/
{
    //
    // Signal the NVRAM store.
    //
    NvramAddressChangeHandler();

    //
    // Update volatile store pointers.
    //
    EfiConvertPointer(0, (void**) &mVariableStore);
}


//
// Variable Service routines.
//


EFI_STATUS
EFIAPI
VariableServiceGetVariable(
    IN              CHAR16*          VariableName,
    IN              EFI_GUID*        VendorGuid,
    OUT OPTIONAL    UINT32* Attributes,
    IN OUT          UINTN*           DataSize,
    OUT             void*            Data
    )
/*++

Routine Description:

    Finds variable in either the volatile store or the NVRAM store.

Arguments:

    VariableName - A null-terminated string that is the name of the vendor's variable.

    VendorGuid - A unique identifier for the vendor.

    Attributes - If not NULL, receives the attribute bitmask of the variable.

    DataSize - On input is the size of the Data buffer.
               On output receives the size of the returned data in the Data buffer.
               If the return value is EFI_BUFFER_TOO_SMALL this receives the size
               needed to complete the request.

    Data - The buffer to return the contents of the variable.

Returns:

    EFI_SUCCESS - The function completed successfully.

    EFI_NOT_FOUND - The variable was not found.

    EFI_INVALID_PARAMETER - The VariableName was NULL.
    EFI_INVALID_PARAMETER - The VendorGuid was NULL.
    EFI_INVALID_PARAMETER - The DataSize was NULL.
    EFI_INVALID_PARAMETER - The DataSize is not too small and Data is NULL.

    EFI_DEVICE_ERROR - The variable could not be retreived due to a hardware error.
--*/
{
    UINTN dataSize;
    EFI_STATUS status;
    VARIABLE_HEADER* variable;


    if ((VariableName == NULL) || (VendorGuid == NULL) || (DataSize == NULL))
    {
        return EFI_INVALID_PARAMETER;
    }

    if (Data == NULL && *DataSize != 0)
    {
        // This is a symptom of usually someone calling GetVariable with DataSize
        // uninitialized. Regardless, this is just the caller being wrong, which
        // is always a bug.
        //
        // Assert false here to help find those bugs on debug builds.
        ASSERT(FALSE);
        return EFI_INVALID_PARAMETER;
    }

#if !defined(MDEPKG_NDEBUG)
    NvramDebugLog("GetVariable for '%s' DataSize 0x%x", VariableName, *DataSize);
#endif

    //
    // First check the volatile store
    //
    status = FindVariable(VariableName, VendorGuid, &variable);
    if (variable == NULL || EFI_ERROR(status))
    {
        //
        // Not found so dispatch to the non-volatile store.
        //
        return NvramGetVariable(VariableName, VendorGuid, Attributes, DataSize, Data);
    }

    //
    // Have volatile variable.
    //
    dataSize = variable->DataSize;

    //
    // Check if room.
    //
    if (*DataSize < dataSize)
    {
        *DataSize = dataSize;
        return EFI_BUFFER_TOO_SMALL;
    }

    //
    // Check if no buffer supplied.
    //
    if (Data == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Output the variable.
    //
    CopyMem(Data, GetVariableDataPtr(variable), dataSize);
    if (Attributes != NULL)
    {
        *Attributes = variable->Attributes;
    }

    *DataSize = dataSize;
    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
VariableServiceGetNextVariableName(
    IN OUT  UINTN    *VariableNameSize,
    IN OUT  CHAR16   *VariableName,
    IN OUT  EFI_GUID *VendorGuid
     )
/*++

Routine Description:

    Enumerates the current variable names.

    Start the enumeration with a call to this function supplying an
    empty string in VariableName.

    Get the next variable by supplying the VariableName and VendorGuid that
    was returned in the previous call.

Arguments:

    VariableNameSize - The size fo the VariableName buffer.

    VariableName - On input supplies the last VariableName that was
                   returned by this function.
                   On output receives the null-terminated string of the
                   variable following the one input.

    VendorGuid - On input supplies the last VendorGuidthat was returned
                 by this function.
                 On output receives the VendorGuid of the variable
                 following the one input.

Returns:

    EFI_SUCCESS - The function completed successfully
    EFI_NOT_FOUND - The next variable was not found, i.e., the end of the enumeration.
    EFI_BUFFER_TOO_SMALL - The VariableNameSize was too small for the result.
                           VariableNameSize received the size needed to complete the
                           request.
    EFI_INVALID_PARAMETER - VariableNameSize is Null.
    EFI_INVALID_PARAMETER - VariableName is Null.
    EFI_INVALID_PARAMETER - VendorGuid is Null.
    EFI_DEVICE_ERROR - Failure due to hardware device error.

--*/
{
    EFI_STATUS status;
    VARIABLE_HEADER* variable;
    UINT32 varNameSize;
    static CHAR16 emptyName[1];


    if (VariableNameSize == NULL || VariableName == NULL || VendorGuid == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

#if !defined(MDEPKG_NDEBUG)
    NvramDebugLog("GetNextVariable for '%s'", VariableName);
#endif

    //
    // First check the non-volatile store for the requested variable.
    //
    status = FindVariable(VariableName, VendorGuid, &variable);
    if (variable == NULL || EFI_ERROR(status))
    {
        //
        // Volatile store is empty or caller has enumerated past its end.
        //
        // Dispatch to the non-volatile store.
        //
        return NvramGetNextVariableName(VariableNameSize, VariableName, VendorGuid);
    }

    //
    // Variable was found in the volatile store.
    //
    // If input variable name is not the empty string then get the next variable in volatile store.
    //
    if (VariableName[0] != 0)
    {
      variable = GetNextVariablePtr(variable);
    }

    while(TRUE)
    {
        //
        // If at end of volatile store dispatch to non-volatile store to get first variable there.
        //
        if (variable >= GetEndPointer(mVariableStore) || variable == NULL)
        {
            return NvramGetFirstVariableName(VariableNameSize, VariableName, VendorGuid);
        }

        //
        // Check current store pointer.
        //
        if (IsValidVariableHeader(variable) && ((variable->State == VAR_ADDED)))
        {
            //
            // Have a valid variable.  Check runtime access.
            //
            if (!(EfiAtRuntime() && !(variable->Attributes & EFI_VARIABLE_RUNTIME_ACCESS)))
            {
                //
                // OK to output.
                //
                varNameSize = variable->NameSize;
                if (varNameSize <= *VariableNameSize)
                {
                    CopyMem(VariableName, GetVariableNamePtr(variable), varNameSize);
                    CopyMem(VendorGuid, &variable->VendorGuid, sizeof(EFI_GUID));
                    status = EFI_SUCCESS;
                }
                else
                {
                    status = EFI_BUFFER_TOO_SMALL;
                }

                *VariableNameSize = varNameSize;
                return status;
            }
        }

        //
        // Next volatile variable.
        //
        variable = GetNextVariablePtr(variable);
    }

    return EFI_NOT_FOUND;
}


EFI_STATUS
EFIAPI
VariableServiceSetVariable(
    IN  CHAR16*   VariableName,
    IN  EFI_GUID* VendorGuid,
    IN  UINT32    Attributes,
    IN  UINTN     DataSize,
    IN  void*     Data

    )
/*++

Routine Description:

    Sets the value of a variable.

Arguments:

    VariableName - A Null-terminated string that is the name of the vendor's variable.

    VendorGuid - A unique identifier for the vendor.

    Attributes - Attributes bitmask to set for the variable.

    DataSize - The size in bytes of the Data buffer. Unless the
               EFI_VARIABLE_APPEND_WRITE or EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS
               attribute is set, a size of zero causes the variable to be deleted.
               When the EFI_VARIABLE_APPEND_WRITE attribute is set, then a SetVariable call
               with a DataSize of zero will not cause any change to the variable value (the
               timestamp associated with the variable my be updated).

    Data - The contents for the variable.

Returns:

    EFI_SUCCESS - Variable was successfully stored as defined by its attributes.

    EFI_INVALID_PARAMETER - An invalid combination of attribute bits was supplied or
                            the DataSize exceeds the maximum allowed.

    EFI_INVALID_PARAMETER - VariableName is an empty string.

    EFI_OUT_OF_RESOURCES - Not enough storage is available to hold the variable and its data.

    EFI_DEVICE_ERROR - Variable can not be saved due to hardware failure.

    EFI_WRITE_PROTECTED - The variable is read-only.

    EFI_WRITE_PROTECTED - The variable cannot be deleted.

    EFI_SECURITY_VIOLATION - The variable could not be written due to
                             EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS
                             being set but the supplied authentication info did
                             not pass the validation check.

    EFI_NOT_FOUND - The variable to be deleted does not exist.

--*/
{
    VARIABLE_HEADER* variable;
    BOOLEAN volatileExists = FALSE;
    UINTN payloadSize;
    EFI_STATUS status;


    //
    // Check input parameters.
    //
    if (VariableName == NULL || VariableName[0] == 0 || VendorGuid == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

#if !defined(MDEPKG_NDEBUG)
    NvramDebugLog("SetVariable for '%s' Attr 0x%x DataSize 0x%x",
        VariableName, Attributes, DataSize);
#endif

    //
    // Check for read-only variables.
    //
    if (IsReadOnlyVariable (VariableName, VendorGuid))
    {
        return EFI_WRITE_PROTECTED;
    }

    //
    // Must supply data if size is non-zero.
    //
    if (DataSize != 0 && Data == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Check for reserved bits in variable attribute.
    //
    if ((Attributes & (~EFI_VARIABLE_ATTRIBUTES_MASK)) != 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    //
    //  If RT is set then BS must be also.
    //
    if ((Attributes & (EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS))
        == EFI_VARIABLE_RUNTIME_ACCESS)
    {
        return EFI_INVALID_PARAMETER;
    }

    //
    // The authentication attributes cannot both be set.
    //
    if (((Attributes & EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS) != 0) &&
       ((Attributes & EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) != 0))
    {
        return EFI_INVALID_PARAMETER;
    }

    //
    // EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS is simply not supported.
    //
    if ((Attributes & EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS) != 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Sanity check for EFI_VARIABLE_AUTHENTICATION_2 descriptor.
    //
    if ((Attributes & EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) != 0)
    {
        if (DataSize < OFFSET_OF_AUTHINFO2_CERT_DATA ||
            ((EFI_VARIABLE_AUTHENTICATION_2 *)Data)->AuthInfo.Hdr.dwLength >
                DataSize - (OFFSET_OF(EFI_VARIABLE_AUTHENTICATION_2, AuthInfo)) ||
            ((EFI_VARIABLE_AUTHENTICATION_2 *) Data)->AuthInfo.Hdr.dwLength <
                OFFSET_OF(WIN_CERTIFICATE_UEFI_GUID, CertData))
        {
            return EFI_SECURITY_VIOLATION;
        }
        payloadSize = DataSize - AUTHINFO2_SIZE(Data);
    }
    else
    {
        payloadSize = DataSize;
    }

    //
    // Check name and data do not exceed maximums.
    //
    if ((payloadSize > EFI_MAX_VARIABLE_DATA_SIZE) ||
        (StrSize(VariableName) > EFI_MAX_VARIABLE_NAME_SIZE))
    {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Check if the variable already exists in the volatile store.
    //
    status = FindVariable(VariableName, VendorGuid, &variable);
    if (!EFI_ERROR(status))
    {
        volatileExists = TRUE;
    }

    //
    // Refuse to do authentication on an existing volatile variable.
    //
    if (volatileExists &&
       ((Attributes & EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) != 0))
    {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Dispatch to volatile store if it already exists there.
    // Dispatch to volatile store if new volatile variable with actual data.
    //
    if (volatileExists ||
        (((Attributes & EFI_VARIABLE_NON_VOLATILE) == 0) &&
        (DataSize > 0) &&
        (((Attributes & EFI_VARIABLE_BOOTSERVICE_ACCESS) != 0) ||
         ((Attributes & EFI_VARIABLE_RUNTIME_ACCESS) != 0))
        ))
    {
        return UpdateVariable(VariableName, VendorGuid, Data, DataSize, Attributes, variable);
    }

    //
    // Dispatch to non-volatile store.
    //
    return NvramSetVariable(VariableName, VendorGuid, Attributes, DataSize, Data);
 }


EFI_STATUS
EFIAPI
VariableServiceQueryVariableInfo(
        UINT32  Attributes,
    OUT UINT64* MaximumVariableStorageSize,
    OUT UINT64* RemainingVariableStorageSize,
    OUT UINT64* MaximumVariableSize
    )
/*++

Routine Description:

    Returns information about the EFI variables.

Arguments:

    Attributes - Attributes bitmask to specify the type of variables
                 on which to return information.
    MaximumVariableStorageSize - Receives the maximum size of the storage space available
                                 for the EFI variables associated with the attributes specified.
    RemainingVariableStorageSize - Receives the remaining size of the storage space available
                                   for EFI variables associated with the attributes specified.
    MaximumVariableSize - Receives the maximum size of an individual EFI variable
                          associated with the attributes specified.

Returns:

    EFI_INVALID_PARAMETER - An invalid combination of attribute bits was supplied.

    EFI_SUCCESS - Query successfully.

    EFI_UNSUPPORTED - The attribute is not supported on this platform.

--*/
{
    VARIABLE_HEADER* variable;
    VARIABLE_HEADER* nextVariable;
    UINT64 variableSize;

#if !defined(MDEPKG_NDEBUG)
    NvramDebugLog("QueryVariableInfo Attr 0x%x", Attributes);
#endif

    if ((MaximumVariableStorageSize == NULL) ||
        (RemainingVariableStorageSize == NULL) ||
        (MaximumVariableSize == NULL) ||
        (Attributes == 0))
    {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Validate Attributes.
    //
    if ((Attributes &  (EFI_VARIABLE_NON_VOLATILE |
                        EFI_VARIABLE_BOOTSERVICE_ACCESS |
                        EFI_VARIABLE_RUNTIME_ACCESS)) == 0)
    {
        //
        // One of (NV, BS, RT) has to be set.
        //
        return EFI_UNSUPPORTED;
    }
    else if ((Attributes & (EFI_VARIABLE_RUNTIME_ACCESS |
                             EFI_VARIABLE_BOOTSERVICE_ACCESS)) ==
                            EFI_VARIABLE_RUNTIME_ACCESS)
    {
        //
        // BS must be set if RT is set.
        //
        return EFI_INVALID_PARAMETER;
    }
    else if (EfiAtRuntime() && !(Attributes & EFI_VARIABLE_RUNTIME_ACCESS))
    {
        //
        // Make sure RT is set if we are in Runtime phase.
        //
        return EFI_INVALID_PARAMETER;
    }

    //
    // Dispatch to non-volatile store if non-volatile requested.
    //
    if ((Attributes & EFI_VARIABLE_NON_VOLATILE) != 0)
    {
        return NvramQueryInfo(Attributes,
                              MaximumVariableStorageSize,
                              RemainingVariableStorageSize,
                              MaximumVariableSize);
    }

    //
    // MaximumVariableStorageSize is the total storage.
    //
    *MaximumVariableStorageSize = STORE_MAIN_SIZE;

    //
    // MaximumVariableSize is initially the name and data max sizes.
    //
    *MaximumVariableSize = EFI_MAX_VARIABLE_NAME_SIZE + EFI_MAX_VARIABLE_DATA_SIZE;

    //
    // Point to the starting address of the variables.
    //
    variable = GetStartPointer(mVariableStore);

    //
    // Now walk through the variable store reducing remaining by existing variable sizes.
    //
    while ((variable < GetEndPointer(mVariableStore)) && IsValidVariableHeader(variable))
    {
        nextVariable = GetNextVariablePtr(variable);
        variableSize = (UINT64)(UINTN)nextVariable - (UINT64)(UINTN)variable;

        if (EfiAtRuntime())
        {
            //
            // At runtime count size of all variables since nothing will be reclaimed.
            //
            *RemainingVariableStorageSize -= variableSize;
        }
        else
        {
            //
            // Before runtime count only variables with VAR_ADDED as the rest can be reclaimed.
            //
            if (variable->State == VAR_ADDED)
            {
                *RemainingVariableStorageSize -= variableSize;
            }
        }

        variable = nextVariable;
    }

    if (*RemainingVariableStorageSize < sizeof(VARIABLE_HEADER))
    {
        *MaximumVariableSize = 0;
    }
    else if ((*RemainingVariableStorageSize - sizeof(VARIABLE_HEADER)) < *MaximumVariableSize)
    {
        *MaximumVariableSize = *RemainingVariableStorageSize - sizeof(VARIABLE_HEADER);
    }

    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
VariableServiceInitialize(
    IN  EFI_HANDLE        ImageHandle,
    IN  EFI_SYSTEM_TABLE* SystemTable
    )
/*++

Routine Description:

    Variable Services Driver main entry point.

    This function:

    - Initializes the volatile and non-volatile stores
    - Places the 4 EFI runtime variable services in the EFI System Table
    - Installs the Variable Architectural Tag Protocol to indicate variable
      services are available.
    - Registers a notification function for the EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE event.
    - Registers a notification function for the EVT_SIGNAL_EXIT_BOOT_SERVICES event.

    This implementation does not leverage the ReadyToBootEvent to reclaim
    variable storage space before boot.

Arguments:

    ImageHandle - the firmware allocated handle for the EFI image containing this driver.

    SystemTable - A pointer to the EFI System Table.

Returns:

    EFI_STATUS.

--*/
{
    EFI_STATUS status = EFI_SUCCESS;

    status = VariableCommonInitialize();
    ASSERT_EFI_ERROR(status);
    if (EFI_ERROR(status))
    {
        return status;
    }

    //
    // Install the services into the system table.
    //
    SystemTable->RuntimeServices->GetVariable         = VariableServiceGetVariable;
    SystemTable->RuntimeServices->GetNextVariableName = VariableServiceGetNextVariableName;
    SystemTable->RuntimeServices->SetVariable         = VariableServiceSetVariable;
    SystemTable->RuntimeServices->QueryVariableInfo   = VariableServiceQueryVariableInfo;

    //
    // Register a function to update addresses when page tables are changed.
    //
    status = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,
                              TPL_NOTIFY,
                              VirtualAddressChangeHandler,
                              NULL,
                              &gEfiEventVirtualAddressChangeGuid,
                              &mVirtualAddressChangeEvent);
    ASSERT_EFI_ERROR(status);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Register notify function for EVT_SIGNAL_EXIT_BOOT_SERVICES.
    //
    status = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,
                              TPL_NOTIFY,
                              ExitBootServicesHandler,
                              NULL,
                              &gEfiEventExitBootServicesGuid,
                              &mExitBootServicesEvent);
    ASSERT_EFI_ERROR(status);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Install the Variable Runtime Architectural protocol on a new handle.
    //
    mHandle = NULL;
    status = gBS->InstallMultipleProtocolInterfaces(&mHandle,
                                                    &gEfiVariableArchProtocolGuid,
                                                    NULL,
                                                    &gEfiVariableWriteArchProtocolGuid,
                                                    NULL,
                                                    NULL);
    ASSERT_EFI_ERROR(status);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

Cleanup:

    if (EFI_ERROR(status))
    {
        if (mVirtualAddressChangeEvent != NULL) {
            gBS->CloseEvent (mVirtualAddressChangeEvent);
        }

        if (mExitBootServicesEvent != NULL) {
            gBS->CloseEvent (mExitBootServicesEvent);
        }
    }

    return status;
}
