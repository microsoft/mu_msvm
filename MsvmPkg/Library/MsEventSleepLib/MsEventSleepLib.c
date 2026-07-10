/** @file
  Implementation of MsWaitForEventSleep.

  The CPU is placed in a low-power wait between event checks using the
  per-architecture MsEnableInterruptsAndSleep primitive, which enables interrupts and
  sleeps in a single, race-free operation (see the X64/ and AArch64/ assembly
  sources).

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/MsEventSleepLib.h>
#include <Library/UefiBootServicesTableLib.h>

/**
  Enables interrupts and puts the processor to sleep in a single, race-free
  operation. Implemented in per-architecture assembly.

  Performing the enable and the sleep in one instruction sequence avoids a
  wakeup interrupt being delivered in the window between enabling interrupts
  and entering the sleep state, which would otherwise be lost.
**/
VOID
EFIAPI
MsEnableInterruptsAndSleep (
  VOID
  );

/**
  Waits for a single event to be signaled, sleeping the CPU between checks.

  Unlike gBS->WaitForEvent, this imposes no TPL restriction, so it may be called
  at TPL_CALLBACK. It must be called at TPL < TPL_HIGH_LEVEL in order to call
  gBS->CheckEvent.

  @param  Event  The event to wait on. The event must not have a notification
                 function (i.e. it must be a plain signal event).

  @retval EFI_SUCCESS            The event was signaled.
  @retval EFI_INVALID_PARAMETER  The event is invalid or has a notify function.
**/
EFI_STATUS
EFIAPI
MsWaitForEventSleep (
  IN  EFI_EVENT  Event
  )
{
  EFI_STATUS  Status;

  for ( ; ;) {
    Status = gBS->CheckEvent (Event);
    if (Status != EFI_NOT_READY) {
      //
      // EFI_SUCCESS (signaled) or EFI_INVALID_PARAMETER (bad event).
      //
      return Status;
    }

    //
    // Not yet signaled. Sleep until the next interrupt. The device completion
    // interrupt that signals the event will wake the CPU; the periodic timer
    // interrupt bounds the worst-case wakeup latency.
    //
    MsEnableInterruptsAndSleep ();
  }
}
