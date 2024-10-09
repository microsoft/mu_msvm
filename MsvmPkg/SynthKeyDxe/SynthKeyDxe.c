/** @file
  Basic driver entry points for Hyper-V synthetic keyboard devices.

  This code is derived from:
    IntelFrameworkModulePkg\Bus\Isa\Ps2KeyboardDxe\Ps2Keyboard.c

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "SynthKeyDxe.h"
#include "SynthKeyChannel.h"
#include "SynthSimpleTextIn.h"

EFI_STATUS
EFIAPI
SynthKeyDriverSupported(
    IN          EFI_DRIVER_BINDING_PROTOCOL *This,
    IN          EFI_HANDLE                   Controller,
    IN          EFI_DEVICE_PATH_PROTOCOL    *RemainingDevicePath
    );


EFI_STATUS
EFIAPI
SynthKeyDriverStart(
    IN          EFI_DRIVER_BINDING_PROTOCOL *This,
    IN          EFI_HANDLE                   Controller,
    IN          EFI_DEVICE_PATH_PROTOCOL    *RemainingDevicePath
    );


EFI_STATUS
EFIAPI
SynthKeyDriverStop(
    IN          EFI_DRIVER_BINDING_PROTOCOL *This,
    IN          EFI_HANDLE                   Controller,
    IN          UINTN                        NumberOfChildren,
    IN          EFI_HANDLE                  *ChildHandleBuffer
    );


VOID
SynthKeyDriverCleanup(
    IN          PSYNTH_KEYBOARD_DEVICE      Device
    );

//
// DriverBinding Protocol Instance - Define the driver entry points.
// The UEFI runtime will first call the EFI_DRIVER_BINDING_SUPPORTED entry
// and if the device returns EFI_SUCCESS
// EFI_DRIVER_BINDING_START and finally EFI_DRIVER_BINDING_STOP.
//
EFI_DRIVER_BINDING_PROTOCOL gSynthKeyDriverBinding =
{
    SynthKeyDriverSupported,
    SynthKeyDriverStart,
    SynthKeyDriverStop,
    SYNTH_KEYBOARD_VERSION,
    NULL,
    NULL
};


EFI_STATUS
EFIAPI
SynthKeyDriverEntry(
    IN          EFI_HANDLE                  ImageHandle,
    IN          EFI_SYSTEM_TABLE           *SystemTable
    )
/*++

Routine Description:

    The module Entry Point for the SynthKey driver. This will register the
    driver's entry points.

Arguments:

    ImageHandle    The firmware allocated handle for the EFI image.
    SystemTable    A pointer to the EFI System Table.

Return Value:

    EFI_SUCCESS on success.

--*/
{
    EFI_STATUS              status;

    //
    // Install driver model protocol(s).
    //
    status = EfiLibInstallDriverBindingComponentName2(ImageHandle,
                                                      SystemTable,
                                                      &gSynthKeyDriverBinding,
                                                      ImageHandle,
                                                      &gSynthKeyComponentName,
                                                      &gSynthKeyComponentName2);

    ASSERT_EFI_ERROR(status);

    return status;
}


EFI_STATUS
EFIAPI
SynthKeyDriverSupported(
    IN          EFI_DRIVER_BINDING_PROTOCOL *This,
    IN          EFI_HANDLE                   DeviceCandidate,
    IN          EFI_DEVICE_PATH_PROTOCOL    *RemainingDevicePath
    )
