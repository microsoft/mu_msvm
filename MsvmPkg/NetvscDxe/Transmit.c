/** @file
    Implementation of transmitting a packet.

Copyright (c) 2004 - 2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) Microsoft Corporation.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Snp.h"


// MS_HYP_CHANGE BEGIN
/**
  Create the meadia header for the given data buffer.

  @param  Snp              Pointer to SNP driver structure.
  @param  MacHeaderPtr     Address where the media header will be filled in.
  @param  DestAddr         Address of the destination mac address buffer.
  @param  SrcAddr          Address of the source mac address buffer.
  @param  ProtocolPtr      Address of the protocol type.

  @retval EFI_SUCCESS      Successfully completed the undi call.
  @retval Other            Error return from undi call.

**/
EFI_STATUS
PxeFillHeader (
  SNP_DRIVER       *Snp,
  VOID             *MacHeaderPtr,
  EFI_MAC_ADDRESS  *DestAddr,
  EFI_MAC_ADDRESS  *SrcAddr,
  UINT16           *ProtocolPtr
  )
{
  ETHERNET_HEADER    *macHeader;
  UINT32             index;
  EFI_MAC_ADDRESS    *sourceAddr;

  sourceAddr = SrcAddr == NULL? &Snp->Mode.CurrentAddress: SrcAddr;
  macHeader = (ETHERNET_HEADER *) MacHeaderPtr;

  macHeader->Type = (UINT16) PXE_SWAP_UINT16 (*ProtocolPtr);

  for (index = 0; index < PXE_HWADDR_LEN_ETHER; index++)
  {
    macHeader->DestAddr[index] = DestAddr->Addr[index];
    macHeader->SrcAddr[index] = sourceAddr->Addr[index];
  }

  return EFI_SUCCESS;
}
// MS_HYP_CHANGE END

/**
  This routine calls NetVsc to transmit the given data buffer

  @param  Snp                 pointer to SNP driver structure
  @param  Buffer           data buffer pointer
  @param  BufferSize        Size of data in the Buffer

  @retval EFI_SUCCESS         if successfully completed the undi call
  @retval Other               error return from undi call.

**/
EFI_STATUS
PxeTransmit (
  SNP_DRIVER  *Snp,
  VOID        *Buffer,
  UINTN       BufferSize
  )
{
  EFI_STATUS        Status;

  // MS_HYP_CHANGE BEGIN
  Status = NetvscTransmit(&Snp->AdapterContext->NicInfo, Buffer, (UINT32) BufferSize);

  switch (Status){
    case EFI_SUCCESS:
    case EFI_NOT_READY:
    case EFI_DEVICE_ERROR:
      break;

    default:
      Status = EFI_DEVICE_ERROR;
  }
  // MS_HYP_CHANGE END

  return Status;
}

/**
  Places a packet in the transmit queue of a network interface.

  This function places the packet specified by Header and Buffer on the transmit
  queue. If HeaderSize is nonzero and HeaderSize is not equal to
  This->Mode->MediaHeaderSize, then EFI_INVALID_PARAMETER will be returned. If
  BufferSize is less than This->Mode->MediaHeaderSize, then EFI_BUFFER_TOO_SMALL
  will be returned. If Buffer is NULL, then EFI_INVALID_PARAMETER will be
  returned. If HeaderSize is nonzero and DestAddr or Protocol is NULL, then
  EFI_INVALID_PARAMETER will be returned. If the transmit engine of the network
  interface is busy, then EFI_NOT_READY will be returned. If this packet can be
  accepted by the transmit engine of the network interface, the packet contents
  specified by Buffer will be placed on the transmit queue of the network
  interface, and EFI_SUCCESS will be returned. GetStatus() can be used to
  determine when the packet has actually been transmitted. The contents of the
  Buffer must not be modified until the packet has actually been transmitted.
  The Transmit() function performs nonblocking I/O. A caller who wants to perform
  blocking I/O, should call Transmit(), and then GetStatus() until the
  transmitted buffer shows up in the recycled transmit buffer.
  If the driver has not been initialized, EFI_DEVICE_ERROR will be returned.

  @param This       A pointer to the EFI_SIMPLE_NETWORK_PROTOCOL instance.
  @param HeaderSize The size, in bytes, of the media header to be filled in by the
                    Transmit() function. If HeaderSize is nonzero, then it must
                    be equal to This->Mode->MediaHeaderSize and the DestAddr and
                    Protocol parameters must not be NULL.
  @param BufferSize The size, in bytes, of the entire packet (media header and
                    data) to be transmitted through the network interface.
  @param Buffer     A pointer to the packet (media header followed by data) to be
                    transmitted. This parameter cannot be NULL. If HeaderSize is
                    zero, then the media header in Buffer must already be filled
                    in by the caller. If HeaderSize is nonzero, then the media
                    header will be filled in by the Transmit() function.
  @param SrcAddr    The source HW MAC address. If HeaderSize is zero, then this
                    parameter is ignored. If HeaderSize is nonzero and SrcAddr
                    is NULL, then This->Mode->CurrentAddress is used for the
                    source HW MAC address.
  @param DestAddr   The destination HW MAC address. If HeaderSize is zero, then
                    this parameter is ignored.
  @param Protocol   The type of header to build. If HeaderSize is zero, then this
                    parameter is ignored. See RFC 1700, section "Ether Types,"
                    for examples.

  @retval EFI_SUCCESS           The packet was placed on the transmit queue.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_NOT_READY         The network interface is too busy to accept this
                                transmit request.
  @retval EFI_BUFFER_TOO_SMALL  The BufferSize parameter is too small.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported
                                value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
EFIAPI
SnpTransmit(
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  IN UINTN                        HeaderSize,
  IN UINTN                        BufferSize,
  IN VOID                         *Buffer,
  IN EFI_MAC_ADDRESS              *SrcAddr   OPTIONAL,
  IN EFI_MAC_ADDRESS              *DestAddr  OPTIONAL,
  IN UINT16                       *Protocol  OPTIONAL
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

  /*
  MS_HYP_CHANGE: This is a bug, returning at wrong TPL.  Should be removed
  if (Snp == NULL) {
    return EFI_DEVICE_ERROR;
  }
  */

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

  if (Buffer == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto ON_EXIT;
  }

  if (BufferSize < Snp->Mode.MediaHeaderSize) {
    Status = EFI_BUFFER_TOO_SMALL;
    goto ON_EXIT;
  }

  // MS_HYP_CHANGE BEGIN
  if (BufferSize > Snp->Mode.MaxPacketSize) {
    Status = EFI_INVALID_PARAMETER;
    goto ON_EXIT;
  }
  // MS_HYP_CHANGE END

  //
  // if the HeaderSize is non-zero, we need to fill up the header and for that
  // we need the destination address and the protocol
  //
  if (HeaderSize != 0) {
    if ((HeaderSize != Snp->Mode.MediaHeaderSize) || (DestAddr == 0) || (Protocol == 0)) {
      Status = EFI_INVALID_PARAMETER;
      goto ON_EXIT;
    }

    Status = PxeFillHeader (
               Snp,
               Buffer,
               DestAddr,
               SrcAddr,
               Protocol
               );

    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }
  }

  Status = PxeTransmit (Snp, Buffer, BufferSize);

ON_EXIT:
  gBS->RestoreTPL (OldTpl);

  return Status;
}
