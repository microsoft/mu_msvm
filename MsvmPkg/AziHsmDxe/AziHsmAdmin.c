/** @file
  Implementation of the Azure Integrated HSM DXE driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Library/DebugLib.h>
#include "AziHsmAdmin.h"
#include "AziHsmHci.h"

/**
 * Sets the HSM queue count.
 * @param[in]  IdenData  The controller data to be displayed.
 */
STATIC
VOID
AziHsmDisplayIdenData (
  IN AZI_HSM_CTRL_IDEN  *IdenData
  )
{
  UINT8  Sn[AZIHSM_CTRL_IDENT_SN_LEN + 1];
  UINT8  Fr[AZIHSM_CTRL_IDENT_FR_LEN + 1];

  CopyMem (Sn, IdenData->Sn, sizeof (IdenData->Sn));
  CopyMem (Fr, IdenData->Fr, sizeof (IdenData->Fr));
  Sn[AZIHSM_CTRL_IDENT_SN_LEN] = 0;
  Fr[AZIHSM_CTRL_IDENT_FR_LEN] = 0;
  DEBUG ((DEBUG_INFO, "AziHsm: == AZIHSM IDENTIFY CONTROLLER DATA ==\n"));
  DEBUG ((DEBUG_INFO, "AziHsm:    PCI VID   : 0x%x\n", IdenData->Vid));
  DEBUG ((DEBUG_INFO, "AziHsm:    PCI SSVID : 0x%x\n", IdenData->SsVid));
  DEBUG ((DEBUG_INFO, "AziHsm:    SN        : %a\n", Sn));
  DEBUG ((DEBUG_INFO, "AziHsm:    FR        : %a\n", Fr));
  DEBUG ((DEBUG_INFO, "AziHsm:    CTRL ID   : 0x%x\n", IdenData->Ctrl_Id));
  DEBUG ((DEBUG_INFO, "AziHsm:    SQES      : 0x%x\n", IdenData->CP_Sqes.Val));
  DEBUG ((DEBUG_INFO, "AziHsm:    CQES      : 0x%x\n", IdenData->CP_Cqes.Val));
}

/**
 * AziHsmAdminIssueCmd: Fires the Admin command to the controller
 * and waits for the response to finish. This is a generic function
 * to fire any command and get its response. The completion
 * queue entry is then copied to the CQE buffer if provided.
 *
 * @param[in]   QueuePair           : The Admin queue pair representing the submission
 *                                      queue and completion queue.
 * @param[in]   AdminSqe            : The Admin command SQE
 * @param[in]   AdminCqe            : The Admin command CQE, where the reponse CQE is returned.
 * @retval EFI_SUCCESS              : The operation completed successfully.
 * @retval EFI_INVALID_PARAMETER    : The QueuePair parameter is invalid.
 * @retval EFI_DEVICE_ERROR         :
 */
