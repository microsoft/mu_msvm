/** @file

  EFI simple file system protocol over vmbus driver entry and
  driver binding protocol implementation.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "VmbfsEfi.h"

VOID
VmbfsCleanup (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  PVMBFS_SIMPLE_FILE_SYSTEM_PROTOCOL SimpleFileSystemProtocol
    );

EFI_STATUS
EFIAPI
VmbfsComponentNameGetDriverName (
    IN  EFI_COMPONENT_NAME_PROTOCOL *This,
    IN  CHAR8 *Language,
    OUT CHAR16 **DriverName
    );

EFI_STATUS
EFIAPI
VmbfsComponentNameGetControllerName(
    IN  EFI_COMPONENT_NAME_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_HANDLE ChildHandle OPTIONAL,
    IN  CHAR8 *Language,
    OUT CHAR16 **ControllerName
    );

EFI_STATUS
EFIAPI
VmbfsSupported(
    IN  EFI_DRIVER_BINDING_PROTOCOL    *This,
    IN  EFI_HANDLE                     Controller,
    IN  EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
    )

/*++

Routine Description:

    Test to see if this driver supports ControllerHandle. This service
    is called by the EFI boot service ConnectController(). In
    order to make drivers as small as possible, there are a few calling
    restrictions for this service. ConnectController() must
    follow these calling restrictions. If any other agent wishes to call
    Supported() it must also follow these calling restrictions.

Arguments:

    This                Protocol instance pointer.

    ControllerHandle    Handle of device to test.

    RemainingDevicePath Optional parameter use to pick a specific child
                              device to start.

Return Value:

    EFI_SUCCESS         This driver supports this device.

    EFI_ALREADY_STARTED This driver is already running on this device.

    other               This driver does not support this device.

--*/

{
    EFI_STATUS status;
    EFI_VMBUS_PROTOCOL *vmbus;

    status = gBS->OpenProtocol(
        Controller,
        &gEfiVmbusProtocolGuid,
        (VOID **) &vmbus,
        This->DriverBindingHandle,
        Controller,
        EFI_OPEN_PROTOCOL_BY_DRIVER);

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    gBS->CloseProtocol(
        Controller,
        &gEfiVmbusProtocolGuid,
        This->DriverBindingHandle,
        Controller);

    status = EmclChannelTypeSupported(
        Controller,
        &gSyntheticVmbfsClassGuid,
        This->DriverBindingHandle);

Exit:
  return status;
}


EFI_STATUS
EFIAPI
VmbfsStart (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    )

/*++

Routine Description:

    Start this driver on ControllerHandle. This service is called by the
    EFI boot service ConnectController(). In order to make
    drivers as small as possible, there are a few calling restrictions for
    this service. ConnectController() must follow these
    calling restrictions. If any other agent wishes to call Start() it
    must also follow these calling restrictions.

Arguments:

    This                 Protocol instance pointer.

    ControllerHandle     Handle of device to bind driver to.

    RemainingDevicePath  Optional parameter use to pick a specific child
                               device to start.

Return Value:

    EFI_SUCCESS          This driver is added to ControllerHandle

    EFI_ALREADY_STARTED  This driver is already running on ControllerHandle

    other                This driver does not support this device

--*/


