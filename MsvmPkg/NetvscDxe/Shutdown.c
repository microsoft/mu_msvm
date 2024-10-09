/** @file
    Implementation of shutting down a network adapter.

Copyright (c) 2004 - 2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) Microsoft Corporation.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Snp.h"

/**
  Call UNDI to shut down the interface.

  @param  Snp   Pointer to snp driver structure.

  @retval EFI_SUCCESS        UNDI is shut down successfully.
  @retval EFI_DEVICE_ERROR   UNDI could not be shut down.

**/
EFI_STATUS
PxeShutdown (
  IN SNP_DRIVER  *Snp
  )
{
  // MS_HYP_CHANGE BEGIN
  EFI_STATUS status = NetvscShutdown(&Snp->AdapterContext->NicInfo);

  if (EFI_ERROR(status))
  {
    return EFI_DEVICE_ERROR;
  }

  // MS_HYP_CHANGE END

  return EFI_SUCCESS;
}

/**
  Resets a network adapter and leaves it in a state that is safe for another
  driver to initialize.

  This function releases the memory buffers assigned in the Initialize() call.
  Pending transmits and receives are lost, and interrupts are cleared and disabled.
  After this call, only the Initialize() and Stop() calls may be used. If the
  network interface was successfully shutdown, then EFI_SUCCESS will be returned.
  If the driver has not been initialized, EFI_DEVICE_ERROR will be returned.

  @param  This  A pointer to the EFI_SIMPLE_NETWORK_PROTOCOL instance.

  @retval EFI_SUCCESS           The network interface was shutdown.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_INVALID_PARAMETER This parameter was NULL or did not point to a valid
                                EFI_SIMPLE_NETWORK_PROTOCOL structure.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.

**/
EFI_STATUS
EFIAPI
SnpShutdown(
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This
  )
{
  SNP_DRIVER  *Snp;
  EFI_STATUS  Status;
  EFI_TPL     OldTpl;

  //
  // Get pointer to SNP driver instance for *This.
  //
  if (This == NULL) {
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

  Status = PxeShutdown (Snp);

  Snp->Mode.State                = EfiSimpleNetworkStarted;
  Snp->Mode.ReceiveFilterSetting = 0;

  Snp->Mode.MCastFilterCount     = 0;
  Snp->Mode.ReceiveFilterSetting = 0;
  ZeroMem (Snp->Mode.MCastFilter, sizeof Snp->Mode.MCastFilter);
  CopyMem (
    &Snp->Mode.CurrentAddress,
    &Snp->Mode.PermanentAddress,
    sizeof (EFI_MAC_ADDRESS)
    );

  gBS->CloseEvent (Snp->Snp.WaitForPacket);

ON_EXIT:
  gBS->RestoreTPL (OldTpl);

  return Status;
}