/*++

Routine Description:

    Test if the device is a Hyper-V supported synthetic Keyboard Controller.

Arguments:

    This                Pointer EFI_DRIVER_BINDING_PROTOCOL register by the
                        keyboard driver at start up.

    DeviceCandidate     Device/Controller to determine support for

    RemainingDevicePath UNUSED - children device path

Return Value:

    EFI_UNSUPPORTED controller is not synthetic keyboard
    EFI_SUCCESS     controller is synthetic keyboard

--*/
{
    EFI_STATUS status;

    ASSERT(This == &gSynthKeyDriverBinding);

    //
    // First verify that the device supports the VMBUS protocol
    // EMCL calls require this.
    //
    status = gBS->OpenProtocol(DeviceCandidate,
                               &gEfiVmbusProtocolGuid,
                               NULL,
                               This->DriverBindingHandle,
                               DeviceCandidate,
                               EFI_OPEN_PROTOCOL_TEST_PROTOCOL);

    if (EFI_ERROR(status))
    {
        return status;
    }

    status = EmclChannelTypeSupported(DeviceCandidate,
                                      &gSyntheticKeyboardClassGuid,
                                      This->DriverBindingHandle);

    if (!(EFI_ERROR(status)))
    {
        DEBUG((EFI_D_VERBOSE, "--- %a: synthetic keyboard device found - handle %p \n", __FUNCTION__, DeviceCandidate));
    }

    return status;
}


EFI_STATUS
EFIAPI
SynthKeyDriverStart(
    IN          EFI_DRIVER_BINDING_PROTOCOL *This,
    IN          EFI_HANDLE                   Controller,
    IN          EFI_DEVICE_PATH_PROTOCOL    *RemainingDevicePath
    )