STATIC
EFI_STATUS
AziHsmAdminIssueCmd (
  IN     EFI_PCI_IO_PROTOCOL  *PciIo,
  IN OUT AZIHSM_IO_QUEUE_PAIR  *QueuePair,
  IN OUT AZIHSM_ADMIN_CMD_SQE  *AdminSqe,
  OUT    AZIHSM_ADMIN_CMD_CQE  *AdminCqe
  )
{
  EFI_STATUS            Status;
  AZIHSM_ADMIN_CMD_SQE  *DestSqe         = NULL;
  AZIHSM_ADMIN_CMD_CQE  *DestCqe         = NULL;
  UINT16                PsfBitBeforePost = 0;
  UINT32                WaitTime         = ADMIN_CMD_TIME_OUT_MS;

  if (!QueuePair || !AdminSqe) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsm: [%a][Line:%d]Invalid Paramters Passed -- return EFI_INVALID_PARAMETER\n",
      __FUNCTION__,
      __LINE__
      ));

    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  DestSqe = (AZIHSM_ADMIN_CMD_SQE *)QueuePair->SubmissionQueue.Buffer.HostAddress;
  DestSqe = &DestSqe[QueuePair->SubmissionQueue.u.Tail];

  DestCqe = (AZIHSM_ADMIN_CMD_CQE *)QueuePair->CompletionQueue.Buffer.HostAddress;
  DestCqe = &DestCqe[QueuePair->CompletionQueue.u.Head];

  // Get the Value of the phase bit before posting.
  PsfBitBeforePost = DestCqe->Psf.Val;
  *DestSqe         = *AdminSqe;

  // Increment The Tail and Write It
  AZIHSM_SQ_INC_TAIL (QueuePair->SubmissionQueue);

  Status = AziHsmHciWrSqTailDbReg (PciIo, QueuePair->Id, QueuePair->SubmissionQueue.u.Tail);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "AziHsm: [%a]: AziHsmHciWrSqTailDbReg Failed %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  Status = EFI_TIMEOUT;
  while (WaitTime) {
    if (DestCqe->Psf.Bits.P != (PsfBitBeforePost & 0x01)) {
      Status = EFI_SUCCESS;
      break;
    }

    gBS->Stall (1000);  // 1 Millisec wait
    WaitTime--;
  }

  if (Status == EFI_TIMEOUT) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsm: [%a]: Timedout Waiting For Command Cpl [PsfBitBeforePost:0x%x PsfAfterPost:0x%x \n",
      __FUNCTION__,
      PsfBitBeforePost,
      DestCqe->Psf.Val
      ));

    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  //
  // We have recieved the completion, It can be success or failure.
  // Our posting and completing the command is good. If the device
  // has failed the command, let the called take action on it.
  // We will just copy the entire CQE to the caller and indicate
  // Success from this function.
  //
  if (Status == EFI_SUCCESS) {
    // Increment The Head And Write It
    AZIHSM_CQ_INC_HEAD (QueuePair->CompletionQueue);

    AziHsmHciWrCqHeadReg (PciIo, QueuePair->Id, QueuePair->CompletionQueue.u.Head);

    // Check for the completion Status
    if (DestCqe->Psf.Bits.Sc != 0) {
      DEBUG ((
        DEBUG_ERROR,
        "AziHsm: [%a]: Command Failed By Firmware [Status:0x%x] \n",
        __FUNCTION__,
        DestCqe->Psf.Val
        ));
    }
  }

  if (AdminCqe) {
    *AdminCqe = *DestCqe;
  }

Exit:
  return Status;
}

/**
* Fire the Identify controller command to the controller
* On success, copy the identify data to the user provided
* buffer.
*
* @param[in]  State        The State of the controller
* @param[out] Buffer       If a valid buffer is provided, the identify data
*                          is copied over to this buffer.
* @retval EFI_SUCCESS        The operation completed successfully.
* @retval EFI_DEVICE_ERROR   The Command Failed.
*/
EFI_STATUS
EFIAPI
AziHsmAdminIdentifyCtrl (
  IN AZIHSM_CONTROLLER_STATE  *State,
  OUT UINT8                   *Buffer
  )
{
  EFI_STATUS            Status;
  AZIHSM_ADMIN_CMD_SQE  IdenSqe;
  AZIHSM_ADMIN_CMD_CQE  IdenCqe;
  AZIHSM_DMA_BUFFER     DmaBuffer;

  ZeroMem (&IdenSqe, sizeof (AZIHSM_ADMIN_CMD_SQE));
  ZeroMem (&IdenCqe, sizeof (AZIHSM_ADMIN_CMD_CQE));

  Status = AziHsmDmaBufferAlloc (State->PciIo, 1, &DmaBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a]: DMA Buffer Allocation Failed %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  IdenSqe.Ident.Hdr.Opc          = AZIHSM_ADMIN_CMD_OP_IDENT;
  IdenSqe.Ident.Hdr.dptr.prp.Fst = DmaBuffer.DeviceAddress;
  IdenSqe.Ident.Cns              = 0x01;

  Status = AziHsmAdminIssueCmd (State->PciIo, &State->AdminQueue, &IdenSqe, &IdenCqe);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a]: AziHsmAdminIssueCmd Failed %r\n", __FUNCTION__, Status));
    goto Free_Buffer_And_Exit;
  }

  if (IdenCqe.Psf.Bits.Sc != 0) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a]: Identify Controller Failed By Firmware %r\n", __FUNCTION__, Status));
    Status = EFI_DEVICE_ERROR;
    goto Free_Buffer_And_Exit;
  }

  AziHsmDisplayIdenData ((AZI_HSM_CTRL_IDEN *)DmaBuffer.HostAddress);

  if (Buffer) {
    CopyMem (Buffer, DmaBuffer.HostAddress, sizeof (AZI_HSM_CTRL_IDEN));
  }

