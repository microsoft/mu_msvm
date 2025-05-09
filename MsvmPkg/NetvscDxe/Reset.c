/** @file
    Implementation of resetting a network adapter.

Copyright (c) 2004 - 2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) Microsoft Corporation.
SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Snp.h"

/**
  Call NetVsc to reset the NIC.

  @param  Snp                 Pointer to the snp driver structure.

  @return EFI_SUCCESSFUL      The NIC was reset.
  @retval EFI_DEVICE_ERROR    The NIC cannot be reset.

**/
EFI_STATUS
PxeReset (
  SNP_DRIVER  *Snp
  )
{
  // MS_HYP_CHANGE BEGIN
  EFI_STATUS status = EFI_SUCCESS;
  EFI_NETWORK_STATISTICS savedStats;
  UINT32 savedFilters;

  CopyMem(&savedStats, &Snp->AdapterContext->NicInfo.Statistics, sizeof(EFI_NETWORK_STATISTICS));
  savedFilters = Snp->AdapterContext->NicInfo.RxFilter;

  status = NetvscShutdown(&Snp->AdapterContext->NicInfo);
  if (EFI_ERROR(status))
  {
    goto Cleanup;
  }

  status = NetvscInit(&Snp->AdapterContext->NicInfo);
  if (EFI_ERROR(status))
  {
    goto Cleanup;
  }

  CopyMem(&Snp->AdapterContext->NicInfo.Statistics, &savedStats, sizeof(EFI_NETWORK_STATISTICS));

  status = NetvscSetFilter(&Snp->AdapterContext->NicInfo, savedFilters);

Cleanup:

  if (EFI_ERROR(status))
  {
    //
    // NetVsc could not be reset. Return NetVsc error.
    //
    return EFI_DEVICE_ERROR;
  }
  // MS_HYP_CHANGE BEGIN

  return EFI_SUCCESS;
}

/**
  Resets a network adapter and reinitializes it with the parameters that were
  provided in the previous call to Initialize().

  This function resets a network adapter and reinitializes it with the parameters
  that were provided in the previous call to Initialize(). The transmit and
  receive queues are emptied and all pending interrupts are cleared.
  Receive filters, the station address, the statistics, and the multicast-IP-to-HW
  MAC addresses are not reset by this call. If the network interface was
  successfully reset, then EFI_SUCCESS will be returned. If the driver has not
  been initialized, EFI_DEVICE_ERROR will be returned.

  @param This                 A pointer to the EFI_SIMPLE_NETWORK_PROTOCOL instance.
  @param ExtendedVerification Indicates that the driver may perform a more
                              exhaustive verification operation of the device
                              during reset.

  @retval EFI_SUCCESS           The network interface was reset.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
EFIAPI
SnpReset(
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  IN BOOLEAN                      ExtendedVerification
  )
{
  SNP_DRIVER  *Snp;
  EFI_TPL     OldTpl;
  EFI_STATUS  Status;

  // MS_HYP_CHANGE
  // Ignoring ExtendedVerification as it doesn't change how vNIC is reset.

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Snp = EFI_SIMPLE_NETWORK_DEV_FROM_THIS (This);

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

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

  Status = PxeReset (Snp);

ON_EXIT:
  gBS->RestoreTPL (OldTpl);

  return Status;
}
