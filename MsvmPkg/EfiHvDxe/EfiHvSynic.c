/** @file
  Implements Hyper-V synthetic interrupt controller support for EfiHvDxe.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "EfiHvInternal.h"
#include "MsBarrier.h"

VOID
EFIAPI
EfiHvInterruptHandler (
#if defined(MDE_CPU_X64)
  IN  EFI_EXCEPTION_TYPE InterruptType,
#elif defined(MDE_CPU_AARCH64)
  IN  HARDWARE_INTERRUPT_SOURCE InterruptType,
#endif
  IN  EFI_SYSTEM_CONTEXT SystemContext
  )
/*++
  The interrupt handler for SINT interrupts. Raises to high level and
  calls out to the connected handler.

  @param  InterruptType The interrupt vector of the arriving interrupt.

  @param  SystemContext A pointer to a structure containing the processor context
    when the processor was interrupted.

  @returns nothing
--*/
{
  EFI_TPL tpl;
  PEFI_HV_SINT_CONFIGURATION sintConfiguration;

  tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);
  if (!mAutoEoi)
  {
#if defined(MDE_CPU_X64)

    SendApicEoi();

#elif defined(MDE_CPU_AARCH64)

    mHwInt->EndOfInterrupt(mHwInt, InterruptType);

#endif
  }

  sintConfiguration = &mSintConfiguration[mVectorSint[InterruptType]];
  if (sintConfiguration->InterruptHandler != NULL)
  {
    sintConfiguration->InterruptHandler(sintConfiguration->Context);
  }

  gBS->RestoreTPL(tpl);
}

EFI_STATUS
EFIAPI
EfiHvConnectSint (
  IN  EFI_HV_PROTOCOL *This,
  IN  HV_SYNIC_SINT_INDEX SintIndex,
  IN  UINT8 Vector,
  IN  BOOLEAN NoProxy,
  IN  EFI_HV_INTERRUPT_HANDLER InterruptHandler,
  IN  VOID *Context
  )
/*++
  Enables a SINT and provides an interrupt routine to be called at
  TPL_HIGH_LEVEL when the interrupt arrives.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @param SintIndex The SINT to connect.

  @param Vector The vector to use for the SINT interrupt.

  @param NoProxy If TRUE, the paravisor SINT will not be configured as a proxy
    even if hardware isolated. This flag has no effect if hardware isolation
    is not in use.

  @param InterruptHandler A pointer to the interrupt handler for the SINT.

  @param Context An opaque context to pass to the interrupt handler.

  @returns EFI STATUS
--*/
{
  HV_SYNIC_SINT sint;
  PEFI_HV_SINT_CONFIGURATION sintConfiguration;
  EFI_STATUS status;
  EFI_TPL tpl;

  //
  // Disable interrupts while manipulating interrupts.
  //
  tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);

  //
  // Ensure the SINT is not already registered.
  //
  sintConfiguration = &mSintConfiguration[SintIndex];
  if (sintConfiguration->Vector != 0)
  {
    status = EFI_ALREADY_STARTED;
    DEBUG((DEBUG_ERROR, "--- %a: SINT is already registered - %r \n", __func__, status));
    goto Cleanup;
  }

  //
  // Register the interrupt handler.
  //
#if defined(MDE_CPU_X64)

  status = mCpu->RegisterInterruptHandler(mCpu, Vector, EfiHvInterruptHandler);

#elif defined(MDE_CPU_AARCH64)

  status = mHwInt->RegisterInterruptSource(mHwInt, (UINTN)Vector, EfiHvInterruptHandler);

