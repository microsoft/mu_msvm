/** @file
  MsvmPkg specific interface to RNG functionality.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

/**
  The constructor function checks whether or not RNDR instruction is supported
  by the host hardware.

  The constructor function checks whether or not RNDR instruction is supported.
  It will ASSERT() if RNDR instruction is not supported.
  It will always return EFI_SUCCESS.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
BaseRngLibConstructor (
  VOID
  );


/**
  Generate RNG fom the host using the BiosDevice.

  @param SizeInBytes   Bytes to generate.
  @param[out] Rand     Buffer pointer to store the random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
ProcessUsingHostEmulation (
  UINTN SizeInBytes,
  OUT UINT8  *Rand
  );
