/** @file
  Provides an implementation of the EFI_TIMER_ARCH_PROTOCOL architectural
  protocol with a Hyper-V synthetic timer. This is more efficient than using
  the 8254 timer (PIT).

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/


#include <PiDxe.h>

#include <Protocol/EfiHv.h>
#include <Protocol/Timer.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

EFI_STATUS
EFIAPI
SynicTimerRegisterHandler (
    IN  EFI_TIMER_ARCH_PROTOCOL  *This,
    IN  EFI_TIMER_NOTIFY         NotifyFunction
    );

EFI_STATUS
EFIAPI
SynicTimerSetTimerPeriod (
    IN  EFI_TIMER_ARCH_PROTOCOL  *This,
    IN  UINT64                   TimerPeriod
    );

EFI_STATUS
EFIAPI
SynicTimerGetTimerPeriod (
    IN  EFI_TIMER_ARCH_PROTOCOL   *This,
    OUT UINT64                   *TimerPeriod
    );

EFI_STATUS
EFIAPI
SynicTimerGenerateSoftInterrupt (
    IN  EFI_TIMER_ARCH_PROTOCOL *This
    );

EFI_TIMER_ARCH_PROTOCOL mTimer = {
  SynicTimerRegisterHandler,
  SynicTimerSetTimerPeriod,
  SynicTimerGetTimerPeriod,
  SynicTimerGenerateSoftInterrupt
};

EFI_HANDLE mTimerHandle;
EFI_HV_PROTOCOL *mHv;
EFI_TIMER_NOTIFY mTimerNotifyFunction;
UINT64 mTimerPeriod;
UINT64 mLastTime;
BOOLEAN mUseDirectTimer;
BOOLEAN mSintConnected;
BOOLEAN mTimerConfigured;
HV_SYNIC_SINT_INDEX mSintIndex;
UINT32 mTimerIndex;

VOID
SynicTimerCallNotifyFunction (
    VOID
    )
/*++

Routine Description:

    Calls DxeCore to notify it that the timer has expired.

Arguments:

    None.

Return Value:

    None.

--*/
{
    UINT64 time;

    time = mHv->GetReferenceTime(mHv);

    ASSERT(time > mLastTime);

    if (mTimerNotifyFunction != NULL)
    {
        mTimerNotifyFunction(time - mLastTime);
    }

    mLastTime = time;
}


EFI_STATUS
EFIAPI
SynicTimerRegisterHandler (
    IN  EFI_TIMER_ARCH_PROTOCOL *This,
    IN  EFI_TIMER_NOTIFY NotifyFunction
    )
