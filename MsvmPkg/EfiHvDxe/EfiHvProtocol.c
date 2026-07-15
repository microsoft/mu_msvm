/** @file
  Implements EFI_HV_PROTOCOL message, signal, and VP startup calls.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "EfiHvInternal.h"

EFI_STATUS
EFIAPI
EfiHvPostMessage (
  IN  EFI_HV_PROTOCOL *This,
  IN  HV_CONNECTION_ID ConnectionId,
  IN  HV_MESSAGE_TYPE MessageType,
  IN  VOID *Payload,
  IN  UINT32 PayloadSize,
  IN  BOOLEAN DirectHypercall
  )
/*++
  Posts a message to a hypervisor message port.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @param ConnectionId The connection ID of the message port.

  @param MessageType The type of the message.

  @param Payload A pointer to the payload buffer.

  @param PayloadSize The length of the payload buffer, in bytes.

  @param DirectHypercall Do not bypass the paravisor, if one is present.

  @returns EFI status.

--*/
{
  PHV_INPUT_POST_MESSAGE input;
  HV_STATUS hvStatus;
  EFI_STATUS status;
  EFI_TPL oldTpl;

  DEBUG((DEBUG_VERBOSE, ">>> %a: ConnId 0x%x MessageType 0x%x Payload 0x%p Size 0x%x\n",
    __func__, ConnectionId, MessageType, Payload, PayloadSize));

  //
  // A direct hypercall is only valid if we are hardware isolated with a
  // paravisor.
  //

  if (DirectHypercall && (!mUseBypassContext || mBypassOnly))
  {
    return EFI_INVALID_PARAMETER;
  }

  if ((PayloadSize > HV_MESSAGE_PAYLOAD_BYTE_COUNT) ||
    ((PayloadSize != 0) && (Payload == NULL)))
  {
    return EFI_INVALID_PARAMETER;
  }

  oldTpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);
  input = (PHV_INPUT_POST_MESSAGE)(DirectHypercall ? mHvPages->HypercallInputPage : mHvInputPage);
  input->ConnectionId = ConnectionId;
  input->Reserved = 0;
  input->MessageType = MessageType;
  input->PayloadSize = PayloadSize;
  if (PayloadSize != 0)
  {
    CopyMem(input->Payload, Payload, PayloadSize);
  }

  ZeroMem((UINT8 *)input->Payload + PayloadSize,
      sizeof(input->Payload) - PayloadSize);

  hvStatus =
    HvHypercallIssue(
      (mUseBypassContext && !DirectHypercall) ? &mHvBypassContext : &mHvContext,
      HvCallPostMessage,
      FALSE,
      0,
      EfiHvpBasePa((UINTN)input),
      0,
      NULL);

  gBS->RestoreTPL(oldTpl);
  switch (hvStatus)
  {
  default:
    status = EfiHvConvertStatus(hvStatus);
    break;

  //
  // The following status values will be returned if the message queue is full
  // or if the VM has been throttled. Convert this to EFI_NOT_READY so
  // that the caller can retry later.
  //
  // N.B. The paravisor should not throttle messages, so treat it as an error
  //      in that case.
  //
  case HV_STATUS_INVALID_CONNECTION_ID:
    if (DirectHypercall)
    {
      status = EFI_DEVICE_ERROR;
    }
    else
    {
      status = EFI_NOT_READY;
    }

    break;

  case HV_STATUS_INSUFFICIENT_BUFFERS:
    status = EFI_NOT_READY;
  }

  return status;
}

EFI_STATUS
EFIAPI
EfiHvSignalEvent (
  IN  EFI_HV_PROTOCOL *This,
  IN  HV_CONNECTION_ID ConnectionId,
  IN  UINT16 FlagNumber
  )
/*++
  Signals a hypervisor event port.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @param ConnectionId The connection ID of the port.

  @param FlagNumber The flag number offset.

  @returns EFI status.

--*/
{
  HV_STATUS hvStatus;
  PHV_INPUT_SIGNAL_EVENT input;
  UINT64 registers[2];

  ZeroMem(registers, sizeof(registers));

  input = (PHV_INPUT_SIGNAL_EVENT)registers;
  input->ConnectionId = ConnectionId;
  input->FlagNumber = FlagNumber;
  input->RsvdZ = 0;
  hvStatus = EfiHvIssueHypercall(HvCallSignalEvent,
                   TRUE,
                   registers[0],
                   registers[1]);

  return EfiHvConvertStatus(hvStatus);
}

EFI_STATUS
EFIAPI
EfiHvStartApplicationProcessor (
  IN  EFI_HV_PROTOCOL *This,
  IN  UINT64 VpIndex,
  IN  PHV_INITIAL_VP_CONTEXT VpContext
  )
/*++
  Start an application processor.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @param VpIndex The VP Index on which the application processor will start.

  @param VpContext The initial context for the VP.

  @returns EFI status.

--*/
{
  PHV_INPUT_START_VIRTUAL_PROCESSOR input;
  HV_STATUS hvStatus;

  input = (PHV_INPUT_START_VIRTUAL_PROCESSOR)mHvPages->HypercallInputPage;

  input->ReservedZ0 = 0;
  input->ReservedZ1 = 0;
  input->PartitionId = HV_PARTITION_ID_SELF;
  input->TargetVtl = 0;
  CopyMem(&input->VpContext, VpContext, sizeof(HV_INITIAL_VP_CONTEXT));
  input->VpIndex = (HV_VP_INDEX)VpIndex;

  hvStatus =
    EfiHvIssueHypercall(
      HvCallStartVirtualProcessor,
      FALSE,
      EfiHvpBasePa((UINTN)input),
      0);

  return EfiHvConvertStatus(hvStatus);
}
