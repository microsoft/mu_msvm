/** @file

  Provides the implementation for Hyper-V serial ports.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent


**/

#include "Serial.h"

EFI_GUID gMsvmSerialBusProtocolGuid = MSVM_SERIAL_BUS_PROTOCOL_GUID;

//
// The instance of the Driver Binding Protocol for the image handle.
//
EFI_DRIVER_BINDING_PROTOCOL gSerialDriver =
{
    SerialDriverSupported,
    SerialDriverStart,
    SerialDriverStop,
    0xa,
    NULL,
    NULL
};


//
// Starting template for Serial Device objects.
//
SERIAL_DEVICE gSerialDeviceTempate =
{
    SERIAL_DEVICE_SIGNATURE,
    NULL,
    FALSE,
    { // SerialIo
        SERIAL_IO_INTERFACE_REVISION,
        SerialReset,
        SerialSetAttributes,
        SerialSetControl,
        SerialGetControl,
        SerialWrite,
        SerialRead,
        NULL
    },
    { // SerialMode
        SERIAL_PORT_SUPPORT_CONTROL_MASK,
        SERIAL_PORT_DEFAULT_TIMEOUT,
        FixedPcdGet64(PcdUartDefaultBaudRate),     // BaudRate
        SERIAL_PORT_DEFAULT_RECEIVE_FIFO_DEPTH,
        FixedPcdGet8(PcdUartDefaultDataBits),      // DataBits
        FixedPcdGet8(PcdUartDefaultParity),        // Parity
        FixedPcdGet8(PcdUartDefaultStopBits)       // StopBits
    },
    NULL,
    { // UartDevicePath
        {
            MESSAGING_DEVICE_PATH,
            MSG_UART_DP,
            {
                (UINT8)(sizeof(UART_DEVICE_PATH)),
                (UINT8)((sizeof(UART_DEVICE_PATH)) >> 8)
            }
        },
        0,
        FixedPcdGet64(PcdUartDefaultBaudRate),
        FixedPcdGet8(PcdUartDefaultDataBits),
        FixedPcdGet8(PcdUartDefaultParity),
        FixedPcdGet8(PcdUartDefaultStopBits)
    },
    0,    //BaseAddress
    FALSE,
#if defined(MDE_CPU_X64)
    Uart16550A,
#elif defined(MDE_CPU_AARCH64)
    UartPL011,
#endif
    NULL
};


//
// Starting templates for the Serial Port protocol instances.
//
#if defined(MDE_CPU_X64)
SERIAL_DEVICE_PROPERTIES gSerialProperties[] =
{
    // COM1
    {
        0x3F8,              // BaseAddress
        EISA_PNP_ID(0x501), // HID
        1                   // UID
    },
    // COM2
    {
        0x2F8,              // BaseAddress
        EISA_PNP_ID(0x501), // HID
        2                   // UID
    }
};
#elif defined(MDE_CPU_AARCH64)
SERIAL_DEVICE_PROPERTIES gSerialProperties[] =
{
    // COM1
    {
        0xEFFEC000,         // BaseAddress
        EISA_PNP_ID(0x501), // HID  TODO: This ID probably isn't correct.
        1                   // UID
    },
    // COM2
    {
        0xEFFEB000,         // BaseAddress
        EISA_PNP_ID(0x501), // HID  TODO: This ID probably isn't correct.
        2                   // UID
    }
};
#endif

#if defined(MDE_CPU_X64)
#define UARTINITIALIZEPORT  PCUartInitializePort
#define UARTGETCONTROL      PCUartGetControl
#define UARTSETCONTROL      PCUartSetControl
#define UARTLIBREAD         PCUartRead
#define UARTLIBWRITE        PCUartWrite
#elif defined(MDE_CPU_AARCH64)
#define UARTINITIALIZEPORT  PL011UartInitializePort
#define UARTGETCONTROL      PL011UartGetControl
#define UARTSETCONTROL      PL011UartSetControl
#define UARTLIBREAD         PL011UartRead
#define UARTLIBWRITE        PL011UartWrite
#endif

//
// Pcd config value
//
UINT32 gUartClkInHz =
#if defined(MDE_CPU_X64)
                      FixedPcdGet32(PcdSerialClockRate);
#elif defined(MDE_CPU_AARCH64)
                      FixedPcdGet32(PL011UartClkInHz);
#endif


//
// The handle of the dummy root device.
//
EFI_HANDLE gRootDevice = NULL;


