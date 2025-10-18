/** @file
  Implementation of the Azure Integrated HSM DXE driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/
#include <Library/BaseMemoryLib.h>
#include "AziHsmDma.h"

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
  )
{
  EFI_STATUS  Status;
  UINTN       NumberOfBytes;

  if ((DmaBuffer == NULL) || (PciIo == NULL) || (NumberOfPages == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  NumberOfBytes = EFI_PAGES_TO_SIZE (NumberOfPages);

  Status = PciIo->AllocateBuffer (
                    PciIo,
                    AllocateAnyPages,
                    EfiBootServicesData,
                    NumberOfPages,
                    (VOID **)&DmaBuffer->HostAddress,
                    0
                    );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = PciIo->Map (
                    PciIo,
                    EfiPciIoOperationBusMasterCommonBuffer,
                    DmaBuffer->HostAddress,
                    &NumberOfBytes,
                    &DmaBuffer->DeviceAddress,
                    &DmaBuffer->Mapping
                    );
  if (EFI_ERROR (Status)) {
    goto CleanupBuffer;
  }

  DmaBuffer->NumberOfBytes = NumberOfBytes;
  DmaBuffer->PciIo         = PciIo;

  ZeroMem (DmaBuffer->HostAddress, DmaBuffer->NumberOfBytes);
  goto Exit;

CleanupBuffer:
  PciIo->FreeBuffer (PciIo, NumberOfPages, DmaBuffer->HostAddress);

Exit:
  return Status;
}

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
  )
{
  UINTN  NumberOfPages = 0;
  
  if (DmaBuffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Validate DmaBuffer fields before using them
  if (DmaBuffer->PciIo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Only calculate pages if we have a valid size
  if (DmaBuffer->NumberOfBytes > 0) {
    NumberOfPages = EFI_SIZE_TO_PAGES (DmaBuffer->NumberOfBytes);
  }

  // Unmap the DMA buffer if it was mapped
  if (DmaBuffer->Mapping != NULL) {
    DmaBuffer->PciIo->Unmap (DmaBuffer->PciIo, DmaBuffer->Mapping);
    DmaBuffer->Mapping = NULL;  // Clear to prevent use-after-free
  }

  // Free the buffer if it was allocated
  if ((DmaBuffer->HostAddress != NULL) && (NumberOfPages > 0)) {
    DmaBuffer->PciIo->FreeBuffer (DmaBuffer->PciIo, NumberOfPages, DmaBuffer->HostAddress);
    DmaBuffer->HostAddress = NULL;  // Clear to prevent use-after-free
  }

  // Clear remaining fields to prevent use-after-free
  DmaBuffer->DeviceAddress = 0;
  DmaBuffer->NumberOfBytes = 0;
  DmaBuffer->PciIo = NULL;

  return EFI_SUCCESS;
}
