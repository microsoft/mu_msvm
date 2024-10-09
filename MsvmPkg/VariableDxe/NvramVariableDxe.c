/** @file
  NVRAM Variable Services driver.

  The module acts as a proxy and sends non-volatile variable requests
  to the Hyper-V BiosDevice.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/


#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BiosDeviceLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>

#include <BiosInterface.h>
#include <NvramVariableDxe.h>
#include <IsolationTypes.h>

#define WITHIN_4_GB_LL (0xFFFFFFFFLL)

//
// Events this driver handles
//
static EFI_EVENT mVirtualAddressChangeEvent          = NULL;


//
// Descriptor and Data buffers.
//
static EFI_PHYSICAL_ADDRESS        mNvramCommandDescriptorGpa   = 0;
static PNVRAM_COMMAND_DESCRIPTOR   mNvramCommandDescriptor      = NULL;
static EFI_PHYSICAL_ADDRESS        mNvramCommandDataBufferGpa   = 0;
static UINT8*                      mNvramCommandDataBuffer      = NULL;

//
// NVRAM is not allowed on Hardware Isolated systems without a paravisor (even if a bios emulator is present).
// In a hardware isolated system the host is not part of the TCB thus we should not depend on host for NVRAM
// information and all calls should fail appropriately.
//

static BOOLEAN mNvramNotAllowed = FALSE;

//
// Private routines
//

EFI_STATUS
IssueBiosDeviceNvramCommand()
/*++

Routine Description:

    Performs an enlightened NVRAM command through the BiosDevice.

Arguments:

    None.

Returns:

    EFI_STATUS

--*/
{
    if (mNvramNotAllowed)
    {
        return EFI_UNSUPPORTED;
    }

    //
    // Worker process can fail guest state related requests due to
    // storage being temporarily "not ready". Retry until success or
    // fatal error.
    //
    for (;;)
    {
        //
        // Send the request to the BiosDevice.
        // Cast of descriptor GPA is safe as it is allocated below 4GB.
        //
        WriteBiosDevice(BiosConfigNvramCommand, (UINT32)mNvramCommandDescriptorGpa);

        if (mNvramCommandDescriptor->Status == EFI_SUCCESS)
        {
            return EFI_SUCCESS;
        }
        //
        // Worker process returns unencoded error values.
        //
        if (ENCODE_ERROR(mNvramCommandDescriptor->Status) != EFI_NOT_READY)
        {
            return ENCODE_ERROR(mNvramCommandDescriptor->Status);
        }
    }
}


//
// Entry points
//


EFI_STATUS
NvramInitialize(
    )