{
    EFI_STATUS status;
    PVMBFS_SIMPLE_FILE_SYSTEM_PROTOCOL simpleFileSystemProtocol = NULL;
    PFILESYSTEM_INFORMATION fileSystemInformation = NULL;
    EFI_EMCL_PROTOCOL *emclProtocol = NULL;
    EFI_DEVICE_PATH_PROTOCOL *devicePath = NULL;
    BOOLEAN EmclInstalled = FALSE;

    //
    // Check if device already running.
    //
    status = gBS->OpenProtocol(
        ControllerHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID **)&simpleFileSystemProtocol,
        This->DriverBindingHandle,
        ControllerHandle,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    if (!EFI_ERROR(status))
    {
        return EFI_ALREADY_STARTED;
    }

    simpleFileSystemProtocol = NULL;

    //
    // Connect to EMCL.
    //
    status = EmclInstallProtocol(ControllerHandle);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    EmclInstalled = TRUE;

    status =
        gBS->AllocatePool(EfiBootServicesData,
                          sizeof(*simpleFileSystemProtocol),
                          &simpleFileSystemProtocol);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    fileSystemInformation = &simpleFileSystemProtocol->FileSystemInformation;
    ZeroMem(fileSystemInformation, sizeof(*fileSystemInformation));

    status = gBS->OpenProtocol(
        ControllerHandle,
        &gEfiDevicePathProtocolGuid,
        &devicePath,
        This->DriverBindingHandle,
        ControllerHandle,
        EFI_OPEN_PROTOCOL_BY_DRIVER);

    if(EFI_ERROR(status))
    {
        goto Cleanup;
    }

    fileSystemInformation->DevicePathProtocol = devicePath;

    status = gBS->OpenProtocol(
        ControllerHandle,
        &gEfiEmclProtocolGuid,
        (VOID **)&emclProtocol,
        This->DriverBindingHandle,
        ControllerHandle,
        EFI_OPEN_PROTOCOL_BY_DRIVER);

    if(EFI_ERROR(status))
    {
        goto Cleanup;
    }

    fileSystemInformation->EmclProtocol = emclProtocol;

    CopyMem(&simpleFileSystemProtocol->EfiSimpleFileSystemProtocol,
            &gVmbFsSimpleFileSystemProtocol,
            sizeof(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL));

    status = gBS->InstallMultipleProtocolInterfaces(&ControllerHandle,
                                                    &gEfiSimpleFileSystemProtocolGuid,
                                                    simpleFileSystemProtocol,
                                                    NULL);

    if(EFI_ERROR(status))
    {
        goto Cleanup;
    }

Cleanup:
    if (EFI_ERROR(status))
    {
        if (simpleFileSystemProtocol != NULL)
        {
            VmbfsCleanup(This, ControllerHandle, simpleFileSystemProtocol);
        }

        if (EmclInstalled)
        {
            EmclUninstallProtocol(ControllerHandle);
        }
    }

    return status;
}


VOID
VmbfsCleanup (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  PVMBFS_SIMPLE_FILE_SYSTEM_PROTOCOL SimpleFileSystemProtocol
    )

/*++

Routine Description:

    Cleanup the given VMBus simple file system protocol structure,
    freeing resources and releasing handles.

Arguments:

    This - A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.

    ControllerHandle - A handle to the device being stopped. The handle must
        support a bus specific I/O protocol for the driver to use to stop the
        device.

    SimpleFileSystemProtocol - Pointer to the Vmbus simple file system protocol
        to cleanup.

Return Value:

    None.

--*/

{
    PFILESYSTEM_INFORMATION fileSystemInformation;

    fileSystemInformation = &SimpleFileSystemProtocol->FileSystemInformation;

    if (fileSystemInformation->EmclProtocol != NULL)
    {
        gBS->CloseProtocol(ControllerHandle,
                           &gEfiEmclProtocolGuid,
                           This->DriverBindingHandle,
                           ControllerHandle);
    }

    if (fileSystemInformation->DevicePathProtocol != NULL)
    {
        gBS->CloseProtocol(ControllerHandle,
                           &gEfiDevicePathProtocolGuid,
                           This->DriverBindingHandle,
                           ControllerHandle);
    }

    gBS->FreePool(SimpleFileSystemProtocol);
}


EFI_STATUS
EFIAPI
VmbfsStop (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  UINTN NumberOfChildren,
    IN  EFI_HANDLE *ChildHandleBuffer OPTIONAL
)

