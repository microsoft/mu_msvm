/** @file

    Implementation of ComponentName protocol for StorvscDxe.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "StorvscDxe.h"

EFI_COMPONENT_NAME2_PROTOCOL gStorvscComponentName2 =
{
    StorvscComponentNameGetDriverName,
    StorvscComponentNameGetControllerName,
    "en"
};

EFI_COMPONENT_NAME_PROTOCOL gStorvscComponentName =
{
    (EFI_COMPONENT_NAME_GET_DRIVER_NAME) StorvscComponentNameGetDriverName,
    (EFI_COMPONENT_NAME_GET_CONTROLLER_NAME)StorvscComponentNameGetControllerName,
    "eng"
};

EFI_UNICODE_STRING_TABLE gStorvscDriverNameTable[] =
{
    { "eng;en", (CHAR16 *)L"Hyper-V SCSI Driver"},
    { NULL, NULL }
};

EFI_UNICODE_STRING_TABLE gStorvscControllerNameTable[] =
{
    { "eng;en", (CHAR16 *)L"Hyper-V SCSI Controller"},
    { NULL, NULL }
};


EFI_STATUS
EFIAPI
StorvscComponentNameGetDriverName (
    IN  EFI_COMPONENT_NAME2_PROTOCOL *This,
    IN  CHAR8 *Language,
    OUT CHAR16 **DriverName
    )
/*++

Routine Description:

    Retrieves a string that is the user readable name of the EFI Driver.

Arguments:

    This - A pointer to the EFI_COMPONENT_NAME2_PROTOCOL instance.

    Language - A pointer to a Null-terminated ASCII string array
        indicating the language.

    DriverName - A pointer to the string to return.

Return Value:

    EFI_STATUS.

--*/
{
    return LookupUnicodeString2(
        Language,
        This->SupportedLanguages,
        gStorvscDriverNameTable,
        DriverName,
        (BOOLEAN)(This != &gStorvscComponentName2));
}


EFI_STATUS
EFIAPI
StorvscComponentNameGetControllerName (
    IN  EFI_COMPONENT_NAME2_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_HANDLE ChildHandle OPTIONAL,
    IN  CHAR8 *Language,
    OUT CHAR16 **ControllerName
    )
/*++

Routine Description:

    Retrieves a string that is the user readable name of the controller
    that is being managed by an EFI Driver.

Arguments:

    This - A pointer to the EFI_COMPONENT_NAME2_PROTOCOL instance.

    ControllerHandle - The controller whose name is to be returned.

    ChildHandle - The handle of the child controller to retrieve the name of.

    Language - A pointer to a Null-terminated ASCII string array
        indicating the language.

    ControllerName -A pointer to the string to return.

--*/
{
    EFI_STATUS status;

    //
    // ChildHandle must be NULL for a Device Driver
    //
    if (ChildHandle != NULL)
    {
        return EFI_UNSUPPORTED;
    }

    //
    // Make sure this driver is currently managing ControllerHandle
    //
    status = EfiTestManagedDevice(
        ControllerHandle,
        gStorvscDriverBinding.DriverBindingHandle,
        &gEfiEmclProtocolGuid);

    if (EFI_ERROR(status))
    {
        return status;
    }

    return LookupUnicodeString2(
        Language,
        This->SupportedLanguages,
        gStorvscControllerNameTable,
        ControllerName,
        (BOOLEAN)(This != &gStorvscComponentName2));
}