/*++

Routine Description:

    Initializes this module.

Arguments:

    None.

Return Value:

    TRUE on successful initialization.

--*/
{
    EFI_STATUS status = EFI_SUCCESS;

    if (IsHardwareIsolatedNoParavisor())
    {
        mNvramNotAllowed = TRUE;

        // No allocations needed as we are not allowed to send NVRAM commands.
        return EFI_SUCCESS;
    }

#define BELOW_4GB (0xFFFFFFFFULL)

    //
    // Allocate the descriptor from physcial memory below 4GB.
    //
    mNvramCommandDescriptorGpa = BELOW_4GB;
    status = gBS->AllocatePages(AllocateMaxAddress,
                                EfiRuntimeServicesData,
                                EFI_SIZE_TO_PAGES(sizeof(NVRAM_COMMAND_DESCRIPTOR)),
                                &mNvramCommandDescriptorGpa);
    if (EFI_ERROR(status))
    {

        mNvramCommandDescriptorGpa = 0;
        goto Cleanup;
    }

    //
    // Allocate the name/data buffer from physcial memory below 4GB.
    //
    mNvramCommandDataBufferGpa = BELOW_4GB;
    status = gBS->AllocatePages(AllocateMaxAddress,
                                EfiRuntimeServicesData,
                                EFI_SIZE_TO_PAGES(EFI_MAX_VARIABLE_NAME_SIZE + EFI_MAX_VARIABLE_DATA_SIZE),
                                &mNvramCommandDataBufferGpa);
    if (EFI_ERROR(status))
    {

        mNvramCommandDataBufferGpa = 0;
        goto Cleanup;
    }

    //
    // Addresses are identity mapped before runtime.  GVA == GPA at this point.
    //
    mNvramCommandDescriptor = (PNVRAM_COMMAND_DESCRIPTOR)(UINTN)mNvramCommandDescriptorGpa;
    mNvramCommandDataBuffer = (UINT8*)(UINTN)mNvramCommandDataBufferGpa;

#if defined(MDE_CPU_AARCH64)

    //
    // The MMIO registers for the BIOS device must be declared as runtime so they are
    // included in the guest os call to SetVirtualAddressMap and can be converted to a GVA.
    // While there is a centralized BiosDeviceBaseLib, we can't do the AddMemorySpace there since
    // only _one_ driver can add the memory space, and that constructor is called by each
    // driver that includes that lib. This one wins and registers the memory space.
    //
    status = gDS->AddMemorySpace(EfiGcdMemoryTypeMemoryMappedIo,
                                 PcdGet32(PcdBiosBaseAddress),
                                 EFI_PAGE_SIZE,
                                 EFI_MEMORY_UC | EFI_MEMORY_RUNTIME);
    ASSERT_EFI_ERROR(status);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = gDS->SetMemorySpaceAttributes(PcdGet32(PcdBiosBaseAddress),
                                           EFI_PAGE_SIZE,
                                           EFI_MEMORY_UC | EFI_MEMORY_RUNTIME);
    ASSERT_EFI_ERROR(status);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }
#endif

Cleanup:

    if (EFI_ERROR(status))
    {
        if (mNvramCommandDataBufferGpa != 0)
        {

            gBS->FreePages(
                mNvramCommandDataBufferGpa,
                EFI_SIZE_TO_PAGES(EFI_MAX_VARIABLE_NAME_SIZE + EFI_MAX_VARIABLE_DATA_SIZE));
            mNvramCommandDataBufferGpa = 0;
        }
        if (mNvramCommandDescriptorGpa != 0)
        {

            gBS->FreePages(
                mNvramCommandDescriptorGpa,
                EFI_SIZE_TO_PAGES(sizeof(NVRAM_COMMAND_DESCRIPTOR)));
            mNvramCommandDescriptorGpa = 0;
        }
    }

    return status;
}


VOID
NvramAddressChangeHandler()
/*++

Routine Description:

    Callback to handle converting internal pointers due to
    the page tables being updated.

Arguments:

    None.

Return Value:

    None.

--*/
{
    EFI_STATUS status;

    if (mNvramNotAllowed)
    {
        return;
    }

#if 0
    DEBUG((DEBUG_VERBOSE, ">>> %a\n", __FUNCTION__));
    DEBUG((DEBUG_VERBOSE, "--- %a mNvramCommandDescriptor %p\n", __FUNCTION__, mNvramCommandDescriptor));
    DEBUG((DEBUG_VERBOSE, "--- %a mNvramCommandDataBuffer %p\n", __FUNCTION__, mNvramCommandDataBuffer));
#endif

    //
    // Physical addresses (GPA's) won't change.
    // Convert the virtual addresses of the buffers.
    //
    status = EfiConvertPointer(0, (void**)&mNvramCommandDescriptor);
    ASSERT(!EFI_ERROR(status));
    status = EfiConvertPointer(0, (void**)&mNvramCommandDataBuffer);
    ASSERT(!EFI_ERROR(status));

#if 0
    DEBUG((DEBUG_VERBOSE, "--- %a mNvramCommandDescriptor %p\n", __FUNCTION__, mNvramCommandDescriptor));
    DEBUG((DEBUG_VERBOSE, "--- %a mNvramCommandDataBuffer %p\n", __FUNCTION__, mNvramCommandDataBuffer));
    DEBUG((DEBUG_VERBOSE, "<<< %a\n", __FUNCTION__));
#endif
}



