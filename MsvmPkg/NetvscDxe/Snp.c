/** @file
  Implementation of driver entry point and driver binding protocol.

Copyright (c) 2004 - 2019, Intel Corporation. All rights reserved.<BR>
Copyright (c) Microsoft Corporation.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Snp.h"

/**
  One notified function to stop UNDI device when gBS->ExitBootServices() called.

  @param  Event                   Pointer to this event
  @param  Context                 Event handler private data

**/
VOID
EFIAPI
SnpNotifyExitBootServices (
  EFI_EVENT  Event,
  VOID       *Context
  )
{
  SNP_DRIVER  *Snp;

  Snp = (SNP_DRIVER *)Context;

  //
  // Shutdown and stop NetVsc driver
  //
  // MS_HYP_CHANGE Do NOT shutdown the driver, as this causes runtime
  // memory map changes for as-of-yet unknown reasons.
  // PxeShutdown (Snp);
  PxeStop (Snp);

  // MU_CHANGE [BEGIN] - Shutdown SnpDxe in BeforeExitBootServices
  // Since BeforeExitBootServices is run on each call, close event
  // to prevent reentry.
  gBS->CloseEvent (Event);
  // MU_CHANGE [END]
}


EFI_STATUS
AppendMac2DevPath(
    OUT EFI_DEVICE_PATH_PROTOCOL      **DevPtr,
    IN  EFI_DEVICE_PATH_PROTOCOL       *BaseDevPtr,
    IN  SNP_DRIVER                     *Snp
  )
/*++

Routine Description:

    Using the NIC data structure information, get the MAC address and then
    allocate space for a new devicepath (**DevPtr) which will contain the original device path the
    NIC was found on (*BaseDevPtr) and an added MAC node.

Arguments:

    DevPtr         Pointer which will point to the newly created device path with the MAC node attached.

    BaseDevPtr  Pointer to the device path which the device driver is latching on to.

    Snp  Pointer to the SNP_DRIVER instance..

Returns:

    EFI_SUCCESS         A MAC address was successfully appended to the Base Device Path.

    other                   Not enough resources available to create new Device Path node.

--*/
{
    EFI_MAC_ADDRESS macAddress;
    MAC_ADDR_DEVICE_PATH macAddrNode;
    EFI_DEVICE_PATH_PROTOCOL *endNode;
    UINT8 *devicePtr;
    UINT16 totalPathLen;
    UINT16 basePathLen;
    EFI_STATUS status;

    CopyMem(&macAddress, &Snp->Mode.CurrentAddress, sizeof(EFI_MAC_ADDRESS));

    //
    // Fill the mac address node first.
    //
    ZeroMem((CHAR8 *)&macAddrNode, sizeof(macAddrNode));

    //
    // The MAC address is intentionally *not* being put in this device node.
    // This is because the MAC address is not always known prior to device
    // power on in the Hyper-V host virtualizaton stack.  The virt stack is
    // constructing and modifying device paths in boot entries prior to
    // powering on this device.  There is now an explicit agreement between
    // this driver and the Hyper-V management code that this device node
    // will always contain zeros for the MAC address.
    //

    macAddrNode.Header.Type       = MESSAGING_DEVICE_PATH;
    macAddrNode.Header.SubType    = MSG_MAC_ADDR_DP;
    macAddrNode.Header.Length[0]  = sizeof(macAddrNode);
    macAddrNode.Header.Length[1]  = 0;

    //
    // Find the size of the base dev path.
    //
    endNode = BaseDevPtr;

    while (!IsDevicePathEnd(endNode))
    {
        endNode = NextDevicePathNode(endNode);
    }

    basePathLen = (UINT16)((UINTN)(endNode) - (UINTN)(BaseDevPtr));

    //
    // Create space for full dev path.
    //
    totalPathLen = (UINT16)(basePathLen + sizeof(macAddrNode) + sizeof(EFI_DEVICE_PATH_PROTOCOL));

    status = gBS->AllocatePool(
        EfiBootServicesData,
        totalPathLen,
        &devicePtr);

    if (status != EFI_SUCCESS)
    {
        goto Exit;
    }

    //
    // Copy the base path, mac addr and end_dev_path nodes.
    //

    *DevPtr = (EFI_DEVICE_PATH_PROTOCOL *)devicePtr;
    CopyMem(devicePtr, (CHAR8 *)BaseDevPtr, basePathLen);
    devicePtr += basePathLen;
    CopyMem(devicePtr, (CHAR8 *)&macAddrNode, sizeof(macAddrNode));
    devicePtr += sizeof(macAddrNode);
    CopyMem(devicePtr, (CHAR8 *)endNode, sizeof (EFI_DEVICE_PATH_PROTOCOL));

Exit:
    return status;
}