#endif

  if (EFI_ERROR(status))
  {
    DEBUG((DEBUG_ERROR, "--- %a: failed to register the interrupt handler - %r \n", __func__, status));
    goto Cleanup;
  }

  //
  // Register the SINT with the hypervisor.
  //
  sint.AsUINT64 = 0;
  sint.Vector = Vector;
  sint.Masked = FALSE;
  sint.AutoEoi = mAutoEoi;

  if (mUseBypassContext)
  {

    //
    // Register the SINT with the host hypervisor before registering it with the paravisor as a proxy interrupt,
    // unless the caller requested that the SINT not be proxied.
    //
    HvHypercallSetVpRegister64Self(&mHvBypassContext, HvRegisterSint0 + SintIndex, sint.AsUINT64);
    sint.Proxy = !NoProxy;
  }

  if (!mBypassOnly)
  {
    HvHypercallSetVpRegister64Self(&mHvContext, HvRegisterSint0 + SintIndex, sint.AsUINT64);
  }

  //
  // Store the state used by the interrupt handler.
  //
  sintConfiguration->InterruptHandler = InterruptHandler;
  sintConfiguration->Context = Context;
  sintConfiguration->Vector = Vector;
  mVectorSint[Vector] = (UINT8)SintIndex;
  status = EFI_SUCCESS;

Cleanup:

  gBS->RestoreTPL(tpl);

  return status;
}

VOID
EFIAPI
EfiHvEventInterruptHandler (
  VOID *Context
  )
/*++
  An interrupt handler for a SINT interrupt that just signals an event.

  @param Context A pointer to the interrupt handler context.

  @returns nothing

--*/
{
  EFI_EVENT *event;

  event = Context;
  gBS->SignalEvent(event);
}

EFI_STATUS
EFIAPI
EfiHvConnectSintToEvent (
  IN  EFI_HV_PROTOCOL *This,
  IN  HV_SYNIC_SINT_INDEX SintIndex,
  IN  UINT8 Vector,
  IN  EFI_EVENT Event
  )
/*++
  Enables a SINT and provides an event to be signaled when the interrupt
  arrives.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @param SintIndex The SINT to connect.

  @param Vector The vector to use for the SINT interrupt.

  @param Event An EFI event to signal when the interrupt arrives.

  @returns EFI status.

--*/
{
  EFI_STATUS status;

  status =
    EfiHvConnectSint(
      This,
      SintIndex,
      Vector,
      FALSE,
      EfiHvEventInterruptHandler,
      Event);

  return status;
}

VOID
EFIAPI
EfiHvDisconnectSint (
  IN  EFI_HV_PROTOCOL *This,
  IN  HV_SYNIC_SINT_INDEX SintIndex
  )
/*++
  Disables a SINT that was previously enabled with EfiHvConnectSint
  or EfiHvConnectSintToEvent.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @param SintIndex The SINT to disconnect.

  @returns nothing.

--*/
{
  HV_SYNIC_SINT sint;
  PEFI_HV_SINT_CONFIGURATION sintConfiguration;
  EFI_TPL tpl;

  tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);

  //
  // Unregister the SINT with the hypervisor.
  //
  sint.AsUINT64 = 0;
  sint.Masked = 1;

  if (mUseBypassContext)
  {
    HvHypercallSetVpRegister64Self(&mHvBypassContext, HvRegisterSint0 + SintIndex, sint.AsUINT64);
  }

  if (!mBypassOnly)
  {
    HvHypercallSetVpRegister64Self(&mHvContext, HvRegisterSint0 + SintIndex, sint.AsUINT64);
  }

  //
  // Unregister the interrupt handler.
  //
  sintConfiguration = &mSintConfiguration[SintIndex];
  if (sintConfiguration->Vector != 0)
  {
#if defined(MDE_CPU_X64)

    mCpu->RegisterInterruptHandler(mCpu, sintConfiguration->Vector, NULL);

#elif defined(MDE_CPU_AARCH64)

    mHwInt->RegisterInterruptSource(mHwInt, sintConfiguration->Vector, NULL);

#endif
    mVectorSint[sintConfiguration->Vector] = 0;
  }

  sintConfiguration->Vector = 0;
  sintConfiguration->InterruptHandler = NULL;
  sintConfiguration->Context = NULL;

  gBS->RestoreTPL(tpl);

}

HV_MESSAGE *
EFIAPI
EfiHvGetSintMessage (
  IN  EFI_HV_PROTOCOL *This,
  IN  HV_SYNIC_SINT_INDEX SintIndex,
  IN  BOOLEAN Direct
  )
