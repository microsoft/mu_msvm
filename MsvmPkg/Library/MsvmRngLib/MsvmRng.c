/** @file
  Random number generator services that uses RdRand instruction access
  to provide high-quality random numbers when RdRand is present. Otherwise,
  it relies on host emulation for random number generation.
  If host emulation is used, it is required to run this lib in memory.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

// MS_HYP_CHANGE BEGIN

#include <Uefi.h>
#include <Uefi/UefiBaseType.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BiosDeviceLib.h>
#include <Library/CrashLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <BiosInterface.h>
#include <IsolationTypes.h>

#include "BaseRngLibInternals.h"
#include "MsvmRngLibInternals.h"

#define WITHIN_4_GB_LL (0xFFFFFFFFLL)

static PCRYPTO_COMMAND_DESCRIPTOR   mCryptoCommandDescriptor;
static EFI_PHYSICAL_ADDRESS         mCryptoCommandDescriptorGpa;



/**
  Generates a random number using host emulation if host emulation is configured.

  @param[in] SizeInBytes  Number of bytes for generating the random number.
  @param[out] Rand        Buffer pointer to store the random value.

  @retval TRUE            Random number generated successfully.
  @retval FALSE           Failed to generate the random number.

**/
BOOLEAN
ProcessUsingHostEmulation (
  UINTN SizeInBytes,
  OUT UINT8  *Rand
  )
{
  //
  // We should never be sending more than 8 bytes for this implementation.
  // Any requests coming in for larger buffers should be chunked before reaching
  // here.
  //
  ASSERT(SizeInBytes <= 8);

  //
  // Retrieve the Random Number by issuing a command to Bios device.
  //
  ZeroMem(mCryptoCommandDescriptor, sizeof(CRYPTO_COMMAND_DESCRIPTOR));
  mCryptoCommandDescriptor->Command = CryptoGetRandomNumber;
  mCryptoCommandDescriptor->Status = EFI_DEVICE_ERROR;
  mCryptoCommandDescriptor->U.GetRandomNumberParams.BufferAddress = (UINT64) Rand;
  mCryptoCommandDescriptor->U.GetRandomNumberParams.BufferSize = (UINT32) SizeInBytes;

  //
  // Perform NVRAM command.
  // Cast of descriptor is safe because we allocated mVariableModuleGlobal below 4GB.
  //
  WriteBiosDevice(BiosConfigCryptoCommand, (UINT32)mCryptoCommandDescriptorGpa);

  if (mCryptoCommandDescriptor->Status == 0)
  {
    return TRUE;
  }
  else
  {
    DEBUG((DEBUG_ERROR, "%a: Host emulation failed - %r \n", __FUNCTION__, ENCODE_ERROR(mCryptoCommandDescriptor->Status)));
    return FALSE;
  }

}

/**
  The constructor function checks whether or not to use the RDRAND instruction.

  The constructor function checks whether or not RDRAND instruction is supported
  and the isolation status. If we are running isolated, we must have RDRAND present
  and not rely on the host emulation for getting random numbers.

  @param ImageHandle        Image handle this driver.
  @param SystemTable        Pointer to the System Table.

  @retval RETURN_SUCCESS    The constructor always returns EFI_SUCCESS.

**/
RETURN_STATUS
EFIAPI
MsvmRngLibConstructor (
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
  )

{
  BaseRngLibConstructor();

  if (!ArchIsRngSupported())
  {
    //
    // If we are running isolated, we must use hardware RNG for a secure implementation of
    // random number generation.
    //
    if (IsHardwareIsolated())
    {
      DEBUG((DEBUG_ERROR, "%a: Hardware RNG is not present on an isolated guest..\n", __FUNCTION__));
      FAIL_FAST_INITIALIZATION_FAILURE(EFI_SECURITY_VIOLATION);
    }

#if defined(MDE_CPU_X64)
    DEBUG((DEBUG_INFO, "%a: RDRAND is not present. Using host emulation.\n", __FUNCTION__));
#elif defined(MDE_CPU_AARCH64)
    DEBUG((DEBUG_VERBOSE, "%a: RNDR is not present. Using host emulation.\n", __FUNCTION__));
#else
#error Unsupported Architecture
#endif

    EFI_PHYSICAL_ADDRESS address = WITHIN_4_GB_LL;
    EFI_STATUS status = gBS->AllocatePages(AllocateMaxAddress,
                                     EfiBootServicesData,
                                     EFI_SIZE_TO_PAGES(sizeof(CRYPTO_COMMAND_DESCRIPTOR)),
                                     &address);

    if (EFI_ERROR(status))
    {
      // Fail fast since there is no way forward from this failure.
      FAIL_FAST_INITIALIZATION_FAILURE(status);
    }

    mCryptoCommandDescriptor = (PCRYPTO_COMMAND_DESCRIPTOR) address;

    if (mCryptoCommandDescriptor == NULL)
    {
      // Fail fast since there is no way forward from this failure.
      FAIL_FAST_INITIALIZATION_FAILURE(EFI_OUT_OF_RESOURCES);
    }

    mCryptoCommandDescriptorGpa = (EFI_PHYSICAL_ADDRESS) mCryptoCommandDescriptor;
  }

  return RETURN_SUCCESS;
}

// MS_HYP_CHANGE END