Free_Buffer_And_Exit:
  AziHsmDmaBufferFree (&DmaBuffer);
Exit:
  return Status;
}

/**
 * Sets the HSM queue count.
 * @param[in]  State        The State of the controller
 * @param[in]  QueCnt       The HSM queue cnt to be set.
 * @retval EFI_SUCCESS        The operation completed successfully.
 * @retval EFI_DEVICE_ERROR   The Command Failed.
 */
EFI_STATUS
EFIAPI
AziHsmAdminSetHsmQueCnt (
  IN AZIHSM_CONTROLLER_STATE  *State,
  OUT UINT32                  *QueCnt
  )
{
  EFI_STATUS            Status;
  AZIHSM_ADMIN_CMD_SQE  SetFeatSqe;
  AZIHSM_ADMIN_CMD_CQE  SetFeatCqe;

  ZeroMem (&SetFeatSqe, sizeof (AZIHSM_ADMIN_CMD_SQE));
  ZeroMem (&SetFeatCqe, sizeof (AZIHSM_ADMIN_CMD_CQE));

  AZIHSM_ADMIN_CMD_SQE_HDR_INIT (SetFeatSqe.CreateSq.Hdr, AZIHSM_ADMIN_CMD_OP_SET_FEAT, 0);

  SetFeatSqe.SetFeat.FeatId              = AZIHSM_CTRL_CMD_FEAT_ID_HSM_QUEUE_CNT;
  SetFeatSqe.SetFeat.Data.QueueCnt.SqCnt = AZIHSM_HSM_MAX_QUEUE_CNT;
  SetFeatSqe.SetFeat.Data.QueueCnt.CqCnt = AZIHSM_HSM_MAX_QUEUE_CNT;

  Status = AziHsmAdminIssueCmd (State->PciIo, &State->AdminQueue, &SetFeatSqe, &SetFeatCqe);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a]: AziHsmAdminIssueCmd Failed %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  if (SetFeatCqe.Psf.Bits.Sc != 0) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a]: Set Feature Cmd Failed By Firmware %r\n", __FUNCTION__, Status));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  ASSERT (SetFeatCqe.Cs.QueueCnt.Sq == SetFeatCqe.Cs.QueueCnt.Cq);

  /*
   * Firmware returns a zero based queue count. We add 1 to that for actual queue count
   * This function returns the queue count that the driver is supposed to create.
  */

  *QueCnt = (AZIHSM_HSM_CREATE_QUEUE_CNT < (SetFeatCqe.Cs.QueueCnt.Sq + 1)) ?
            AZIHSM_HSM_CREATE_QUEUE_CNT : (SetFeatCqe.Cs.QueueCnt.Sq + 1);

  DEBUG ((DEBUG_INFO, "AziHsm: [%a]: Returning QueCnt %d\n", __FUNCTION__, *QueCnt));

Exit:
  return Status;
}

/**
 * Deletes a Submission Queue in the device
 * @param[in]  State        The State of the controller
 * @param[in]  Id           The Queue Identifier of the completion queue
 * @param[in]  IoQueue      AZIHSM_IO_QUEUE Structure which has information about
 *                          addresses to be used to delete the queue
 * @retval EFI_SUCCESS        The operation completed successfully.
 * @retval EFI_DEVICE_ERROR   The Command Failed.
 */
