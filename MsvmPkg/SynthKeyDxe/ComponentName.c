/** @file
  Implementation of ComponentName protocol for SynthKeyDxe.

  This code is derived from:
    IntelFrameworkModulePkg\Bus\Isa\Ps2KeyboardDxe\ComponentName.c

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SynthKeyDxe.h"


EFI_STATUS
EFIAPI
SynthKeyComponentNameGetDriverName(
    IN          EFI_COMPONENT_NAME2_PROTOCOL   *This,
    IN          CHAR8                          *Language,
    OUT         CHAR16                        **DriverName
    );

EFI_STATUS
EFIAPI
SynthKeyComponentNameGetControllerName(
    IN          EFI_COMPONENT_NAME2_PROTOCOL   *This,
    IN          EFI_HANDLE                      ControllerHandle,
    IN OPTIONAL EFI_HANDLE                      ChildHandle,
    IN          CHAR8                          *Language,
    OUT         CHAR16                        **ControllerName
    );


EFI_COMPONENT_NAME2_PROTOCOL gSynthKeyComponentName2 =
{
    SynthKeyComponentNameGetDriverName,
    SynthKeyComponentNameGetControllerName,
    "en"
};

EFI_COMPONENT_NAME_PROTOCOL gSynthKeyComponentName =
{
    (EFI_COMPONENT_NAME_GET_DRIVER_NAME)SynthKeyComponentNameGetDriverName,
    (EFI_COMPONENT_NAME_GET_CONTROLLER_NAME)SynthKeyComponentNameGetControllerName,
    "eng"
};

EFI_UNICODE_STRING_TABLE gSynthKeyDriverNameTable[] =
{
    { "eng;en", (CHAR16 *)L"Hyper-V Keyboard Driver"},
    { NULL, NULL }
};

EFI_UNICODE_STRING_TABLE gSynthKeyControllerNameTable[] =
{
    { "eng;en", (CHAR16 *)L"Hyper-V Keyboard Controller"},
    { NULL, NULL }
};


EFI_STATUS
EFIAPI
SynthKeyComponentNameGetDriverName(
    IN          EFI_COMPONENT_NAME2_PROTOCOL   *This,
    IN          CHAR8                          *Language,
    OUT         CHAR16                        **DriverName
    )
/*++

Routine Description:

    Retrieves a Unicode string that is the user readable name of the driver.

Arguments:

    This        A pointer to the EFI_COMPONENT_NAME2_PROTOCOL instance.

    Language    A pointer to a Null-terminated ASCII string array indicating the language.

    DriverName  On success will contain the driver name string.

Returns:

    EFI_SUCCESS driver name is returned.
    EFI_UNSUPPORTED no driver name is available in the specified language.

--*/
{

    //
    // This function handles both the EFI_COMPONENT_NAME_PROTOCOL and
    // EFI_COMPONENT_NAME2_PROTOCOL interfaces.  The main difference between the two
    // is how the language names are specified.
    //   EFI_COMPONENT_NAME_PROTOCOL uses ISO 639-2 language codes
    //   EFI_COMPONENT_NAME2_PROTOCOL uses RFC 4646
    //
    // LookupUnicodeString2 can handle both language formats with the final
    // parameter (Iso639Language) used to signify which format to use.
    //
    // Here use the protocol instance pointer to determine which interface is actually
    // being used and therefore which language description format should be used.
    //
    return LookupUnicodeString2(Language,
                                This->SupportedLanguages,
                                gSynthKeyDriverNameTable,
                                DriverName,
                                (BOOLEAN)(This != &gSynthKeyComponentName2));
}


EFI_STATUS
EFIAPI
SynthKeyComponentNameGetControllerName(
    IN          EFI_COMPONENT_NAME2_PROTOCOL   *This,
    IN          EFI_HANDLE                      ControllerHandle,
    IN OPTIONAL EFI_HANDLE                      ChildHandle,
    IN          CHAR8                          *Language,
    OUT         CHAR16                        **ControllerName
    )
/*++

Routine Description:

    Retrieves a user readable string for the controller that is being managed by an EFI Driver.

Arguments:

    This                A pointer to the EFI_COMPONENT_NAME2_PROTOCOL instance.

    ControllerHandle    The controller whose name is to be returned.

    ChildHandle         The handle of the child controller to retrieve the name of.

    Language            A pointer to a Null-terminated ASCII string array indicating the language.

    ControllerName      A pointer to the string to return.

Returns:

    EFI_SUCCESS
    EFI_UNSUPPORTED no controller name is available in the specified language.

--*/
{
    EFI_STATUS status;

    //
    // This is a device driver, so ChildHandle must be NULL.
    //
    if (ChildHandle != NULL)
    {
        return EFI_UNSUPPORTED;
    }

    //
    // Make sure this driver is currently managing ControllerHandle
    //
    status = EfiTestManagedDevice(ControllerHandle,
                                  gSynthKeyDriverBinding.DriverBindingHandle,
                                  &gEfiEmclProtocolGuid);

    if (EFI_ERROR(status))
    {
        return status;
    }

    //
    // See note in SynthKeyComponentNameGetDriverName for
    // the purpose of (BOOLEAN)(This != &gSynthKeyComponentName2)
    //
    return LookupUnicodeString2(Language,
                                This->SupportedLanguages,
                                gSynthKeyControllerNameTable,
                                ControllerName,
                                (BOOLEAN)(This != &gSynthKeyComponentName2));
}