/*++

Routine Description:

    Registers a routine to call when the timer expires.

Arguments:

    This - A pointer to the EFI_TIMER_ARCH_PROTOCOL instance.

    NotifyFunction - A pointer to the notify function.

Return Value:

    EFI status.

--*/
{
    EFI_STATUS status;
    if (NotifyFunction == NULL && mTimerNotifyFunction == NULL)
    {
        status = EFI_INVALID_PARAMETER;
        DEBUG((EFI_D_ERROR, "--- %a: failed to register handler - %r \n", __FUNCTION__, status));
        return status;
    }

    if (NotifyFunction != NULL && mTimerNotifyFunction != NULL)
    {
        status = EFI_ALREADY_STARTED;
        DEBUG((EFI_D_ERROR, "--- %a: failed to register handler - %r \n", __FUNCTION__, status));
        return status;
    }

    mTimerNotifyFunction = NotifyFunction;
    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
SynicTimerSetTimerPeriod (
    IN  EFI_TIMER_ARCH_PROTOCOL *This,
    IN  UINT64 TimerPeriod
    )
/*++

Routine Description:

    Updates the timer period.

Arguments:

    This - A pointer to the EFI_TIMER_ARCH_PROTOCOL instance.

    TimerPeriod - The new timer period, in 100ns units. If 0, disables the
        timer.

Return Value:

    EFI status.

--*/
{
    mHv->SetTimer(mHv, mTimerIndex, TimerPeriod);
    mTimerPeriod = TimerPeriod;
    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
SynicTimerGetTimerPeriod (
    IN  EFI_TIMER_ARCH_PROTOCOL   *This,
    OUT UINT64                   *TimerPeriod
    )
/*++

Routine Description:

    Retrieves the current timer period.

Arguments:

    This - A pointer to the EFI_TIMER_ARCH_PROTOCOL instance.

    TimerPeriod - Returns the timer period, in 100ns units. If the timer
        is disabled, returns 0.

Return Value:

    EFI status.

--*/
{
    EFI_STATUS status;

    if (TimerPeriod == NULL)
    {
        status = EFI_INVALID_PARAMETER;
        DEBUG((EFI_D_ERROR, "--- %a: failed to register handler - %r \n", __FUNCTION__, status));
        return status;
    }

    *TimerPeriod = mTimerPeriod;
    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
SynicTimerGenerateSoftInterrupt (
    IN  EFI_TIMER_ARCH_PROTOCOL *This
    )
/*++

Routine Description:

    Simulates the expiry of the timer by calling the notify function.

Arguments:

    This - A pointer to the EFI_TIMER_ARCH_PROTOCOL instance.

Return Value:

    EFI status.

--*/
{
    EFI_TPL tpl;

    tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);
    SynicTimerCallNotifyFunction();
    gBS->RestoreTPL(tpl);
    return EFI_SUCCESS;
}


VOID
EFIAPI
SynicTimerInterruptHandler (
    IN  VOID *Context
    )
/*++

Routine Description:

    The interrupt handler for the timer.

Arguments:

    Context - A pointer to the interrupt context.

Return Value:

    None.

--*/
{
    HV_MESSAGE *message;
    HV_MESSAGE_TYPE messageType;

#if defined(MDE_CPU_X64) && defined(LEGACY_DEBUGGER)
    DebugPollDebugger();
#endif

    if (mUseDirectTimer == FALSE)
    {
        message = mHv->GetSintMessage(mHv, mSintIndex, FALSE);
        if (message != NULL)
        {
            messageType = message->Header.MessageType;
            if (messageType != HvMessageTimerExpired)
            {
                DEBUG((EFI_D_ERROR, "%a: Unexpected message type 0xlx%", __FUNCTION__, messageType));
            }
            mHv->CompleteSintMessage(mHv, mSintIndex, FALSE);
        }
    }

    SynicTimerCallNotifyFunction();
}

EFI_STATUS
EFIAPI
SynicTimerInitialize (
    IN  EFI_HANDLE ImageHandle,
    IN  EFI_SYSTEM_TABLE *SystemTable
    )
/*++

Routine Description:

    Initializes the SynicTimer driver.

Arguments:

    ImageHandle - The handle of the loaded image.

    SystemTable - A pointer to the system table.

Return Value:

    EFI status.

--*/
{
    EFI_STATUS status;

    mSintIndex = PcdGet8(PcdSynicTimerSintIndex);
    mTimerIndex = PcdGet8(PcdSynicTimerTimerIndex);

    //
    // Make sure the Timer Architectural Protocol is not already installed in
    // the system
    //
    ASSERT_PROTOCOL_ALREADY_INSTALLED(NULL, &gEfiTimerArchProtocolGuid);

    status = gBS->LocateProtocol(&gEfiHvProtocolGuid, NULL, (VOID **)&mHv);
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to locate the HV protocol - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    mUseDirectTimer = mHv->DirectTimerSupported();

    if (!mUseDirectTimer)
    {
        status = mHv->ConnectSint(mHv,
                                  mSintIndex,
                                  PcdGet8(PcdSynicTimerVector),
                                  FALSE,
                                  SynicTimerInterruptHandler,
                                  NULL);

        if (EFI_ERROR(status))
        {
            DEBUG((EFI_D_ERROR, "--- %a: failed to connect the SINT - %r \n", __FUNCTION__, status));
            goto Cleanup;
        }

        mSintConnected = TRUE;
    }

    //
    // Enable the timer.
    //
    status = mHv->ConfigureTimer(mHv,
                                 mTimerIndex,
                                 mSintIndex,
                                 TRUE, // periodic
                                 mUseDirectTimer,
                                 PcdGet8(PcdSynicTimerVector),
                                 SynicTimerInterruptHandler);
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to configure the timer - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    mTimerConfigured = TRUE;

    status = SynicTimerSetTimerPeriod(&mTimer, PcdGet64(PcdSynicTimerDefaultPeriod));
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to set the timer period - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    //
    // Install the Timer Architectural Protocol onto a new handle.
    //
    status = gBS->InstallMultipleProtocolInterfaces(
                    &mTimerHandle,
                    &gEfiTimerArchProtocolGuid, &mTimer,
                    NULL);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to install the protocol - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    status = EFI_SUCCESS;

Cleanup:
    if (EFI_ERROR(status))
    {
        if (mTimerConfigured)
        {
            SynicTimerSetTimerPeriod(&mTimer, 0);
        }

        if (mSintConnected)
        {
            mHv->DisconnectSint(mHv, mSintIndex);
            mSintConnected = FALSE;
        }
    }

    return status;
}

