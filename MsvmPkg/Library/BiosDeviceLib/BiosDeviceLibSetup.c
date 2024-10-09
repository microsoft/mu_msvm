/** @file
  Library setup for BiosDeviceLib

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <PiDxe.h>

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

extern UINTN mBiosBaseAddress;
extern void SetupBaseAddress();

EFI_STATUS
EFIAPI
BiosDeviceLibConstructor (
  VOID
  )
{
    EFI_STATUS status = EFI_SUCCESS;

    SetupBaseAddress();

    return status;
}