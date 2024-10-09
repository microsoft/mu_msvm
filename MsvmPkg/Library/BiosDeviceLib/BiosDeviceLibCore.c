/** @file
  Library for accessing the BiosDevice emulated device registers.
  Abstracts away the type of IO required from callers.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Base.h>
#include <Library/Baselib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>

// Use MMIO access on ARM64 otherwise use IO access
#if defined(MDE_CPU_AARCH64)
#define _USING_BIOS_MMIO_ 1
#elif defined(MDE_CPU_X64)
#define _USING_BIOS_MMIO_ 0
#else
#error Unsupported Architecture
#endif

// Physical and virtual device base address
UINTN       mBiosBaseAddressGpa = 0;
UINTN       mBiosBaseAddress = 0;

VOID
SetupBaseAddress()
{
    mBiosBaseAddressGpa = (UINTN)PcdGet32(PcdBiosBaseAddress);
    mBiosBaseAddress = mBiosBaseAddressGpa;
}

VOID
WriteBiosDevice(
    IN UINT32 AddressRegisterValue,
    IN UINT32 DataRegisterValue
    )
{
#if _USING_BIOS_MMIO_
    MmioWrite32(mBiosBaseAddress, AddressRegisterValue);
    MmioWrite32(mBiosBaseAddress + 4, DataRegisterValue);
#else
    IoWrite32(mBiosBaseAddress, AddressRegisterValue);
    IoWrite32(mBiosBaseAddress + 4, DataRegisterValue);
#endif
}

UINT32
ReadBiosDevice(
    IN UINT32 AddressRegisterValue
    )
{
#if _USING_BIOS_MMIO_
    MmioWrite32(mBiosBaseAddress, AddressRegisterValue);
    return MmioRead32(mBiosBaseAddress + 4);
#else
    IoWrite32(mBiosBaseAddress, AddressRegisterValue);
    return IoRead32(mBiosBaseAddress + 4);
#endif
}
