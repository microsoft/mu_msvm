/** @file
  ARM variation the DebugTransportLib that wraps the IO implementation
  of PL011SerialPortLib to change port address.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Library/PL011UartClockLib.h>
#include <Library/PL011UartLib.h>
#include <Library/SerialPortLib.h>
#include <Library/PcdLib.h>


/**
  Initializes the debug transport if needed.

  @retval   EFI_SUCCESS   The debug transport was successfully initialized.
  @retval   Other         The debug transport initialization failed.
**/
EFI_STATUS
EFIAPI
DebugTransportInitialize (
  VOID
  )
{
  UINTN               UartBase;
  UINT64              BaudRate;
  UINT32              ReceiveFifoDepth;
  EFI_PARITY_TYPE     Parity;
  UINT8               DataBits;
  EFI_STOP_BITS_TYPE  StopBits;

  UartBase         = FixedPcdGet64 (PcdFeatureDebuggerPortUartBase);
  BaudRate         = FixedPcdGet64 (PcdUartDefaultBaudRate);
  ReceiveFifoDepth = 0;         // Use default FIFO depth
  Parity           = (EFI_PARITY_TYPE)FixedPcdGet8 (PcdUartDefaultParity);
  DataBits         = FixedPcdGet8 (PcdUartDefaultDataBits);
  StopBits         = (EFI_STOP_BITS_TYPE)FixedPcdGet8 (PcdUartDefaultStopBits);

  return PL011UartInitializePort (
           UartBase,
           PL011UartClockGetFreq (),
           &BaudRate,
           &ReceiveFifoDepth,
           &Parity,
           &DataBits,
           &StopBits
        );
}

/**
  Reads data from the debug transport.

  @param[out]   Buffer          The buffer to read the data to.
  @param[out]   NumberOfBytes   The number of bytes to read from the transport.
  @param[out]   Timeout         UNUSED

  @retval       The number of bytes read from the transport.
**/
UINTN
EFIAPI
DebugTransportRead (
  OUT UINT8  *Buffer,
  IN UINTN   NumberOfBytes,
  IN UINTN   Timeout
  )
{
  return PL011UartRead(FixedPcdGet64(PcdFeatureDebuggerPortUartBase), Buffer, NumberOfBytes);
}

/**
  Writes data to the debug transport.

  @param[out]   Buffer          The buffer of the data to be written.
  @param[out]   NumberOfBytes   The number of bytes to write to the transport.

  @retval       The number of bytes written to the transport.
**/
UINTN
EFIAPI
DebugTransportWrite (
  IN UINT8  *Buffer,
  IN UINTN  NumberOfBytes
  )
{
  return PL011UartWrite(FixedPcdGet64(PcdFeatureDebuggerPortUartBase), Buffer, NumberOfBytes);
}

/**
  Checks if there is pending data to read.

  @retval   TRUE    Data is pending read from the transport
  @retval   FALSE   There is no data is pending read from the transport
**/
BOOLEAN
EFIAPI
DebugTransportPoll (
  VOID
  )
{
  return PL011UartPoll(FixedPcdGet64(PcdFeatureDebuggerPortUartBase));
}