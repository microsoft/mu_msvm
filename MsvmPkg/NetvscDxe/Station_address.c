/** @file
    Implementation of reading the MAC address of a network adapter.

Copyright (c) 2004 - 2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) Microsoft Corporation.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Snp.h"


/**
  Call UNDI to read the MAC address of the NIC and update the mode structure
  with the address.

  @param  Snp         Pointer to snp driver structure.

  @retval EFI_SUCCESS       The MAC address of the NIC is read successfully.
  @retval EFI_DEVICE_ERROR  Failed to read the MAC address of the NIC.

**/
EFI_STATUS
PxeGetStnAddr (
  SNP_DRIVER  *Snp
  )
{
  // MS_HYP_CHANGE BEGIN

  //
  // Set new station address in SNP->Mode structure and return success.
  //
  CopyMem (
    &(Snp->Mode.CurrentAddress),
    &(Snp->AdapterContext->NicInfo.CurrentNodeAddress),
    Snp->Mode.HwAddressSize
    );

  CopyMem (
    &Snp->Mode.BroadcastAddress,
    &Snp->AdapterContext->NicInfo.BroadcastNodeAddress,
    Snp->Mode.HwAddressSize
    );

  CopyMem (
    &Snp->Mode.PermanentAddress,
    &Snp->AdapterContext->NicInfo.PermNodeAddress,
    Snp->Mode.HwAddressSize
    );

  // MS_HYP_CHANGE END

  return EFI_SUCCESS;
}

/**
  Modifies or resets the current station address, if supported.

  This function modifies or resets the current station address of a network
  interface, if supported. If Reset is TRUE, then the current station address is
  set to the network interface's permanent address. If Reset is FALSE, and the
  network interface allows its station address to be modified, then the current
  station address is changed to the address specified by New. If the network
  interface does not allow its station address to be modified, then
  EFI_INVALID_PARAMETER will be returned. If the station address is successfully
  updated on the network interface, EFI_SUCCESS will be returned. If the driver
  has not been initialized, EFI_DEVICE_ERROR will be returned.

  @param This  A pointer to the EFI_SIMPLE_NETWORK_PROTOCOL instance.
  @param Reset Flag used to reset the station address to the network interface's
               permanent address.
  @param New   New station address to be used for the network interface.


  @retval EFI_SUCCESS           The network interface's station address was updated.
  @retval EFI_NOT_STARTED       The Simple Network Protocol interface has not been
                                started by calling Start().
  @retval EFI_INVALID_PARAMETER The New station address was not accepted by the NIC.
  @retval EFI_INVALID_PARAMETER Reset is FALSE and New is NULL.
  @retval EFI_DEVICE_ERROR      The Simple Network Protocol interface has not
                                been initialized by calling Initialize().
  @retval EFI_DEVICE_ERROR      An error occurred attempting to set the new
                                station address.
  @retval EFI_UNSUPPORTED       The NIC does not support changing the network
                                interface's station address.

**/
EFI_STATUS
EFIAPI
SnpStationAddress(
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  IN BOOLEAN                      Reset,
  IN EFI_MAC_ADDRESS              *New OPTIONAL
  )
{
  SNP_DRIVER  *Snp;
  EFI_STATUS  Status;
  EFI_TPL     OldTpl;

  //
  // Check for invalid parameter combinations.
  //
  if ((This == NULL) ||
      (!Reset && (New == NULL)))
  {
    return EFI_INVALID_PARAMETER;
  }

  Snp = EFI_SIMPLE_NETWORK_DEV_FROM_THIS (This);

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  //
  // Return error if the SNP is not initialized.
  //
  switch (Snp->Mode.State) {
    case EfiSimpleNetworkInitialized:
      break;

    case EfiSimpleNetworkStopped:
      Status = EFI_NOT_STARTED;
      goto ON_EXIT;

    default:
      Status = EFI_DEVICE_ERROR;
      goto ON_EXIT;
  }

  // MS_HYP_CHANGE
  // Setting CurrentAddress is not supported
  Status = EFI_UNSUPPORTED;

ON_EXIT:
  gBS->RestoreTPL (OldTpl);

  return Status;
}
