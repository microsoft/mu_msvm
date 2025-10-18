/** @file
  Implementation of the Azure Integrated HSM DXE driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include "AziHsmQueue.h"

#include "AziHsmDma.h"

/**
 * Initialize a queue pair.
 *
 * @param[in]  QueuePair      The queue pair to initialize.
 *
 * @retval EFI_SUCCESS        The operation completed successfully.
 * @retval EFI_INVALID_PARAMETER  The QueuePair parameter is invalid.
 */
EFI_STATUS
EFIAPI
AziHsmQueuePairInitialize (
  IN  AZIHSM_IO_QUEUE_PAIR  *QueuePair,
  IN  EFI_PCI_IO_PROTOCOL   *PciIo,
  IN  UINT16                QueueId,
  IN  UINT16                QueueSize,
  IN  UINTN                 SqeSize,
  IN  UINTN                 CqeSize,
  IN  UINT16                DoorbellStride
  )
{
  EFI_STATUS  Status;

  if ((QueuePair == NULL) || (PciIo == NULL)) {
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  Status = AziHsmDmaBufferAlloc (PciIo, 1, &QueuePair->SubmissionQueue.Buffer);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = AziHsmDmaBufferAlloc (PciIo, 1, &QueuePair->CompletionQueue.Buffer);
  if (EFI_ERROR (Status)) {
    goto CleanupSubmissionQueue;
  }

  QueuePair->PciIo                     = PciIo;
  QueuePair->Id                        = QueueId;
  QueuePair->Phase                     = 0;
  QueuePair->DoorbellStride            = DoorbellStride;
  QueuePair->SubmissionQueue.Size      = QueueSize;
  QueuePair->SubmissionQueue.u.Tail    = 0;
  QueuePair->SubmissionQueue.EntrySize = SqeSize;
  QueuePair->CompletionQueue.Size      = QueueSize;
  QueuePair->CompletionQueue.EntrySize = CqeSize;
  QueuePair->SubmissionQueue.u.Head    = 0;

  goto Exit;

CleanupSubmissionQueue:
  AziHsmDmaBufferFree (&QueuePair->SubmissionQueue.Buffer);

Exit:
  return Status;
}

/**
 * Uninitialize a queue pair.
 *
 * @param[in]  QueuePair      The queue pair to uninitialize.
 *
 * @retval EFI_SUCCESS        The operation completed successfully.
 * @retval EFI_INVALID_PARAMETER  The QueuePair parameter is invalid.
 */
EFI_STATUS
EFIAPI
AziHsmQueuePairUninitialize (
  IN  AZIHSM_IO_QUEUE_PAIR  *QueuePair
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  if (QueuePair == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  AziHsmDmaBufferFree (&QueuePair->SubmissionQueue.Buffer);
  AziHsmDmaBufferFree (&QueuePair->CompletionQueue.Buffer);

Exit:
  return Status;
}
