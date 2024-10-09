/** @file
  Implementation of Watchdog Timer Architectural Protocol
  If available, this driver will use the Hyper-V BIOS device watchdog equivelent to a
  hardware watchdog timer.  If the hardware based timer is not available, a software timer
  will be used.

  Copyright (c) 2006 - 2011, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  This code is not similar enough to justify an override but it is loosely derived
  from the following:
            MdeModulePkg\Universal\WatchdogTimerDxe\WatchdogTimer.c

**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/WatchdogTimerLib.h>
#include <Guid/EventGroup.h>
#include <Protocol/WatchdogTimer.h>
#include <BiosInterface.h>
#include <IsolationTypes.h>

#define SEC_TO_100NS(x) ((x) * 10LL * 1000 * 1000)
#define _100NS_TO_S(x)  ((x) / (10LL * 1000 * 1000))

EFI_STATUS
EFIAPI
WatchdogSoftSetPeriod (
    IN  EFI_WATCHDOG_TIMER_ARCH_PROTOCOL    *This,
        UINT64                              TimerPeriod
    );

EFI_STATUS
EFIAPI
WatchdogHwSetPeriod (
    IN  EFI_WATCHDOG_TIMER_ARCH_PROTOCOL    *This,
        UINT64                              TimerPeriod
    );

EFI_STATUS
EFIAPI
WatchdogRegisterHandler (
    IN  EFI_WATCHDOG_TIMER_ARCH_PROTOCOL    *This,
    IN  EFI_WATCHDOG_TIMER_NOTIFY           NotifyFunction
    );

EFI_STATUS
EFIAPI
WatchdogGetTimerPeriod (
    IN  EFI_WATCHDOG_TIMER_ARCH_PROTOCOL    *This,
        UINT64                              *TimerPeriod
    );

//
// The Watchdog Timer Architectural Protocol instance produced by this driver
// The SetTimerPeriod handler will be updated in WatchdogInitialize.
//
EFI_WATCHDOG_TIMER_ARCH_PROTOCOL  mWatchdogTimer =
{
    WatchdogRegisterHandler,
    NULL,
    WatchdogGetTimerPeriod
};

//
// Handle for the Watchdog Timer Architectural Protocol instance produced by this driver
//
EFI_HANDLE  mWatchdogTimerHandle = NULL;

//
// Timer event used for resetting the HW or Soft Watchdog expiration.
//
EFI_EVENT   mWatchdogTimerEvent;

//
// The notification function to call if the watchdig timer fires
//
EFI_WATCHDOG_TIMER_NOTIFY  mWatchdogTimerNotifyFunction = NULL;

//
// The watchdog timer period in 100 ns units
//
UINT64      mWatchdogTimerPeriod = 0;

//
// Cached period of the hardware watchdog timer.
//
UINT32      mWatchdogTimerHwResolution = 0;

//
// Notification for when ExitBootServices is called.
//
static EFI_EVENT    mExitBootServicesEvent = NULL;

VOID
EFIAPI
WatchdogSoftTimer (
    IN  EFI_EVENT   Timer,
    IN  VOID        *Context
    )
/*++

Routine Description:

    Notification function that is called if the software based watchdog timer has expired.

Arguments:

    Timer               The periodic timer event.

    Context             The context that was registered when the event Timer was created.

Return Value:

    None.

--*/
{
    REPORT_STATUS_CODE(EFI_ERROR_CODE | EFI_ERROR_MINOR,
        (EFI_COMPUTING_UNIT_HOST_PROCESSOR | EFI_CU_HP_EC_TIMER_EXPIRED));

    if (mWatchdogTimerNotifyFunction != NULL)
    {
        mWatchdogTimerNotifyFunction(mWatchdogTimerPeriod);
    }

    DEBUG ((EFI_D_ERROR, "Watchdog Timer reseting system\n"));

    gRT->ResetSystem (EfiResetCold, EFI_TIMEOUT, 0, NULL);
}

EFI_STATUS
EFIAPI
WatchdogSoftSetPeriod (
    IN  EFI_WATCHDOG_TIMER_ARCH_PROTOCOL    *This,
        UINT64                              TimerPeriod
    )
