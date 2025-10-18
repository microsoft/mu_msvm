/** @file
  Implementation of the Azure Integrated HSM DXE driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include "AziHsmPci.h"

#include <IndustryStandard/Pci.h>

/**
  Reads the vendor ID from the PCI configuration space.

  @param[in]  PciIo      A pointer to the EFI_PCI_IO_PROTOCOL instance.
  @param[out] VendorId   A pointer to the variable that will receive the vendor ID.

  @retval EFI_SUCCESS           The vendor ID was read successfully.
  @retval EFI_INVALID_PARAMETER  PciIo or VendorId is NULL.
  @retval EFI_DEVICE_ERROR      Failed to read the vendor ID.
**/
EFI_STATUS
EFIAPI
AziHsmPciReadVendorId (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  OUT UINT16              *VendorId
  )
{
  if ((PciIo == NULL) || (VendorId == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  return PciIo->Pci.Read (
                      PciIo,
                      EfiPciIoWidthUint16,
                      PCI_VENDOR_ID_OFFSET,
                      1,
                      VendorId
                      );
}

/**
  Reads the device ID from the PCI configuration space.

  @param[in]  PciIo      A pointer to the EFI_PCI_IO_PROTOCOL instance.
  @param[out] DeviceId   A pointer to the variable that will receive the device ID.

  @retval EFI_SUCCESS           The device ID was read successfully.
  @retval EFI_INVALID_PARAMETER  PciIo or DeviceId is NULL.
  @retval EFI_DEVICE_ERROR      Failed to read the device ID.
**/
EFI_STATUS
EFIAPI
AziHsmPciReadDeviceId (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  OUT UINT16              *DeviceId
  )
{
  if ((PciIo == NULL) || (DeviceId == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  return PciIo->Pci.Read (
                      PciIo,
                      EfiPciIoWidthUint16,
                      PCI_DEVICE_ID_OFFSET,
                      1,
                      DeviceId
                      );
}
