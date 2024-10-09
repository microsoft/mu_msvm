/** @file
  This module implements the UEFI debug library interface.
  It sends the strings to the Hyper-V BiosDevice via an intercept.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DebugPrintErrorLevelLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>

#include <BiosInterface.h>


// -------------------------------------------------------------------- Defines

#define DEBUG_PRINT_MAX_SIZE 1024

// ------------------------------------------------------------------ Functions

//
// Read/write Bios Device helper functions.
//
// N.B. Don't use the common library as PEI should not use mutable global
// variables, which only work in our environment because the whole UEFI image is
// located in read/write system memory. In the case of MMIO, the address space
// is identity mapped throughout PEI and does not change.
//
static
VOID
WriteBiosDevice(
    IN UINT32 AddressRegisterValue,
    IN UINT32 DataRegisterValue
    )
{
    UINTN biosBaseAddress = PcdGet32(PcdBiosBaseAddress);
#if defined(MDE_CPU_AARCH64)
    MmioWrite32(biosBaseAddress, AddressRegisterValue);
    MmioWrite32(biosBaseAddress + 4, DataRegisterValue);
#elif defined(MDE_CPU_X64)
    IoWrite32(biosBaseAddress, AddressRegisterValue);
    IoWrite32(biosBaseAddress + 4, DataRegisterValue);
#endif
}

static
UINT32
ReadBiosDevice(
    IN UINT32 AddressRegisterValue
    )
{
    UINTN biosBaseAddress = PcdGet32(PcdBiosBaseAddress);
#if defined(MDE_CPU_AARCH64)
    MmioWrite32(biosBaseAddress, AddressRegisterValue);
    return MmioRead32(biosBaseAddress + 4);
#elif defined(MDE_CPU_X64)
    IoWrite32(biosBaseAddress, AddressRegisterValue);
    return IoRead32(biosBaseAddress + 4);
#endif
}

VOID
EFIAPI
DebugPrintString(
    IN  CHAR8 *String,
    IN  UINTN Length
    );


VOID
EFIAPI
DebugPrint(
    IN  UINTN       ErrorLevel,
    IN  CONST CHAR8 *String,
    ...
    )
/*++

Routine Description:

    This function Prints a debug message to the debug output device if the specified error
    level is enabled.

    If any bit in ErrorLevel is also set in DebugPrintErrorLevelLib function
    GetDebugPrintErrorLevel (), then print the message specified by Format and the  associated
    variable argument list to the debug output device.

    If Format is NULL, then ASSERT().

Arguments:

    ErrorLevel - Supplies the error level of the debug message.

    Format - Format string for the debug message to print.

    ... - Variable argument list whose contents are accessed based on the format string
           specified by Format.

Return value:

    None.

--*/
{
    CHAR8 buffer[DEBUG_PRINT_MAX_SIZE];
    VA_LIST marker;

    ASSERT(String != NULL);

    VA_START(marker, String);
    AsciiVSPrint(buffer, sizeof(buffer), String, marker);
    VA_END (marker);

    DebugPrintString(buffer, sizeof(buffer));
}


VOID
EFIAPI
DebugAssert (
    IN  CONST CHAR8 *FileName,
    IN  UINTN       LineNumber,
    IN  CONST CHAR8 *Description
    )
/*++

Routine Description:

    Normally this breaks into the debugger with assertion status.
    In this particular library implementation it just outputs an "assert" message.

Arguments:

    FileName - The pointer to the name of the source file that generated the assert condition.

    LineNumber - The line number in the source file that generated the assert condition

    Description - The pointer to the description of the assert condition.

Return Value:

    None.

--*/
{
    CHAR8 buffer[DEBUG_PRINT_MAX_SIZE];
    UINTN length = AsciiSPrint(buffer,
                               DEBUG_PRINT_MAX_SIZE - 1,
                               "**ASSERT** FILE: %a LINE: %ull DESC: %a\n",
                               FileName,
                               LineNumber,
                               Description);

    DebugPrintString(buffer, length + 1); // Include null

    return;
}


VOID *
EFIAPI
DebugClearMemory (
    OUT VOID    *Buffer,
    IN  UINTN   Length
    )
/*++

Routine Description:

    Normally this function in an implementation of this library fills a target buffer
    with PcdDebugClearMemoryValue, and returns the target buffer.

    This function does *nothing* and returns the Buffer.

    It should not be called since DebugClearMemoryEnabled below returns FALSE.

Arguments:

    Buffer - Supplies The pointer to the target buffer to be filled with
        PcdDebugClearMemoryValue.

    Length - Supplies the number of bytes in Buffer to fill with
        PcdDebugClearMemoryValue.

Return Value:

    Cleared buffer.

--*/
{
    return Buffer;
}


BOOLEAN
EFIAPI
DebugAssertEnabled (
    VOID
    )
/*++

Routine Description:

    Returns true if assert macros are enabled.

Arguments:

    None.

Return Value:

    TRUE if assert macros are enabled, else false.

--*/
{
    return TRUE;
}


BOOLEAN
EFIAPI
DebugPrintEnabled (
    VOID
    )
/*++

Routine Description:

    Returns true if DEBUG() macros are enabled.

Arguments:

    None.

Return Value:

    TRUE if debug prints are enabled, else false.

--*/
{
    return TRUE;
}


BOOLEAN
EFIAPI
DebugCodeEnabled (
    VOID
    )
/*++

Routine Description:

    Returns true if DEBUG_CODE() macros are enabled.

Arguments:

    None.

Return Value:

    TRUE if DEBUG_CODE() macros are enabled, else false.

--*/
{
    return TRUE;
}


BOOLEAN
EFIAPI
DebugClearMemoryEnabled (
    VOID
    )
/*++

Routine Description:

    Returns true if DEBUG_CLEAR_MEMORY() macros are enabled.

Arguments:

    None.

Return Value:

    TRUE if DEBUG_CLEAR_MEMORY() macros are enabled, else false.

--*/
{
    return FALSE;
}


BOOLEAN
EFIAPI
DebugPrintLevelEnabled(
    IN  CONST UINTN        ErrorLevel
    )
/*++

Routine Description:

    Returns TRUE if any one of the bit is set both in ErrorLevel and PcdFixedDebugPrintErrorLevel.

    This function compares the bit mask of ErrorLevel and PcdFixedDebugPrintErrorLevel.

Arguments:

    ErrorLevel - the bit mask to test against the Pcd value.

Return Value:

    TRUE    Current ErrorLevel is supported.
    FALSE   Current ErrorLevel is not supported.

--*/
{
  return (BOOLEAN) ((ErrorLevel & PcdGet32(PcdDebugPrintErrorLevel)) != 0);
}


VOID
EFIAPI
DebugPrintString(
    IN  CHAR8 *String,
    IN  UINTN Length
    )
/*++

Routine Description:

    Issues a debug print command to the debugger. In this particular instance
    it sends the formatted string over to the worker process and it will
    output the string to an attached debugger.

Arguments:

    String - A pointer to the string to write.

    Length - The maximum length of the string, in bytes.

Return Value:

    None.

--*/
{
    //
    // Ensure it is null terminated.
    //
    String[Length - 1] = 0;

    //
    // Intercept the Bios VDev with the correct codepoint and buffer GPA.
    //
    WriteBiosDevice(BiosDebugOutputString, (UINT32)(UINTN)String);
}
