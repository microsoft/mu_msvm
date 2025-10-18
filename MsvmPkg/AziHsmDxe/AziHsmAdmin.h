/** @file
  Implementation of the Azure Integrated HSM DXE driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  This header file contains all the admin queue commands
  and the definitions that are needed for them
--*/

#ifndef __AZIHSMADMIN_H__
#define __AZIHSMADMIN_H__

#include "AziHsmQueue.h"
#include "AziHsmCp.h"
#include "AziHsmDxe.h"

#define ADMIN_CMD_TIME_OUT_MS      100 // Millisec Wait
#define AZIHSM_ADMIN_CMD_CQE_SIZE  16
#define AZIHSM_ADMIN_CMD_SQE_SIZE  64

enum AziHsmAdminCmdFeatId {
  AZIHSM_CTRL_CMD_FEAT_ID_HSM_QUEUE_CNT = 0x07,
  AZIHSM_CTRL_CMD_FEAT_ID_AES_QUEUE_CNT = 0xC1,
};

#pragma pack(push, 1)

typedef union  {
  UINT32    Val;
  struct {
    UINT32    Sq : 16;
    UINT32    Cq : 16;
  } QueueCnt;
} AZIHSM_ADMIN_CMD_CQE_CS;

STATIC_ASSERT (sizeof (AZIHSM_ADMIN_CMD_CQE_CS) == 4, "AZIHSM_ADMIN_CMD_CQE_CS Size Incompatible.");
//
// Command completion queue entry
//
typedef struct  {
  AZIHSM_ADMIN_CMD_CQE_CS    Cs;
  UINT32                     Reserved;
  UINT16                     SqHd;
  UINT16                     SqId;
  UINT16                     cid;
  union {
    UINT16    Val;
    struct {
      UINT16    P    : 1;
      UINT16    Sc   : 11;
      UINT16    Rsvd : 4;
    } Bits;
  } Psf;
} AZIHSM_ADMIN_CMD_CQE,
*PAZIHSM_ADMIN_CMD_CQE;

STATIC_ASSERT (sizeof (AZIHSM_ADMIN_CMD_CQE) == AZIHSM_ADMIN_CMD_CQE_SIZE, "AZIHSM_ADMIN_CMD_CQE Size Incompatible.");

#define AZIHSM_ADMIN_CMD_SQE_HDR_INIT(_Hdr, _OpCode, _prp1) \
{\
  _Hdr.Opc = _OpCode;\
  _Hdr.Psdt = 0;\
  _Hdr.Cid = 0;\
  _Hdr.dptr.prp.Fst = _prp1;\
  _Hdr.dptr.prp.Snd = 0;\
}

//
// Command submission queue Header
//
typedef struct {
  UINT8     Opc;
  UINT8     Rsvd1 : 6;
  UINT8     Psdt  : 2;
  UINT16    Cid;
  UINT32    Rsvd2[3];
  UINT64    Mptr;
  union {
    UINT8    Val[16];
    struct {
      UINT64    Fst;
      UINT64    Snd;
    } prp;
  } dptr;
} AZIHSM_ADMIN_CMD_SQE_HDR;

//
// Command submission queue delete completion/submission queue
//
typedef struct  {
  AZIHSM_ADMIN_CMD_SQE_HDR    Hdr;
  UINT16                      Id;
  UINT8                       Rsvd[22];
} AZIHSM_ADMIN_CMD_SQE_DELETE;

STATIC_ASSERT (
  sizeof (AZIHSM_ADMIN_CMD_SQE_DELETE) == AZIHSM_ADMIN_CMD_SQE_SIZE,
  "AZIHSM_ADMIN_CMD_SQE_DELETE Size Incompatable."
  );

//
// Command submission queue create completion/submission queue
//
typedef struct {
  AZIHSM_ADMIN_CMD_SQE_HDR    Hdr;
  UINT32                      Id    : 16;
  UINT32                      Size  : 16;
  UINT32                      Pc    : 1;
  UINT32                      Ien   : 1;
  UINT32                      Rsvd1 : 14;
  UINT32                      Ivec  : 16;
  UINT32                      Rsvd2[4];
}  AZIHSM_ADMIN_CMD_SQE_CREATE_CQ;

STATIC_ASSERT (
  sizeof (AZIHSM_ADMIN_CMD_SQE_CREATE_CQ) == AZIHSM_ADMIN_CMD_SQE_SIZE,
  "AZIHSM_ADMIN_CMD_SQE_CREATE Size Incompatable."
  );

