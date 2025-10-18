/** @file
  Implementation of the Azure Integrated HSM DXE driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#ifndef __AZIHSMQUEUE_H__
#define __AZIHSMQUEUE_H__

#include <Uefi.h>

#include "AziHsmDma.h"

#define AZIHSM_MAX_QUE_ID      1
#define AZIHSM_QUEUE_ID_ADMIN  0
#define AZIHSM_QUEUE_SIZE      1
#define AZIHSM_SQE_SIZE        64
#define AZIHSM_CQE_SIZE        16

#define AZIHSM_CQ_INC_HEAD(_CplQ)  {\
  _CplQ.u.Head ^= 1; \
}

#define AZIHSM_SQ_INC_TAIL(_SQ)  {\
  _SQ.u.Tail ^= 1; \
}

typedef struct _AZIHSM_IO_QUEUE {
  AZIHSM_DMA_BUFFER    Buffer;
  UINT16               Size;
  UINTN                EntrySize;
  union {
    UINT16    Tail;
    UINT16    Head;
  } u;
} AZIHSM_IO_QUEUE;

typedef struct _AZIHSM_IO_QUEUE_PAIR {
  EFI_PCI_IO_PROTOCOL    *PciIo;
  UINT16                 Id;
  UINT8                  Phase;
  UINT16                 DoorbellStride;
  AZIHSM_IO_QUEUE        SubmissionQueue;
  AZIHSM_IO_QUEUE        CompletionQueue;
} AZIHSM_IO_QUEUE_PAIR;

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
  );

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
  );

#endif // __AZIHSMQUEUE_H__
