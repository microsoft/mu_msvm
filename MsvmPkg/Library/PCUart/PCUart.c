/** @file
  Serial I/O Port library functions with no library constructor/destructor

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/PCUart.h>


//
// 16550 UART register offsets and bitfields
//
#define R_UART_RXBUF          0
#define R_UART_TXBUF          0
#define R_UART_BAUD_LOW       0
#define R_UART_BAUD_HIGH      1
#define R_UART_FCR            2
#define   B_UART_FCR_FIFOE    BIT0
#define   B_UART_FCR_FIFO64   BIT5
#define R_UART_LCR            3
#define   B_UART_LCR_DLAB     BIT7
#define R_UART_MCR            4
#define   B_UART_MCR_DTRC     BIT0
#define   B_UART_MCR_RTS      BIT1
#define R_UART_LSR            5
#define   B_UART_LSR_RXRDY    BIT0
#define   B_UART_LSR_TXRDY    BIT5
#define   B_UART_LSR_TEMT     BIT6
#define R_UART_MSR            6
#define   B_UART_MSR_CTS      BIT4
#define   B_UART_MSR_DSR      BIT5
#define   B_UART_MSR_RI       BIT6
#define   B_UART_MSR_DCD      BIT7

/**

  Initialize the serial port to the specified settings.
  The serial port is re-configured only if the specified settings
  are different from the current settings.
  All unspecified settings will be set to the default values.

  @param  UartBase                The base address of the serial device.
  @param  UartClkInHz             The clock in Hz for the serial device.
                                  Ignored if the PCD PL011UartInteger is not 0
  @param  BaudRate                The baud rate of the serial device. If the
                                  baud rate is not supported, the speed will be
                                  reduced to the nearest supported one and the
                                  variable's value will be updated accordingly.
  @param  ReceiveFifoDepth        The number of characters the device will
                                  buffer on input.  Value of 0 will use the
                                  device's default FIFO depth.
  @param  Parity                  If applicable, this is the EFI_PARITY_TYPE
                                  that is computed or checked as each character
                                  is transmitted or received. If the device
                                  does not support parity, the value is the
                                  default parity value.
  @param  DataBits                The number of data bits in each character.
  @param  StopBits                If applicable, the EFI_STOP_BITS_TYPE number
                                  of stop bits per character.
                                  If the device does not support stop bits, the
                                  value is the default stop bit value.

  @retval RETURN_SUCCESS            All attributes were set correctly on the
                                    serial device.
  @retval RETURN_INVALID_PARAMETER  One or more of the attributes has an
                                    unsupported value.

**/
RETURN_STATUS
EFIAPI
PCUartInitializePort (
  IN     UINTN               UartBase,
  IN     UINT32              UartClkInHz,
  IN OUT UINT64              *BaudRate,
  IN OUT UINT32              *ReceiveFifoDepth,
  IN OUT EFI_PARITY_TYPE     *Parity,
  IN OUT UINT8               *DataBits,
  IN OUT EFI_STOP_BITS_TYPE  *StopBits
  )
{
    UINT32  Divisor;
    UINT8   LcrData;
    UINT8   LcrParity;
    UINT8   LcrStop;

    *ReceiveFifoDepth = 16;

    if ((*DataBits < 5) || (*DataBits > 8))
    {
      return RETURN_INVALID_PARAMETER;
    }
    // Map 5..8 to 0..3
    LcrData = (UINT8)(*DataBits - (UINT8)5);

    switch (*Parity)
    {
        case NoParity:
            LcrParity = 0;
            break;

        case EvenParity:
            LcrParity = 3;
            break;

        case OddParity:
            LcrParity = 1;
            break;

        case SpaceParity:
            LcrParity = 7;
            break;

        case MarkParity:
            LcrParity = 5;
            break;

        default:
            return RETURN_INVALID_PARAMETER;
    }

    switch (*StopBits)
    {
        case OneStopBit:
            LcrStop = 0;
            break;

        case OneFiveStopBits:
        case TwoStopBits:
            LcrStop = 1;
            break;

        default:
            return RETURN_INVALID_PARAMETER;
    }

    // Calculate divisor for baud generator
    //    Ref_Clk_Rate / Baud_Rate / 16
    Divisor = UartClkInHz / ((UINT32)(*BaudRate) * 16);
    if ((UartClkInHz % ((UINT32)(*BaudRate) * 16)) >= ((UINT32)(*BaudRate) * 8))
    {
        Divisor++;
    }

    // Configure baud rate
    IoWrite8(UartBase + R_UART_LCR, B_UART_LCR_DLAB);
    IoWrite8(UartBase + R_UART_BAUD_HIGH, (UINT8) (Divisor >> 8));
    IoWrite8(UartBase + R_UART_BAUD_LOW, (UINT8) (Divisor & 0xff));

    // Clear DLAB and configure Data Bits, Parity, and Stop Bits.
    // Strip reserved bits from line control value
    LcrData = (UINT8) ((LcrParity << 3) | (LcrStop << 2) | LcrData);
    IoWrite8(UartBase + R_UART_LCR, (UINT8) (LcrData & 0x3F));

    return RETURN_SUCCESS;

}