/*++

Routine Description:

    If NumberOfChildren is zero, then the driver specified by This is either
    a device driver or a bus driver, and it is being requested to stop the
    controller specified by ControllerHandle. If ControllerHandle is stopped,
    then EFI_SUCCESS is returned. In either case, this function is required
    to undo what was performed in Start(). Whatever resources are allocated
    in Start() must be freed in Stop().

Arguments:

    This - A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.

    ControllerHandle - A handle to the device being stopped. The handle must
        support a bus specific I/O protocol for the driver to use to stop the
        device.

    NumberOfChildren - The number of child device handles in ChildHandleBuffer.
        Expected to be 0 since Vmbfs is not a bus driver.

    ChildHandleBuffer - An array of child handles to be freed.
        May be NULL if NumberOfChildren is 0.

Return Value:

    EFI_SUCCESS - The device was stopped.

    EFI_DEVICE_ERROR - The device could not be stopped due to a device error.

--*/

{
    EFI_STATUS status;
    PVMBFS_SIMPLE_FILE_SYSTEM_PROTOCOL simpleFileSystemProtocol;

    status = gBS->OpenProtocol(
        ControllerHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID **)&simpleFileSystemProtocol,
        This->DriverBindingHandle,
        ControllerHandle,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    if (EFI_ERROR(status))
    {
        return EFI_DEVICE_ERROR;
    }

    status = gBS->UninstallMultipleProtocolInterfaces(ControllerHandle,
                                                      &gEfiSimpleFileSystemProtocolGuid,
                                                      NULL);

    ASSERT(simpleFileSystemProtocol->FileSystemInformation.ReferenceCount == 0);

    VmbfsCleanup(This, ControllerHandle, simpleFileSystemProtocol);

    EmclUninstallProtocol(ControllerHandle);

    return EFI_SUCCESS;
}

//
// Driver name table
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE gVmbfsDriverNameTable[] =
{
    { "eng;en", (CHAR16 *)L"Hyper-V VMBus FileSystem Driver"},
    { NULL, NULL }
};


//
// Controller name table
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE gVmbfsControllerNameTable[] =
{
    { "eng;en", (CHAR16 *)L"Hyper-V VMBus FileSystem Controller"},
    { NULL, NULL }
};


//
// EFI Component Name Protocol
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME_PROTOCOL gVmbfsComponentName =
{
    VmbfsComponentNameGetDriverName,
    VmbfsComponentNameGetControllerName,
    "eng"
};

//
// EFI Component Name 2 Protocol
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME2_PROTOCOL gVmbfsComponentName2 =
{
    (EFI_COMPONENT_NAME2_GET_DRIVER_NAME) VmbfsComponentNameGetDriverName,
    (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME) VmbfsComponentNameGetControllerName,
    "en"
};


EFI_DRIVER_BINDING_PROTOCOL gVmbfsDriverBindingProtocol =
{
    VmbfsSupported,
    VmbfsStart,
    VmbfsStop,
    0x1,
    0,
    0
};


EFI_STATUS
EFIAPI
VmbfsComponentNameGetDriverName (
    IN  EFI_COMPONENT_NAME_PROTOCOL *This,
    IN  CHAR8 *Language,
    OUT CHAR16 **DriverName
    )
/*++

Routine Description:

    Retrieves a Unicode string that is the user readable name of the EFI Driver.

    This function retrieves the user readable name of a driver in the form of a
    Unicode string. If the driver specified by This has a user readable name in
    the language specified by Language, then a pointer to the driver name is
    returned in DriverName, and EFI_SUCCESS is returned. If the driver specified
    by This does not support the language specified by Language,
    then EFI_UNSUPPORTED is returned.

Arguments:

    This - A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or EFI_COMPONENT_NAME_PROTOCOL instance.

    Language - A pointer to a Null-terminated ASCII string array indicating the language.

    DriverName - A pointer to the string to return.

Return Value:

    EFI_STATUS.

--*/
{
    return LookupUnicodeString2(
        Language,
        This->SupportedLanguages,
        gVmbfsDriverNameTable,
        DriverName,
        (BOOLEAN)(This == &gVmbfsComponentName));
}


