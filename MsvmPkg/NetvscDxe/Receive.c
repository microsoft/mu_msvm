/** @file
  Implementation of receiving a packet from a network interface.

Copyright (c) 2004 - 2010, Intel Corporation. All rights reserved.<BR>
Copyright (c) Microsoft Corporation.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#include "Snp.h"

/**
  Call NetVsc to receive a packet and fills in the data in the input pointers.

  @param  Snp          Pointer to snp driver structure
  @param  Buffer       Pointer to the memory for the received data
  @param  BufferSize   Pointer to the length of the buffer on entry and contains
                       the length of the received data on return
  @param  HeaderSize   Pointer to the header portion of the data received.
  @param  SrcAddr      Pointer to contain the source ethernet address on return
  @param  DestAddr     Pointer to contain the destination ethernet address on
                       return
  @param  Protocol     Pointer to contain the protocol type from the ethernet
                       header on return


  @retval EFI_SUCCESS           The received data was stored in Buffer, and
                                BufferSize has been updated to the number of
                                bytes received.
  @retval EFI_DEVICE_ERROR      Fail to execute UNDI command.
  @retval EFI_NOT_READY         No packets have been received on the network
                                interface.
  @retval EFI_BUFFER_TOO_SMALL  BufferSize is too small for the received
                                packets. BufferSize has been updated to the
                                required size.

**/
EFI_STATUS
PxeReceive (
  SNP_DRIVER       *Snp,
  VOID             *Buffer,
  UINTN            *BufferSize,
  UINTN            *HeaderSize,
  EFI_MAC_ADDRESS  *SrcAddr,
  EFI_MAC_ADDRESS  *DestAddr,
  UINT16           *Protocol
  )
{
  UINTN            BuffSize;

  BuffSize = *BufferSize;

  // MS_HYP_CHANGE BEGIN
  EFI_STATUS status = NetvscReceive(
        &Snp->AdapterContext->NicInfo,
        Buffer,
        BufferSize,
        HeaderSize,
        SrcAddr,
        DestAddr,
        Protocol);

  switch(status)
  {
    case EFI_SUCCESS:
      break;

    case EFI_NOT_READY:
      return EFI_NOT_READY;

    default:
      return EFI_DEVICE_ERROR;
  }

  // MS_HYP_CHANGE END

  return (*BufferSize <= BuffSize) ? EFI_SUCCESS : EFI_BUFFER_TOO_SMALL;
}

/**
  Receives a packet from a network interface.

  This function retrieves one packet from the receive queue of a network interface.
  If there are no packets on the receive queue, then EFI_NOT_READY will be
  returned. If there is a packet on the receive queue, and the size of the packet
  is smaller than BufferSize, then the contents of the packet will be placed in
  Buffer, and BufferSize will be updated with the actual size of the packet.
  In addition, if SrcAddr, DestAddr, and Protocol are not NULL, then these values
  will be extracted from the media header and returned. EFI_SUCCESS will be
  returned if a packet was successfully received.
  If BufferSize is smaller than the received packet, then the size of the receive
  packet will be placed in BufferSize and EFI_BUFFER_TOO_SMALL will be returned.
  If the driver has not been initialized, EFI_DEVICE_ERROR will be returned.

  @param This       A pointer to the EFI_SIMPLE_NETWORK_PROTOCOL instance.
  @param HeaderSize The size, in bytes, of the media header received on the network
                    interface. If this parameter is NULL, then the media header size
                    will not be returned.
  @param BufferSize On entry, the size, in bytes, of Buffer. On exit, the size, in
                    bytes, of the packet that was received on the network interface.
  @param Buffer     A pointer to the data buffer to receive both the media
                    header and the data.
  @param SrcAddr    The source HW MAC address. If this parameter is NULL, the HW
                    MAC source address will not be extracted from the media header.
  @param DestAddr   The destination HW MAC address. If this parameter is NULL,
                    the HW MAC destination address will not be extracted from
                    the media header.
  @param Protocol   The media header type. If this parameter is NULL, then the
                    protocol will not be extracted from the media header. See
                    RFC 1700 section "Ether Types" for examples.

  @retval EFI_SUCCESS           The received data was stored in Buffer, and
                                BufferSize has been updated to the number of
                                bytes received.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_NOT_READY         No packets have been received on the network interface.
  @retval EFI_BUFFER_TOO_SMALL  BufferSize is too small for the received packets.
                                BufferSize has been updated to the required size.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                * The This parameter is NULL
                                * The This parameter does not point to a valid
                                  EFI_SIMPLE_NETWORK_PROTOCOL structure.
                                * The BufferSize parameter is NULL
                                * The Buffer parameter is NULL
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.

**/
EFI_STATUS
EFIAPI
SnpReceive(
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  OUT UINTN                       *HeaderSize OPTIONAL,
  IN OUT UINTN                    *BufferSize,
  OUT VOID                        *Buffer,
  OUT EFI_MAC_ADDRESS             *SrcAddr OPTIONAL,
  OUT EFI_MAC_ADDRESS             *DestAddr OPTIONAL,
  OUT UINT16                      *Protocol OPTIONAL
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
    case EfiSimpleNetworkInitialized:
      break;

    case EfiSimpleNetworkStopped:
      Status = EFI_NOT_STARTED;
      goto ON_EXIT;

    default:
      Status = EFI_DEVICE_ERROR;
      goto ON_EXIT;
  }

  if ((BufferSize == NULL) || (Buffer == NULL)) {
    Status = EFI_INVALID_PARAMETER;
    goto ON_EXIT;
  }

  if (Snp->Mode.ReceiveFilterSetting == 0) {
    Status = EFI_DEVICE_ERROR;
    goto ON_EXIT;
  }

  Status = PxeReceive (
             Snp,
             Buffer,
             BufferSize,
             HeaderSize,
             SrcAddr,
             DestAddr,
             Protocol
             );

ON_EXIT:
  gBS->RestoreTPL (OldTpl);

  return Status;
}
