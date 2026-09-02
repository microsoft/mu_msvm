/** @file
  Implements shared helpers for the EfiHvDxe implementation files.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "EfiHvInternal.h"

UINTN
EfiHvpSharedPa (
  VOID  *Address
  )

/**
  Given an address, which may be either a VA or a PA, removes any
  canonicalization bits and returns the shared GPA corresponding to the
  address.

  @param Address Input address.

  @returns Shared GPA.

**/
{
  UINTN  addr;

  addr  = (UINTN)Address;
  addr &= ~mCanonicalizationMask;
  if (addr < mSharedGpaBoundary) {
    addr += mSharedGpaBoundary;
  }

  return addr;
}

VOID *
EfiHvpSharedVa (
  VOID  *Address
  )

/**
  Given an address, which may be either a VA or a PA, returns a canonicalized
  pointer pointing to the shared GPA alias.

  @param Address Input address.

  @returns Shared VA pointer.

**/
{
  return (VOID *)(EfiHvpSharedPa (Address) | mCanonicalizationMask);
}

UINTN
EfiHvpBasePa (
  UINTN  Address
  )

/**
  Given an address, returns the private alias GPA corresponding to that
  address.

  @param Address Input address.

  @returns Shared GPA.

**/
{
  Address &= ~mCanonicalizationMask;
  if (Address >= mSharedGpaBoundary) {
    Address -= mSharedGpaBoundary;
  }

  return Address;
}

HV_STATUS
EfiHvIssueHypercall (
  IN  HV_CALL_CODE  CallCode,
  IN  BOOLEAN       Fast,
  IN  UINT64        FirstRegister,
  IN  UINT64        SecondRegister
  )

/*++
  Issues a hypercall.

  @param CallCode The hypercall code.

  @param Fast If TRUE, this is a fast hypercall.

  @param FirstRegister The first register value for the hypercall. If a slow hypercall, this must refer
    to the non-shared alias of the GPA.

  @param SecondRegister The second register value for the hypercall. If a slow hypercall, this must
    refer to the non-shared alias of the GPA.

  @returns The hypercall status.

--*/
{
  return
    HvHypercallIssue (
      mUseBypassContext ? &mHvBypassContext : &mHvContext,
      CallCode,
      Fast,
      0,
      FirstRegister,
      SecondRegister,
      NULL
      );
}

EFI_STATUS
EfiHvConvertStatus (
  IN  HV_STATUS  Status
  )

/*++
  Converts a hypervisor status code into an EFI status code.

  @param Status The hypervisor status code.

  @returns EFI status.

--*/
{
  switch (Status) {
    case HV_STATUS_SUCCESS:
      return EFI_SUCCESS;

    case HV_STATUS_INVALID_PARAMETER:
      return EFI_INVALID_PARAMETER;

    default:
      return EFI_DEVICE_ERROR;
  }
}
