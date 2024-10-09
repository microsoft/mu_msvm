/** @file

    This file contains types and constants shared between
    the MSFT0101 virtual TPM device and the UEFI firmware.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

//
// VTPM configuration ports. Each port is 8-bit long. Accessing 32 bits
// requires 4 ports.
//
// Use a pair of I/O ports to establish TPM 2.0 Command-Response Interface memory.
// It is expected that UEFI TPM driver sets up the GPAs for the communication buffer.
//
enum
{
    TpmControlPort         = 0x1040,   // 4 ports for 32-bit access
    TpmDataPort            = 0x1044,   // 4 ports for 32-bit access
};

enum
{
    TcgProtocolTrEE         = 0,
    TcgProtocolTcg2         = 1,
};

//
// I/O port command defintiions
//
enum
{
    //
    // It can be used for engine vs. guest version negotiation.
    // Not used.
    //
    TpmIoVersion                = 0,

    //
    // Map command-response interface buffer.
    //
    TpmIoMapSharedMemory        = 1,

    //
    // Query host if map is succeeded.
    //
    TpmIoEstablished            = 2,

    //
    // Get pending TPM operation requested by the OS.
    // PPI function Id 3.
    //
    TpmIoPPIGetPendingOperation = 3, // do not change

    //
    // Get TPM Operation Response to OS.
    // PPI function Id 5
    //
    TpmIoPPIGetLastOperation     = 5,    // do not change
    TpmIoPPIGetLastResult        = 6,    // do not change

    //
    // Set TPM operation requested by the OS.
    // PPI function Id 7
    //
    TpmIoPPISetOperation         = 7,    // do not change. This is TpmIoPPISetOperationArg3Integer1

    //
    // Get user confirmation status for operation. Used in PPI over ACPI.
    // PPI function Id 8
    //
    TpmIoPPIGetUserConfirmation  = 8,    // do not change

    //
    // The command to set PPI func ID 7 Arg3 (Package) Integer 2.
    //
    TpmIoPPISetOperationArg3Integer2  = 32,

    //
    // Get Tcg Protocol Version
    //
    TpmIoGetTcgProtocolVersion   = 64,

    //
    // Report the supported hash bitmap in TPM capability.
    //
    TpmIoCapabilityHashAlgBitmap = 65,
};