//
// Configuration state.
//
BOOLEAN gSerialEnabled = FALSE;
BOOLEAN gDebuggerEnabled = FALSE;
UINT32  gConsoleMode = ConfigLibConsoleModeDefault;


BOOLEAN
IsUartFlowControlNode(
    IN  UART_FLOW_CONTROL_DEVICE_PATH *FlowControl
    )
/*++

Routine Description:

    Check if a device path node is a Flow Control node.

Arguments:

    FlowControl             A pointer to a device path node.

Return Value:

    TRUE                    The node is the flow control node.

    FALSE                   Otherwise.

--*/
{
    return (BOOLEAN)((DevicePathType(FlowControl) == MESSAGING_DEVICE_PATH) &&
                     (DevicePathSubType(FlowControl) == MSG_VENDOR_DP) &&
                     (CompareGuid(&FlowControl->Guid, &gEfiUartDevicePathGuid)));
}


BOOLEAN
ContainsFlowControl(
    IN  EFI_DEVICE_PATH_PROTOCOL    *DevicePath
    )
/*++

Routine Description:

    Check if a device path contains a Flow Control node.

Arguments:

    DevicePath              A pointer to a device path.

Return Value:

    TRUE                    The path contains a flow control node.

    FALSE                   Otherwise.

--*/
{
    while (!IsDevicePathEnd(DevicePath))
    {
        if (IsUartFlowControlNode((UART_FLOW_CONTROL_DEVICE_PATH *) DevicePath))
        {
            return TRUE;
        }
        DevicePath = NextDevicePathNode(DevicePath);
    }
    return FALSE;
}


VOID
SerialDestroyChildDevice(
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE                  ParentController,
    IN  SERIAL_DEVICE               *SerialDevice OPTIONAL
    )
/*++

Routine Description:

    Destroys a SERIAL_DEVICE object. The SERIAL_DEVICE doesn't
    need to be fully constructed so this can be used for error cleanup.

Arguments:

    This - Pointer to the driver binding protocol.

    ParentController - Device handle of parent device.

    SerialDevice - Pointer to the SERIAL_DEVICE object to destroy.
                   If null this routine is a no-op.

Return Value:

    none

--*/
{
    EFI_STATUS status;

    if (SerialDevice == NULL)
    {
        //
        // Nothing to do.
        //
        return;
    }

    if (SerialDevice->Handle != NULL)
    {
        //
        // Close the protocol opened BY_CHILD_CONTROLLER.
        //
        if (SerialDevice->BusProtocolOpened)
        {
            status = gBS->CloseProtocol(ParentController,
                                        &gMsvmSerialBusProtocolGuid,
                                        This->DriverBindingHandle,
                                        SerialDevice->Handle);
            DEBUG((DEBUG_INFO, "SerialDriverStop(child): CloseProtocol %r\n", status));
        }
        //
        // Remove the protocols from the child handle. This should delete the handle.
        //
        if (SerialDevice->Handle != NULL)
        {
            status = gBS->UninstallMultipleProtocolInterfaces(SerialDevice->Handle,
                                                              &gEfiDevicePathProtocolGuid,
                                                              SerialDevice->DevicePath,
                                                              &gEfiSerialIoProtocolGuid,
                                                              &SerialDevice->SerialIo,
                                                              NULL);
            DEBUG((DEBUG_INFO, "SerialDriverStop(child): UninstallMPIs %r\n", status));
        }
    }

    if (SerialDevice->DevicePath != NULL)
    {
        gBS->FreePool(SerialDevice->DevicePath);
    }
    FreeUnicodeStringTable(SerialDevice->ControllerNameTable);
    gBS->FreePool(SerialDevice);

}


EFI_STATUS
SerialCreateChildDevice(
    IN  EFI_DRIVER_BINDING_PROTOCOL    *This,
    IN  EFI_HANDLE                     ParentController,
    IN  SERIAL_DEVICE_PROPERTIES       *Properties
    )