/*++
  Retrieves the next message from the SINT message queue.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @param SintIndex The index of the SINT.

  @param Direct Do not bypass the paravisor, if one is present.

  @returns A pointer to the next message, or NULL if there is currently no message.

--*/
{
  volatile HV_MESSAGE *message;
  PHV_HYPERCALL_CONTEXT context;

  context = (mUseBypassContext && !Direct) ? &mHvBypassContext : &mHvContext;
  if (context->MessagePage.Page == NULL)
  {
    return NULL;
  }

  message = &((PHV_MESSAGE_PAGE)context->MessagePage.Page)->SintMessage[SintIndex];
  if (message->Header.MessageType == HvMessageTypeNone)
  {
    return NULL;
  }

  return (HV_MESSAGE *)message;
}

EFI_STATUS
EFIAPI
EfiHvCompleteSintMessage (
  IN  EFI_HV_PROTOCOL *This,
  IN  HV_SYNIC_SINT_INDEX SintIndex,
  IN  BOOLEAN Direct
  )
/*++
  Marks the current message in the SINT message queue as complete so
  that the next message can be processed.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @param SintIndex The index of the SINT.

  @param Direct Do not bypass the paravisor, if one is present.

  @returns nothing.

--*/
{
  volatile HV_MESSAGE *message;
  PHV_HYPERCALL_CONTEXT context;

  context = (mUseBypassContext && !Direct) ? &mHvBypassContext : &mHvContext;
  if (context->MessagePage.Page == NULL)
  {
    return EFI_UNSUPPORTED;
  }

  message = &((PHV_MESSAGE_PAGE)context->MessagePage.Page)->SintMessage[SintIndex];
  message->Header.MessageType = HvMessageTypeNone;
  MemoryBarrier();
  if (message->Header.MessageFlags.MessagePending)
  {
    HvHypercallSetVpRegister64Self(context, HvRegisterEom, 0);
  }

  return EFI_SUCCESS;
}

volatile HV_SYNIC_EVENT_FLAGS *
EFIAPI
EfiHvGetSintEventFlags (
  IN  EFI_HV_PROTOCOL *This,
  IN  HV_SYNIC_SINT_INDEX SintIndex,
  IN  BOOLEAN Direct
  )
/*++
  Retrieves a pointer to the event flags for a SINT.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @param SintIndex The index of the SINT.

  @param Direct Do not bypass the paravisor, if one is present.

  @returns A pointer to the event flags.

--*/
{
  PHV_HYPERCALL_CONTEXT context;
  volatile HV_SYNIC_EVENT_FLAGS *pFlags;

  context = (mUseBypassContext && !Direct) ? &mHvBypassContext : &mHvContext;
  if (context->EventFlagsPage.Page == NULL)
  {
    return NULL;
  }

  pFlags = &((PHV_SYNIC_EVENT_FLAGS_PAGE)context->EventFlagsPage.Page)->SintEventFlags[SintIndex];

  return pFlags;
}

UINT64
EFIAPI
EfiHvGetReferenceTime (
  IN  EFI_HV_PROTOCOL *This
  )
/*++
  Retrieves the current hypervisor reference time, in 100ns units.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @returns The time, in 100ns units.

--*/
{
  UINT64 refTime;

  //
  // Always use the local hypervisor context, even if only the bypass
  // context has been configured, since the ref timer MSR is always locally
  // available.
  //
  refTime = HvHypercallGetVpRegister64Self(&mHvContext, HvRegisterTimeRefCount);
  return refTime;
}

UINT32
EFIAPI
EfiHvGetCurrentVpIndex (
  IN  EFI_HV_PROTOCOL *This
  )
/*++
  Retrieves the current virtual processor index.

  @param This A pointer to the EFI_HV_PROTOCOL instance.

  @returns The VP index.

--*/
{
  UINT32 vpIndex;

  //
  // Always use the local hypervisor context, even if only the bypass
  // context has been configured, since the VP index MSR is always locally
  // available.
  //
  vpIndex = (UINT32)HvHypercallGetVpRegister64Self(&mHvContext, HvRegisterVpIndex);
  return vpIndex;
}