typedef struct _AZIHSM_ADMIN_CMD_SQE_CREATE_SQ_ {
  AZIHSM_ADMIN_CMD_SQE_HDR    Hdr;
  UINT32                      QueId   : 16; // Queue Identifier To be used in Completions
  UINT32                      QueSz   : 16; // The size of the queue in number of slots
  UINT32                      PhyCont : 1;  // Physically Contiguous
  UINT32                      QuePri  : 2;  // Queue Priority 00-Urgent 01-High 10-Medium 11-Low
  UINT32                      Rsvd1   : 13;
  UINT32                      CqId    : 16; // Completion Queue Associated with this SQ
  UINT32                      Rsvd2[4];
} AZIHSM_ADMIN_CMD_SQE_CREATE_SQ, *PAZIHSM_ADMIN_CMD_SQE_CREATE_SQ;

//
// Command submission queue Identity
//
typedef struct {
  AZIHSM_ADMIN_CMD_SQE_HDR    Hdr;
  UINT32                      Cns    : 8;
  UINT32                      Rsvd1  : 8;
  UINT32                      CtrlId : 16;
  UINT32                      Rsvd2[5];
} AZIHSM_ADMIN_CMD_SQE_IDENT;

STATIC_ASSERT (
  sizeof (AZIHSM_ADMIN_CMD_SQE_IDENT) == AZIHSM_ADMIN_CMD_SQE_SIZE,
  "AZIHSM_ADMIN_CMD_SQE_IDENT Size Incompatable."
  );

//
// Command submission queue Abort
//
typedef struct {
  AZIHSM_ADMIN_CMD_SQE_HDR    Hdr;
  UINT32                      Sqid : 16;
  UINT32                      Cid  : 16;
  UINT32                      Rsvd[5];
} AZIHSM_ADMIN_CMD_SQE_ABORT;

STATIC_ASSERT (
  sizeof (AZIHSM_ADMIN_CMD_SQE_ABORT) == AZIHSM_ADMIN_CMD_SQE_SIZE,
  "AZIHSM_ADMIN_CMD_SQE_ABORT Size Incompatable."
  );

//
// Command feat data
//
typedef union {
  UINT32    Val;
  struct {
    UINT16    SqCnt;
    UINT16    CqCnt;
  } QueueCnt;
} AZIHSM_ADMIN_CMD_FEAT_DATA;

STATIC_ASSERT (sizeof (AZIHSM_ADMIN_CMD_FEAT_DATA) == 4, "AZIHSM_ADMIN_CMD_FEAT_DATA Size Incompatible.");

//
// Command submission set feat
//
typedef struct {
  AZIHSM_ADMIN_CMD_SQE_HDR      Hdr;
  UINT32                        FeatId : 8;
  UINT32                        Rsvd1  : 24;
  AZIHSM_ADMIN_CMD_FEAT_DATA    Data;
  UINT32                        Rsvd2[4];
} AZIHSM_ADMIN_CMD_SQE_SET_FEAT;

STATIC_ASSERT (
  sizeof (AZIHSM_ADMIN_CMD_SQE_SET_FEAT) == AZIHSM_ADMIN_CMD_SQE_SIZE,
  "AZIHSM_ADMIN_CMD_SQE_SET_FEAT Size Incompatable."
  );

//
// Command submission get feat
//
typedef struct {
  AZIHSM_ADMIN_CMD_SQE_HDR    Hdr;
  UINT8                       FeatId;
  UINT8                       Rsvd1[23];
} AZIHSM_ADMIN_CMD_SQE_GET_FEAT;

STATIC_ASSERT (
  sizeof (AZIHSM_ADMIN_CMD_SQE_GET_FEAT) == AZIHSM_ADMIN_CMD_SQE_SIZE,
  "AZIHSM_ADMIN_CMD_SQE_GET_FEAT Size Incompatable."
  );

//
// Command submission set reset count
//
typedef struct {
  AZIHSM_ADMIN_CMD_SQE_HDR    Hdr;
  UINT32                      CtrlId;
  UINT32                      Cnt;
  UINT8                       Rsvd2[16];
} AZIHSM_ADMIN_CMD_SQE_SET_RES_CNT;

STATIC_ASSERT (
  sizeof (AZIHSM_ADMIN_CMD_SQE_SET_RES_CNT) == AZIHSM_ADMIN_CMD_SQE_SIZE,
  "AZIHSM_ADMIN_CMD_SQE_SET_RES_CNT Size Incompatable."
  );

