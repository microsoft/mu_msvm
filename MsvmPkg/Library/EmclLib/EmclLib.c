/** @file
  Utility functions for EMCL.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>

#include <Protocol/Emcl.h>
#include <Protocol/Vmbus.h>
#include <Protocol/InternalEventServices.h>

#include <Library/EmclLib.h>

typedef struct _EMCL_LIB_COMPLETION_CONTEXT
{
    EFI_EVENT Event;
    VOID *Packet;
    UINT32 PacketSize;

} EMCL_LIB_COMPLETION_CONTEXT;


extern EFI_GUID gEfiEmclTagProtocolGuid;
extern EFI_GUID gEfiVmbusChannelDevicePathGuid;

INTERNAL_EVENT_SERVICES_PROTOCOL *mInternalEventServices = NULL;


EFI_STATUS
EFIAPI
EmclInstallProtocol (
    IN  EFI_HANDLE ControllerHandle
    )
/*++

Routine Description:

    This method connects a device handle with the EMCL driver.
    The device handle has to have VMBUS protocoll installed.

Arguments:

    ControllerHandle - The handle to connect to EMCL driver.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_HANDLE handle;
    UINTN handleBufferSize;
    EFI_DRIVER_BINDING_PROTOCOL *driverBinding;
    EFI_STATUS status;

    handleBufferSize = sizeof(handle);

    status = gBS->LocateHandle(
        ByProtocol,
        &gEfiEmclTagProtocolGuid,
        NULL,
        &handleBufferSize,
        &handle);

    if (EFI_ERROR(status))
    {
        if (status == EFI_BUFFER_TOO_SMALL)
        {
            DEBUG((EFI_D_ERROR, "Multiple EMCL images found"));
        }

        goto Cleanup;
    }

    status = gBS->HandleProtocol(handle,
                                 &gEfiDriverBindingProtocolGuid,
                                 &driverBinding);

    ASSERT_EFI_ERROR(status);

    status = driverBinding->Start(driverBinding,
                                  ControllerHandle,
                                  NULL);

Cleanup:
    return status;
}


VOID
EFIAPI
EmclUninstallProtocol (
    IN  EFI_HANDLE ControllerHandle
    )
/*++

Routine Description:

    This method disconnects a device handle from the EMCL driver.

Arguments:

    ControllerHandle - The handle to disconnect from the EMCL driver.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_HANDLE handle;
    UINTN handleBufferSize;
    EFI_STATUS status;

    handle = NULL;
    handleBufferSize = sizeof(handle);

    status = gBS->LocateHandle(
        ByProtocol,
        &gEfiEmclTagProtocolGuid,
        NULL,
        &handleBufferSize,
        &handle);

    ASSERT_EFI_ERROR(status);

    //
    // DisconnectController can correctly fail here if EMCL has already been
    // uninstalled due to the VMBus protocol being uninstalled.
    //

    gBS->DisconnectController(ControllerHandle,
                              handle,
                              NULL);
}


VOID
EmclSynchronousPacketCompletion (
    IN  VOID *Context OPTIONAL,
    IN  VOID *Buffer,
    IN  UINT32 BufferLength
    )
/*++

Routine Description:

    This function is called when a packet has completed.

Arguments:

    Context - An event to be signaled when the packet is completed.

    Buffer - The completion packet received for the request.

    BufferLength - The length of the received packet.

Return Value:

    None.

--*/
{
    EMCL_LIB_COMPLETION_CONTEXT *completionContext;

    completionContext = Context;

    if (BufferLength <= completionContext->PacketSize)
    {
        CopyMem(completionContext->Packet, Buffer, BufferLength);
    }
    else
    {
        completionContext->Packet = NULL;
    }

    gBS->SignalEvent(completionContext->Event);
}



EFI_STATUS
EFIAPI
EmclSendPacketSync (
    IN  EFI_EMCL_PROTOCOL *This,
    IN  VOID *InlineBuffer,
    IN  UINT32 InlineBufferLength,
    IN  EFI_EXTERNAL_BUFFER *ExternalBuffers,
    IN  UINT32 ExternalBufferCount
    )