VOID
NvramExitBootServicesHandler(
    BOOLEAN VsmAware
    )
/*++

Routine Description:

    Callback to handle when ExitBootServices is called by an application/boot loader.
    This function simply notifies the BiosDevice of this event.

Arguments:

    VsmAware - Supplies a boolean indicating if any boot app opt-ed to leverage
               VSM by setting the necessary bit in OsLoaderIndcationsSupported.

Returns:

    None

--*/
{
    if (mNvramNotAllowed)
    {
        return;
    }

    //DEBUG((DEBUG_VERBOSE, ">>> %a\n", __FUNCTION__));
    ZeroMem(mNvramCommandDescriptor, sizeof(NVRAM_COMMAND_DESCRIPTOR));
    mNvramCommandDescriptor->Command = NvramSignalRuntimeCommand;
    mNvramCommandDescriptor->U.SignalRuntimeCommand.S.VsmAware = VsmAware;
    (void)IssueBiosDeviceNvramCommand();
    //DEBUG((DEBUG_VERBOSE, "<<< %a\n", __FUNCTION__));
}



EFI_STATUS
NvramQueryInfo(
        UINT32  Attributes,
    OUT UINT64* MaximumVariableStorageSize,
    OUT UINT64* RemainingVariableStorageSize,
    OUT UINT64* MaximumVariableSize
    )
/*++

Routine Description:

    This code returns information about the EFI variables.

Arguments:

    Attributes - Attributes bitmask to specify the type of variables
                 on which to return information.
    MaximumVariableStorageSize - Pointer to the maximum size of the storage space available
                                 for the EFI variables associated with the attributes specified.
    RemainingVariableStorageSize - Pointer to the remaining size of the storage space available
                                   for EFI variables associated with the attributes specified.
    MaximumVariableSize - Pointer to the maximum size of an individual EFI variables
                          associated with the attributes specified.

Returns:

    EFI STATUS:

        EFI_INVALID_PARAMETER - An invalid combination of attribute bits was supplied.

        EFI_SUCCESS - Query successfully.

        EFI_UNSUPPORTED - The attribute is not supported on this platform.

--*/
{
    EFI_STATUS status;

    ASSERT(MaximumVariableStorageSize);
    ASSERT(RemainingVariableStorageSize);
    ASSERT(MaximumVariableSize);

    if (mNvramNotAllowed)
    {
        return EFI_DEVICE_ERROR;
    }

    ZeroMem(mNvramCommandDescriptor, sizeof(NVRAM_COMMAND_DESCRIPTOR));
    mNvramCommandDescriptor->Status = EFI_DEVICE_ERROR;
    mNvramCommandDescriptor->Command = NvramQueryInfoCommand;
    mNvramCommandDescriptor->U.QueryInfo.Attributes = Attributes;

    status = IssueBiosDeviceNvramCommand();
    if (status == EFI_SUCCESS)
    {
        *MaximumVariableStorageSize =
            mNvramCommandDescriptor->U.QueryInfo.MaximumVariableStorage;
        *RemainingVariableStorageSize =
            mNvramCommandDescriptor->U.QueryInfo.RemainingVariableStorage;
        *MaximumVariableSize =
            mNvramCommandDescriptor->U.QueryInfo.MaximumVariableSize;
    }
    return status;
}


EFI_STATUS
NvramSetVariable(
    IN  CHAR16*   VariableName,
    OUT EFI_GUID* VendorGuid,
        UINT32    Attributes,
        UINTN     DataSize,
    IN  void*     Data
    )
