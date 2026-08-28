/** @file
  Helper for waiting on an EFI_EVENT at an elevated TPL without using
  gBS->WaitForEvent (which is only legal at TPL_APPLICATION).

  Synthetic device drivers (VmBus, EMCL, NetVSC, StorVSC) issue synchronous
  requests from TPL_CALLBACK and must block until a completion event is
  signaled. Those completion events are signaled from the VMBus SINT interrupt
  handler (see EfiHvDxe/VmbusDxe), which runs at TPL_HIGH_LEVEL. Because
  interrupts are enabled at any TPL below TPL_HIGH_LEVEL, the CPU can be put to
  sleep between checks instead of busy-looping: it wakes on the device
  completion interrupt, and the periodic timer interrupt bounds the wakeup
  latency.

  This replaces the former INTERNAL_EVENT_SERVICES_PROTOCOL crutch, which was
  only published by the C MdeModulePkg DXE Core and is unavailable under the
  Patina (Rust) DXE Core.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MS_EVENT_SLEEP_LIB_H
#define MS_EVENT_SLEEP_LIB_H

#include <Uefi.h>

/**
  Waits for a single event to be signaled, sleeping the CPU between checks.

  Unlike gBS->WaitForEvent, this imposes no TPL restriction, so it may be called
  at TPL_CALLBACK. It must be called at TPL < TPL_HIGH_LEVEL so that interrupts
  remain enabled and the CPU can be woken from sleep.

  @param  Event  The event to wait on. The event must not have a notification
                 function (i.e. it must be a plain signal event).

  @retval EFI_SUCCESS            The event was signaled.
  @retval EFI_INVALID_PARAMETER  The event is invalid or has a notify function.
**/
EFI_STATUS
EFIAPI
MsWaitForEventSleep (
  IN  EFI_EVENT  Event
  );

#endif // MS_EVENT_SLEEP_LIB_H
