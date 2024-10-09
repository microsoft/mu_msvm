/** @file
  This module is responsible for runtime initialization of the SPCR APCI
  table.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Library/IoLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Protocol/SerialIo.h>
#include "AcpiPlatform.h"


#if defined(MDE_CPU_X64)
#define _SPCR_INTEL_
#elif defined(MDE_CPU_AARCH64)
#define _SPCR_ARM_
#else
#error Unsupported Architecture!
#endif


EFI_STATUS
SpcrInitializeTable(
    IN OUT  EFI_ACPI_DESCRIPTION_HEADER* Table
    )
/*++

Routine Description:

    Initializes the SPCR table based on configuration data.

Arguments:

    Table - The SPCR Table, expressed as an EFI_ACPI_DESCRIPTION_HEADER*.

Return Value:

    EFI_SUCCESS     if console is to be redirected and was properly initialized.
    EFI_UNSUPPORTED if console is not to be redirected. Causes table to not be added.

--*/
{
    EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE *spcr;

    //
    // Get configuration to determine if this table is needed.
    //
    UINT32 consoleMode = PcdGet8(PcdConsoleMode);
    BOOLEAN serialEnabled = PcdGetBool(PcdSerialControllersEnabled);
    BOOLEAN debuggerEnabled = PcdGetBool(PcdDebuggerEnabled);


    //
    // Serial console won't work if no serial ports.
    // Serial console on COM1 won't work if UEFI debugger enabled.
    // Default console doesn't need this table.
    //
    switch (consoleMode)
    {
        case ConfigLibConsoleModeCOM1:
            if (!serialEnabled || debuggerEnabled)
            {
                return EFI_UNSUPPORTED;
            }
            break;

        case ConfigLibConsoleModeCOM2:
            if (!serialEnabled)
            {
                return EFI_UNSUPPORTED;
            }
            break;

        case ConfigLibConsoleModeDefault:
        default:
            return EFI_UNSUPPORTED;
    }

    //
    // Init table based on config.
    //
    spcr = (EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE *)Table;

    spcr->BaseAddress.Address = (consoleMode == ConfigLibConsoleModeCOM1) ?
        FixedPcdGet32(PcdCom1RegisterBase) : FixedPcdGet32(PcdCom2RegisterBase);

#if defined(_SPCR_INTEL_)
    spcr->Irq = (consoleMode == ConfigLibConsoleModeCOM1) ?
        FixedPcdGet8(PcdCom1Vector) : FixedPcdGet8(PcdCom2Vector);
#elif defined(_SPCR_ARM_)
    spcr->GlobalSystemInterrupt = (consoleMode == ConfigLibConsoleModeCOM1) ?
        FixedPcdGet8(PcdCom1Vector) : FixedPcdGet8(PcdCom2Vector);
#endif

    switch (FixedPcdGet64(PcdUartDefaultBaudRate))
    {
        //
        // Only four specific baud rates specified as supported in the SPCR spec.
        //
        case 9600:
            spcr->BaudRate = 3;
            break;
        case 19200:
            spcr->BaudRate = 4;
            break;
        case 57600:
            spcr->BaudRate = 6;
            break;
        case 115200:
            spcr->BaudRate = 7;
            break;
        default:
            return EFI_UNSUPPORTED;
    }

    switch (FixedPcdGet8(PcdUartDefaultParity))
    {
        //
        // Only "no parity bits" specified as supported in the SPCR spec.
        //
        case DefaultParity:
        case NoParity:
            spcr->Parity = 0;
            break;
        default:
            return EFI_UNSUPPORTED;
    }

    switch (FixedPcdGet8(PcdUartDefaultStopBits))
    {
        case DefaultStopBits:
        case OneStopBit:
            spcr->StopBits = 1;
            break;
        default:
            return EFI_UNSUPPORTED;
    }

    //
    //  PcdDefaultTerminalType
    //      0-PCANSI, 1-VT100, 2-VT100+, 3-UTF8
    //  SPCR TerminalType
    //      0-VT100, 1-VT100+, 2-VTUTF8, 3-ANSI
    //
    switch (FixedPcdGet8(PcdDefaultTerminalType))
    {
        case 0:
            spcr->TerminalType = 3;
            break;
        case 1:
            spcr->TerminalType = 0;
            break;
        case 2:
            spcr->TerminalType = 1;
            break;
        case 3:
            spcr->TerminalType = 2;
            break;
        default:
            return EFI_UNSUPPORTED;
    }

    return EFI_SUCCESS;
}


