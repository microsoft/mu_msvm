/** @file
  Implementation of the Azure Integrated HSM DXE driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  This header file contains all the admin queue commands
  and the definitions that are needed for them
--*/

#ifndef __AZIHSMCP_H__
#define __AZIHSMCP_H__
#include "AziHsmDxe.h"

#define AZIHSM_HSM_MAX_QUEUE_CNT     128    // MAX Queues Supported By HSM
#define AZIHSM_HSM_CREATE_QUEUE_CNT  1      // Queue Cnt that we are going to create
#define AZIHSM_HSM_QUEUE_ID          1

#define AZIHSM_HSM_CMD_CQE_SIZE  16
#define AZIHSM_HSM_CP_SQE_SZ     64     // Size of Each Slot

#pragma pack(push, 1)

// Values for Opcode In AZIHSM_CP_SESSION_CTRL_FLAGS
#define MCR_OPCODE_FLOW_NO_SESSION     0
#define MCR_OPCODE_FLOW_OPEN_SESSION   1
#define MCR_OPCODE_FLOW_CLOSE_SESSION  2
#define MCR_OPCODE_FLOW_IN_SESSION     3

typedef struct _AZIHSM_CP_SESSION_CTRL_FLAGS {
  UINT8    Opcode             : 2;
  UINT8    InSessionCmd       : 1;
  UINT8    ShortAppIdValid    : 1;
  UINT8    SafeToCloseSession : 1;
  UINT8    Rsvd               : 3;
} AZIHSM_CP_SESSION_CTRL_FLAGS;

typedef union _AZIHSM_CP_CMD_SQE_SRC_DATA_ {
  struct _AZIHSM_CP_CMD_SQE_SESSION {
    AZIHSM_CP_SESSION_CTRL_FLAGS    SessionCtrlFlags;
    UINT8                           rsvd_1[3];
    UINT16                          SessionId;
    UINT8                           rsvd_2[14];
  } SqeSessionData;

  UINT8    val[20];
} AZIHSM_CP_CMD_SQE_SRC_DATA;

typedef union _AZIHSM_CP_SQE_DPTR_ {
  UINT8    FstSnd[16]; // First and Second PRP Values
  struct {
    UINT64    Fst;  // PRP 1 Entry
    UINT64    Snd;  // PRP 2 Entry
  } Prp;
} AZIHSM_CP_SQE_DPTR;

// Define the data transfer type as PRP or SGL
typedef enum _PSDT_TYPE_ {
  PSDT_PRP = 0, FP_PSDT_SGL = 1
} PSDT_TYPE;

typedef enum _CP_CMD_SET_ {
  CP_CMD_SET_SESSION_GENERIC = 0x0,
  CP_CMD_SET_TEST            = 0x0F
} CP_CMD_SET;

// We do not want to copy the entire HSM command set here
// Right now we will have SQE and CQE Definitions so that
// We can implement the Fire command function
typedef struct _AZIHSM_CP_SQE_ {
  UINT32                        OpCode : 10;
  UINT32                        CmdSet : 4;
  UINT32                        Psdt   : 2;
  UINT32                        CmdId  : 16;

  UINT32                        SrcLen;
  AZIHSM_CP_SQE_DPTR            Src;

  UINT32                        DstLen;
  AZIHSM_CP_SQE_DPTR            Dst;

  AZIHSM_CP_CMD_SQE_SRC_DATA    SqeData;
} AZIHSM_CP_SQE, *PAZIHSM_CP_SQE;
static_assert ((sizeof (AZIHSM_CP_SQE) == AZIHSM_HSM_CP_SQE_SZ), "CP_SQE: Size Mismatch");

/*
 * Control Processor Completion Queue Entry
 */

typedef union _AZIHSM_CP_CMD_CQE_DATA_ {
  struct _AziHsmCpCqeSession {
    UINT16                          ByteCount;
    AZIHSM_CP_SESSION_CTRL_FLAGS    SessionCtrlFlags;
    UINT8                           rsvd_1;
    UINT16                          SessionId;

    /* 8-bit short app id.
     * The validity of this field
     * is dependent on the control fields
     */
    UINT8                           ShortAppId;
    UINT8                           rsvd_2;
  } SessionData;

  UINT8    val[8];
} AZIHSM_CP_CMD_CQE_DATA;

typedef struct _AZIHSM_CP_CQE_ {
  AZIHSM_CP_CMD_CQE_DATA    CqeData;
  UINT16                    SqHead;
  UINT16                    SqId;
  UINT16                    CmdId;

  union {
    UINT16    PhStsVal;  // Phase & Status field value
    struct {
      UINT16    Phase : 1;    // Phase
      UINT16    Sts   : 11;   // Status code
      UINT16    Rsvd  : 4;
    } PhStsBits;
  } PhAndSts;
} AZIHSM_CP_CQE, *PAZIHSM_CP_CQE;

static_assert (sizeof (AZIHSM_CP_CQE) == AZIHSM_HSM_CMD_CQE_SIZE, "AZIHSM_CP_CQE Size Incompatible");

#pragma pack(pop)

/**
 * Initialization of the Control (HSM) Path
 * Of the controller. This function creates the
 * IO queues in the hardware.
 *
 * @param [in]  State         Pointer to the HSM controller state.
 *
 * @retval EFI_SUCCESS        The operation completed successfully.
 * @retval EFI_DEVICE_ERROR   The Command Failed.
 *
 */
EFI_STATUS
EFIAPI
AziHsmInitHsm (
  IN AZIHSM_CONTROLLER_STATE  *State
  );

/**
 * This function fires the HSM command and gets the reponse from
 * the device. The function returns the status of operation.
 * The status returned by the firmware is also returned.
 *
 * @param [in]  State         Pointer to the HSM controller state.
 * @param [in]  DmaBufferIn   Pointer to the input DMA buffer.
 * @param [in]  DmaBufferOut  Pointer to the output DMA buffer.
 * @param [in]  OpCode        The operation code for the command.
 * @param [in]  SessionData   Session Information As maintained by caller.
 * @param [out] FwSts         Pointer to the firmware status variable.
 *                            Status returned by the firmware is returned
 *                            in this variable, for both success and failed
 *                            operations.
 *
 * @retval EFI_SUCCESS        The operation completed successfully.
 * @retval EFI_DEVICE_ERROR   The Command Failed.

 */
EFI_STATUS
EFIAPI
AziHsmFireHsmCmd (
  IN AZIHSM_CONTROLLER_STATE     *State,
  IN OUT AZIHSM_DMA_BUFFER       *DmaBufferIn,
  IN OUT UINT32                  *InXferBuffSz,
  IN OUT AZIHSM_DMA_BUFFER       *DmaBufferOut,
  IN OUT UINT32                  *OutXferBuffSz,
  IN UINT32                      OpCode,
  IN AZIHSM_CP_CMD_SQE_SRC_DATA  *SessionData,
  OUT UINT32                     *FwSts
  );

#endif // __AZIHSMCP_H__
