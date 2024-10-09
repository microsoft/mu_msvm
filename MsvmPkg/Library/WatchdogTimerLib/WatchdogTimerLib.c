/** @file
  This module contains code to interact with the Hyper-V watchdog timer.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Library/BaseLib.h>
#include <Library/BiosDeviceLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/WatchdogTimerLib.h>

#include <BiosInterface.h>
#include <IsolationTypes.h>
#include <Uefi.h>

VOID
WatchdogConfigure(
    UINT32          Count,
    WATCHDOG_MODE   Mode
    )
/*++

Routine Description:

     Sets the initial watchdog count and starts the watchdog timer.
     If the watchdog timer was previously suspended it is resumed.

Arguments:

    Count       Count value.  This value is derived from the desired expiration in
                seconds divided by the period.

    Mode        Mode for the watchdog timer.

Return Value:

    None.

--*/
{
    UINT32  configValue = 0;
    BOOLEAN interruptState;

    if (IsHardwareIsolatedNoParavisor())
    {
        // No hardware watchdog when no paravisor.
        return;
    }

    interruptState = SaveAndDisableInterrupts();

    if (Count != 0)
    {
        configValue = BIOS_WATCHDOG_RUNNING;

        ASSERT((Mode == WatchdogOneShot) ||
               (Mode == WatchdogPeriodic));

        if (Mode == WatchdogOneShot)
        {
            configValue |= BIOS_WATCHDOG_ONE_SHOT;
        }

        WriteBiosDevice(BiosConfigWatchdogCount, Count);
    }
    else
    {
        ASSERT(Mode == WatchdogDisabled);
    }

    WriteBiosDevice(BiosConfigWatchdogConfig, configValue);
    SetInterruptState(interruptState);
}


VOID
WatchdogSetCount(
    UINT32  Count
    )
/*++

Routine Description:

    Sets the value of the watchdog count register.
    A value of zero disables the watchdog timer without changing the count value.

Arguments:

    Count     Value to store as the current watchdog count

Return Value:

    None.

--*/
{
    BOOLEAN interruptState = SaveAndDisableInterrupts();

    if (Count == 0)
    {
        WriteBiosDevice(BiosConfigWatchdogConfig, 0);
    }
    else
    {
        WriteBiosDevice(BiosConfigWatchdogCount, Count);
    }

    SetInterruptState(interruptState);
}


UINT32
WatchdogGetResolution()
/*++

Routine Description:

     Returns the resolution of the hardware timer.

Arguments:

    None.

Return Value:

    The hardware timer's resolution in seconds.
    If the hardware timer is not available a value of 0xFFFFFFFF is returned.

--*/
{
    UINT32  resolution;
    BOOLEAN interruptState = SaveAndDisableInterrupts();

    resolution = ReadBiosDevice(BiosConfigWatchdogResolution);

    SetInterruptState(interruptState);

    return resolution;
}


BOOLEAN
WatchdogSuspend()
/*++

Routine Description:

     Suspends the watchdog timer.  When suspended the hardware timer is still active
     but the watchdog count is not decremented.  If the watchdog timer is not configured
     this routine has no effect.

     The return value should be used as the input to WatchdogResume

Arguments:

    None.

Return Value:

    TRUE if the watchdog was configured and enabled before the call to suspend.
    FALSE if not.

--*/
{
    UINT32  value;
    BOOLEAN previousState;
    BOOLEAN interruptState = SaveAndDisableInterrupts();

    value = ReadBiosDevice(BiosConfigWatchdogConfig);

    previousState = ((value & BIOS_WATCHDOG_RUNNING) == BIOS_WATCHDOG_RUNNING);
    value &= ~BIOS_WATCHDOG_ENABLED;

    WriteBiosDevice(BiosConfigWatchdogConfig, value);

    SetInterruptState(interruptState);

    return previousState;
}


VOID
WatchdogResume(
    BOOLEAN PreviouslyRunning
    )
/*++

Routine Description:

     Resumes the watchdog timer.
     If the watchdog timer is not configured, this routine has no effect.

Arguments:

    PreviouslyRunning   Previous state of the watchdog timer prior to a call to WatchdogSuspend.
                        If TRUE the watchdog will be resumed.
                        If FALSE no change will be made.

Return Value:

    None.

--*/
{
    UINT32  value;
    BOOLEAN interruptState;

    if (PreviouslyRunning == FALSE)
    {
        return;
    }

    interruptState = SaveAndDisableInterrupts();

    value = ReadBiosDevice(BiosConfigWatchdogConfig);
    value |= BIOS_WATCHDOG_ENABLED;
    WriteBiosDevice(BiosConfigWatchdogConfig, value);

    SetInterruptState(interruptState);
}
