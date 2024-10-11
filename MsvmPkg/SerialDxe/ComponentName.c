/** @file

  Provides the UEFI Component Name and Name2 protocol for the Hyper-V serial driver.

  Copyright (c) 2006 - 2011, Intel Corporation. All rights reserved.
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  This code is derived from the following:
            IntelFrameworkModulePackage\Bus\Isa\IsaSerialDxe\ComponentName.c


**/

#include "Serial.h"

extern EFI_GUID gMsvmSerialBusProtocolGuid;

//
// EFI Component Name Protocol.
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME_PROTOCOL  gSerialComponentName =
{
    SerialComponentNameGetDriverName,
    SerialComponentNameGetControllerName,
    "eng"
};

//
// EFI Component Name 2 Protocol.
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME2_PROTOCOL gSerialComponentName2 =
{
    (EFI_COMPONENT_NAME2_GET_DRIVER_NAME) SerialComponentNameGetDriverName,
    (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME) SerialComponentNameGetControllerName,
    "en"
};

//
// Root Controller name table.
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE gSerialControllerNameTable[] =
{
    { "eng;en", (CHAR16 *)L"Hyper-V Serial Bus Controller" },
    { NULL, NULL }
};

//
// Driver name table.
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE mSerialDriverNameTable[] =
{
    { "eng;en", L"Hyper-V Serial Driver" },
    { NULL, NULL }
};


EFI_STATUS
EFIAPI
SerialComponentNameGetDriverName(
    IN  EFI_COMPONENT_NAME_PROTOCOL   *This,
    IN  CHAR8                         *Language,
    OUT CHAR16                        **DriverName
    )
/*++

Routine Description:

    Retrieves a Unicode string that is the user readable name of the driver.

    This function retrieves the user readable name of a driver in the form of a
    Unicode string. If the driver specified by This has a user readable name in
    the language specified by Language, then a pointer to the driver name is
    returned in DriverName, and EFI_SUCCESS is returned. If the driver specified
    by This does not support the language specified by Language,
    then EFI_UNSUPPORTED is returned.

Arguments:

    This            A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                    EFI_COMPONENT_NAME_PROTOCOL instance.

    Language        A pointer to a Null-terminated ASCII string
                    array indicating the language. This is the
                    language of the driver name that the caller is
                    requesting, and it must match one of the
                    languages specified in SupportedLanguages. The
                    number of languages supported by a driver is up
                    to the driver writer. Language is specified
                    in RFC 4646 or ISO 639-2 language code format.

    DriverName      A pointer to the Unicode string to return.
                    This Unicode string is the name of the
                    driver specified by This in the language
                    specified by Language.

Return Value:

    EFI_SUCCESS           The Unicode string for the Driver specified by
                          This and the language specified by Language was
                          returned in DriverName.

    EFI_INVALID_PARAMETER Language is NULL.

    EFI_INVALID_PARAMETER DriverName is NULL.

    EFI_UNSUPPORTED       The driver specified by This does not support
                          the language specified by Language.

--*/
{
    return LookupUnicodeString2(Language,
                                This->SupportedLanguages,
                                mSerialDriverNameTable,
                                DriverName,
                                (BOOLEAN)(This == &gSerialComponentName));
}


EFI_STATUS
EFIAPI
SerialComponentNameGetControllerName(
    IN  EFI_COMPONENT_NAME_PROTOCOL     *This,
    IN  EFI_HANDLE                      ControllerHandle,
    IN  EFI_HANDLE                      ChildHandle OPTIONAL,
    IN  CHAR8                           *Language,
    OUT CHAR16                          **ControllerName
    )
