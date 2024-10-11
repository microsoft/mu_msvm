/** @file
    Implementation of stopping a network interface.

Copyright (c) 2004 - 2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) Microsoft Corporation.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Snp.h"

/**
  Call UNDI to stop the interface and changes the snp state.

  @param  Snp   Pointer to snp driver structure

  @retval EFI_SUCCESS            The network interface was stopped.
  @retval EFI_DEVICE_ERROR       SNP is not initialized.

**/
EFI_STATUS
PxeStop (
  SNP_DRIVER  *Snp
  )
{
  // MS_HYP_CHANGE BEGIN
  switch (Snp->Mode.State)
  {
    case EfiSimpleNetworkStarted:
      break;

    case EfiSimpleNetworkStopped:
      return EFI_NOT_STARTED;

    default:
      return EFI_DEVICE_ERROR;
  }
  // MS_HYP_CHANGE END

  //
  // Set simple network state to Stopped and return success.
  //
  Snp->Mode.State = EfiSimpleNetworkStopped;
  return EFI_SUCCESS;
}

/**
  Changes the state of a network interface from "started" to "stopped."

  This function stops a network interface. This call is only valid if the network
  interface is in the started state. If the network interface was successfully
  stopped, then EFI_SUCCESS will be returned.

  @param  This                    A pointer to the EFI_SIMPLE_NETWORK_PROTOCOL
                                  instance.


  @retval EFI_SUCCESS             The network interface was stopped.
  @retval EFI_NOT_STARTED         The network interface has not been started.
  @retval EFI_INVALID_PARAMETER   This parameter was NULL or did not point to a
                                  valid EFI_SIMPLE_NETWORK_PROTOCOL structure.
  @retval EFI_DEVICE_ERROR        The command could not be sent to the network
                                  interface.
  @retval EFI_UNSUPPORTED         This function is not supported by the network
                                  interface.

**/
EFI_STATUS
EFIAPI
SnpStop(
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This
  )
{
  SNP_DRIVER  *Snp;
  EFI_TPL     OldTpl;
  EFI_STATUS  Status;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Snp = EFI_SIMPLE_NETWORK_DEV_FROM_THIS (This);

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  switch (Snp->Mode.State) {
    case EfiSimpleNetworkStarted:
      break;

    case EfiSimpleNetworkStopped:
      Status = EFI_NOT_STARTED;
      goto ON_EXIT;

    default:
      Status = EFI_DEVICE_ERROR;
      goto ON_EXIT;
  }

  Status = PxeStop (Snp);

ON_EXIT:
  gBS->RestoreTPL (OldTpl);

  return Status;
}