/**
  Test to see if this driver supports ControllerHandle. This service
  is called by the EFI boot service ConnectController(). In
  order to make drivers as small as possible, there are a few calling
  restrictions for this service. ConnectController() must
  follow these calling restrictions. If any other agent wishes to call
  Supported() it must also follow these calling restrictions.

  @param  This                Protocol instance pointer.
  @param  ControllerHandle    Handle of device to test.
  @param  RemainingDevicePath Optional parameter use to pick a specific child
                              device to start.

  @retval EFI_SUCCESS         This driver supports this device.
  @retval EFI_ALREADY_STARTED This driver is already running on this device.
  @retval other               This driver does not support this device.

**/
EFI_STATUS
EFIAPI
SimpleNetworkDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
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
        &gSyntheticNetworkClassGuid,
        This->DriverBindingHandle);

Exit:
  return status;
}


VOID
EFIAPI
NetvscCleanupController (
    IN  EFI_DRIVER_BINDING_PROTOCOL    *This,
    IN  EFI_HANDLE                     ControllerHandle,
    IN  BOOLEAN                        CloseDevicePathProtocol,
    IN  BOOLEAN                        CloseEmclProtocol
    )
/**

Routine Description:

    Reverse work done to root controller in SimpleNetworkDriverStart (aka. Start()).

Arguments:

    This                     Protocol instance pointer.

    ControllerHandle         Handle of controller.

    CloseDevicePathProtocol  Whether to close DevicePath protocol.

    CloseEmclProtocol        Whether to close EMCL protocol.

Return Value:

    None.

**/
{
    //
    // Close protocols on the root handle
    //
    if (CloseDevicePathProtocol)
    {
        gBS->CloseProtocol(
                ControllerHandle,
                &gEfiDevicePathProtocolGuid,
                This->DriverBindingHandle,
                ControllerHandle);
    }

    if (CloseEmclProtocol)
    {
        gBS->CloseProtocol(
                ControllerHandle,
                &gEfiEmclProtocolGuid,
                This->DriverBindingHandle,
                ControllerHandle);
    }

    EmclUninstallProtocol(ControllerHandle);
}


VOID
EFIAPI
NetvscCleanupDevice (
    IN  EFI_DRIVER_BINDING_PROTOCOL    *This,
    IN  EFI_HANDLE                     ControllerHandle,
    IN  EFI_HANDLE                  DeviceHandle OPTIONAL,
    IN  BOOLEAN                        SnpInstalled,
    IN  BOOLEAN                        DevicePathInstalled,
    IN  SNP_DRIVER                  *SnpDriver OPTIONAL,
    IN  NETVSC_ADAPTER_CONTEXT      *AdapterContext OPTIONAL
    )
