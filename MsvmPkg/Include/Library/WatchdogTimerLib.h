/** @file
  This module contains code to interact with the Hyper-V watchdog timer.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

typedef enum
{
    //
    // Watchdog is disabled.  This should only be used with a count
    // value of zero.
    //
    WatchdogDisabled,
    //
    //  The count represents the amount of time before the timer will
    //  expired.
    //
    WatchdogOneShot,
    //
    // The hardware timer will run periodically and decrement the count.
    // The timer is expired when the count reaches zero.
    // When used in periodic mode there is normally a periodic entity in
    // UEFI that will reset the count to its original value.
    //
    WatchdogPeriodic
} WATCHDOG_MODE;


VOID
WatchdogConfigure(
    UINT32          Count,
    WATCHDOG_MODE   Mode
    );


VOID
WatchdogSetCount(
    UINT32  Count
    );


UINT32
WatchdogGetResolution();


BOOLEAN
WatchdogSuspend();


VOID
WatchdogResume(
    BOOLEAN PreviouslyRunning
    );