/**

  Assert or deassert the control signals on a serial port.
  The following control signals are set according their bit settings :
  . Request to Send
  . Data Terminal Ready

  @param[in]  UartBase  UART registers base address
  @param[in]  Control   The following bits are taken into account :
                        . EFI_SERIAL_REQUEST_TO_SEND : assert/deassert the
                          "Request To Send" control signal if this bit is
                          equal to one/zero.
                        . EFI_SERIAL_DATA_TERMINAL_READY : assert/deassert
                          the "Data Terminal Ready" control signal if this
                          bit is equal to one/zero.
                        . EFI_SERIAL_HARDWARE_LOOPBACK_ENABLE : enable/disable
                          the hardware loopback if this bit is equal to
                          one/zero.
                        . EFI_SERIAL_SOFTWARE_LOOPBACK_ENABLE : not supported.
                        . EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE : not supported.

  @retval  RETURN_SUCCESS      The new control bits were set on the device.
  @retval  RETURN_UNSUPPORTED  The device does not support this operation.

**/
RETURN_STATUS
EFIAPI
PCUartSetControl(
    IN UINTN   UartBase,
    IN UINT32  Control
    )
{
    UINT8 mcr;

    if ((Control & (~(EFI_SERIAL_REQUEST_TO_SEND | EFI_SERIAL_DATA_TERMINAL_READY))) != 0)
    {
        return RETURN_UNSUPPORTED;
    }

    mcr = IoRead8(UartBase + R_UART_MCR);
    mcr &= (~(B_UART_MCR_DTRC | B_UART_MCR_RTS));

    if ((Control & EFI_SERIAL_DATA_TERMINAL_READY) == EFI_SERIAL_DATA_TERMINAL_READY)
    {
        mcr |= B_UART_MCR_DTRC;
    }
    if ((Control & EFI_SERIAL_REQUEST_TO_SEND) == EFI_SERIAL_REQUEST_TO_SEND)
    {
        mcr |= B_UART_MCR_RTS;
    }
    IoWrite8(UartBase + R_UART_MCR, mcr);

    return RETURN_SUCCESS;
}

