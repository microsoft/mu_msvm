/** @file
  Implementation of the Azure Integrated HSM DXE driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include "AziHsmCp.h"
#include "AziHsmAdmin.h"
#include "AziHsmHci.h"

STATIC
VOID
AziHsmDumpSqe (
  AZIHSM_CP_SQE  *Sqe
  )
{
  if (Sqe == NULL) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a]: Invalid SQE pointer\n", __FUNCTION__));
    return;
  }

  DEBUG ((DEBUG_INFO, "AziHsm: [%a]: Dumping SQE\n", __FUNCTION__));
  DEBUG ((DEBUG_INFO, "  CmdId: %d\n", Sqe->CmdId));
  DEBUG ((DEBUG_INFO, "  CmdSet: %d\n", Sqe->CmdSet));
  DEBUG ((DEBUG_INFO, "  OpCode: %d\n", Sqe->OpCode));
  DEBUG ((DEBUG_INFO, "  Psdt: %d\n", Sqe->Psdt));
  DEBUG ((DEBUG_INFO, "  SrcLen: %d\n", Sqe->SrcLen));
  DEBUG ((DEBUG_INFO, "  DstLen: %d\n", Sqe->DstLen));
  DEBUG ((DEBUG_INFO, "  Src.Fst: 0x%x Src.Snd: 0x%x\n", Sqe->Src.Prp.Fst, Sqe->Src.Prp.Snd));
  DEBUG ((DEBUG_INFO, "  Dst.Fst: 0x%x Dst.Snd: 0x%x\n", Sqe->Dst.Prp.Fst, Sqe->Dst.Prp.Snd));
  DEBUG ((
    DEBUG_INFO,
    "  SessionFlags: OpCode[0x%x] InSessCmd[0x%x] ShortAppIdValid[0x%x] SafeToCloseSess[0x%x]\n",
    Sqe->SqeData.SqeSessionData.SessionCtrlFlags.Opcode,
    Sqe->SqeData.SqeSessionData.SessionCtrlFlags.InSessionCmd,
    Sqe->SqeData.SqeSessionData.SessionCtrlFlags.ShortAppIdValid,
    Sqe->SqeData.SqeSessionData.SessionCtrlFlags.SafeToCloseSession
    ));
}

EFI_STATUS
EFIAPI
AziHsmInitHsm (
  IN AZIHSM_CONTROLLER_STATE  *State
  )
{
  EFI_STATUS  Status;
  UINT32      QueCnt = AZIHSM_HSM_CREATE_QUEUE_CNT;

  Status = AziHsmQueuePairInitialize (
             &State->HsmQueue,
             State->PciIo,
             AZIHSM_HSM_QUEUE_ID,
             AZIHSM_QUEUE_SIZE,
             AZIHSM_HSM_CP_SQE_SZ,
             AZIHSM_HSM_CMD_CQE_SIZE,
             0
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to initialize admin queue pair. Status: %r\n", Status));
    goto Exit;
  }

  Status = AziHsmAdminSetHsmQueCnt (State, &QueCnt);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a]: AziHsmAdminSetHsmQueCnt Failed %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  ASSERT (QueCnt == AZIHSM_HSM_CREATE_QUEUE_CNT);
  Status = AziHsmAdminCreateDeviceIoQuePair (State, &State->HsmQueue);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a]: AziHsmAdminSetHsmQueCnt Failed %r\n", __FUNCTION__, Status));
    goto Exit;
  }

Exit:
  return Status;
}

EFI_STATUS
EFIAPI
AziHsmFireHsmCmd (
  IN AZIHSM_CONTROLLER_STATE     *State,
  IN AZIHSM_DMA_BUFFER           *DmaBufferIn,
  IN OUT UINT32                  *InXferBuffSz,
  IN AZIHSM_DMA_BUFFER           *DmaBufferOut,
  IN OUT UINT32                  *OutXferBuffSz,
  IN UINT32                      OpCode,
  IN AZIHSM_CP_CMD_SQE_SRC_DATA  *SessionData,
  OUT UINT32                     *FwSts
  )
{
  EFI_STATUS            Status;
  AZIHSM_CP_SQE         HsmSqe;
  AZIHSM_CP_SQE         *DeviceSqe       = NULL;
  AZIHSM_CP_CQE         *DeviceCqe       = NULL;
  AZIHSM_IO_QUEUE_PAIR  *QuePair         = &State->HsmQueue;
  UINT16                PsfBitBeforePost = 0;
  UINT32                WaitTime         = ADMIN_CMD_TIME_OUT_MS;

  if (!State || !DmaBufferIn || !DmaBufferOut || !SessionData || !InXferBuffSz || !OutXferBuffSz) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsm: [%a]: Invalid Parameters [State: %p, DmaBufferIn: %p, DmaBufferOut: %p, SessionData: %p, InXferBuffSz: %p, OutXferBuffSz: %p]\n",
      __FUNCTION__,
      State,
      DmaBufferIn,
      DmaBufferOut,
      SessionData
      ));

    return EFI_INVALID_PARAMETER;
  }

  if ((*InXferBuffSz == 0) || (*OutXferBuffSz == 0)) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsm: [%a]: Invalid Buffer Sizes [InXferBuffSz: %d, OutXferBuffSz: %d]\n",
      __FUNCTION__,
      *InXferBuffSz,
      *OutXferBuffSz
      ));
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&HsmSqe, sizeof (HsmSqe));
  HsmSqe.CmdId  = 0;
  HsmSqe.CmdSet = CP_CMD_SET_SESSION_GENERIC;
  HsmSqe.OpCode = OpCode;
  HsmSqe.Psdt   = PSDT_PRP;

  HsmSqe.SqeData = *SessionData;

  // Fill in the Source Buffer Params
  HsmSqe.Src.Prp.Fst = DmaBufferIn->DeviceAddress;
  HsmSqe.SrcLen      = (UINT32)*InXferBuffSz;

  HsmSqe.Dst.Prp.Fst = DmaBufferOut->DeviceAddress;
  HsmSqe.DstLen      = (UINT32)*OutXferBuffSz;

  DeviceSqe = (AZIHSM_CP_SQE *)QuePair->SubmissionQueue.Buffer.HostAddress;
  DeviceSqe = &DeviceSqe[QuePair->SubmissionQueue.u.Tail];

  DeviceCqe = (AZIHSM_CP_CQE *)QuePair->CompletionQueue.Buffer.HostAddress;
  DeviceCqe = &DeviceCqe[QuePair->CompletionQueue.u.Head];

  PsfBitBeforePost = DeviceCqe->PhAndSts.PhStsVal;

  // Copy the SQE over to the device
  *DeviceSqe = HsmSqe;

  AZIHSM_SQ_INC_TAIL (QuePair->SubmissionQueue);

  // Ring the doorbell register
  Status = AziHsmHciWrSqTailDbReg (State->PciIo, QuePair->Id, QuePair->SubmissionQueue.u.Tail);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [%a]: AziHsmHciWrSqTailDbReg Failed %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  Status = EFI_TIMEOUT;
  while (WaitTime) {
    if (DeviceCqe->PhAndSts.PhStsBits.Phase != (PsfBitBeforePost & 0x01)) {
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
      DeviceCqe->PhAndSts.PhStsVal
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
    AZIHSM_CQ_INC_HEAD (QuePair->CompletionQueue);

    AziHsmHciWrCqHeadReg (State->PciIo, QuePair->Id, QuePair->CompletionQueue.u.Head);

    // Check for the completion Status, if failed copy it over to
    // the caller
    if (DeviceCqe->PhAndSts.PhStsBits.Sts != 0) {
      DEBUG ((
        DEBUG_ERROR,
        "AziHsm: [%a]: Command Failed By Firmware [Status:0x%x] \n",
        __FUNCTION__,
        DeviceCqe->PhAndSts.PhStsVal
        ));

      if (FwSts != NULL) {
        *FwSts = DeviceCqe->PhAndSts.PhStsBits.Sts;
      }
    } else {
      *OutXferBuffSz = DeviceCqe->CqeData.SessionData.ByteCount;
    }
  }

Exit:
  return Status;
}
