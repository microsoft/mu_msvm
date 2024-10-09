/** @file

  Provides the header for the Hyper-V serial ports.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent


**/

#pragma once

#include <Protocol/SerialIo.h>
#include <Protocol/DevicePath.h>

#include <Library/DebugLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/PcdLib.h>
#include <UefiConstants.h>
#if defined (MDE_CPU_X64)
#include <Library/PCUart.h>
#elif defined(MDE_CPU_AARCH64)
#include <Library/PL011UartLib.h>
#endif
#include "MsvmSerial.h"

//
// Driver Binding Externs
//
extern EFI_DRIVER_BINDING_PROTOCOL  gSerialDriver;
extern EFI_COMPONENT_NAME_PROTOCOL  gSerialComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL gSerialComponentName2;

//
// Internal Data Structures
//
#define SERIAL_DEVICE_SIGNATURE  SIGNATURE_32('s', 'e', 'r', 'd')
#define SERIAL_MAX_BUFFER_SIZE   16
#define TIMEOUT_STALL_INTERVAL   10

//
//  Name:   SERIAL_DEV_FIFO
//  Purpose:  To define Receive FIFO and Transmit FIFO
//  Context:  Used by serial data transmit and receive
//  Fields:
//      First UINT32: The index of the first data in array Data[]
//      Last  UINT32: The index, which you can put a new data into array Data[]
//      Surplus UINT32: Identify how many data you can put into array Data[]
//      Data[]  UINT8 : An array, which used to store data
//
typedef struct
{
    UINT32  First;
    UINT32  Last;
    UINT32  Surplus;
    UINT8   Data[SERIAL_MAX_BUFFER_SIZE];
} SERIAL_DEV_FIFO;

typedef enum
{
    Uart8250  = 0,
    Uart16450 = 1,
    Uart16550 = 2,
    Uart16550A= 3,
    UartPL011 = 4,
} EFI_UART_TYPE;

typedef struct
{
    UINTN  BaseAddress;
    UINT32 HID;
    UINT32 UID;
} SERIAL_DEVICE_PROPERTIES;


//
//  Name:   SERIAL_DEVICE
//  Purpose:  To provide device specific information
//  Context:
//  Fields:
//      Signature UINTN: The identity of the serial device
//      SerialIo  SERIAL_IO_PROTOCOL: Serial I/O protocol interface
//      SerialMode  SERIAL_IO_MODE:
//      DevicePath  EFI_DEVICE_PATH_PROTOCOL *: Device path of the serial device
//      Handle      EFI_HANDLE: The handle instance attached to serial device
//      BaseAddress UINT16: The base address of specific serial device
//      Receive     SERIAL_DEV_FIFO: The FIFO used to store data,
//                  which is received by UART
//      Transmit    SERIAL_DEV_FIFO: The FIFO used to store data,
//                  which you want to transmit by UART
//      SoftwareLoopbackEnable BOOLEAN:
//      Type    EFI_UART_TYPE: Specify the UART type of certain serial device
//
typedef struct
{
    UINTN                                  Signature;
    EFI_HANDLE                             Handle;
    BOOLEAN                                BusProtocolOpened;
    EFI_SERIAL_IO_PROTOCOL                 SerialIo;
    EFI_SERIAL_IO_MODE                     SerialMode;
    EFI_DEVICE_PATH_PROTOCOL               *DevicePath;
    UART_DEVICE_PATH                       UartDevicePath;
    UINTN                                  BaseAddress;
    BOOLEAN                                HardwareFlowControl;
    EFI_UART_TYPE                          Type;
    EFI_UNICODE_STRING_TABLE               *ControllerNameTable;
} SERIAL_DEVICE;

#define SERIAL_DEVICE_FROM_THIS(a) CR(a, SERIAL_DEVICE, SerialIo, SERIAL_DEVICE_SIGNATURE)

//
// Serial Driver Defaults
//
#define SERIAL_PORT_DEFAULT_RECEIVE_FIFO_DEPTH  1
#define SERIAL_PORT_DEFAULT_TIMEOUT             1000000
#define SERIAL_PORT_SUPPORT_CONTROL_MASK        (EFI_SERIAL_CLEAR_TO_SEND                | \
                                                 EFI_SERIAL_DATA_SET_READY               | \
                                                 EFI_SERIAL_RING_INDICATE                | \
                                                 EFI_SERIAL_CARRIER_DETECT               | \
                                                 EFI_SERIAL_REQUEST_TO_SEND              | \
                                                 EFI_SERIAL_DATA_TERMINAL_READY          | \
                                                 EFI_SERIAL_HARDWARE_LOOPBACK_ENABLE     | \
                                                 EFI_SERIAL_SOFTWARE_LOOPBACK_ENABLE     | \
                                                 EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE | \
                                                 EFI_SERIAL_OUTPUT_BUFFER_EMPTY          | \
                                                 EFI_SERIAL_INPUT_BUFFER_EMPTY)


