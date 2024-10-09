/** @file

    EFI Driver for Synthetic SCSI Controller.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "StorvscDxe.h"

EFI_DRIVER_BINDING_PROTOCOL gStorvscDriverBinding =
{
    StorvscDriverBindingSupported,
    StorvscDriverBindingStart,
    StorvscDriverBindingStop,
    STORVSC_VERSION,
    NULL,
    NULL
};

STORVSC_ADAPTER_CONTEXT gStorvscAdapterContextTemplate =
{
    STORVSC_ADAPTER_CONTEXT_SIGNATURE,
    0, // Handle

    NULL, // Emcl

    {   // ScsiPassThruExt
        NULL,
        StorvscExtScsiPassThruPassThru,
        StorvscExtScsiPassThruGetNextTargetLun,
        StorvscExtScsiPassThruBuildDevicePath,
        StorvscExtScsiPassThruGetTargetLun,
        StorvscExtScsiPassThruResetChannel,
        StorvscExtScsiPassThruResetTargetLun,
        StorvscExtScsiPassThruGetNextTarget
    },

    {   // ExtScsiPassThruMode
        //
        // AdapterId. StorVSP does not have a reserved LUN for the adapter, so this
        // must be set this to an invalid LUN.
        //
        0xFFFFFFFF,
        //
        // According to UEFI2.3 spec Section 14.7, Drivers for non-RAID SCSI
        // controllers should set both EFI_EXT_SCSI_PASS_THRU_ATTRIBUTES_PHYSICAL
        // and EFI_EXT_SCSI_PASS_THRU_ATTRIBUTES_LOGICAL
        // bits.
        //
        EFI_EXT_SCSI_PASS_THRU_ATTRIBUTES_PHYSICAL |
        EFI_EXT_SCSI_PASS_THRU_ATTRIBUTES_LOGICAL |
        EFI_EXT_SCSI_PASS_THRU_ATTRIBUTES_NONBLOCKIO,
        //
        // IoAlign
        //
        VSTORAGE_ALIGNMENT_MASK + 1
    },

    NULL, // ChannelContext

    {   // LunList
        NULL,
        NULL
    }
};


EFI_STATUS
EFIAPI
StorvscDriverEntryPoint (
    IN  EFI_HANDLE ImageHandle,
    IN  EFI_SYSTEM_TABLE *SystemTable
    )
/*++

Routine Description:

    The Entry Point of the module.

Arguments:

    ImageHandle - The firmware allocated handle for the EFI image.

    SystemTable - A pointer to the EFI System Table.

Return Value:

    EFI_STATUS.

--*/
{

    EFI_STATUS status;

    //
    // Install UEFI Driver Model protocols.
    //
    status = EfiLibInstallDriverBindingComponentName2(
        ImageHandle,
        SystemTable,
        &gStorvscDriverBinding,
        ImageHandle,
        &gStorvscComponentName,
        &gStorvscComponentName2);

    return status;
}


