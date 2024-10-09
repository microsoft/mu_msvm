/** @file
     Implementation of initializing a network adapter.

Copyright (c) 2004 - 2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) Microsoft Corporation.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Snp.h"

/**
  Call UNDI to initialize the interface.

  @param  Snp                   Pointer to snp driver structure.
  @param  CableDetectFlag       Do/don't detect the cable (depending on what
                                undi supports).

  @retval EFI_SUCCESS           UNDI is initialized successfully.
  @retval EFI_DEVICE_ERROR      UNDI could not be initialized.
  @retval Other                 Other errors as indicated.

**/
EFI_STATUS
PxeInit (
  SNP_DRIVER  *Snp,
  UINT16      CableDetectFlag
  )
{
  EFI_STATUS          Status;

  // MS_HYP_CHANGE BEGIN
  Status = NetvscInit(&Snp->AdapterContext->NicInfo);

  if (Status == EFI_SUCCESS)
  {
    if (Snp->Mode.MediaPresentSupported)
    {
        Snp->Mode.MediaPresent = Snp->AdapterContext->NicInfo.MediaPresent;
    }

    Snp->Mode.State = EfiSimpleNetworkInitialized;
  }
  else
  {
  // MS_HYP_CHANGE END
    Status = EFI_DEVICE_ERROR;
  }

  return Status;
}

/**
  Resets a network adapter and allocates the transmit and receive buffers
  required by the network interface; optionally, also requests allocation of
  additional transmit and receive buffers.

  This function allocates the transmit and receive buffers required by the network
  interface. If this allocation fails, then EFI_OUT_OF_RESOURCES is returned.
  If the allocation succeeds and the network interface is successfully initialized,
  then EFI_SUCCESS will be returned.

  @param This               A pointer to the EFI_SIMPLE_NETWORK_PROTOCOL instance.

  @param ExtraRxBufferSize  The size, in bytes, of the extra receive buffer space
                            that the driver should allocate for the network interface.
                            Some network interfaces will not be able to use the
                            extra buffer, and the caller will not know if it is
                            actually being used.
  @param ExtraTxBufferSize  The size, in bytes, of the extra transmit buffer space
                            that the driver should allocate for the network interface.
                            Some network interfaces will not be able to use the
                            extra buffer, and the caller will not know if it is
                            actually being used.

  @retval EFI_SUCCESS           The network interface was initialized.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_OUT_OF_RESOURCES  There was not enough memory for the transmit and
                                receive buffers.
  @retval EFI_INVALID_PARAMETER This parameter was NULL or did not point to a valid
                                EFI_SIMPLE_NETWORK_PROTOCOL structure.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       The increased buffer size feature is not supported.

**/
EFI_STATUS
EFIAPI
SnpInitialize(
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  IN UINTN                        ExtraRxBufferSize OPTIONAL,
  IN UINTN                        ExtraTxBufferSize OPTIONAL
  )
{
  EFI_STATUS  EfiStatus;
  SNP_DRIVER  *Snp;
  EFI_TPL     OldTpl;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Snp = EFI_SIMPLE_NETWORK_DEV_FROM_THIS (This);

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  if (Snp == NULL) {
    EfiStatus = EFI_INVALID_PARAMETER;
    goto ON_EXIT;
  }

  switch (Snp->Mode.State) {
    case EfiSimpleNetworkStarted:
      break;

    case EfiSimpleNetworkStopped:
      EfiStatus = EFI_NOT_STARTED;
      goto ON_EXIT;

    default:
      EfiStatus = EFI_DEVICE_ERROR;
      goto ON_EXIT;
  }

  EfiStatus = gBS->CreateEvent (
                     EVT_NOTIFY_WAIT,
                     TPL_NOTIFY,
                     &SnpWaitForPacketNotify,
                     Snp,
                     &Snp->Snp.WaitForPacket
                     );

  if (EFI_ERROR (EfiStatus)) {
    Snp->Snp.WaitForPacket = NULL;
    EfiStatus              = EFI_DEVICE_ERROR;
    goto ON_EXIT;
  }

  //
  //
  //
  Snp->Mode.MCastFilterCount     = 0;
  Snp->Mode.ReceiveFilterSetting = 0;
  ZeroMem (Snp->Mode.MCastFilter, sizeof Snp->Mode.MCastFilter);
  CopyMem (
    &Snp->Mode.CurrentAddress,
    &Snp->Mode.PermanentAddress,
    sizeof (EFI_MAC_ADDRESS)
    );

  EfiStatus = PxeInit (Snp, PXE_OPFLAGS_INITIALIZE_DO_NOT_DETECT_CABLE);
  if (EFI_ERROR (EfiStatus)) {
    gBS->CloseEvent (Snp->Snp.WaitForPacket);
    goto ON_EXIT;
  }

  // MS_HYP_CHANGE
  // Try to update the MediaPresent field
  PxeGetStatus (Snp, NULL, NULL);

ON_EXIT:
  gBS->RestoreTPL (OldTpl);

  return EfiStatus;
}