/**

  Retrieve the status of the control bits on a serial device.

  @param[in]   UartBase  UART registers base address
  @param[out]  Control   Status of the control bits on a serial device :

                         . EFI_SERIAL_DATA_CLEAR_TO_SEND,
                           EFI_SERIAL_DATA_SET_READY,
                           EFI_SERIAL_RING_INDICATE,
                           EFI_SERIAL_CARRIER_DETECT,
                           EFI_SERIAL_REQUEST_TO_SEND,
                           EFI_SERIAL_DATA_TERMINAL_READY
                           are all related to the DTE (Data Terminal Equipment)
                           and DCE (Data Communication Equipment) modes of
                           operation of the serial device.
                         . EFI_SERIAL_INPUT_BUFFER_EMPTY : equal to one if the
                           receive buffer is empty, 0 otherwise.
                         . EFI_SERIAL_OUTPUT_BUFFER_EMPTY : equal to one if the
                           transmit buffer is empty, 0 otherwise.
                         . EFI_SERIAL_HARDWARE_LOOPBACK_ENABLE : equal to one if
                           the hardware loopback is enabled (the ouput feeds the
                           receive buffer), 0 otherwise.
                         . EFI_SERIAL_SOFTWARE_LOOPBACK_ENABLE : equal to one if
                           a loopback is accomplished by software, 0 otherwise.
                         . EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE : equal to
                           one if the hardware flow control based on CTS (Clear
                           To Send) and RTS (Ready To Send) control signals is
                           enabled, 0 otherwise.

  @retval RETURN_SUCCESS  The control bits were read from the serial device.

**/
RETURN_STATUS
EFIAPI
PCUartGetControl(
    IN UINTN     UartBase,
    OUT UINT32  *Control
    )
{
    UINT8 msr;
    UINT8 mcr;
    UINT8 lsr;

    *Control = 0;

    msr = IoRead8(UartBase + R_UART_MSR);

    if ((msr & B_UART_MSR_CTS) == B_UART_MSR_CTS)
    {
        *Control |= EFI_SERIAL_CLEAR_TO_SEND;
    }
    if ((msr & B_UART_MSR_DSR) == B_UART_MSR_DSR)
    {
        *Control |= EFI_SERIAL_DATA_SET_READY;
    }
    if ((msr & B_UART_MSR_RI) == B_UART_MSR_RI)
    {
        *Control |= EFI_SERIAL_RING_INDICATE;
    }
    if ((msr & B_UART_MSR_DCD) == B_UART_MSR_DCD)
    {
        *Control |= EFI_SERIAL_CARRIER_DETECT;
    }

    mcr = IoRead8(UartBase + R_UART_MCR);

    if ((mcr & B_UART_MCR_DTRC) == B_UART_MCR_DTRC)
    {
        *Control |= EFI_SERIAL_DATA_TERMINAL_READY;
    }
    if ((mcr & B_UART_MCR_RTS) == B_UART_MCR_RTS)
    {
        *Control |= EFI_SERIAL_REQUEST_TO_SEND;
    }

    lsr = IoRead8(UartBase + R_UART_LSR);

    if ((lsr & (B_UART_LSR_TEMT | B_UART_LSR_TXRDY)) == (B_UART_LSR_TEMT | B_UART_LSR_TXRDY))
    {
        *Control |= EFI_SERIAL_OUTPUT_BUFFER_EMPTY;
    }
    if ((lsr & B_UART_LSR_RXRDY) == 0)
    {
        *Control |= EFI_SERIAL_INPUT_BUFFER_EMPTY;
    }

    return RETURN_SUCCESS;
}

/**
  Write data to serial device.

  @param  Buffer           Point of data buffer which need to be written.
  @param  NumberOfBytes    Number of bytes in the Buffer.

  @retval 0                Write data failed.
  @retval !0               Actual number of bytes written to serial device.

**/
UINTN
EFIAPI
PCUartWrite (
  IN  UINTN    UartBase,
  IN UINT8     *Buffer,
  IN UINTN     NumberOfBytes
  )
{
    UINT8* CONST Final = &Buffer[NumberOfBytes];

    while (Buffer < Final)
    {
        // Wait until transmit holding register is empty
        while((IoRead8(UartBase + R_UART_LSR) & B_UART_LSR_TXRDY) == 0);
        IoWrite8(UartBase + R_UART_TXBUF, *Buffer++);
    }
    return NumberOfBytes;
}

/**
  Read data from serial device and save the data in buffer.

  @param  Buffer           Point of data buffer which need to be written.
  @param  NumberOfBytes    Size of the buffer.

  @retval 0                Read data failed.
  @retval !0               Actual number of bytes read from serial device.

**/
UINTN
EFIAPI
PCUartRead(
    IN  UINTN     UartBase,
    OUT UINT8     *Buffer,
    IN  UINTN     NumberOfBytes
    )
{
    UINTN   Count;

    for (Count = 0; (Count < NumberOfBytes) && PCUartPoll(UartBase); Count++, Buffer++)
    {
        *Buffer = IoRead8(UartBase + R_UART_RXBUF);
    }
    return Count;
}

/**
  Check to see if any data is available to be read.

  @retval TRUE       At least one byte of data is available to be read
  @retval FALSE      No data is available to be read

**/
BOOLEAN
EFIAPI
PCUartPoll(
    IN  UINTN     UartBase
    )
{
    return (IoRead8(UartBase + R_UART_LSR) & B_UART_LSR_RXRDY);
}