/*++

Routine Description:

    This method sends an EMCL packet and waits for it to complete.

Arguments:

    This - Instance of the EMCL protocol

    InlineBuffer - The buffer containing the packet

    InlineBufferLength - The length of the buffer containing the packet

    ExternalBuffers - Additional buffers that will be sent along with the packet

    ExternalBufferCount - Number of additional buffers to be sent along with the packet

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    UINTN signaledEventIndex;
    EMCL_LIB_COMPLETION_CONTEXT context;

    ZeroMem(&context, sizeof(EMCL_LIB_COMPLETION_CONTEXT));

    if (mInternalEventServices == NULL)
    {
        status = gBS->LocateProtocol(
                        &gInternalEventServicesProtocolGuid,
                        NULL,
                        (VOID **)&mInternalEventServices);
        ASSERT_EFI_ERROR(status);
    }

    status = gBS->CreateEvent(
        0,
        0,
        NULL,
        NULL,
        &context.Event);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    context.Packet = InlineBuffer;
    context.PacketSize = InlineBufferLength;

    status = This->SendPacket(
        This,
        InlineBuffer,
        InlineBufferLength,
        ExternalBuffers,
        ExternalBufferCount,
        EmclSynchronousPacketCompletion,
        &context);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    // This can be called from TPL_CALLBACK. Use WaitForEventInternal instead of gBS->WaitForEvent
    // which enforces a TPL check for TPL_APPLICATION.
    status = mInternalEventServices->WaitForEventInternal(1, &context.Event, &signaledEventIndex);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    if (context.Packet == NULL)
    {
        status = EFI_BUFFER_TOO_SMALL;
        goto Cleanup;
    }

    status = EFI_SUCCESS;

Cleanup:
    if (context.Event != NULL)
    {
        gBS->CloseEvent(context.Event);
    }

    return status;
}


EFI_STATUS
EmclChannelTypeSupported (
    IN  EFI_HANDLE ControllerHandle,
    IN  const EFI_GUID *ChannelType,
    IN  EFI_HANDLE AgentHandle
    )
/*++

Routine Description:

    This method checks if a controller supports an EMCL channel type.
    The controller handle must support VMBUS protocol.

Arguments:

    ControllerHandle - The handle of the controller to test.

    ChannelType - Channel type to be checked if supported.

    AgentHandle - The handle of the agent that is opening the protocol interface.

Return Value:

    EFI_STATUS.

--*/
{
    return EmclChannelTypeAndInstanceSupported(ControllerHandle,
        ChannelType,
        AgentHandle,
        NULL);
}

/// \brief        This method checks if a controller supports an EMCL channel
///               type and optionally with the given channel instance guid. The
///               controller handle must support VMBUS protocol.
///
/// \param[in]    ControllerHandle  The handle of the controller to test.
/// \param[in]    ChannelType       Channel type to be checked if supported.
/// \param[in]    AgentHandle       The handle of the agent that is opening the
///                                 protocol interface.
/// \param[inopt] ChannelInstance   The channel instance guid to optionally
///                                 filter on
///
/// \return       EFI_STATUS
///
EFI_STATUS
EmclChannelTypeAndInstanceSupported (
    IN  EFI_HANDLE ControllerHandle,
    IN  const EFI_GUID *ChannelType,
    IN  EFI_HANDLE AgentHandle,
    IN  const EFI_GUID *ChannelInstance OPTIONAL
    )
{
    EFI_STATUS status;
    EFI_DEVICE_PATH_PROTOCOL *devicePathNode;
    VMBUS_DEVICE_PATH *vmbusDevicePath;
    VENDOR_DEVICE_PATH *vendorDevicePath;
    BOOLEAN devicePathProtocolOpened = FALSE;

    if (AgentHandle != NULL)
    {
        status = gBS->OpenProtocol(
            ControllerHandle,
            &gEfiDevicePathProtocolGuid,
            (VOID **) &devicePathNode,
            AgentHandle,
            ControllerHandle,
            EFI_OPEN_PROTOCOL_BY_DRIVER);
    }
    else
    {
        status = gBS->OpenProtocol(
            ControllerHandle,
            &gEfiDevicePathProtocolGuid,
            (VOID **) &devicePathNode,
            NULL,
            ControllerHandle,
            EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    }

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    devicePathProtocolOpened = TRUE;
    status = EFI_UNSUPPORTED;

    while (!IsDevicePathEnd(devicePathNode))
    {
        if ((DevicePathType(devicePathNode) == HARDWARE_DEVICE_PATH) &&
            (DevicePathSubType(devicePathNode) == HW_VENDOR_DP))
        {
            vendorDevicePath = (VENDOR_DEVICE_PATH*) devicePathNode;

            if (CompareGuid(
                &vendorDevicePath->Guid,
                &gEfiVmbusChannelDevicePathGuid))
            {
                vmbusDevicePath = (VMBUS_DEVICE_PATH*) devicePathNode;
                if (CompareGuid(
                    &vmbusDevicePath->InterfaceType,
                    ChannelType))
                {
                    if (ChannelInstance != NULL)
                    {
                        if (CompareGuid(&vmbusDevicePath->InterfaceInstance,
                            ChannelInstance))
                        {
                            status = EFI_SUCCESS;
                        }
                    }
                    else
                    {
                        status = EFI_SUCCESS;
                    }

                    break;
                }
            }
        }
        devicePathNode = NextDevicePathNode(devicePathNode);
    }

Cleanup:

    if (devicePathProtocolOpened)
    {
        gBS->CloseProtocol(
            ControllerHandle,
            &gEfiDevicePathProtocolGuid,
            AgentHandle,
            ControllerHandle);
    }

    return status;
}