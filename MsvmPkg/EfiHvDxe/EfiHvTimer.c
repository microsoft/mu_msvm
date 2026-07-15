/** @file
  Implements Hyper-V timer support for EfiHvDxe.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "EfiHvInternal.h"

VOID
EFIAPI
EfiHvSetTimer (
  IN  EFI_HV_PROTOCOL *This,
  IN  UINT32 TimerIndex,
  IN  UINT64 Expiration
  )
/*++
  Sets a hypervisor timer to expire.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @param TimerIndex The index of the timer.

  @param Expiration The time to expire. If the timer is periodic, then this
            is the period. Otherwise, this is an absolute time, based on the
            reference time base.
            If 0, then the timer is cancelled.

  @returns nothing.

--*/
{
  HvHypercallSetVpRegister64Self(
    mBypassOnly ? &mHvBypassContext : &mHvContext,
    HvRegisterStimer0Count + (2 * TimerIndex),
    Expiration);
}

BOOLEAN
EFIAPI
EfiHvDirectTimerSupported (
  VOID
  )
/*++
  Indicates whether the hypervisor supports direct-mode timers.

  @param None.

  @returns TRUE if direct mode timers are supported.

--*/
{
  return mDirectTimerSupported;
}

VOID
EFIAPI
EfiHvDirectTimerInterruptHandler (
#if defined(MDE_CPU_X64)
  IN  EFI_EXCEPTION_TYPE InterruptType,
#elif defined(MDE_CPU_AARCH64)
  IN  HARDWARE_INTERRUPT_SOURCE InterruptType,
#endif
  IN  EFI_SYSTEM_CONTEXT SystemContext
  )
/*++
  The interrupt handler for direct-mode timers. Raises to high level and
  calls out to the connected handler.

  @param InterruptType The interrupt vector of the arriving interrupt.

  @param SystemContext A pointer to a structure containing the processor context
              when the processor was interrupted.

  @returns nothing.

--*/
{
  EFI_TPL tpl;

  tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);

#if defined(MDE_CPU_X64)

  SendApicEoi();

#elif defined(MDE_CPU_AARCH64)

  mHwInt->EndOfInterrupt(mHwInt, InterruptType);

#endif

  if (mDirectTimerInterruptHandlers[InterruptType] != NULL)
  {
    mDirectTimerInterruptHandlers[InterruptType](NULL);
  }

  gBS->RestoreTPL(tpl);
}

EFI_STATUS
EFIAPI
EfiHvConfigureTimer (
  IN          EFI_HV_PROTOCOL             *This,
  IN          UINT32                      TimerIndex,
  IN          HV_SYNIC_SINT_INDEX         SintIndex,
  IN          BOOLEAN                     Periodic,
  IN          BOOLEAN                     DirectMode,
  IN          UINT8                       Vector,
  IN OPTIONAL EFI_HV_INTERRUPT_HANDLER    InterruptHandler
  )
/*++
  Configures a timer for use. Start it with EfiHvSetTimer.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @param TimerIndex The index of the timer.

  @param SintIndex The SINT to deliver a message to when the timer expires.

  @param Periodic TRUE if this is a periodic timer.

  @param DirectMode TRUE if direct mode.

  @param Vector Interrupt vector/number.

  @param InterruptHandler A pointer to the interrupt handler for the timer.

  @returns EFI status.

--*/
{
  HV_X64_MSR_STIMER_CONFIG_CONTENTS config;
  EFI_STATUS status;
  DEBUG((DEBUG_VERBOSE, ">>> %a: tindex 0x%x sindex 0x%x periodic %s direct %s vector 0x%x\n",
    __func__, TimerIndex, SintIndex, Periodic ? L"TRUE" : L"FALSE",
    DirectMode ? L"TRUE" : L"FALSE", Vector));

  if (TimerIndex >= HV_SYNIC_STIMER_COUNT)
  {
    status = EFI_INVALID_PARAMETER;
    DEBUG((DEBUG_ERROR, "--- %a: invalid timer index - %r \n", __func__, status));
    return status;
  }

  //
  // Verify that an existing timer is not being reconfigured with an incompatible configuration.
  //
  if (DirectMode)
  {
    if (mTimerConfiguration[TimerIndex].Enable)
    {
      if (!mTimerConfiguration[TimerIndex].DirectMode ||
        (mTimerConfiguration[TimerIndex].ApicVector != Vector) ||
        (mDirectTimerInterruptHandlers[Vector] != InterruptHandler))
      {
        status = EFI_INVALID_PARAMETER;
        DEBUG((DEBUG_ERROR, "--- %a: invalid timer configuration - %r \n", __func__, status));
        return status;
      }
    }
    else
    {

      //
      // Configure the interrupt handler for this timer.
      //
#if defined(MDE_CPU_X64)

      status = mCpu->RegisterInterruptHandler(mCpu, Vector, EfiHvDirectTimerInterruptHandler);

#elif defined(MDE_CPU_AARCH64)

      status = mHwInt->RegisterInterruptSource(mHwInt, (UINTN)Vector, EfiHvDirectTimerInterruptHandler);

#endif

      if (EFI_ERROR(status))
      {
        DEBUG((DEBUG_ERROR, "--- %a: failed to register the interrupt handler - %r \n", __func__, status));
        return status;
      }

      mDirectTimerInterruptHandlers[Vector] = InterruptHandler;
    }
  }
  else
  {
    if (mTimerConfiguration[TimerIndex].DirectMode)
    {
      status = EFI_INVALID_PARAMETER;
      DEBUG((DEBUG_ERROR, "--- %a: invalid timer configuration (DirectMode) - %r \n", __func__, status));
      return status;
    }
  }

  //
  // Stop the timer if it's already running.
  //
  EfiHvSetTimer(&mHv, TimerIndex, 0);

  //
  // Configure the timer. Always use lazy mode if the timer is periodic.
  //
  config.AsUINT64 = 0;
  config.Periodic = (Periodic != FALSE);
  config.Lazy = (Periodic != FALSE);
  config.AutoEnable = TRUE;
  if (DirectMode)
  {
    config.DirectMode = 1;
    config.ApicVector = Vector;
  }
  else
  {
    config.SINTx = SintIndex;
  }
  mTimerConfiguration[TimerIndex] = config;
  mTimerConfiguration[TimerIndex].Enable = 1;
  HvHypercallSetVpRegister64Self(
    mBypassOnly ? &mHvBypassContext : &mHvContext,
    HvRegisterStimer0Config + (2 * TimerIndex),
    config.AsUINT64);

  return EFI_SUCCESS;
}
