/** @file
    Implementation of starting a network adapter.

Copyright (c) 2004 - 2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) Microsoft Corporation.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Snp.h"

/**
  Call UNDI to start the interface and changes the snp state.

  @param  Snp                    pointer to snp driver structure.

  @retval EFI_SUCCESS            UNDI is started successfully.
  @retval EFI_DEVICE_ERROR       UNDI could not be started.

**/
EFI_STATUS
PxeStart (
  IN SNP_DRIVER  *Snp
  )
{
  // MS_HYP_CHANGE

  //
  // Set simple network state to Started and return success.
  //
  Snp->Mode.State = EfiSimpleNetworkStarted;

  return EFI_SUCCESS;
}

/**
  Change the state of a network interface from "stopped" to "started."

  This function starts a network interface. If the network interface successfully
  starts, then EFI_SUCCESS will be returned.

  @param  This                   A pointer to the EFI_SIMPLE_NETWORK_PROTOCOL instance.

  @retval EFI_SUCCESS            The network interface was started.
  @retval EFI_ALREADY_STARTED    The network interface is already in the started state.
  @retval EFI_INVALID_PARAMETER  This parameter was NULL or did not point to a valid
                                 EFI_SIMPLE_NETWORK_PROTOCOL structure.
  @retval EFI_DEVICE_ERROR       The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED        This function is not supported by the network interface.

**/
EFI_STATUS
EFIAPI
SnpStart(
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This
  )
{
  SNP_DRIVER  *Snp;
  EFI_STATUS  Status;
  EFI_TPL     OldTpl;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Snp = EFI_SIMPLE_NETWORK_DEV_FROM_THIS (This);

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  switch (Snp->Mode.State) {
    case EfiSimpleNetworkStopped:
      break;

    case EfiSimpleNetworkStarted:
    case EfiSimpleNetworkInitialized:
      Status = EFI_ALREADY_STARTED;
      goto ON_EXIT;

    default:
      Status = EFI_DEVICE_ERROR;
      goto ON_EXIT;
  }

  Status = PxeStart (Snp);
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  // MS_HYP_CHANGE

  Snp->Mode.MCastFilterCount = 0;

ON_EXIT:
  gBS->RestoreTPL (OldTpl);

  return Status;
}