/*++

Routine Description:

    Creates a SerialPortDevice object.

Arguments:

    This - Pointer to the driver binding protocol.

    ParentController - Device handle of parent device.

    Properties - The properties of this serial port.

Return Value:

    EFI_STATUS

--*/
{
    EFI_STATUS                  status;
    SERIAL_DEVICE               *serialDevice = NULL;
    EFI_DEVICE_PATH_PROTOCOL    *tempDevicePath = NULL;
    EFI_DEV_PATH                node;
    VOID                        *protocol;

    //
    // Initialize a child serial device instance
    //
    serialDevice = AllocateCopyPool(sizeof(SERIAL_DEVICE), &gSerialDeviceTempate);
    if (serialDevice == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    serialDevice->SerialIo.Mode       = &(serialDevice->SerialMode);
    serialDevice->BaseAddress         = Properties->BaseAddress;
    serialDevice->HardwareFlowControl = FALSE;

    //
    // Construct the child name.
    //
    AddName(serialDevice, Properties);

    //
    // Build a device path and add it to the device structure.
    //
    node.DevPath.Type = ACPI_DEVICE_PATH;
    node.DevPath.SubType = ACPI_DP;
    SetDevicePathNodeLength(&node.DevPath, sizeof(ACPI_HID_DEVICE_PATH));
    node.Acpi.HID = Properties->HID;
    node.Acpi.UID = Properties->UID;

    tempDevicePath = AppendDevicePathNode(NULL, &node.DevPath);
    if (tempDevicePath == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }
    serialDevice->DevicePath = AppendDevicePathNode(tempDevicePath,
                                    (EFI_DEVICE_PATH_PROTOCOL *)&serialDevice->UartDevicePath);
    if (serialDevice->DevicePath == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    //
    // Fill in Serial I/O Mode structure based on defaults.
    //
    serialDevice->SerialMode.BaudRate = serialDevice->UartDevicePath.BaudRate;
    serialDevice->SerialMode.DataBits = serialDevice->UartDevicePath.DataBits;
    serialDevice->SerialMode.Parity   = serialDevice->UartDevicePath.Parity;
    serialDevice->SerialMode.StopBits = serialDevice->UartDevicePath.StopBits;

    //
    // Issue a reset to initialize the COM port
    //
    status = serialDevice->SerialIo.Reset(&serialDevice->SerialIo);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Create child handle and install protocol interfaces for the serial device.
    //
    status = gBS->InstallMultipleProtocolInterfaces(&serialDevice->Handle,
                                                    &gEfiDevicePathProtocolGuid,
                                                    serialDevice->DevicePath,
                                                    &gEfiSerialIoProtocolGuid,
                                                    &serialDevice->SerialIo,
                                                    NULL);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Open the bus protocol BY_CHILD_CONTROLLER so the relationship
    // to the parent handle is tracked.
    //
    status = gBS->OpenProtocol(ParentController,
                               &gMsvmSerialBusProtocolGuid,
                               &protocol, // returns null, state tracked in device
                               This->DriverBindingHandle,
                               serialDevice->Handle,
                               EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }
    serialDevice->BusProtocolOpened = TRUE;

Cleanup:

    if (tempDevicePath != NULL)
    {
        gBS->FreePool(tempDevicePath);
    }

    if (EFI_ERROR(status))
    {
        SerialDestroyChildDevice(This, ParentController, serialDevice);
    }

    return status;
}


EFI_STATUS
EFIAPI
SerialEntryPoint(
    IN  EFI_HANDLE         ImageHandle,
    IN  EFI_SYSTEM_TABLE   *SystemTable
    )
/*++

Routine Description:

    Entry point into this driver.

Arguments:

    ImageHandle - Handle of the driver image.

    SystemTable - EFI system table.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_DEVICE_PATH_PROTOCOL *devicePath;

    //
    // Install the driver model protocol(s) on the image handle.
    //
    status = EfiLibInstallDriverBindingComponentName2(ImageHandle,
                                                      SystemTable,
                                                      &gSerialDriver,
                                                      ImageHandle,
                                                      &gSerialComponentName,
                                                      &gSerialComponentName2);
    if (EFI_ERROR(status))
    {
        return status;
    }

    //
    // Get the serial port and UEFI debugger configuration.
    //
    gSerialEnabled = PcdGetBool(PcdSerialControllersEnabled);
    gDebuggerEnabled = PcdGetBool(PcdDebuggerEnabled);
    gConsoleMode = PcdGet8(PcdConsoleMode);

    //
    // Do nothing and return success if the serial ports are not configured.
    //
    if (!gSerialEnabled)
    {
        return EFI_SUCCESS;
    }

    //
    // Create a root handle with the device path protocol and a tag protocol
    //
    devicePath = AppendDevicePathNode(NULL, NULL); // empty device path
    if (devicePath == NULL)
    {
        return EFI_OUT_OF_RESOURCES;
    }
    status = gBS->InstallMultipleProtocolInterfaces(&gRootDevice,
                                                    &gEfiDevicePathProtocolGuid,
                                                    devicePath,
                                                    &gMsvmSerialBusProtocolGuid,
                                                    NULL,
                                                    NULL);

    return status;
}


EFI_STATUS
EFIAPI
SerialDriverSupported(
    IN  EFI_DRIVER_BINDING_PROTOCOL    *This,
    IN  EFI_HANDLE                     ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
    )
/*++

Routine Description:

    Check to see if this driver supports the given controller

Arguments:

    This                    A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.

    ControllerHandle        The handle of the controller to test.

    RemainingDevicePath     A pointer to the remaining portion of a device path.


Return Value:

    EFI_SUCCESS             This driver can support the given controller

    EFI_ALREADY_STARTED     A driver already is managing this controller.

    EFI_UNSUPPORTED         This driver cannot support the given controller.

--*/
{
    EFI_STATUS                status;
    EFI_DEVICE_PATH_PROTOCOL  *parentDevicePath = NULL;
    VOID                      *protocol;

    //
    // Briefly open (BY_DRIVER) the private serial bus protocol as a
    // simple way to determine if the ControllerHandle is our device and
    // that it is already started.
    //
    status = gBS->OpenProtocol(ControllerHandle,
                               &gMsvmSerialBusProtocolGuid,
                               &protocol, // required but returns NULL
                               This->DriverBindingHandle,
                               ControllerHandle,
                               EFI_OPEN_PROTOCOL_BY_DRIVER);
    if (EFI_ERROR(status))
    {
        return status;
    }
    gBS->CloseProtocol(ControllerHandle,
                       &gMsvmSerialBusProtocolGuid,
                       This->DriverBindingHandle,
                       ControllerHandle);

    //
    // Test if the Device Path protocol is available. It is required.
    //
    status = gBS->OpenProtocol(ControllerHandle,
                               &gEfiDevicePathProtocolGuid,
                               (VOID **)&parentDevicePath,
                               This->DriverBindingHandle,
                               ControllerHandle,
                               EFI_OPEN_PROTOCOL_TEST_PROTOCOL);
    if (EFI_ERROR(status))
    {
        return status;
    }

    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
SerialDriverStart(
    IN  EFI_DRIVER_BINDING_PROTOCOL    *This,
    IN  EFI_HANDLE                     Controller,
    IN  EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
    )
/*++

Routine Description:

    Start managing a controller.

Arguments:

    This                    A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.

    Controller              The handle of the controller to manage.

    RemainingDevicePath     A pointer to the remaining portion of a device path.

Return Value:

    EFI_SUCCESS             Driver started successfully.

    EFI_ALREADY_STARTED     A driver already is managing this controller.

--*/
{
    EFI_STATUS                  status;
    VOID                        *protocol;
    UINT32                      index;

    //
    // Open the bus tag protocol to indicate the driver is now managing
    // the root device handle.
    //
    status = gBS->OpenProtocol(Controller,
                               &gMsvmSerialBusProtocolGuid,
                               (VOID **) &protocol,
                               This->DriverBindingHandle,
                               Controller,
                               EFI_OPEN_PROTOCOL_BY_DRIVER
                               );
    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    //
    // Create the child handles.
    //
    for (index = 0; index < ARRAY_SIZE(gSerialProperties); index++)
    {
        //
        // Don't create the first child handle (COM1) if the UEFI debugger is enabled
        // or the port is not configured as the console.
        //
        if ((index == 0) && (gDebuggerEnabled || (gConsoleMode != ConfigLibConsoleModeCOM1)))
        {
            continue;
        }
        //
        // Don't create the second child handle (COM2) if the port is not
        // configured as the console.
        //
        if ((index == 1) && (gConsoleMode != ConfigLibConsoleModeCOM2))
        {
            continue;
        }
        SerialCreateChildDevice(This, Controller, &gSerialProperties[index]);
    }

Exit:

    return status;
}


EFI_STATUS
EFIAPI
SerialDriverStop(
    IN  EFI_DRIVER_BINDING_PROTOCOL     *This,
    IN  EFI_HANDLE                      Controller,
    IN  UINTN                           NumberOfChildren,
    IN  EFI_HANDLE                      *ChildHandleBuffer
    )
/*++

Routine Description:

    Disconnect this driver from a controller and uninstall related protocol instances.

Arguments:

    This                    A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.

    Controller              The handle of the controller to stop.

    NumberOfChildren        The Number of child devices. Can be zero to indicate
                            stop managing the Controller.

    ChildHandleBuffer       An array of child device handles. Null if NumberOfChildren
                            is zero.

Return Value:

    EFI_SUCCESS             Driver stopped successfully.

--*/
{
    EFI_STATUS              status;
    UINTN                   index;
    BOOLEAN                 allChildrenStopped;
    EFI_SERIAL_IO_PROTOCOL  *serialIo;
    SERIAL_DEVICE           *serialDevice;

    DEBUG((DEBUG_VERBOSE, "SerialDriverStop: ControllerHandle %x\n", Controller));
    DEBUG((DEBUG_VERBOSE, "                  NumberOfChildren %x\n", NumberOfChildren));
    for (index = 0; index < NumberOfChildren; index++)
    {
    DEBUG((DEBUG_VERBOSE, "                  ChildHandle      %x\n", ChildHandleBuffer[index]));
    }

    //
    // Check if stopping child device handles or the main controller handle.
    //
    if (NumberOfChildren == 0)
    {
        //
        // Close the tag protocol on the controller handle.
        //
        status = gBS->CloseProtocol(Controller,
                                    &gMsvmSerialBusProtocolGuid,
                                    This->DriverBindingHandle,
                                    Controller);
        return status;
    }

    allChildrenStopped = TRUE;

    for (index = 0; index < NumberOfChildren; index++)
    {
        //
        // Get a pointer to the Serial IO protocol in order to offset to the device structure.
        //
        status = gBS->OpenProtocol(ChildHandleBuffer[index],
                                   &gEfiSerialIoProtocolGuid,
                                   (VOID **) &serialIo,
                                   This->DriverBindingHandle,
                                   Controller,
                                   EFI_OPEN_PROTOCOL_GET_PROTOCOL
                                   );

        if (!EFI_ERROR(status))
        {
            //
            // Destroy the child device.
            //
            serialDevice = SERIAL_DEVICE_FROM_THIS(serialIo);
            SerialDestroyChildDevice(This, Controller, serialDevice);
        }
        else
        {
            allChildrenStopped = FALSE;
        }
    }

    if (!allChildrenStopped)
    {
        return EFI_DEVICE_ERROR;
    }

    return EFI_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
// EFI_SERIAL_IO_PROTOCOL
//

EFI_STATUS
EFIAPI
SerialReset(
    IN  EFI_SERIAL_IO_PROTOCOL  *This
    )
/*++

Routine Description:

    Resets a serial device.

Arguments:

    This                A pointer to an EFI_SERIAL_IO_PROTOCOL

Return Value:

    EFI_SUCCESS         Reset successfully.

    EFI_DEVICE_ERROR    Failed to reset.

--*/
{
    EFI_STATUS          status;
    SERIAL_DEVICE       *serialDevice;
    UINT64              BaudRate;
    UINT32              ReceiveFifoDepth;
    EFI_PARITY_TYPE     Parity;
    UINT8               DataBits;
    EFI_STOP_BITS_TYPE  StopBits;
    EFI_TPL             tpl;

    serialDevice = SERIAL_DEVICE_FROM_THIS(This);

    BaudRate = FixedPcdGet64 (PcdUartDefaultBaudRate);
    ReceiveFifoDepth = 0;         // Use default FIFO depth
    Parity = (EFI_PARITY_TYPE)FixedPcdGet8 (PcdUartDefaultParity);
    DataBits = FixedPcdGet8 (PcdUartDefaultDataBits);
    StopBits = (EFI_STOP_BITS_TYPE) FixedPcdGet8 (PcdUartDefaultStopBits);


    tpl = gBS->RaiseTPL(TPL_NOTIFY);

    //
    // Reset hardware to defaults.
    //
    status = UARTINITIALIZEPORT(serialDevice->BaseAddress,
                                gUartClkInHz,
                                &BaudRate,
                                &ReceiveFifoDepth,
                                &Parity,
                                &DataBits,
                                &StopBits);

    gBS->RestoreTPL(tpl);

    return status;
}


EFI_STATUS
EFIAPI
SerialSetAttributes(
    IN  EFI_SERIAL_IO_PROTOCOL  *This,
    IN  UINT64                  BaudRate,
    IN  UINT32                  ReceiveFifoDepth,
    IN  UINT32                  Timeout,
    IN  EFI_PARITY_TYPE         Parity,
    IN  UINT8                   DataBits,
    IN  EFI_STOP_BITS_TYPE      StopBits
    )
/*++

Routine Description:

      Set new attributes to a serial device.

Arguments:

    This                    Pointer to EFI_SERIAL_IO_PROTOCOL

    BaudRate                The baudrate of the serial device

    ReceiveFifoDepth        The depth of receive FIFO buffer

    Timeout                 The request timeout for a single char

    Parity                  The type of parity used in serial device

    DataBits                Number of databits used in serial device

    StopBits                Number of stopbits used in serial device


Return Value:

    EFI_SUCCESS             The new attributes were set

    EFI_INVALID_PARAMETERS  One or more attributes have an unsupported value

    EFI_UNSUPPORTED         Data Bits can not set to 5 or 6

    EFI_DEVICE_ERROR        The serial device is not functioning correctly (no return)

--*/
{
    EFI_STATUS                status;
    SERIAL_DEVICE             *serialDevice;
    UART_DEVICE_PATH          *uart;
    EFI_TPL                   tpl;

    serialDevice = SERIAL_DEVICE_FROM_THIS(This);

    //
    // Check for default settings and fill in actual values.
    //
    if (BaudRate == 0)
    {
        BaudRate = PcdGet64(PcdUartDefaultBaudRate);
    }

    if (ReceiveFifoDepth == 0)
    {
        ReceiveFifoDepth = SERIAL_PORT_DEFAULT_RECEIVE_FIFO_DEPTH;
    }

    if (Timeout == 0)
    {
        Timeout = SERIAL_PORT_DEFAULT_TIMEOUT;
    }

    if (Parity == DefaultParity)
    {
        Parity = (EFI_PARITY_TYPE)PcdGet8(PcdUartDefaultParity);
    }

    if (DataBits == 0)
    {
        DataBits = PcdGet8(PcdUartDefaultDataBits);
    }

    if (StopBits == DefaultStopBits)
    {
        StopBits = (EFI_STOP_BITS_TYPE) PcdGet8(PcdUartDefaultStopBits);
    }
    //
    // 5 and 6 data bits can not be verified on a 16550A UART
    // Return EFI_INVALID_PARAMETER if an attempt is made to use these settings.
    //
    if ((DataBits == 5) || (DataBits == 6))
    {
        return EFI_INVALID_PARAMETER;
    }
    //
    // Make sure all parameters are valid
    //
    if ((BaudRate > SERIAL_PORT_MAX_BAUD_RATE) || (BaudRate < SERIAL_PORT_MIN_BAUD_RATE))
    {
        return EFI_INVALID_PARAMETER;
    }
    //
    // 50,75,110,134,150,300,600,1200,1800,2000,2400,3600,4800,7200,9600,19200,
    // 38400,57600,115200
    //
    if (BaudRate < 75)
    {
        BaudRate = 50;
    }
    else if (BaudRate < 110)
    {
        BaudRate = 75;
    }
    else if (BaudRate < 134)
    {
        BaudRate = 110;
    }
    else if (BaudRate < 150)
    {
        BaudRate = 134;
    }
    else if (BaudRate < 300)
    {
        BaudRate = 150;
    }
    else if (BaudRate < 600)
    {
        BaudRate = 300;
    }
    else if (BaudRate < 1200)
    {
        BaudRate = 600;
    }
    else if (BaudRate < 1800)
    {
        BaudRate = 1200;
    }
    else if (BaudRate < 2000)
    {
        BaudRate = 1800;
    }
    else if (BaudRate < 2400)
    {
        BaudRate = 2000;
    }
    else if (BaudRate < 3600)
    {
        BaudRate = 2400;
    } else if (BaudRate < 4800)
    {
        BaudRate = 3600;
    }
    else if (BaudRate < 7200)
    {
        BaudRate = 4800;
    }
    else if (BaudRate < 9600)
    {
        BaudRate = 7200;
    }
    else if (BaudRate < 19200)
    {
        BaudRate = 9600;
    }
    else if (BaudRate < 38400)
    {
        BaudRate = 19200;
    }
    else if (BaudRate < 57600)
    {
        BaudRate = 38400;
    }
    else if (BaudRate < 115200)
    {
        BaudRate = 57600;
    }
    else if (BaudRate <= SERIAL_PORT_MAX_BAUD_RATE)
    {
        BaudRate = 115200;
    }

    if ((ReceiveFifoDepth < 1) || (ReceiveFifoDepth > SERIAL_PORT_MAX_RECEIVE_FIFO_DEPTH))
    {
        return EFI_INVALID_PARAMETER;
    }

    if ((Timeout < SERIAL_PORT_MIN_TIMEOUT) || (Timeout > SERIAL_PORT_MAX_TIMEOUT))
    {
        return EFI_INVALID_PARAMETER;
    }

    if ((Parity < NoParity) || (Parity > SpaceParity))
    {
        return EFI_INVALID_PARAMETER;
    }

    if ((DataBits < 5) || (DataBits > 8))
    {
        return EFI_INVALID_PARAMETER;
    }

    if ((StopBits < OneStopBit) || (StopBits > TwoStopBits))
    {
        return EFI_INVALID_PARAMETER;
    }

    //
    // for DataBits = 6,7,8, StopBits can not set OneFiveStopBits
    //
    if ((DataBits >= 6) && (DataBits <= 8) && (StopBits == OneFiveStopBits))
    {
        return EFI_INVALID_PARAMETER;
    }

    tpl = gBS->RaiseTPL(TPL_NOTIFY);

    //
    // set the hardware
    //
    status = UARTINITIALIZEPORT(serialDevice->BaseAddress,
                                gUartClkInHz,
                                &BaudRate,
                                &ReceiveFifoDepth,
                                &Parity,
                                &DataBits,
                                &StopBits);
    if (EFI_ERROR(status))
    {
        gBS->RestoreTPL(tpl);
        return status;
    }

    //
    // Set the Serial I/O mode values
    //
    This->Mode->BaudRate          = BaudRate;
    This->Mode->ReceiveFifoDepth  = ReceiveFifoDepth;
    This->Mode->Timeout           = Timeout;
    This->Mode->Parity            = Parity;
    This->Mode->DataBits          = DataBits;
    This->Mode->StopBits          = StopBits;

    //
    // See if Device Path Node has actually changed
    //
    if (serialDevice->UartDevicePath.BaudRate == BaudRate &&
        serialDevice->UartDevicePath.DataBits == DataBits &&
        serialDevice->UartDevicePath.Parity == Parity &&
        serialDevice->UartDevicePath.StopBits == StopBits)
    {
        gBS->RestoreTPL(tpl);
        return EFI_SUCCESS;
    }
    //
    // Update the device path
    //
    serialDevice->UartDevicePath.BaudRate = BaudRate;
    serialDevice->UartDevicePath.DataBits = DataBits;
    serialDevice->UartDevicePath.Parity   = (UINT8) Parity;
    serialDevice->UartDevicePath.StopBits = (UINT8) StopBits;

    status = EFI_SUCCESS;
    if (serialDevice->Handle != NULL)
    {
        uart = (UART_DEVICE_PATH *)((UINTN) serialDevice->DevicePath - END_DEVICE_PATH_LENGTH);
        CopyMem(uart, &serialDevice->UartDevicePath, sizeof(UART_DEVICE_PATH));
        status = gBS->ReinstallProtocolInterface(serialDevice->Handle,
                                                 &gEfiDevicePathProtocolGuid,
                                                 serialDevice->DevicePath,
                                                 serialDevice->DevicePath);
    }

    gBS->RestoreTPL(tpl);

    return status;
}


EFI_STATUS
EFIAPI
SerialSetControl(
    IN  EFI_SERIAL_IO_PROTOCOL  *This,
    IN  UINT32                  Control
    )
/*++

Routine Description:

    Set Control Bits.

Arguments:

    This                    Pointer to EFI_SERIAL_IO_PROTOCOL

    Control                 Control bits that can be settable

Return Value:

    EFI_SUCCESS       New Control bits were set successfully

    EFI_UNSUPPORTED   The Control bits to set are not supported

--*/
{
    SERIAL_DEVICE                 *serialDevice;
    EFI_TPL                       tpl;
    UART_FLOW_CONTROL_DEVICE_PATH *flowControl;
    EFI_STATUS                    status;

    serialDevice = SERIAL_DEVICE_FROM_THIS(This);

    tpl = gBS->RaiseTPL(TPL_NOTIFY);

    serialDevice->HardwareFlowControl = FALSE;

    status = UARTSETCONTROL(serialDevice->BaseAddress, Control);

    if (EFI_ERROR(status))
    {
        gBS->RestoreTPL(tpl);
        return status;
    }

    if ((Control & EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE) ==
         EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE)
    {
        serialDevice->HardwareFlowControl = TRUE;
    }

    status = EFI_SUCCESS;
    if (serialDevice->Handle != NULL)
    {
        flowControl = (UART_FLOW_CONTROL_DEVICE_PATH *)(
                        (UINTN) serialDevice->DevicePath
                        - END_DEVICE_PATH_LENGTH
                        + sizeof(UART_DEVICE_PATH)
                        );
        if (IsUartFlowControlNode(flowControl) &&
            ((ReadUnaligned32(&flowControl->FlowControlMap) == UART_FLOW_CONTROL_HARDWARE)
                ^ serialDevice->HardwareFlowControl))
        {
            //
            // Flow Control setting is changed, need to reinstall device path protocol
            //
            WriteUnaligned32(&flowControl->FlowControlMap,
                             serialDevice->HardwareFlowControl ? UART_FLOW_CONTROL_HARDWARE : 0);
            status = gBS->ReinstallProtocolInterface(serialDevice->Handle,
                                                     &gEfiDevicePathProtocolGuid,
                                                     serialDevice->DevicePath,
                                                     serialDevice->DevicePath);
        }
    }

    gBS->RestoreTPL(tpl);

    return status;
}


EFI_STATUS
EFIAPI
SerialGetControl(
    IN  EFI_SERIAL_IO_PROTOCOL  *This,
    OUT UINT32                  *Control
    )
/*++

Routine Description:

    Get Control Bits.

Arguments:

    This            Pointer to EFI_SERIAL_IO_PROTOCOL

    Control         Control bits of the serial device

Return Value:

    EFI_SUCCESS     Control bits were gotten successfully

--*/
{
    SERIAL_DEVICE   *serialDevice;
    EFI_STATUS      status;
    EFI_TPL         tpl;

    serialDevice = SERIAL_DEVICE_FROM_THIS(This);

    *Control= 0;

    tpl= gBS->RaiseTPL(TPL_NOTIFY);

    status = UARTGETCONTROL(serialDevice->BaseAddress, Control);

    gBS->RestoreTPL(tpl);

    return status;
}


EFI_STATUS
EFIAPI
SerialWrite(
    IN      EFI_SERIAL_IO_PROTOCOL  *This,
    IN OUT  UINTN                   *BufferSize,
    IN      VOID                    *Buffer
    )
/*++

Routine Description:

    Write the specified number of bytes to serial device.

Arguments:

    This            Pointer to EFI_SERIAL_IO_PROTOCOL

    BufferSize      On input the size of Buffer, on output the amount of
                    data actually written.

    Buffer          The buffer of data to write

Return Value:

    EFI_SUCCESS        The data bytes were written successfully

    EFI_DEVICE_ERROR   The device reported an error

    EFI_TIMEOUT        The write operation was stopped due to timeout

--*/
{
    SERIAL_DEVICE  *serialDevice;
    EFI_TPL        tpl;

    serialDevice  = SERIAL_DEVICE_FROM_THIS(This);

    if (*BufferSize == 0)
    {
        return EFI_SUCCESS;
    }

    if (Buffer == NULL)
    {
        return EFI_DEVICE_ERROR;
    }

    tpl = gBS->RaiseTPL(TPL_NOTIFY);

    *BufferSize = UARTLIBWRITE(serialDevice->BaseAddress, (UINT8*)Buffer, *BufferSize);

    gBS->RestoreTPL(tpl);

    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
SerialRead(
    IN     EFI_SERIAL_IO_PROTOCOL  *This,
    IN OUT UINTN                   *BufferSize,
    OUT    VOID                    *Buffer
    )
/*++

Routine Description:

    Read the specified number of bytes from serial device.

Arguments:

    This            Pointer to EFI_SERIAL_IO_PROTOCOL

    BufferSize      On input the size of Buffer, on output the amount of
                    data returned in the buffer.

    Buffer          The buffer to return the data into

Return Value:

    EFI_SUCCESS        The data bytes were read successfully

    EFI_DEVICE_ERROR   The device reported an error

    EFI_TIMEOUT        The read operation was stopped due to timeout

--*/
{
    SERIAL_DEVICE   *serialDevice;
    UINT32          index;
    UINT8           *charBuffer;
    UINTN           elapsed;
    EFI_TPL         tpl;

    serialDevice = SERIAL_DEVICE_FROM_THIS(This);
    elapsed = 0;

    if (*BufferSize == 0)
    {
        return EFI_SUCCESS;
    }

    if (Buffer == NULL)
    {
        return EFI_DEVICE_ERROR;
    }

    tpl = gBS->RaiseTPL(TPL_NOTIFY);

    charBuffer = (UINT8 *)Buffer;

    for (index = 0; index < *BufferSize; index++)
    {
        while(UARTLIBREAD(serialDevice->BaseAddress, &(charBuffer[index]), 1) != 1)
        {
            if (elapsed > This->Mode->Timeout)
            {
                *BufferSize = index;
                gBS->RestoreTPL(tpl);
                return EFI_TIMEOUT;
            }

            gBS->Stall(TIMEOUT_STALL_INTERVAL);
            elapsed += TIMEOUT_STALL_INTERVAL;
        }

        //
        //  Successful read so reset timeout
        //
        elapsed = 0;
    }

    gBS->RestoreTPL(tpl);

    return EFI_SUCCESS;
}
