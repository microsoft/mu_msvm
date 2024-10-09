/** @file
  Library that provides version agnostic access to virtual machine configuration.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/BiosDeviceLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>

#include <BiosInterface.h>

UINT32
GetNfitSize(
    void
    )
/*++

Routine Description:

    Returns the size of the NFIT.

Arguments:

    None

Return Value:

    The size of the NFIT.

--*/
{
    return ReadBiosDevice(BiosConfigNfitSize);
}

void
GetNfit(
    UINT64 Address
    )
/*++

Routine Description:

    Gets the NFIT.

Arguments:

    Address - the GPA at which to write the NFIT table

Return Value:

    None.

--*/
{
    ASSERT((UINT64) Address < 0xFFFFFFFFULL);
    WriteBiosDevice(BiosConfigNfitPopulate, (UINT32) Address);
}

void
SetVpmemACPIBuffer(
    UINT64 Address
    )
/*++

Routine Description:

    Sets the pointer to the VPMem ACPI Method Buffer.

Arguments:

    None

Return Value:

    None.

--*/
{
    ASSERT((UINT64) Address < 0xFFFFFFFFULL);
    WriteBiosDevice(BiosConfigVpmemSetACPIBuffer, (UINT32) Address);
}

void
SetGenerationIdAddress(
    UINT64 Value
    )
/*++

Routine Description:

    Communicates the Generation ID memory location to the BiosDevice.

Arguments:

    Value - the GPA of the Generation ID

Return Value:

    n/a

--*/
{
    WriteBiosDevice(BiosConfigGenerationIdPtrLow, (UINT32)Value);
    WriteBiosDevice(BiosConfigGenerationIdPtrHigh, (UINT32)(Value >> 32));
}