EFI_STATUS
EFIAPI
AziHsmDeleteSubQueue (
  IN AZIHSM_CONTROLLER_STATE  *State,
  IN UINT16                   Id,
  IN AZIHSM_IO_QUEUE          *IoQueue
  )
{
  EFI_STATUS            Status;
  AZIHSM_ADMIN_CMD_SQE  DeleteSqe;
  AZIHSM_ADMIN_CMD_CQE  DeleteCqe;

  ZeroMem (&DeleteSqe, sizeof (AZIHSM_ADMIN_CMD_SQE));
  ZeroMem (&DeleteCqe, sizeof (AZIHSM_ADMIN_CMD_CQE));

  DEBUG ((DEBUG_INFO, "AziHsm: [%a] [%d]: Deleting SQ [Id:%d]\n", __FUNCTION__, __LINE__, Id));

  // Fill up the SQE Here To Delete The Sub Queue
  AZIHSM_ADMIN_CMD_SQE_HDR_INIT (DeleteSqe.DeleteSq.Hdr, AZIHSM_ADMIN_CMD_OP_DELETE_SQ, IoQueue->Buffer.DeviceAddress);
  DeleteSqe.DeleteSq.Id = (UINT8)Id;

  Status = AziHsmAdminIssueCmd (State->PciIo, &State->AdminQueue, &DeleteSqe, &DeleteCqe);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a]: AziHsmAdminIssueCmd Failed %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  if (DeleteCqe.Psf.Bits.Sc != 0) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a]: Delete IO Submission Queue Failed By Firmware %r\n", __FUNCTION__, Status));
    goto Exit;
  }

Exit:
  DEBUG ((DEBUG_INFO, "AziHsm: [%a] [%d]: Delete SQ [Id:%d] Status: %r\n", __FUNCTION__, __LINE__, Id, Status));
  return Status;
}

/**
 * Deletes a Completion Queue in the device
 * @param[in]  State        The State of the controller
 * @param[in]  Id           The Queue Identifier of the completion queue
 * @param[in]  IoQueue      AZIHSM_IO_QUEUE Structure which has information about
 *                          addresses to be used to delete the queue
 * @retval EFI_SUCCESS        The operation completed successfully.
 * @retval EFI_DEVICE_ERROR   The Command Failed.
 */
EFI_STATUS
EFIAPI
AziHsmDeleteCplQueue (
  IN AZIHSM_CONTROLLER_STATE  *State,
  IN UINT16                   Id,
  IN AZIHSM_IO_QUEUE          *IoQueue
  )
{
  EFI_STATUS            Status;
  AZIHSM_ADMIN_CMD_SQE  DeleteSqe;
  AZIHSM_ADMIN_CMD_CQE  DeleteCqe;

  ZeroMem (&DeleteSqe, sizeof (AZIHSM_ADMIN_CMD_SQE));
  ZeroMem (&DeleteCqe, sizeof (AZIHSM_ADMIN_CMD_CQE));

  DEBUG ((DEBUG_INFO, "AziHsm: [%a] [%d]: Deleting CQ [Id:%d]\n", __FUNCTION__, __LINE__, Id));

  // Fill up the SQE Here To Delete The Cpl Queue
  AZIHSM_ADMIN_CMD_SQE_HDR_INIT (DeleteSqe.DeleteSq.Hdr, AZIHSM_ADMIN_CMD_OP_DELETE_CQ, IoQueue->Buffer.DeviceAddress);
  DeleteSqe.DeleteSq.Id = (UINT8)Id;

  Status = AziHsmAdminIssueCmd (State->PciIo, &State->AdminQueue, &DeleteSqe, &DeleteCqe);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a]: AziHsmAdminIssueCmd Failed %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  if (DeleteCqe.Psf.Bits.Sc != 0) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a]: Delete IO Completion Queue Failed By Firmware %r\n", __FUNCTION__, Status));
    goto Exit;
  }

Exit:
  DEBUG ((DEBUG_INFO, "AziHsm: [%a] [%d]: Delete CQ [Id:%d] Status: %r\n", __FUNCTION__, __LINE__, Id, Status));
  return Status;
}

