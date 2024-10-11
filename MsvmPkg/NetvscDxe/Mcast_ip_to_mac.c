/** @file
     Implementation of converting an multicast IP address to multicast HW MAC
     address.

Copyright (c) 2004 - 2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) Microsoft Corporation.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Snp.h"

/**
  Call undi to convert an multicast IP address to a MAC address.

  @param  Snp   Pointer to snp driver structure.
  @param  IPv6  Flag to indicate if this is an ipv6 address.
  @param  IP    Multicast IP address.
  @param  MAC   Pointer to hold the return MAC address.

  @retval EFI_SUCCESS           The multicast IP address was mapped to the
                                multicast HW MAC address.
  @retval EFI_INVALID_PARAMETER Invalid UNDI command.
  @retval EFI_UNSUPPORTED       Command is not supported by UNDI.
  @retval EFI_DEVICE_ERROR      Fail to execute UNDI command.

**/
EFI_STATUS
PxeIp2Mac (
  IN SNP_DRIVER           *Snp,
  IN BOOLEAN              IPv6,
  IN EFI_IP_ADDRESS       *IP,
  IN OUT EFI_MAC_ADDRESS  *MAC
  )
{
  // MS_HYP_CHANGE BEGIN
  if (IPv6)
  {
      if (IP->v6.Addr[0] != 0xFF)
      {
          return EFI_INVALID_PARAMETER;
      }
      else
      {
          MAC->Addr[0] = 0x33;
          MAC->Addr[1] = 0x33;
          MAC->Addr[2] = IP->v6.Addr[12];
          MAC->Addr[3] = IP->v6.Addr[13];
          MAC->Addr[4] = IP->v6.Addr[14];
          MAC->Addr[5] = IP->v6.Addr[15];
      }
  }
  else
  {
      if ((IP->v4.Addr[0] & 0xF0) != 0xE0)
      {
          return EFI_INVALID_PARAMETER;
      }
      else
      {
          MAC->Addr[0] = 0x01;
          MAC->Addr[1] = 0x00;
          MAC->Addr[2] = 0x5E;
          MAC->Addr[3] = (IP->v4.Addr[1] & 0x7F);
          MAC->Addr[4] = IP->v4.Addr[2];
          MAC->Addr[5] = IP->v4.Addr[3];
      }
  }
  // MS_HYP_CHANGE BEGIN

  return EFI_SUCCESS;
}


/**
  Converts a multicast IP address to a multicast HW MAC address.

  This function converts a multicast IP address to a multicast HW MAC address
  for all packet transactions. If the mapping is accepted, then EFI_SUCCESS will
  be returned.

  @param This A pointer to the EFI_SIMPLE_NETWORK_PROTOCOL instance.
  @param IPv6 Set to TRUE if the multicast IP address is IPv6 [RFC 2460].
              Set to FALSE if the multicast IP address is IPv4 [RFC 791].
  @param IP   The multicast IP address that is to be converted to a multicast
              HW MAC address.
  @param MAC  The multicast HW MAC address that is to be generated from IP.

  @retval EFI_SUCCESS           The multicast IP address was mapped to the
                                multicast HW MAC address.
  @retval EFI_NOT_STARTED       The Simple Network Protocol interface has not
                                been started by calling Start().
  @retval EFI_INVALID_PARAMETER IP is NULL.
  @retval EFI_INVALID_PARAMETER MAC is NULL.
  @retval EFI_INVALID_PARAMETER IP does not point to a valid IPv4 or IPv6
                                multicast address.
  @retval EFI_DEVICE_ERROR      The Simple Network Protocol interface has not
                                been initialized by calling Initialize().
  @retval EFI_UNSUPPORTED       IPv6 is TRUE and the implementation does not
                                support IPv6 multicast to MAC address conversion.

**/
EFI_STATUS
EFIAPI
SnpMcastIpToMac(
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  IN BOOLEAN                      IPv6,
  IN EFI_IP_ADDRESS               *IP,
  OUT EFI_MAC_ADDRESS             *MAC
  )
{
  SNP_DRIVER  *Snp;
  EFI_TPL     OldTpl;
  EFI_STATUS  Status;

  //
  // Get pointer to SNP driver instance for *this.
  //
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((IP == NULL) || (MAC == NULL)) {
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

  Status = PxeIp2Mac (Snp, IPv6, IP, MAC);

ON_EXIT:
  gBS->RestoreTPL (OldTpl);

  return Status;
}