/**

Routine Description:

    Reverse work done to created device handle in SimpleNetworkDriverStart (aka. Start()).

Arguments:

    This                Protocol instance pointer.

    ControllerHandle    Handle of controller.

    DeviceHandle        OPTIONAL Device handle created by Start().

    SnpInstalled        Whether SNP protocol has been installed.

    DevicePathInstalled Whether DevicePath protocol has been installed.

    SnpDriver           OPTIONAL the SNP driver object created by start()

    AdapterContext      OPTIONAL the adapter context created by NetvscDriverStart()

Return Value:

    None.

**/
{
    EFI_DEVICE_PATH_PROTOCOL *DevicePath = NULL;
    EFI_SIMPLE_NETWORK_PROTOCOL *SnpProtocol = NULL;

    if (DeviceHandle == NULL && AdapterContext != NULL)
    {
        DeviceHandle = AdapterContext->DeviceHandle;
    }

    //
    // Obtain SnpProtocol if not present
    //
    if (DeviceHandle != NULL && SnpInstalled)
    {
        ASSERT(DeviceHandle != NULL);
        gBS->OpenProtocol(
            DeviceHandle,
            &gEfiSimpleNetworkProtocolGuid,
            &SnpProtocol,
            This->DriverBindingHandle,
            ControllerHandle,
            EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    }

    //
    // Obtain SnpDriver if not present
    //
    if (SnpDriver == NULL && SnpProtocol != NULL)
    {
        SnpDriver = EFI_SIMPLE_NETWORK_DEV_FROM_THIS (SnpProtocol);
    }

    //
    // Obtain AdapterContext if not present
    //
    if (AdapterContext == NULL && SnpDriver != NULL)
    {
        AdapterContext = SnpDriver->AdapterContext;
    }

    //
    // Obtain DevicePath if not present
    //
    if (AdapterContext != NULL)
    {
        DevicePath = AdapterContext->DevPath;
    }

    //
    // Uninstall protocols on DeviceHandle
    //
    if (DeviceHandle != NULL && SnpDriver != NULL)
    {
        if (SnpInstalled && SnpProtocol != NULL)
        {
            gBS->UninstallMultipleProtocolInterfaces(
                    DeviceHandle,
                    &gEfiSimpleNetworkProtocolGuid,
                    SnpProtocol,
                    NULL
                    );
        }

        if (PcdGetBool (PcdSnpCreateExitBootServicesEvent)) {
            //
            // Close EXIT_BOOT_SERVICES Event
            //
            gBS->CloseEvent (SnpDriver->ExitBootServicesEvent);
        }

        if (AdapterContext->NicInfo.Emcl != NULL)
        {
            PxeShutdown (SnpDriver);
            PxeStop (SnpDriver);

            gBS->CloseProtocol(
                    ControllerHandle,
                    &gEfiEmclProtocolGuid,
                    This->DriverBindingHandle,
                    DeviceHandle
                    );
        }

        if (DevicePathInstalled)
        {
            gBS->UninstallMultipleProtocolInterfaces(
                    DeviceHandle,
                    &gEfiDevicePathProtocolGuid,
                    AdapterContext->DevPath,
                    NULL
                    );
        }

    }

    //
    // Free DevicePath
    //
    if (DevicePath != NULL)
    {
        gBS->FreePool(DevicePath);
    }

    //
    // Free SnpDriver
    //
    if (SnpDriver != NULL)
    {
        gBS->FreePool(SnpDriver);
    }

    //
    // Free AdapterContext
    //
    if (AdapterContext != NULL)
    {
        gBS->FreePool(AdapterContext);
    }
}


EFI_STATUS
EFIAPI
NetvscInitializeController(
    IN  EFI_DRIVER_BINDING_PROTOCOL    *This,
    IN  EFI_HANDLE                     ControllerHandle,
    OUT EFI_DEVICE_PATH_PROTOCOL      **BaseDevicePath
    )
/**

Routine Description:

    Initialize the controller. Open required EFI services
    on ControllerHandle.

Arguments:

    This                 Protocol instance pointer.

    ControllerHandle     Handle of device to bind driver to.

    BaseDevicePath       The Device Path interface opened.

Return Value:

    EFI_SUCCESS          This driver is added to ControllerHandle

    other                This driver does not support this device

**/
{
    EFI_STATUS status;
    EFI_EMCL_PROTOCOL *emclProtocol = NULL;

    //
    // Connect to EMCL.
    //
    status = EmclInstallProtocol(ControllerHandle);

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    status = gBS->OpenProtocol(
        ControllerHandle,
        &gEfiDevicePathProtocolGuid,
        (VOID **)BaseDevicePath,
        This->DriverBindingHandle,
        ControllerHandle,
        EFI_OPEN_PROTOCOL_BY_DRIVER);

    if(EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = gBS->OpenProtocol(
        ControllerHandle,
        &gEfiEmclProtocolGuid,
        (VOID **)&emclProtocol,
        This->DriverBindingHandle,
        ControllerHandle,
        EFI_OPEN_PROTOCOL_BY_DRIVER);

Cleanup:
    if (EFI_ERROR(status))
    {
        NetvscCleanupController(This, ControllerHandle, (*BaseDevicePath != NULL), (emclProtocol != NULL));
    }

Exit:
    return status;
}


EFI_STATUS
EFIAPI
NetvscCreateDevice(
    IN  EFI_DRIVER_BINDING_PROTOCOL   *This,
    IN  EFI_HANDLE                    ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL      *BaseDevicePath
    )
/**

Routine Description:

    Create a Netvsc NIC device.

Arguments:

    This                 Protocol instance pointer.

    ControllerHandle     Handle of root controller.

    BaseDevicePath       The Device Path of the root controller.

Return Value:

    EFI_SUCCESS          The NIC has been successfully created.

**/
{
    EFI_STATUS                 status;
    SNP_DRIVER                 *snpDriver = NULL;
    VOID                       *address;
    PNETVSC_ADAPTER_CONTEXT    adapterContext = NULL;
    BOOLEAN                    snpInstalled = FALSE;
    BOOLEAN                    devicePathInstalled = FALSE;

    //
    // Allocate and initialize the adapter context.
    //
    status = gBS->AllocatePool(
        EfiBootServicesData,
        sizeof (NETVSC_ADAPTER_CONTEXT),
        &adapterContext);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    ZeroMem(adapterContext, sizeof (NETVSC_ADAPTER_CONTEXT));

    adapterContext->ControllerHandle = ControllerHandle;
    adapterContext->BaseDevPath = BaseDevicePath;

    if (EFI_ERROR(status))
    {
        goto Cleanup;

    }

    //
    // Allocate and initialize a new Simple Network Protocol structure.
    //
    status = gBS->AllocatePool(
        EfiBootServicesData,
        sizeof (SNP_DRIVER),
        &address);

    if (status != EFI_SUCCESS)
    {
        DEBUG((EFI_D_ERROR, "\nCould not allocate SNP_DRIVER structure.\n"));
        goto Cleanup;
    }

    snpDriver = (SNP_DRIVER *) (UINTN) address;

    ZeroMem(snpDriver, sizeof (SNP_DRIVER));

    snpDriver->Signature  = SNP_DRIVER_SIGNATURE;

    snpDriver->Snp.Revision       = EFI_SIMPLE_NETWORK_PROTOCOL_REVISION;
    snpDriver->Snp.Start          = SnpStart;
    snpDriver->Snp.Stop           = SnpStop;
    snpDriver->Snp.Initialize     = SnpInitialize;
    snpDriver->Snp.Reset          = SnpReset;
    snpDriver->Snp.Shutdown       = SnpShutdown;
    snpDriver->Snp.ReceiveFilters = SnpReceiveFilters;
    snpDriver->Snp.StationAddress = SnpStationAddress;
    snpDriver->Snp.Statistics     = SnpStatistics;
    snpDriver->Snp.MCastIpToMac   = SnpMcastIpToMac;
    snpDriver->Snp.NvData         = SnpNvData;
    snpDriver->Snp.GetStatus      = SnpGetStatus;
    snpDriver->Snp.Transmit       = SnpTransmit;
    snpDriver->Snp.Receive        = SnpReceive;
    snpDriver->Snp.WaitForPacket  = NULL;

    snpDriver->Snp.Mode           = &snpDriver->Mode;

    snpDriver->AdapterContext     = adapterContext;

    //
    // Initialize Simple Network Protocol mode structure.
    //
    snpDriver->Mode.State               = EfiSimpleNetworkStopped;
    snpDriver->Mode.HwAddressSize       = PXE_HWADDR_LEN_ETHER;
    snpDriver->Mode.MediaHeaderSize     = PXE_MAC_HEADER_LEN_ETHER;
    snpDriver->Mode.MaxPacketSize       = MAXIMUM_ETHERNET_PACKET_SIZE;
    snpDriver->Mode.NvRamAccessSize     = 0;
    snpDriver->Mode.NvRamSize           = 0;
    snpDriver->Mode.IfType              = PXE_IFTYPE_ETHERNET;
    snpDriver->Mode.MaxMCastFilterCount = MAX_MCAST_FILTER_CNT;
    snpDriver->Mode.MCastFilterCount    = 0;

    snpDriver->Mode.MediaPresentSupported = TRUE;
    snpDriver->Mode.MediaPresent = FALSE;

    snpDriver->Mode.MacAddressChangeable = FALSE;
    snpDriver->Mode.MultipleTxSupported = FALSE;
    snpDriver->Mode.ReceiveFilterMask = EFI_SIMPLE_NETWORK_RECEIVE_UNICAST;
    snpDriver->Mode.ReceiveFilterMask |= EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS_MULTICAST;
    snpDriver->Mode.ReceiveFilterMask |= EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS;
    snpDriver->Mode.ReceiveFilterMask |= EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST;
    snpDriver->Mode.ReceiveFilterMask |= EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST;
    snpDriver->Mode.ReceiveFilterMask |= EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS_MULTICAST;
    snpDriver->Mode.ReceiveFilterSetting = 0;

    //
    // Create device handle and install SNP protocol on it
    //
    status = gBS->InstallMultipleProtocolInterfaces(
        &snpDriver->AdapterContext->DeviceHandle,
        &gEfiSimpleNetworkProtocolGuid,
        &(snpDriver->Snp),
        NULL);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    snpInstalled = TRUE;

    //
    // Open EMCL protocol on the new device handle
    //
    status = gBS->OpenProtocol(
                ControllerHandle,
                &gEfiEmclProtocolGuid,
                (VOID **)&adapterContext->NicInfo.Emcl,
                This->DriverBindingHandle,
                snpDriver->AdapterContext->DeviceHandle,
                EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Calling Snp->Start
    //
    status = SnpStart(&snpDriver->Snp);

    if (status != EFI_SUCCESS)
    {
        goto Cleanup;
    }

    //
    // The station address needs to be saved in the mode structure. We need to
    // initialize the SNP driver first for this.
    //
    status              = PxeInit (snpDriver, PXE_OPFLAGS_INITIALIZE_DO_NOT_DETECT_CABLE);

    if (EFI_ERROR(status))
    {
        SnpStop(&snpDriver->Snp);
        goto Cleanup;
    }

    status = PxeGetStnAddr(snpDriver);

    if (status != EFI_SUCCESS)
    {
        DEBUG ((EFI_D_ERROR, "\nSnp->get_station_addr() failed.\n"));
        PxeShutdown (snpDriver);
        PxeStop (snpDriver);
        goto Cleanup;
    }

    //
    // We should not leave SNP started and initialized here.
    // The NetVsc layer will be started when upper layers call Snp->start.
    // How ever, this DriverStart() must fill up the snp mode structure which
    // contains the MAC address of the NIC. For this reason we started and
    // initialized SNP here, now we are done, do a shutdown and stop of the
    // NetVsc interface.
    //
    PxeShutdown (snpDriver);
    PxeStop (snpDriver);

    if (PcdGetBool (PcdSnpCreateExitBootServicesEvent)) {
        //
        // Create EXIT_BOOT_SERIVES Event
        //
        status = gBS->CreateEventEx (
                        EVT_NOTIFY_SIGNAL,
                        TPL_CALLBACK,
                        SnpNotifyExitBootServices,
                        snpDriver,
                        &gEfiEventBeforeExitBootServicesGuid,   // MU_CHANGE - Shutdown SnpDxe in BeforeExitBootServices
                        &snpDriver->ExitBootServicesEvent
                        );

        if (EFI_ERROR (status)) {
            DEBUG((EFI_D_ERROR, "--- %a: failed to create the exit boot services event - %r \n", __FUNCTION__, status));
            goto Cleanup;
        }
    }

    status = AppendMac2DevPath(
        &snpDriver->AdapterContext->DevPath,
        snpDriver->AdapterContext->BaseDevPath,
        snpDriver);

    if(EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Install the device path protocol to the device handle
    //
    status = gBS->InstallMultipleProtocolInterfaces(
        &snpDriver->AdapterContext->DeviceHandle,
        &gEfiDevicePathProtocolGuid,
        snpDriver->AdapterContext->DevPath,
        NULL);

    if(EFI_ERROR(status))
    {
        goto Cleanup;
    }

    devicePathInstalled = TRUE;

Cleanup:
    if (EFI_ERROR(status))
    {
        NetvscCleanupDevice(
            This,
            ControllerHandle,
            NULL,
            snpInstalled,
            devicePathInstalled,
            snpDriver,
            adapterContext);
    }

    return status;
}

/**
  Start this driver on ControllerHandle. This service is called by the
  EFI boot service ConnectController(). In order to make
  drivers as small as possible, there are a few calling restrictions for
  this service. ConnectController() must follow these
  calling restrictions. If any other agent wishes to call Start() it
  must also follow these calling restrictions.

  @param  This                 Protocol instance pointer.
  @param  ControllerHandle     Handle of device to bind driver to.
  @param  RemainingDevicePath  Optional parameter use to pick a specific child
                               device to start.

  @retval EFI_SUCCESS          This driver is added to ControllerHandle
  @retval EFI_DEVICE_ERROR     This driver could not be started due to a device error
  @retval other                This driver does not support this device

**/
EFI_STATUS
EFIAPI
SimpleNetworkDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
    EFI_STATUS                 status;
    EFI_DEVICE_PATH_PROTOCOL      *baseDevicePath;

    status = NetvscInitializeController(This, Controller, &baseDevicePath);

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    status = NetvscCreateDevice(This, Controller, baseDevicePath);

    if (EFI_ERROR(status))
    {
        NetvscCleanupController(This, Controller, TRUE, TRUE);
        goto Exit;
    }

    status = EFI_SUCCESS;

Exit:
    return status;
}


/**
  Stop this driver on ControllerHandle. This service is called by the
  EFI boot service DisconnectController(). In order to
  make drivers as small as possible, there are a few calling
  restrictions for this service. DisconnectController()
  must follow these calling restrictions. If any other agent wishes
  to call Stop() it must also follow these calling restrictions.

  @param  This              Protocol instance pointer.
  @param  ControllerHandle  Handle of device to stop driver on
  @param  NumberOfChildren  Number of Handles in ChildHandleBuffer. If number of
                            children is zero stop the entire bus driver.
  @param  ChildHandleBuffer List of Child Handles to Stop.

  @retval EFI_SUCCESS       This driver is removed ControllerHandle
  @retval other             This driver was not removed from this device

**/
EFI_STATUS
EFIAPI
SimpleNetworkDriverStop(
    IN  EFI_DRIVER_BINDING_PROTOCOL    *This,
    IN  EFI_HANDLE                     ControllerHandle,
    IN  UINTN                          NumberOfChildren,
    IN  EFI_HANDLE                     *ChildHandleBuffer
    )
{
    EFI_HANDLE deviceHandle = NULL;
    UINTN index;

    if (NumberOfChildren > 0)
    {
        //
        // Stop the created NIC device. Only one NIC device should be
        // created.
        //
        ASSERT(NumberOfChildren == 1);

        for (index = 0; index < NumberOfChildren; index++)
        {
            deviceHandle = ChildHandleBuffer[index];
            NetvscCleanupDevice(This, ControllerHandle, deviceHandle, TRUE, TRUE, NULL, NULL);
        }
    }
    else
    {
        //
        // Stop the root controller.
        //
        NetvscCleanupController(This, ControllerHandle, TRUE, TRUE);
    }

    return EFI_SUCCESS;
}

//
// Simple Network Protocol Driver Global Variables
//
EFI_DRIVER_BINDING_PROTOCOL mSimpleNetworkDriverBinding = {
    SimpleNetworkDriverSupported,
    SimpleNetworkDriverStart,
    SimpleNetworkDriverStop,
    0xa,
    NULL,
    NULL
};

/**
  The SNP driver entry point.

  @param ImageHandle       The driver image handle.
  @param SystemTable       The system table.

  @retval EFI_SUCCESS      Initialization routine has found UNDI hardware,
                           loaded it's ROM, and installed a notify event for
                           the Network Identifier Interface Protocol
                           successfully.
  @retval Other            Return value from HandleProtocol for
                           DeviceIoProtocol or LoadedImageProtocol

**/
EFI_STATUS
EFIAPI
InitializeSnpDriver(
    IN  EFI_HANDLE       ImageHandle,
    IN  EFI_SYSTEM_TABLE *SystemTable
    )
{
    return EfiLibInstallDriverBindingComponentName2(
        ImageHandle,
        SystemTable,
        &mSimpleNetworkDriverBinding,
        ImageHandle,
        &gSimpleNetworkComponentName,
        &gSimpleNetworkComponentName2);
}