/**
 * Create a Submission Queue in the device
 * @param[in]  State        The State of the controller
 * @param[in]  Id           The Queue Identifier of the completion queue
 * @param[in]  IoQueue      AZIHSM_IO_QUEUE Structure which has information about
 *                          addresses to be used to create the queue
 * @retval EFI_SUCCESS        The operation completed successfully.
 * @retval EFI_DEVICE_ERROR   The Command Failed.
 */
STATIC
EFI_STATUS
AziHsmCreateSubQueue (
  IN AZIHSM_CONTROLLER_STATE  *State,
  IN UINT16                   Id,
  IN AZIHSM_IO_QUEUE          *IoQueue
  )
{
  EFI_STATUS            Status;
  AZIHSM_ADMIN_CMD_SQE  CreateSqe;
  AZIHSM_ADMIN_CMD_CQE  CreateCqe;

  ZeroMem (&CreateSqe, sizeof (AZIHSM_ADMIN_CMD_SQE));
  ZeroMem (&CreateCqe, sizeof (AZIHSM_ADMIN_CMD_CQE));

  DEBUG ((DEBUG_INFO, "AziHsm: [%a] [%d]: Creating SQ [Id:%d]\n", __FUNCTION__, __LINE__, Id));

  AZIHSM_ADMIN_CMD_SQE_HDR_INIT (CreateSqe.CreateSq.Hdr, AZIHSM_ADMIN_CMD_OP_CREATE_SQ, IoQueue->Buffer.DeviceAddress);
  CreateSqe.CreateSq.QueId   = Id;
  CreateSqe.CreateSq.QueSz   = AZIHSM_QUEUE_SIZE;
  CreateSqe.CreateSq.PhyCont = 1;
  CreateSqe.CreateSq.CqId    = Id;
  CreateSqe.CreateSq.QuePri  = 0;

  // Create the Sub Queue in the hardware
  Status = AziHsmAdminIssueCmd (State->PciIo, &State->AdminQueue, &CreateSqe, &CreateCqe);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a] [%d]: AziHsmAdminIssueCmd Failed %r\n", __FUNCTION__, __LINE__, Status));
    goto Exit;
  }

  if (CreateCqe.Psf.Bits.Sc != 0) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a]: CreateSqe  Failed By Firmware %r\n", __FUNCTION__, Status));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

Exit:
  DEBUG ((DEBUG_INFO, "AziHsm: [%a] [%d]: Create SQ [Id:%d] Status: %r\n", __FUNCTION__, __LINE__, Id, Status));
  return Status;
}

/**
 * Create a Completion Queue in the device
 * @param[in]  State        The State of the controller
 * @param[in]  Id           The Queue Identifier of the completion queue
 * @param[in]  IoQueue      AZIHSM_IO_QUEUE Structure which has information about
 *                          addresses to be used to create the queue
 * @retval EFI_SUCCESS        The operation completed successfully.
 * @retval EFI_DEVICE_ERROR   The Command Failed.
 */
STATIC
EFI_STATUS
AziHsmCreateCplQueue (
  IN AZIHSM_CONTROLLER_STATE  *State,
  IN UINT16                   Id,
  IN AZIHSM_IO_QUEUE          *IoQueue
  )
{
  EFI_STATUS            Status;
  AZIHSM_ADMIN_CMD_SQE  CreateSqe;
  AZIHSM_ADMIN_CMD_CQE  CreateCqe;

  ZeroMem (&CreateSqe, sizeof (AZIHSM_ADMIN_CMD_SQE));
  ZeroMem (&CreateCqe, sizeof (AZIHSM_ADMIN_CMD_CQE));

  DEBUG ((DEBUG_INFO, "AziHsm: [%a] [%d]: Creating CQ [Id:%d]\n", __FUNCTION__, __LINE__, Id));

  // Fill up the SQE Here To Create The Cpl Queue
  AZIHSM_ADMIN_CMD_SQE_HDR_INIT (CreateSqe.CreateCq.Hdr, AZIHSM_ADMIN_CMD_OP_CREATE_CQ, IoQueue->Buffer.DeviceAddress);
  CreateSqe.CreateCq.Id   = Id;
  CreateSqe.CreateCq.Size = AZIHSM_QUEUE_SIZE;
  CreateSqe.CreateCq.Ien  = 0;
  CreateSqe.CreateCq.Ivec = 0;
  CreateSqe.CreateCq.Pc   = 1;

  Status = AziHsmAdminIssueCmd (State->PciIo, &State->AdminQueue, &CreateSqe, &CreateCqe);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a] [%d]: AziHsmAdminIssueCmd Failed %r\n", __FUNCTION__, __LINE__, Status));
    goto Exit;
  }

  if (CreateCqe.Psf.Bits.Sc != 0) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a] [%d]: CreateSqe  Failed By Firmware %r\n", __FUNCTION__, __LINE__, Status));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

