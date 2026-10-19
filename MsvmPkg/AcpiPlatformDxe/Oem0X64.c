/** @file
  X64 hardware entropy support for the OEM0 ACPI table.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Library/BaseMemoryLib.h>
#include "AcpiPlatform.h"

#define RDSEED_RETRY_LIMIT  100

BOOLEAN
AsmRdSeed64 (
  OUT UINT64  *Value
  );

EFI_STATUS
Oem0GetHardwareEntropy (
  OUT UINT8  *Buffer,
  IN  UINTN  BufferSize
  )
{
  UINTN   bytesToCopy;
  UINTN   retryCount;
  UINT64  randomValue;

  while (BufferSize > 0) {
    for (retryCount = 0; retryCount < RDSEED_RETRY_LIMIT; retryCount++) {
      if (AsmRdSeed64 (&randomValue)) {
        break;
      }
    }

    if (retryCount == RDSEED_RETRY_LIMIT) {
      return EFI_DEVICE_ERROR;
    }

    bytesToCopy = MIN (BufferSize, sizeof (randomValue));
    CopyMem (Buffer, &randomValue, bytesToCopy);
    Buffer     += bytesToCopy;
    BufferSize -= bytesToCopy;
  }

  return EFI_SUCCESS;
}