/*++

Routine Description:

    Sets a NVRAM variable.

Arguments:

    VariableName - Name of variable to be found.

    VendorGuid - Variable vendor GUID.

    Attributes - Attribute value of the variable set.

    DataSize - Size of Data.

    Data - Data pointer

Returns:

    EFI_STATUS

--*/
{
    UINTN length;

    if (mNvramNotAllowed)
    {
        // TODO: BDS currently does not work without returning success on writes. It would be preferable in the
        //       future to return EFI_UNSUPPORTED instead, but this requires fixing BDS.
        return EFI_SUCCESS;
    }

    ZeroMem(mNvramCommandDescriptor, sizeof(NVRAM_COMMAND_DESCRIPTOR));
    mNvramCommandDescriptor->Status = EFI_DEVICE_ERROR;
    mNvramCommandDescriptor->Command = NvramSetVariableCommand;
    mNvramCommandDescriptor->U.VariableCommand.VariableAttributes = Attributes;

    length = StrSize(VariableName);
    ASSERT(length <= (UINT32) -1);
    if (length > (UINT32) -1)
    {
        return EFI_INVALID_PARAMETER;
    }

    ASSERT(DataSize <= (UINT32) -1);
    if (DataSize > (UINT32) -1)
    {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Everything must be able to fit inside the bounce buffer.
    //
    if ((length + DataSize) > EFI_MAX_VARIABLE_NAME_SIZE + EFI_MAX_VARIABLE_DATA_SIZE)
    {
        return EFI_INVALID_PARAMETER;
    }

    CopyMem(mNvramCommandDataBuffer, VariableName, length);
    mNvramCommandDescriptor->U.VariableCommand.VariableNameAddress = mNvramCommandDataBufferGpa;
    mNvramCommandDescriptor->U.VariableCommand.VariableNameBytes = (UINT32) length;
    mNvramCommandDescriptor->U.VariableCommand.VariableVendorGuid = *VendorGuid;

    //
    // Data follows the name.
    //
    CopyMem(mNvramCommandDataBuffer + length, Data, DataSize);
    mNvramCommandDescriptor->U.VariableCommand.VariableDataAddress =
        mNvramCommandDataBufferGpa + length;
    mNvramCommandDescriptor->U.VariableCommand.VariableDataBytes = (UINT32) DataSize;

    return IssueBiosDeviceNvramCommand();
}


EFI_STATUS
NvramGetVariable(
    IN              CHAR16*     VariableName,
    IN              EFI_GUID*   VendorGuid,
    OUT OPTIONAL    UINT32*     Attributes,
    IN OUT          UINTN*      DataSize,
    OUT             void*       Data
    )
/*++

Routine Description:

    Gets an NV variable.

Arguments:

    VariableName - Name of variable to be found.

    VendorGuid - Variable vendor GUID.

    Attributes - Attribute value of the variable found.

    DataSize - Size of Data.

    Data - Data pointer.

Returns:

    EFI_STATUS

--*/
{
    UINTN length;
    UINT32 dataSize;
    EFI_STATUS status;

    ASSERT(VariableName != NULL);
    ASSERT(VendorGuid != NULL);
    ASSERT(DataSize != NULL);
    ASSERT((Data != NULL) || (*DataSize == 0));

    if (mNvramNotAllowed)
    {
        return EFI_NOT_FOUND;
    }

    ZeroMem(mNvramCommandDescriptor, sizeof(NVRAM_COMMAND_DESCRIPTOR));
    mNvramCommandDescriptor->Status = EFI_DEVICE_ERROR;
    mNvramCommandDescriptor->Command = NvramGetVariableCommand;


    //
    // Check the length of the name string.
    //
    length = StrSize(VariableName);
    ASSERT(length <= (UINT32)-1);
    if (length > EFI_MAX_VARIABLE_NAME_SIZE)
    {
        return EFI_INVALID_PARAMETER;
    }

    //
    // *DataSize can be larger than allowed variable size.
    // However cap it before sending.
    //
    dataSize = (UINT32)*DataSize;
    if (dataSize > EFI_MAX_VARIABLE_DATA_SIZE)
    {
        dataSize = EFI_MAX_VARIABLE_DATA_SIZE;
    }



    CopyMem(mNvramCommandDataBuffer, VariableName, length);
    mNvramCommandDescriptor->U.VariableCommand.VariableNameAddress = mNvramCommandDataBufferGpa;
    mNvramCommandDescriptor->U.VariableCommand.VariableNameBytes = (UINT32) length;
    mNvramCommandDescriptor->U.VariableCommand.VariableVendorGuid = *VendorGuid;
    mNvramCommandDescriptor->U.VariableCommand.VariableDataAddress =
        mNvramCommandDataBufferGpa + length;
    mNvramCommandDescriptor->U.VariableCommand.VariableDataBytes = dataSize;

    status = IssueBiosDeviceNvramCommand();
    if (status == EFI_SUCCESS)
    {
        if (Attributes != NULL)
        {
            *Attributes = mNvramCommandDescriptor->U.VariableCommand.VariableAttributes;
        }

        *DataSize = mNvramCommandDescriptor->U.VariableCommand.VariableDataBytes;

        //
        // Copy data out of the bounce buffer.
        //

        CopyMem(Data, mNvramCommandDataBuffer + length, *DataSize);
    }
    else if (status == EFI_BUFFER_TOO_SMALL)
    {
        //
        // This shouldn't happen, it means that the variable was larger
        // than allowed by the bounce buffer, and thus, the spec.
        //
        if (mNvramCommandDescriptor->U.VariableCommand.VariableDataBytes <= *DataSize)
        {
            status = EFI_DEVICE_ERROR;
        }
        else
        {
            *DataSize = mNvramCommandDescriptor->U.VariableCommand.VariableDataBytes;
        }
    }

    return status;
}


EFI_STATUS
NvramGetFirstVariableName(
    OUT     UINTN*      VariableNameSize,
    OUT     CHAR16*     VariableName,
    IN OUT  EFI_GUID*   VendorGuid
    )
/*++

Routine Description:

    This code gets the first NV variable.

Arguments:

    VariableNameSize - Size of the VariableName buffer on input,
                       size of the variable string on output.

    VariableName - The last VariableName returned, or an empty string on input,
                   the next variable name on output.

    VendorGuid - The last VendorGuid that was returned on input, the VendorGuid
                 of the next variable on output.

Returns:

    EFI_STATUS

--*/
{
    EFI_STATUS status;

    ASSERT(VariableNameSize != NULL);
    ASSERT(VendorGuid != NULL);
    ASSERT(VariableName != NULL);

    if (mNvramNotAllowed)
    {
        return EFI_NOT_FOUND;
    }

    ZeroMem(mNvramCommandDescriptor, sizeof(NVRAM_COMMAND_DESCRIPTOR));
    mNvramCommandDescriptor->Status = EFI_DEVICE_ERROR;
    mNvramCommandDescriptor->Command = NvramGetFirstVariableNameCommand;

    if (*VariableNameSize  > (UINT32)-1)
    {
        return EFI_INVALID_PARAMETER;
    }

    if (*VariableNameSize > EFI_MAX_VARIABLE_NAME_SIZE)
    {
        return EFI_INVALID_PARAMETER;
    }

    mNvramCommandDescriptor->U.VariableCommand.VariableNameAddress = mNvramCommandDataBufferGpa;
    mNvramCommandDescriptor->U.VariableCommand.VariableNameBytes = (UINT32) *VariableNameSize;
    mNvramCommandDescriptor->U.VariableCommand.VariableVendorGuid = *VendorGuid;

    status = IssueBiosDeviceNvramCommand();
    if (status == EFI_SUCCESS)
    {
        *VendorGuid = mNvramCommandDescriptor->U.VariableCommand.VariableVendorGuid;
        *VariableNameSize = mNvramCommandDescriptor->U.VariableCommand.VariableNameBytes;

        CopyMem(VariableName, mNvramCommandDataBuffer, *VariableNameSize);
    }
    else if (status == EFI_BUFFER_TOO_SMALL)
    {
        *VariableNameSize = mNvramCommandDescriptor->U.VariableCommand.VariableNameBytes;
    }
    return status;
}



EFI_STATUS
NvramGetNextVariableName(
    IN OUT  UINTN*      VariableNameSize,
    IN OUT  CHAR16*     VariableName,
    IN OUT  EFI_GUID*   VendorGuid
    )
/*++

Routine Description:

    This code gets the next NV variable, or the first NV variable.

Arguments:

    VariableNameSize - Size of the VariableName buffer on input,
                       size of the variable string on output.

    VariableName - The last VariableName returned, or an empty string on input,
                   the next variable name on output.

    VendorGuid - The last VendorGuid that was returned on input, the VendorGuid
                 of the next variable on output.

Returns:

    EFI_STATUS

--*/
{
    EFI_STATUS status;

    ASSERT(VariableNameSize != NULL);
    ASSERT(VendorGuid != NULL);
    ASSERT(VariableName != NULL);

    if (mNvramNotAllowed)
    {
        return EFI_NOT_FOUND;
    }

    ZeroMem(mNvramCommandDescriptor, sizeof(NVRAM_COMMAND_DESCRIPTOR));
    mNvramCommandDescriptor->Status = EFI_DEVICE_ERROR;
    mNvramCommandDescriptor->Command = NvramGetNextVariableNameCommand;

    if (*VariableNameSize  > (UINT32) -1)
    {
        return EFI_INVALID_PARAMETER;
    }

    if (*VariableNameSize > EFI_MAX_VARIABLE_NAME_SIZE)
    {
        return EFI_INVALID_PARAMETER;
    }

    CopyMem(mNvramCommandDataBuffer, VariableName, *VariableNameSize);
    mNvramCommandDescriptor->U.VariableCommand.VariableNameAddress = mNvramCommandDataBufferGpa;
    mNvramCommandDescriptor->U.VariableCommand.VariableNameBytes = (UINT32) *VariableNameSize;
    mNvramCommandDescriptor->U.VariableCommand.VariableVendorGuid = *VendorGuid;

    status = IssueBiosDeviceNvramCommand();
    if (status == EFI_SUCCESS)
    {
        *VendorGuid = mNvramCommandDescriptor->U.VariableCommand.VariableVendorGuid;
        *VariableNameSize = mNvramCommandDescriptor->U.VariableCommand.VariableNameBytes;

        CopyMem(VariableName, mNvramCommandDataBuffer, *VariableNameSize);
    }
    else if (status == EFI_BUFFER_TOO_SMALL)
    {
        *VariableNameSize = mNvramCommandDescriptor->U.VariableCommand.VariableNameBytes;
    }
    return status;
}

extern
VOID
NvramDebugLog(
    IN CONST CHAR8 *Format,
    ...
    )
/*++

Routine Description:

    Formats and sends a log message to the BiosDevice.

Arguments:

    Format - the printf style format string.

    ... - the parameters dependent on the format string.

Returns:

    EFI_STATUS

--*/
{
    CHAR16 buffer[128];
    VA_LIST marker;
    UINTN length;

    ASSERT(sizeof(buffer) < EFI_MAX_VARIABLE_NAME_SIZE);

    if (mNvramNotAllowed)
    {
        return;
    }

    //
    // Do nothing if no format string.
    //
    if (Format == NULL)
    {
        return;
    }

    //
    // Convert the parameters to a Unicode String
    //
    VA_START(marker, Format);
    UnicodeVSPrintAsciiFormat(buffer, sizeof(buffer), Format, marker);
    VA_END (marker);

    //
    // Put the string (plus null) in data buffer.
    //
    length = StrSize(buffer);
    CopyMem(mNvramCommandDataBuffer, buffer, length);

    //
    // Send the string.
    //
    ZeroMem(mNvramCommandDescriptor, sizeof(NVRAM_COMMAND_DESCRIPTOR));
    mNvramCommandDescriptor->Command = NvramDebugStringCommand;
    mNvramCommandDescriptor->U.VariableCommand.VariableNameAddress = mNvramCommandDataBufferGpa;
    mNvramCommandDescriptor->U.VariableCommand.VariableNameBytes = (UINT32)length;

    (void)IssueBiosDeviceNvramCommand();
}


