/** @file
  Implementation of reading the current interrupt status and recycled transmit
  buffer status from a network interface.

Copyright (c) 2004 - 2016, Intel Corporation. All rights reserved.<BR>
Copyright (c) Microsoft Corporation.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Snp.h"

/**
  Get the status of the interrupts, get the list of transmit
  buffers that completed transmitting.. This function will also update the MediaPresent
  field of EFI_SIMPLE_NETWORK_MODE.

  @param[in]   Snp                     Pointer to snp driver structure.
  @param[out]  InterruptStatusPtr      A non null pointer to contain the interrupt
                                       status.
  @param[in]   GetTransmittedBuf       Set to TRUE to retrieve the recycled transmit
                                       buffer address.

  @retval      EFI_SUCCESS             The status of the network interface was retrieved.
  @retval      EFI_DEVICE_ERROR        The command could not be sent to the network
                                       interface.

**/
EFI_STATUS
PxeGetStatus (
  IN     SNP_DRIVER  *Snp,
  OUT UINT32         *InterruptStatusPtr,
  OUT VOID          **TransmitBufferListPtr OPTIONAL
  )
/**

Arguments:

    TransmitBufferListPtrs  A non null pointer to contain the list of
                              pointers of previous transmitted buffers whose
                              transmission was completed asynchronously.

**/
{
  // MS_HYP_CHANGE BEGIN
  NIC_DATA_INSTANCE *adapterInfo;

  //
  // Get the NicInfo.
  //
  adapterInfo = &Snp->AdapterContext->NicInfo;

  //
  // report the values back..
  //
  if (InterruptStatusPtr != NULL) {
    *InterruptStatusPtr = 0;

    if (adapterInfo->RxInterrupt) {
      *InterruptStatusPtr |= EFI_SIMPLE_NETWORK_RECEIVE_INTERRUPT;
      adapterInfo->RxInterrupt = FALSE;
    }

    if (adapterInfo->TxedInterrupt) {
      *InterruptStatusPtr |= EFI_SIMPLE_NETWORK_TRANSMIT_INTERRUPT;
      adapterInfo->TxedInterrupt = FALSE;
    }
  }

  if (TransmitBufferListPtr != NULL)
  {
    if (TxQueueIsEmpty(&adapterInfo->TxedBuffersQueue))
    {
      *TransmitBufferListPtr = NULL;
    }
    else
    {
      TxQueueDequeue(&adapterInfo->TxedBuffersQueue, TransmitBufferListPtr);
    }
  }

  Snp->Mode.MediaPresent = adapterInfo->MediaPresent;
  // MS_HYP_CHANGE END

  return EFI_SUCCESS;
}

/**
  Reads the current interrupt status and recycled transmit buffer status from a
  network interface.

  This function gets the current interrupt and recycled transmit buffer status
  from the network interface. The interrupt status is returned as a bit mask in
  InterruptStatus. If InterruptStatus is NULL, the interrupt status will not be
  read. If TxBuf is not NULL, a recycled transmit buffer address will be retrieved.
  If a recycled transmit buffer address is returned in TxBuf, then the buffer has
  been successfully transmitted, and the status for that buffer is cleared. If
  the status of the network interface is successfully collected, EFI_SUCCESS
  will be returned. If the driver has not been initialized, EFI_DEVICE_ERROR will
  be returned.

  @param This            A pointer to the EFI_SIMPLE_NETWORK_PROTOCOL instance.
  @param InterruptStatus A pointer to the bit mask of the currently active
                         interrupts (see "Related Definitions"). If this is NULL,
                         the interrupt status will not be read from the device.
                         If this is not NULL, the interrupt status will be read
                         from the device. When the interrupt status is read, it
                         will also be cleared. Clearing the transmit interrupt does
                         not empty the recycled transmit buffer array.
  @param TxBuf           Recycled transmit buffer address. The network interface
                         will not transmit if its internal recycled transmit
                         buffer array is full. Reading the transmit buffer does
                         not clear the transmit interrupt. If this is NULL, then
                         the transmit buffer status will not be read. If there
                         are no transmit buffers to recycle and TxBuf is not NULL,
                         TxBuf will be set to NULL.

  @retval EFI_SUCCESS           The status of the network interface was retrieved.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_INVALID_PARAMETER This parameter was NULL or did not point to a valid
                                EFI_SIMPLE_NETWORK_PROTOCOL structure.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network
                                interface.

**/
EFI_STATUS
EFIAPI
SnpGetStatus(
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  OUT UINT32                      *InterruptStatus  OPTIONAL,
  OUT VOID                        **TxBuf           OPTIONAL
  )
{
  SNP_DRIVER  *Snp;
  EFI_TPL     OldTpl;
  EFI_STATUS  Status;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((InterruptStatus == NULL) && (TxBuf == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Snp = EFI_SIMPLE_NETWORK_DEV_FROM_THIS (This);

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  if (Snp == NULL) {
    return EFI_DEVICE_ERROR;
  }

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
  Status = PxeGetStatus(Snp, InterruptStatus, TxBuf);

ON_EXIT:
  gBS->RestoreTPL (OldTpl);

  return Status;
}