Exit:
  DEBUG ((DEBUG_INFO, "AziHsm: [%a] [%d]: Create CQ [Id:%d] Status: %r\n", __FUNCTION__, __LINE__, Id, Status));
  return Status;
}

/**
 * Create a Completion Queue in the device
 * @param[in]  State        The State of the controller
 * @param[in]  QueuePair    The AZIHSM_IO_QUEUE_PAIR structure representing
 *                          memory allocations for the queues.
 *                          addresses to be used to create the queue
 * @retval EFI_SUCCESS        The operation completed successfully.
 * @retval EFI_DEVICE_ERROR   The Command Failed.
 */
EFI_STATUS
EFIAPI
AziHsmAdminCreateDeviceIoQuePair (
  IN AZIHSM_CONTROLLER_STATE  *State,
  IN AZIHSM_IO_QUEUE_PAIR     *QueuePair
  )
{
  EFI_STATUS  Status;

  if ((State == NULL) || (QueuePair == NULL)) {
    Status = EFI_INVALID_PARAMETER;
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a] [%d]: Invalid Params %r\n", __FUNCTION__, __LINE__, Status));
    goto Exit;
  }

  Status = AziHsmCreateCplQueue (State, QueuePair->Id, &QueuePair->CompletionQueue);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a] [%d]: AziHsmCreateCplQueue Failed %r\n", __FUNCTION__, __LINE__, Status));
    goto Exit;
  }

  Status = AziHsmCreateSubQueue (State, QueuePair->Id, &QueuePair->SubmissionQueue);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsm: [%a] [%d]: AziHsmCreateSubQueue Failed %r [Deleting Associated Cpl Queue]\n",
      __FUNCTION__,
      __LINE__,
      Status
      ));

    if (EFI_ERROR (AziHsmDeleteCplQueue (State, QueuePair->Id, &QueuePair->CompletionQueue))) {
      DEBUG ((DEBUG_ERROR, "AziHsm: [%a] [%d]: AziHsmDeleteCplQueue Failed %r\n", __FUNCTION__, __LINE__, Status));
    }

    goto Exit;
  }

  State->HsmQueuesCreated = TRUE;

Exit:
  return Status;
}

/**
* Delete both Submission and Completion IO queues.
*
* @param[in]  State        The State of the controller
* @retval EFI_SUCCESS        The operation completed successfully.
* @retval EFI_DEVICE_ERROR   The Command Failed.
*/
EFI_STATUS
EFIAPI
AziHsmAdminDeleteDeviceIoQueuePair (
  IN AZIHSM_CONTROLLER_STATE  *State,
  IN AZIHSM_IO_QUEUE_PAIR     *QueuePair
  )
{
  EFI_STATUS  Status;

  if (!State->HsmQueuesCreated) {
    Status = EFI_SUCCESS;
    goto Exit;
  }

  Status = AziHsmDeleteSubQueue (State, QueuePair->Id, &QueuePair->SubmissionQueue);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a] [%d]: AziHsmDeleteSubQueue Failed %r\n", __FUNCTION__, __LINE__, Status));
    goto Exit;
  }

  Status = AziHsmDeleteCplQueue (State, QueuePair->Id, &QueuePair->CompletionQueue);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a] [%d]: AziHsmDeleteCplQueue Failed %r\n", __FUNCTION__, __LINE__, Status));
    goto Exit;
  }

Exit:
  return Status;
}