EFI_STATUS
EFIAPI
VmbfsComponentNameGetControllerName(
    IN  EFI_COMPONENT_NAME_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_HANDLE ChildHandle OPTIONAL,
    IN  CHAR8 *Language,
    OUT CHAR16 **ControllerName
    )
/*++

Routine Description:

    Retrieves a Unicode string that is the user readable name of the controller
    that is being managed by a Driver.


    This function retrieves the user readable name of the controller specified by
    ControllerHandle and ChildHandle in the form of a Unicode string. If the
    driver specified by This has a user readable name in the language specified by
    Language, then a pointer to the controller name is returned in ControllerName,
    and EFI_SUCCESS is returned.  If the driver specified by This is not currently
    managing the controller specified by ControllerHandle and ChildHandle,
    then EFI_UNSUPPORTED is returned.  If the driver specified by This does not
    support the language specified by Language, then EFI_UNSUPPORTED is returned.

Arguments:

    This - A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or EFI_COMPONENT_NAME_PROTOCOL instance.

    ControllerHandle - The handle of a controller that the driver specified by This
           is managing.  This handle specifies the controller whose name is to be returned.

    ChildHandle - The handle of the child controller to retrieve the name of. This is an
        optional parameter that may be NULL.  It will be NULL for device drivers.  It will
        also be NULL for a bus drivers that wish to retrieve the name of the bus controller.
        It will not be NULL for a bus  driver that wishes to retrieve the name of a
        child controller.

    Language - A pointer to a Null-terminated ASCII string array indicating the language.
        This is the language of the driver name that the caller is requesting, and it
        must match one of the languages specified in SupportedLanguages. The number of
        languages supported by a driver is up to the driver writer. Language is specified in
        RFC 4646 or ISO 639-2 language code format.

    ControllerName - A pointer to the Unicode string to return. This Unicode string is the
        name of the controller specified by ControllerHandle and ChildHandle in the language
        specified by Language from the point of view of the driver specified by This.

Return Value:

    EFI_STATUS.


--*/
{
    EFI_STATUS status;

    //
    // Make sure this driver is currently managing a ControllerHandle
    //
    status = EfiTestManagedDevice(
        ControllerHandle,
        gVmbfsDriverBindingProtocol.DriverBindingHandle,
        &gEfiEmclProtocolGuid
        );
    if (EFI_ERROR(status))
    {
        return status;
    }

    //
    // ChildHandle must be NULL for a Device Driver
    //
    if (ChildHandle != NULL)
    {
        return EFI_UNSUPPORTED;
    }

    return LookupUnicodeString2(
        Language,
        This->SupportedLanguages,
        gVmbfsControllerNameTable,
        ControllerName,
        (BOOLEAN)(This == &gVmbfsComponentName));
}


EFI_STATUS
EFIAPI
VmbfsEntry (
    IN  EFI_HANDLE       ImageHandle,
    IN  EFI_SYSTEM_TABLE *SystemTable
    )
/*++

Routine Description:

    VMBus File System driver entry point.

Arguments:

    ImageHandle - The driver image handle.

    SystemTable       The system table.

Return Value:

    EFI_SUCEESS - Initialization routine has found a device,
                      loaded it's ROM, and installed a notify event.

    Other - Return value from InstallMultipleProtocolInterfaces.

--*/
{
    EFI_STATUS status;

    gVmbfsDriverBindingProtocol.ImageHandle = ImageHandle;
    gVmbfsDriverBindingProtocol.DriverBindingHandle = ImageHandle;

    //
    // Install the DriverBinding and Component Name protocols onto the driver image handle.
    //
    status = EfiLibInstallDriverBindingComponentName2(ImageHandle,
                                                      SystemTable,
                                                      &gVmbfsDriverBindingProtocol,
                                                      ImageHandle,
                                                      &gVmbfsComponentName,
                                                      &gVmbfsComponentName2
                                                      );
    return status;
}