PEFI_SYNIC_COMPONENT
EfiHvpGetSynicComponent(
  IN  PHV_HYPERCALL_CONTEXT Context,
  IN  HV_REGISTER_NAME Register
  )
/*++
  Gets a synthetic interrupt controller component based on its register.

  @param Context A pointer to the context.

  @param Register The register for the component.

  @returns The component.

--*/
{
  switch (Register)
  {
  case HvRegisterSipp:
    return &Context->MessagePage;

  case HvRegisterSifp:
    return &Context->EventFlagsPage;

  default:
    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();

    // Unreachable but needed to compile.
    return NULL;
  }
}

VOID
EfiHvpEnableSynicComponent(
  IN  HV_REGISTER_NAME Register,
  IN  VOID *Buffer,
  IN  BOOLEAN Direct
  )
/*++
  Enables a synthetic interrupt controller component.

  @param Register The register for the component.

  @param Buffer The buffer to use if the component is not already configured.

  @param Direct If true, configure the component for the paravisor's synic.

  @returns EFI status.

--*/
{
  PHV_HYPERCALL_CONTEXT context;
  PEFI_SYNIC_COMPONENT component;
  UINTN gpa;

  //
  // Use the SIMP format, as they are all the same.
  //

  HV_SYNIC_SIMP simp;

  context = (mUseBypassContext && !Direct) ? &mHvBypassContext : &mHvContext;
  component = EfiHvpGetSynicComponent(context, Register);

  //
  // Check if the component is for the paravisor in a hardware-isolated
  // environment.
  //
  // N.B. When using the paravisor synic, any buffer used must not be host
  //      visible.
  //

  simp.AsUINT64 = HvHypercallGetVpRegister64Self(context, Register);
  if (simp.SimpEnabled != 0)
  {
    gpa = simp.BaseSimpGpa * EFI_PAGE_SIZE;
    if ((!Direct && gpa < mSharedGpaBoundary) ||
      (Direct && mSharedGpaBoundary != 0 && gpa >= mSharedGpaBoundary))
    {

      //
      // Failure is not allowed here - need to fail fast
      //
      FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    if (Direct)
    {
      component->Page = (VOID *)gpa;
    }
    else
    {
      component->Page = EfiHvpSharedVa((VOID *)gpa);
    }
  }
  else
  {
    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE((mUseBypassContext == FALSE) || mBypassOnly || Direct);

    component->Page = Buffer;
    simp.SimpEnabled = 1;
    if (Direct)
    {
      simp.BaseSimpGpa = EfiHvpBasePa((UINTN)component->Page) / EFI_PAGE_SIZE;
    }
    else
    {
      simp.BaseSimpGpa = EfiHvpSharedPa(component->Page) / EFI_PAGE_SIZE;
    }

    HvHypercallSetVpRegister64Self(context, Register, simp.AsUINT64);

    //
    // Only disable the component on cleanup if it was explicitly enabled
    // here.
    //

    component->DisableOnCleanup = TRUE;
  }
}

EFI_STATUS
EfiHvConnectToSynic (
  VOID
  )
/*++
  Initializes a connection to the synthetic interrupt controller.

  @param None.

  @returns EFI status.

--*/
{
  //
  // Enable the message page.
  //
  EfiHvpEnableSynicComponent(HvRegisterSipp, &mHvPages->MessagePage, FALSE);

  //
  // Enable the event page.
  //
  EfiHvpEnableSynicComponent(HvRegisterSifp, &mHvPages->EventFlagsPage, FALSE);

#if defined(MDE_CPU_X64)

  //
  // When hardware isolated, also enable the paravisor's components.
  //

  if (mUseBypassContext && !mBypassOnly)
  {
    EfiHvpEnableSynicComponent(HvRegisterSipp,
                   &mHvPages->ParavisorMessagePage,
                   TRUE);

    EfiHvpEnableSynicComponent(HvRegisterSifp,
                   &mHvPages->ParavisorEventFlagsPage,
                   TRUE);
  }

#endif

  mSynicConnected = TRUE;

  return EFI_SUCCESS;
}

VOID
EfiHvpDisableSynicComponent(
  IN  HV_REGISTER_NAME Register,
  IN  BOOLEAN Direct
  )
/*++
  Disables a synthetic interrupt controller component.

  @param Register The register for the component.

  @param Direct If true, configure the component for the paravisor's synic.

  @returns nothing.

--*/
{
  PHV_HYPERCALL_CONTEXT context;
  PEFI_SYNIC_COMPONENT component;

  //
  // Use the SIMP format, as they are all the same.
  //
  HV_SYNIC_SIMP simp;

  context = (mUseBypassContext && !Direct) ? &mHvBypassContext : &mHvContext;
  component = EfiHvpGetSynicComponent(context, Register);

  //
  // Disable the register only if the component was explicitly enabled before.
  //

  if (component->DisableOnCleanup)
  {
    simp.AsUINT64 = HvHypercallGetVpRegister64Self(context, Register);
    simp.SimpEnabled = 0;
    simp.BaseSimpGpa = 0;
    HvHypercallSetVpRegister64Self(context, Register, simp.AsUINT64);
  }
}

VOID
EfiHvDisconnectFromSynic (
  VOID
  )
/*++
  Tears down the connection to the synthetic interrupt controller.

  @param None.

  @returns nothing.

--*/
{
  HV_HYPERCALL_CONTEXT *context;
  volatile HV_SYNIC_EVENT_FLAGS *flags;
  HV_SYNIC_SINT_INDEX sintIndex;
  UINT32 timerIndex;
  UINT32 flagsIndex = 0;

  if (!mSynicConnected)
  {
    return;
  }

  //
  // Clear all the timers.
  //
  context = mBypassOnly ? &mHvBypassContext : &mHvContext;
  for (timerIndex = 0; timerIndex < HV_SYNIC_STIMER_COUNT; timerIndex += 1)
  {
    HvHypercallSetVpRegister64Self(context, HvRegisterStimer0Count + (2 * timerIndex), 0);
    HvHypercallSetVpRegister64Self(context, HvRegisterStimer0Config + (2 * timerIndex), 0);
  }

  //
  // Disconnect the SINTs and drain all the message queues.
  //
  for (sintIndex = 0; sintIndex < HV_SYNIC_SINT_COUNT; sintIndex += 1)
  {
    EfiHvDisconnectSint(&mHv, sintIndex);
    while (EfiHvGetSintMessage(&mHv, sintIndex, FALSE) != NULL)
    {
      EfiHvCompleteSintMessage(&mHv, sintIndex, FALSE);
    }

    //
    // Zero the event flags for this SINT.
    //
    flags = EfiHvGetSintEventFlags(&mHv, sintIndex, FALSE);

    for (flagsIndex = 0; flagsIndex < HV_EVENT_FLAGS_DWORD_COUNT; flagsIndex++)
    {
      flags->Flags32[flagsIndex] = 0;
    }

#if defined(MDE_CPU_X64)

    //
    // Do the same for the paravisor synic if hardware isolated.
    //

    if (mUseBypassContext && !mBypassOnly)
    {
      while (EfiHvGetSintMessage(&mHv, sintIndex, TRUE) != NULL)
      {
        EfiHvCompleteSintMessage(&mHv, sintIndex, TRUE);
      }

      flags = EfiHvGetSintEventFlags(&mHv, sintIndex, TRUE);
      for (flagsIndex = 0; flagsIndex < HV_EVENT_FLAGS_DWORD_COUNT; flagsIndex++)
      {
        flags->Flags32[flagsIndex] = 0;
      }
    }

#endif

  }


  //
  // Disable the message and event flags pages if they were enabled.
  //
  EfiHvpDisableSynicComponent(HvRegisterSipp, FALSE);
  EfiHvpDisableSynicComponent(HvRegisterSifp, FALSE);

#if defined(MDE_CPU_X64)

  if (mUseBypassContext && !mBypassOnly)
  {
    EfiHvpDisableSynicComponent(HvRegisterSipp, TRUE);
    EfiHvpDisableSynicComponent(HvRegisterSifp, TRUE);
  }

#endif

  mSynicConnected = FALSE;
}