/*++

Routine Description:

    Create and initialize a synthetic keyboard instance for a given controller.

Arguments:

    This                Pointer of EFI_DRIVER_BINDING_PROTOCOL registered by the
                        keyboard driver at start up.

    Controller          Driver controller handle of the device to start

    RemainingDevicePath UNUSED - Children's device path

Return Value:

    EFI_SUCCESS on success
    Other EFI_STATUS values on failure.

--*/
{
    PSYNTH_KEYBOARD_DEVICE  pDevice = NULL;
    EFI_STATUS              status;
    BOOLEAN                 emclInstalled = FALSE;

    ASSERT(This == &gSynthKeyDriverBinding);

    DEBUG((EFI_D_VERBOSE, "--- %a: synthetic keyboard starting - handle %p \n", __FUNCTION__, Controller));

    //
    // Install and open the EMCL protocol. This will be used for vmbus communication.
    // EmclInstallProtocol may return EFI_ALREADY_STARTED if this instance was
    // already started, in which case we will return immediately.
    //
    status = EmclInstallProtocol(Controller);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    emclInstalled = TRUE;

    //
    // Allocate the private device context. Do this as early as possible
    // so we can use common cleanup which assumes a device context was created.
    //
    pDevice = AllocateZeroPool(sizeof(SYNTH_KEYBOARD_DEVICE));

    if (pDevice == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        DEBUG((EFI_D_ERROR, "--- %a: failed to allocate memory - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    pDevice->Signature  = SYNTH_KEYBOARD_DEVICE_SIGNATURE;
    pDevice->Handle     = Controller;

    //
    // Get an instance of the DevicePathProtocol, this will be used for
    // reporting device status as the driver starts and stops.
    //
    status = gBS->OpenProtocol(Controller,
                               &gEfiDevicePathProtocolGuid,
                               (VOID **) &pDevice->DevicePath,
                               This->DriverBindingHandle,
                               Controller,
                               EFI_OPEN_PROTOCOL_BY_DRIVER);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    SynthKeyReportStatus(pDevice, EFI_PROGRESS_CODE, EFI_P_PC_ENABLE);

    status = gBS->OpenProtocol(Controller,
                               &gEfiEmclProtocolGuid,
                               (VOID **) &pDevice->Emcl,
                               This->DriverBindingHandle,
                               Controller,
                               EFI_OPEN_PROTOCOL_BY_DRIVER);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to open the Emcl protocol - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    //
    // Device start for VMBUS devices is close to presense detect
    // (It will attempt to open the channel, etc.)
    //
    SynthKeyReportStatus(pDevice, EFI_PROGRESS_CODE, EFI_P_PC_PRESENCE_DETECT);

    // -------------- Device Specific
    //
    // Perform Device specific initialization
    // This will setup the interfaces and needed info
    //
    status = SimpleTextInInitialize(pDevice);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    return status;

Cleanup:

    DEBUG((EFI_D_ERROR, "--- %a: failed to start the synthetic keyboard - %r \n", __FUNCTION__, status));

    if (pDevice)
    {
        if (pDevice->DevicePath)
        {
            SynthKeyReportStatus(pDevice, EFI_ERROR_CODE, EFI_P_EC_CONTROLLER_ERROR);
        }

        SynthKeyDriverCleanup(pDevice);
    }

    if (emclInstalled)
    {
        EmclUninstallProtocol(pDevice->Handle);
    }

    return status;
}

VOID
SynthKeyDriverCleanup(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice
    )
/*++

Routine Description:

    Performs common cleanup for a synthetic keyboard device.
    This function assumes the device context was at least allocated, but
    handles and pointers contained within the context may or may not be valid.

Arguments:

    pDevice         Keyboard instance to cleanup

    This            Protocol instance pointer register by the keyboard driver at
                    start up.

Return Value:

    None.

--*/
{
    ASSERT(pDevice);

    //
    // SimpleTextInCleanup can be called always and will
    // handle partial or no-initialization.
    //
    SimpleTextInCleanup(pDevice);

    if (pDevice->DevicePath)
    {
        gBS->CloseProtocol(pDevice->Handle,
                           &gEfiDevicePathProtocolGuid,
                           gSynthKeyDriverBinding.DriverBindingHandle,
                           pDevice->Handle);
    }

    if (pDevice->Emcl)
    {
        gBS->CloseProtocol(pDevice->Handle,
                           &gEfiEmclProtocolGuid,
                           gSynthKeyDriverBinding.DriverBindingHandle,
                           pDevice->Handle);
    }

    gBS->FreePool(pDevice);
}


EFI_STATUS
EFIAPI
SynthKeyDriverStop(
  IN            EFI_DRIVER_BINDING_PROTOCOL *This,
  IN            EFI_HANDLE                   Controller,
  IN            UINTN                        NumberOfChildren,
  IN            EFI_HANDLE                  *ChildHandleBuffer
  )
/*++

Routine Description:

    Stops an instance of the driver.

Arguments:

    This              Protocol instance pointer register by the keyboard driver at
                      start up.

    ControllerHandle  Handle of device being stopped.

    NumberOfChildren  UNUSED - Number of Handles in ChildHandleBuffer. If number of
                      children is zero stop the entire bus driver.

    ChildHandleBuffer UNUSED - List of Child Handles to Stop.

Return Value:

    EFI_SUCCESS if the driver is successfully stopped.
    EFI_STATUS on error.

--*/
{
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *conIn;
    PSYNTH_KEYBOARD_DEVICE          pDevice;
    EFI_STATUS                      status;

    ASSERT(This == &gSynthKeyDriverBinding);

    DEBUG((EFI_D_VERBOSE, "--- %a: synthetic keyboard stopping - handle %p \n", __FUNCTION__, Controller));

    // ------------------- Device Specific

    //
    // Attempt to get our instance of simple text in. From there
    // we'll get the device context.
    //
    status = gBS->OpenProtocol(Controller,
                               &gEfiSimpleTextInProtocolGuid,
                               (VOID **) &conIn,
                               This->DriverBindingHandle,
                               Controller,
                               EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (EFI_ERROR(status))
    {
        return status;
    }

    status = gBS->OpenProtocol(Controller,
                               &gEfiSimpleTextInputExProtocolGuid,
                               NULL,
                               This->DriverBindingHandle,
                               Controller,
                               EFI_OPEN_PROTOCOL_TEST_PROTOCOL);   // TODO: Why EFI_OPEN_PROTOCOL_TEST_PROTOCOL here???


    if (EFI_ERROR(status))
    {
        return status;
    }

    pDevice = SYNTH_KEYBOARD_DEVICE_FROM_THIS(conIn);

    ASSERT(pDevice->Handle == Controller);

    //
    // Report that the keyboard is being disabled
    //
    SynthKeyReportStatus(pDevice, EFI_PROGRESS_CODE, EFI_P_PC_DISABLE);

    //
    // Free other resources, this will call device specific cleanup as well.
    //
    SynthKeyDriverCleanup(pDevice);
    pDevice = NULL;

    EmclUninstallProtocol(Controller);

    return EFI_SUCCESS;
}