EFI_STATUS
EFIAPI
StorvscDriverBindingSupported (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    )
/*++

Routine Description:

    Tests to see if this driver supports a given controller.

Arguments:

    This - A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.

    ControllerHandle - The handle of the controller to test.

    RemainingDevicePath - A pointer to the remaining portion of a device path.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_VMBUS_PROTOCOL *vmbus;

    status = gBS->OpenProtocol(
        ControllerHandle,
        &gEfiVmbusProtocolGuid,
        (VOID **) &vmbus,
        This->DriverBindingHandle,
        ControllerHandle,
        EFI_OPEN_PROTOCOL_TEST_PROTOCOL);

    if (EFI_ERROR(status))
    {
        return status;
    }

    return EmclChannelTypeSupported(
        ControllerHandle,
        &gSyntheticStorageClassGuid,
        This->DriverBindingHandle);
}


EFI_STATUS
EFIAPI
StorvscDriverBindingStart (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    )
/*++

Routine Description:

    Starts a device controller.

Arguments:

    This - A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.

    ControllerHandle - The handle of the controller to start.

    RemainingDevicePath - A pointer to the remaining portion of a device path.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    PSTORVSC_ADAPTER_CONTEXT instance = NULL;
    BOOLEAN driverStarted = FALSE;
    BOOLEAN emclInstalled = FALSE;

    status = EmclInstallProtocol(ControllerHandle);

    if (status == EFI_ALREADY_STARTED)
    {
        driverStarted = TRUE;
        goto Cleanup;
    }
    else if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    emclInstalled = TRUE;

    instance = AllocateCopyPool(sizeof(*instance), &gStorvscAdapterContextTemplate);

    if (instance == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    status = gBS->OpenProtocol(
        ControllerHandle,
        &gEfiEmclV2ProtocolGuid,
        (VOID **) &instance->Emcl,
        This->DriverBindingHandle,
        ControllerHandle,
        EFI_OPEN_PROTOCOL_BY_DRIVER);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    instance->Handle = ControllerHandle;
    instance->ExtScsiPassThru.Mode = &instance->ExtScsiPassThruMode;

    InitializeListHead(&instance->LunList);

    status = StorChannelOpen(instance->Emcl, &instance->ChannelContext);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // No locking is required when modifying the lun list, because the
    // ExtScsiPassThruProtocol is not yet installed, so the list is not
    // accessed by any other caller.
    //
    status = StorChannelBuildLunList(instance->ChannelContext, &instance->LunList);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = gBS->InstallMultipleProtocolInterfaces(
        &ControllerHandle,
        &gEfiExtScsiPassThruProtocolGuid, &instance->ExtScsiPassThru,
        NULL);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    driverStarted = TRUE;

Cleanup:
    if (!driverStarted)
    {
        if (instance != NULL)
        {
            StorChannelFreeLunList(&instance->LunList);
            if (instance->ChannelContext != NULL)
            {
                StorChannelClose(instance->ChannelContext);
            }
            FreePool(instance);
        }

        gBS->CloseProtocol(
            ControllerHandle,
            &gEfiEmclV2ProtocolGuid,
            This->DriverBindingHandle,
            ControllerHandle);

        if (emclInstalled)
        {
            EmclUninstallProtocol(ControllerHandle);
        }
    }

    return status;
}


EFI_STATUS
EFIAPI
StorvscDriverBindingStop (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  UINTN NumberOfChildren,
    IN  EFI_HANDLE *ChildHandleBuffer
    )
/*++

Routine Description:

    Stops a device controller.

Arguments:

    This - A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.

    ControllerHandle - A handle to the device being stopped.

    NumberOfChildren - The number of child device handles in ChildHandleBuffer.

    ChildHandleBuffer - An array of child handles to be freed.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    STORVSC_ADAPTER_CONTEXT *instance;
    EFI_EXT_SCSI_PASS_THRU_PROTOCOL *extScsiPassThru;

    status = gBS->OpenProtocol(
        ControllerHandle,
        &gEfiExtScsiPassThruProtocolGuid,
        (VOID **) &extScsiPassThru,
        This->DriverBindingHandle,
        ControllerHandle,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (EFI_ERROR(status))
    {
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    instance = STORVSC_ADAPTER_CONTEXT_FROM_EXT_SCSI_PASS_THRU_THIS(extScsiPassThru);

    StorChannelClose(instance->ChannelContext);

    gBS->UninstallMultipleProtocolInterfaces(
        ControllerHandle,
        &gEfiExtScsiPassThruProtocolGuid, &instance->ExtScsiPassThru,
        NULL);

    gBS->CloseProtocol(
        ControllerHandle,
        &gEfiEmclV2ProtocolGuid,
        This->DriverBindingHandle,
        ControllerHandle);

    StorChannelFreeLunList(&instance->LunList);

    FreePool(instance);
    EmclUninstallProtocol(ControllerHandle);

Cleanup:
    return status;
}

