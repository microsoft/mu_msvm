/** @file
  AArch64 hardware entropy support for the OEM0 ACPI table.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include "AcpiPlatform.h"

EFI_STATUS
Oem0GetHardwareEntropy (
  OUT UINT8  *Buffer,
  IN  UINTN  BufferSize
  )
{
  return EFI_UNSUPPORTED;
}