/*++

Routine Description:

    Retrieves a Unicode string that is the user readable name of the controller
    that is being managed by a driver.

    This function retrieves the user readable name of the controller specified by
    ControllerHandle and ChildHandle in the form of a Unicode string. If the
    driver specified by This has a user readable name in the language specified by
    Language, then a pointer to the controller name is returned in ControllerName,
    and EFI_SUCCESS is returned.  If the driver specified by This is not currently
    managing the controller specified by ControllerHandle and ChildHandle,
    then EFI_UNSUPPORTED is returned.  If the driver specified by This does not
    support the language specified by Language, then EFI_UNSUPPORTED is returned.
    Currently not implemented.

Arguments:

    This                    A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                            EFI_COMPONENT_NAME_PROTOCOL instance.

    ControllerHandle        The handle of a controller that the driver
                            specified by This is managing.  This handle
                            specifies the controller whose name is to be
                            returned.

    ChildHandle             The handle of the child controller to retrieve
                            the name of.  This is an optional parameter that
                            may be NULL.  It will be NULL for device
                            drivers.  It will also be NULL for a bus drivers
                            that wish to retrieve the name of the bus
                            controller.  It will not be NULL for a bus
                            driver that wishes to retrieve the name of a
                            child controller.

    Language                A pointer to a Null-terminated ASCII string
                            array indicating the language.  This is the
                            language of the driver name that the caller is
                            requesting, and it must match one of the
                            languages specified in SupportedLanguages. The
                            number of languages supported by a driver is up
                            to the driver writer. Language is specified in
                            RFC 4646 or ISO 639-2 language code format.

    ControllerName          A pointer to the Unicode string to return.
                            This Unicode string is the name of the
                            controller specified by ControllerHandle and
                            ChildHandle in the language specified by
                            Language from the point of view of the driver
                            specified by This.

Return Value:

    EFI_SUCCES              The Unicode string for the user readable name in
                            the language specified by Language for the
                            driver specified by This was returned in
                            DriverName.

    EFI_INVALID_PARAMETER   ControllerHandle is NULL.

    EFI_INVALID_PARAMETER   ChildHandle is not NULL and it is not a valid
                            EFI_HANDLE.

    EFI_INVALID_PARAMETER   Language is NULL.

    EFI_INVALID_PARAMETER   ControllerName is NULL.

    EFI_UNSUPPORTED         The driver specified by This is not currently
                            managing the controller specified by
                            ControllerHandle and ChildHandle.

    EFI_UNSUPPORTED         The driver specified by This does not support
                            the language specified by Language.

--**/
{
    EFI_STATUS              status;
    EFI_SERIAL_IO_PROTOCOL  *serialIo;
    SERIAL_DEVICE           *serialDevice;

    //
    // Make sure this driver is currently managing a ControllerHandle
    //
    status = EfiTestManagedDevice(ControllerHandle,
                                  gSerialDriver.DriverBindingHandle,
                                  &gMsvmSerialBusProtocolGuid);

    if (ChildHandle != NULL)
    {
        //
        // Get the Serial IO protocol on the child handle
        //
        status = gBS->OpenProtocol(ChildHandle,
                                   &gEfiSerialIoProtocolGuid,
                                   (VOID **) &serialIo,
                                   gSerialDriver.DriverBindingHandle,
                                   ChildHandle,
                                   EFI_OPEN_PROTOCOL_GET_PROTOCOL);
        if (EFI_ERROR(status))
        {
            return status;
        }
        //
        // Offset to the Serial Device structure.
        //
        serialDevice = SERIAL_DEVICE_FROM_THIS(serialIo);

        //
        // Return the device specific string.
        //
        return LookupUnicodeString2(Language,
                                    This->SupportedLanguages,
                                    serialDevice->ControllerNameTable,
                                    ControllerName,
                                    (BOOLEAN)(This == &gSerialComponentName));
    }

    //
    // Just get the name of the root handle.
    //
    return LookupUnicodeString2(Language,
                                This->SupportedLanguages,
                                gSerialControllerNameTable,
                                ControllerName,
                                (BOOLEAN)(This == &gSerialComponentName));
}


VOID
AddName(
    IN  SERIAL_DEVICE               *SerialDevice,
    IN  SERIAL_DEVICE_PROPERTIES    *SerialProperties
    )
/*++

Routine Description:

    Add the ISO639-2 and RFC4646 component name both for the Serial IO child device.

Arguments:

    SerialDevice            A pointer to the SERIAL_DEVICE instance.

    SerialProperties        A pointer to the serial port properties.

Return Value:

    EFI_SUCCESS             If string was added.

    EFI_*                   Error status if not added.

--**/
{
    CHAR16 serialPortName[] = L"Serial Port #9";

    serialPortName[ARRAY_SIZE(serialPortName) - 2] = (CHAR16)(L'0' + (UINT8) SerialProperties->UID);

    AddUnicodeString2("eng",
                      gSerialComponentName.SupportedLanguages,
                      &SerialDevice->ControllerNameTable,
                      (CHAR16 *)serialPortName,
                      TRUE);

    AddUnicodeString2("en",
                      gSerialComponentName2.SupportedLanguages,
                      &SerialDevice->ControllerNameTable,
                      (CHAR16 *)serialPortName,
                      FALSE);
}