typedef union {
  AZIHSM_ADMIN_CMD_SQE_DELETE         DeleteCq;
  AZIHSM_ADMIN_CMD_SQE_CREATE_CQ      CreateCq;
  AZIHSM_ADMIN_CMD_SQE_DELETE         DeleteSq;
  AZIHSM_ADMIN_CMD_SQE_CREATE_SQ      CreateSq;
  AZIHSM_ADMIN_CMD_SQE_IDENT          Ident;
  AZIHSM_ADMIN_CMD_SQE_ABORT          Abort;
  AZIHSM_ADMIN_CMD_SQE_SET_FEAT       SetFeat;
  AZIHSM_ADMIN_CMD_SQE_GET_FEAT       GetFeat;
  AZIHSM_ADMIN_CMD_SQE_SET_RES_CNT    SetResCnt;
} AZIHSM_ADMIN_CMD_SQE;

STATIC_ASSERT (sizeof (AZIHSM_ADMIN_CMD_SQE) == AZIHSM_ADMIN_CMD_SQE_SIZE, "AZIHSM_ADMIN_CMD_SQE Size Incompatible.");

#pragma pack(pop)

typedef enum {
  AZIHSM_ADMIN_CMD_OP_DELETE_SQ   = 0x00,
  AZIHSM_ADMIN_CMD_OP_CREATE_SQ   = 0x01,
  AZIHSM_ADMIN_CMD_OP_DELETE_CQ   = 0x04,
  AZIHSM_ADMIN_CMD_OP_CREATE_CQ   = 0x05,
  AZIHSM_ADMIN_CMD_OP_IDENT       = 0x06,
  AZIHSM_ADMIN_CMD_OP_ABORT       = 0x08,
  AZIHSM_ADMIN_CMD_OP_SET_FEAT    = 0x09,
  AZIHSM_ADMIN_CMD_OP_GET_FEAT    = 0x0A,
  AZIHSM_ADMIN_CMD_OP_SET_RES_CNT = 0xC3,
  AZIHSM_ADMIN_CMD_OP_GET_RES_CNT = 0xC4
} ADMIN_CMD_OPCODES;

#define AZIHSM_CTRL_IDENT_SN_LEN  32
#define AZIHSM_CTRL_IDENT_MN_LEN  4
#define AZIHSM_CTRL_IDENT_FR_LEN  32

//
// AZM HSM contrller Queue Entry Size
//
typedef union _AZI_HSM_CTRL_IDEN_QES {
  UINT8    Val;
  struct {
    UINT8    Max : 4;
    UINT8    Min : 4;
  } Bits;
} AZI_HSM_CTRL_IDEN_QES;

//
// AZM HSM contrller Identy
//
typedef struct _AZI_HSM_CTRL_IDEN {
  UINT16                   Vid;
  UINT16                   SsVid;
  CHAR8                    Sn[AZIHSM_CTRL_IDENT_SN_LEN];
  CHAR8                    Fr[AZIHSM_CTRL_IDENT_FR_LEN];
  UINT8                    Reserved1[4];
  UINT8                    CP_Mdts;
  UINT8                    Reserved2;
  UINT16                   Ctrl_Id;
  UINT8                    Acl;
  AZI_HSM_CTRL_IDEN_QES    CP_Sqes;
  AZI_HSM_CTRL_IDEN_QES    CP_Cqes;
  UINT8                    Reserved3;
  UINT16                   CP_MaxCmd;
  UINT8                    Reserved4;
  AZI_HSM_CTRL_IDEN_QES    Reserved5;
  AZI_HSM_CTRL_IDEN_QES    Reserved6;
  UINT8                    Reserved7;
  UINT16                   Reserved8;
  UINT16                   Oacs;
  UINT16                   Reserved9;
  UINT32                   Sgls;
  UINT32                   Ver;
  UINT8                    Ctrl_Type;
  UINT8                    Frmw;
} AZI_HSM_CTRL_IDEN;

/**
 * APIs exposed by Admin Queues Of the controller
 */

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
  );

/**
 * Create a Queue Pair in the device.
 * @param[in]  State        The State of the controller
 * @retval EFI_SUCCESS        The operation completed successfully.
 * @retval EFI_DEVICE_ERROR   The Command Failed.
 */
EFI_STATUS
EFIAPI
AziHsmAdminCreateDeviceIoQuePair (
  IN AZIHSM_CONTROLLER_STATE  *State,
  AZIHSM_IO_QUEUE_PAIR        *QueuePair
  );

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
  );

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
  );

#endif // __AZIHSMADMIN_H__