//
// (24000000/13)MHz input clock
//
#define SERIAL_PORT_INPUT_CLOCK 1843200

//
// 115200 baud with rounding errors
//
#define SERIAL_PORT_MAX_BAUD_RATE           115200
#define SERIAL_PORT_MIN_BAUD_RATE           50

#define SERIAL_PORT_MAX_RECEIVE_FIFO_DEPTH  16
#define SERIAL_PORT_MIN_TIMEOUT             1         // 1 uS
#define SERIAL_PORT_MAX_TIMEOUT             100000000 // 100 seconds

//
// Prototypes
// Driver model protocol interface
//
EFI_STATUS
EFIAPI
SerialDriverSupported(
    IN  EFI_DRIVER_BINDING_PROTOCOL     *This,
    IN  EFI_HANDLE                      Controller,
    IN  EFI_DEVICE_PATH_PROTOCOL        *RemainingDevicePath
    );

EFI_STATUS
EFIAPI
SerialDriverStart(
    IN  EFI_DRIVER_BINDING_PROTOCOL     *This,
    IN  EFI_HANDLE                      Controller,
    IN  EFI_DEVICE_PATH_PROTOCOL        *RemainingDevicePath
    );

EFI_STATUS
EFIAPI
SerialDriverStop(
    IN   EFI_DRIVER_BINDING_PROTOCOL    *This,
    IN   EFI_HANDLE                     Controller,
    IN   UINTN                          NumberOfChildren,
    IN   EFI_HANDLE                     *ChildHandleBuffer
    );

//
// Serial I/O Protocol Interface
//
EFI_STATUS
EFIAPI
SerialReset(
    IN  EFI_SERIAL_IO_PROTOCOL          *This
    );

EFI_STATUS
EFIAPI
SerialSetAttributes(
    IN  EFI_SERIAL_IO_PROTOCOL          *This,
    IN  UINT64                          BaudRate,
    IN  UINT32                          ReceiveFifoDepth,
    IN  UINT32                          Timeout,
    IN  EFI_PARITY_TYPE                 Parity,
    IN  UINT8                           DataBits,
    IN  EFI_STOP_BITS_TYPE              StopBits
    );

EFI_STATUS
EFIAPI
SerialSetControl(
    IN  EFI_SERIAL_IO_PROTOCOL          *This,
    IN  UINT32                          Control
    );

EFI_STATUS
EFIAPI
SerialGetControl(
    IN  EFI_SERIAL_IO_PROTOCOL          *This,
    OUT UINT32                          *Control
    );

EFI_STATUS
EFIAPI
SerialWrite(
    IN      EFI_SERIAL_IO_PROTOCOL      *This,
    IN OUT  UINTN                       *BufferSize,
    IN      VOID                        *Buffer
    );

EFI_STATUS
EFIAPI
SerialRead(
    IN      EFI_SERIAL_IO_PROTOCOL      *This,
    IN OUT  UINTN                       *BufferSize,
    OUT     VOID                        *Buffer
    );

//
// Internal Functions
//
BOOLEAN
SerialPortPresent(
    IN  SERIAL_DEVICE                   *SerialDevice
    );

BOOLEAN
SerialFifoFull(
    IN  SERIAL_DEV_FIFO                 *Fifo
    );

BOOLEAN
SerialFifoEmpty(
    IN  SERIAL_DEV_FIFO                 *Fifo
    );

EFI_STATUS
SerialFifoAdd(
    IN  SERIAL_DEV_FIFO                 *Fifo,
    IN  UINT8                           Data
    );

EFI_STATUS
SerialFifoRemove(
    IN  SERIAL_DEV_FIFO                 *Fifo,
    OUT UINT8                           *Data
    );

EFI_STATUS
SerialReceiveTransmit(
    IN  SERIAL_DEVICE                   *SerialDevice
    );

UINT8
SerialReadPort(
    IN  UINT16                          BaseAddress,
    IN  UINT32                          Offset
    );

VOID
SerialWritePort(
    IN  UINT16                          BaseAddress,
    IN  UINT32                          Offset,
    IN  UINT8                           Data
    );


//
// EFI Component Name Functions
//
EFI_STATUS
EFIAPI
SerialComponentNameGetDriverName(
    IN  EFI_COMPONENT_NAME_PROTOCOL     *This,
    IN  CHAR8                           *Language,
    OUT CHAR16                          **DriverName
    );


EFI_STATUS
EFIAPI
SerialComponentNameGetControllerName(
    IN  EFI_COMPONENT_NAME_PROTOCOL     *This,
    IN  EFI_HANDLE                      ControllerHandle,
    IN  EFI_HANDLE                      ChildHandle OPTIONAL,
    IN  CHAR8                           *Language,
    OUT CHAR16                          **ControllerName
    );

VOID
AddName(
    IN   SERIAL_DEVICE                 *SerialDevice,
    IN   SERIAL_DEVICE_PROPERTIES      *SerialProperties
    );
