/** @file
  Implementation of the Azure Integrated HSM DXE driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#ifndef __AZIHSMDMA_H__
#define __AZIHSMDMA_H__

#include <Uefi.h>

#include <Protocol/PciIo.h>

/**
 * DMA buffer information.
 */
typedef struct _AZIHSM_DMA_BUFFER {
  UINT8                   *HostAddress;
  UINTN                   NumberOfBytes;
  EFI_PHYSICAL_ADDRESS    DeviceAddress;
  VOID                    *Mapping;
  EFI_PCI_IO_PROTOCOL     *PciIo;
} AZIHSM_DMA_BUFFER;

/**
 * Allocate a DMA buffer.
 *
 * @param[in]  PciIo           The PCI I/O protocol instance.
 * @param[in]  NumberOfPages   The number of pages to allocate.
 * @param[out] DmaBuffer       The DMA buffer information.
 *
 * @retval EFI_SUCCESS        The operation completed successfully.
 * @retval EFI_OUT_OF_RESOURCES  Failed to allocate the requested resources.
 */
EFI_STATUS
EFIAPI
AziHsmDmaBufferAlloc (
  IN EFI_PCI_IO_PROTOCOL    *PciIo,
  IN  UINTN                 NumberOfPages,
  IN OUT AZIHSM_DMA_BUFFER  *DmaBuffer
  );

/**
 * Free a previously allocated DMA buffer.
 *
 * @param[in]  DmaBuffer       The DMA buffer information.
 *
 * @retval EFI_SUCCESS           The operation completed successfully.
 * @retval EFI_INVALID_PARAMETER The DmaBuffer parameter is NULL.
 */
EFI_STATUS
EFIAPI
AziHsmDmaBufferFree (
  IN OUT AZIHSM_DMA_BUFFER  *DmaBuffer
  );

#endif // __AZIHSMDMA_H__