/*++

Routine Description:

    Sets the amount of time to wait before firing the watchdog timer in 100 ns units.
    This function is used only when the watchdog hardware is unavailable.

Arguments:

    This                The EFI_WATCHDOG_TIMER_ARCH_PROTOCOL instance.

    TimerPeriod         The amount of time in 100 ns units to wait before the watchdog
                        timer is fired.
                        If TimerPeriod is zero, then the watchdog timer is disabled.

Return Value:

    EFI_SUCCESS         The watchdog timer has been programmed to fire in Time
                        100 ns units.

    EFI_DEVICE_ERROR    A watchdog timer could not be programmed due to a device
                        error.

    EFI_INVALID_PARAMETER   The given period is less than 1 second.

--*/
{
  mWatchdogTimerPeriod = TimerPeriod;

  return gBS->SetTimer(mWatchdogTimerEvent,
                (mWatchdogTimerPeriod == 0) ? TimerCancel : TimerRelative,
                mWatchdogTimerPeriod);
}

EFI_STATUS
EFIAPI
WatchdogHwSetPeriod (
    IN  EFI_WATCHDOG_TIMER_ARCH_PROTOCOL    *This,
        UINT64                              TimerPeriod
    )
/*++

Routine Description:

    Configures the hardware watchdog counter and enables the hardware timer.

Arguments:

    This                The EFI_WATCHDOG_TIMER_ARCH_PROTOCOL instance.

    TimerPeriod         The amount of time in 100 ns units to wait before the watchdog
                        timer is fired.
                        If TimerPeriod is zero, then the watchdog timer is disabled.

Return Value:

    EFI_SUCCESS         The watchdog timer has been programmed to fire in Time
                        100 ns units.

    EFI_DEVICE_ERROR    A watchdog timer could not be programmed due to a device
                        error.

    EFI_INVALID_PARAMETER   The given period is less than the watchdog timer's resolution.

--*/
{
    UINT32 timerPeriodSeconds;
    UINT32 watchdogCount;

    ASSERT ((mWatchdogTimerHwResolution != BIOS_WATCHDOG_NOT_ENABLED) &&
            (mWatchdogTimerHwResolution != 0));

    if ((TimerPeriod != 0) &&
        (TimerPeriod < (UINT64)SEC_TO_100NS(mWatchdogTimerHwResolution)))
    {
        return EFI_INVALID_PARAMETER;
    }

    mWatchdogTimerPeriod = TimerPeriod;

    //
    // Disable the watchdog before setting the new count.
    // The device is required to be completely disabled
    // before changing the mode or one-shot timeout.
    //
    WatchdogConfigure(0, WatchdogDisabled);

    if (TimerPeriod != 0)
    {

        //
        // Convert the desired expiration into the timer's native format.
        //
        timerPeriodSeconds  = (UINT32)_100NS_TO_S(TimerPeriod);
        watchdogCount       = timerPeriodSeconds / mWatchdogTimerHwResolution;
        DEBUG((EFI_D_INFO, "Hyper-V Watchdog Enabled. Expires in %d seconds (COUNT - %d).\n", timerPeriodSeconds, watchdogCount));
        WatchdogConfigure(watchdogCount, WatchdogOneShot);
    }
    else
    {
        DEBUG((EFI_D_INFO, "Hyper-V Watchdog Disabled.\n"));
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
WatchdogGetTimerPeriod (
    IN  EFI_WATCHDOG_TIMER_ARCH_PROTOCOL    *This,
        UINT64                              *TimerPeriod
    )
/*++

Routine Description:

    Retrieves the amount of time in 100 ns units that the system will wait before
    firing the watchdog timer.

Arguments:

    This                    The EFI_WATCHDOG_TIMER_ARCH_PROTOCOL instance.

    TimerPeriod             A pointer to the amount of time in 100 ns units that the system
                            will wait before the watchdog timer is fired.  If TimerPeriod of
                            zero is returned, then the watchdog timer is disabled.

Return Value:

    EFI_SUCCESS             The amount of time that the system will wait before
                            firing the watchdog timer was returned in TimerPeriod.

    EFI_INVALID_PARAMETER   TimerPeriod is NULL.

--*/
{
    if (TimerPeriod == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    *TimerPeriod = mWatchdogTimerPeriod;

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
WatchdogRegisterHandler (
    IN  EFI_WATCHDOG_TIMER_ARCH_PROTOCOL    *This,
    IN  EFI_WATCHDOG_TIMER_NOTIFY           NotifyFunction
    )
/*++

Routine Description:

    Registers a handler that is to be invoked when the watchdog timer fires.

    N.B.
        When the hardware timer is used the registered function will not
        be called as there is no notification mechanism.

Arguments:

    This                The EFI_WATCHDOG_TIMER_ARCH_PROTOCOL instance.

    NotifyFunction      The function to call when the watchdog timer fires.
                        If NULL, then the handler will be unregistered.

Return Value:

    EFI_SUCCESS           The watchdog timer handler was registered or unregistered.
    EFI_ALREADY_STARTED   NotifyFunction is not NULL, and a handler is already registered.
    EFI_INVALID_PARAMETER NotifyFunction is NULL, and a handler was not previously registered.

--*/
{
    //
    // If NotifyFunction is NULL, and a handler was not previously registered,
    // return EFI_INVALID_PARAMETER.
    //
    if ((NotifyFunction == NULL) && (mWatchdogTimerNotifyFunction == NULL))
    {
        return EFI_INVALID_PARAMETER;
    }

    //
    // If NotifyFunction is not NULL, and a handler is already registered,
    // return EFI_ALREADY_STARTED.
    //
    if ((NotifyFunction != NULL) && (mWatchdogTimerNotifyFunction != NULL))
    {
        return EFI_ALREADY_STARTED;
    }

    mWatchdogTimerNotifyFunction = NotifyFunction;

    return EFI_SUCCESS;
}

/// \brief      Called when the Exit Boot Services event is signaled.
///
/// \param[in]  Event    The event
/// \param      Context  The context
///
VOID
EFIAPI
ExitBootServicesHandler(
    IN  EFI_EVENT   Event,
    IN  void*       Context
    )
{

    //
    // Disable the watchdog as ExitBootServices is about to relinquish control
    // of the system to the bootloader.
    //
    WatchdogConfigure(0, WatchdogDisabled);
}

EFI_STATUS
EFIAPI
WatchdogInitialize (
    IN  EFI_HANDLE          ImageHandle,
    IN  EFI_SYSTEM_TABLE    *SystemTable
    )
/*++

Routine Description:

    Driver entry point. Detects the watchdog timer and registers the
    Watchdog Timer Architectural Protocol.

Arguments:

    ImageHandle         The image handle of this driver.

    SystemTable         The pointer of EFI_SYSTEM_TABLE.

Return Value:

    EFI_STATUS

--*/
{
    EFI_STATUS  status;
    BOOLEAN useSoftwareTimer = FALSE;

    ASSERT_PROTOCOL_ALREADY_INSTALLED(NULL, &gEfiWatchdogTimerArchProtocolGuid);

    if (IsHardwareIsolatedNoParavisor())
    {
        DEBUG((EFI_D_INFO, "Running on an isolated guest without the BIOS emulator. Falling back to software.\n"));
        useSoftwareTimer = TRUE;
    }
    else
    {

        //
        // Read the hardware timer resolution to determine if it is
        // available. Fallback to the software timer if hardware is disabled.
        //
        mWatchdogTimerHwResolution = WatchdogGetResolution();

        if ((mWatchdogTimerHwResolution == 0) ||
            (mWatchdogTimerHwResolution == BIOS_WATCHDOG_NOT_ENABLED))
        {
            DEBUG((EFI_D_INFO, "No watchdog hardware available. Falling back to software.\n"));
            useSoftwareTimer = TRUE;
        }
    }

    if (useSoftwareTimer)
    {
        DEBUG((EFI_D_INFO, "Using software timer.\n"));
        mWatchdogTimerHwResolution = BIOS_WATCHDOG_NOT_ENABLED;
        mWatchdogTimer.SetTimerPeriod = WatchdogSoftSetPeriod;

        status = gBS->CreateEvent (
                      EVT_TIMER | EVT_NOTIFY_SIGNAL,
                      TPL_NOTIFY,
                      WatchdogSoftTimer,
                      NULL,
                      &mWatchdogTimerEvent
                      );
        ASSERT_EFI_ERROR (status);

    }
    else
    {
        DEBUG((EFI_D_INFO, "Using Hyper-V watchdog timer.\n"));
        mWatchdogTimer.SetTimerPeriod = WatchdogHwSetPeriod;

        //
        // No Periodic timer is needed when using the HW timer.
        // This driver will use it in one-shot mode.
        //
    }

    //
    // Register notify function for EVT_SIGNAL_EXIT_BOOT_SERVICES.
    //
    status = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,
                              TPL_NOTIFY,
                              ExitBootServicesHandler,
                              NULL,
                              &gEfiEventExitBootServicesGuid,
                              &mExitBootServicesEvent);
    ASSERT_EFI_ERROR(status);
    if (EFI_ERROR(status))
    {
        DEBUG((DEBUG_ERROR, "--- %a: failed to create the exit boot services event - %r \n", __FUNCTION__, status));
        return status;
    }

    //
    // Install the Watchdog Timer Arch Protocol onto a new handle
    //
    status = gBS->InstallMultipleProtocolInterfaces(
                  &mWatchdogTimerHandle,
                  &gEfiWatchdogTimerArchProtocolGuid,
                  &mWatchdogTimer,
                  NULL
                  );
    ASSERT_EFI_ERROR (status);

    return EFI_SUCCESS;
}
