/** @file
  Provides an implementation of the EFI_HV_PROTOCOL protocol, which provides
  UEFI access to the Hyper-V hypervisor.P initialize support functions for DXE phase.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>

#include <IsolationTypes.h>
#include <Synchronization.h>

#include <Hv/HvGuestHypercall.h>
#include <Hv/HvStatus.h>

#include <Protocol/Cpu.h>
#include <Protocol/EfiHv.h>

#include <Guid/EventGroup.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CrashLib.h>
#include <Library/DebugLib.h>
#include <Library/HostVisibilityLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/HvHypercallLib.h>

#if defined(MDE_CPU_X64)
#include <Library/LocalApicLib.h>
#endif
#if defined(MDE_CPU_AARCH64)
#include <Protocol/HardwareInterrupt.h>
#endif

#define WINHVP_MAX_REPS_PER_HYPERCALL   0xFFF
typedef struct _EFI_HV_SINT_CONFIGURATION
{
    EFI_HV_INTERRUPT_HANDLER InterruptHandler;
    VOID *Context;
    UINT8 Vector;
} EFI_HV_SINT_CONFIGURATION, *PEFI_HV_SINT_CONFIGURATION;

typedef struct _EFI_HV_PAGES
{
    UINT8 HypercallInputPage[EFI_PAGE_SIZE];
    UINT8 HypercallOutputPage[EFI_PAGE_SIZE];
    HV_SYNIC_EVENT_FLAGS_PAGE EventFlagsPage;
    HV_MESSAGE_PAGE MessagePage;

#if defined(MDE_CPU_X64)

    //
    // Additional pages needed to configure the paravisor in an isolated VM, to
    // allow for encrypted communication with the paravisor.
    //

    HV_SYNIC_EVENT_FLAGS_PAGE ParavisorEventFlagsPage;
    HV_MESSAGE_PAGE ParavisorMessagePage;

#endif

} EFI_HV_PAGES, *PEFI_HV_PAGES;

typedef struct _EFI_HV_PROTECTION_OBJECT
{
    LIST_ENTRY ListEntry;
    UINT64 GpaPageBase;
    UINT32 NumberOfPages;
} EFI_HV_PROTECTION_OBJECT, *PEFI_HV_PROTECTION_OBJECT;

//
// When hardware isolation is in use, the main hypercall context is used to
// communicate with the paravisor, while the bypass context is used to
// communicate with the host hypervisor. If hardware isolated without a
// paravisor, only the bypass context is used.
//

HV_HYPERCALL_CONTEXT mHvContext;
HV_HYPERCALL_CONTEXT mHvBypassContext;
BOOLEAN mUseBypassContext;
BOOLEAN mBypassOnly;
PEFI_HV_PAGES mHvPages;

#if defined(MDE_CPU_X64)

UINT8 *mHypercallPage;

#endif

VOID* mHvInputPage;
EFI_HANDLE mHvHandle;
BOOLEAN mSynicConnected;
EFI_EVENT mExitBootServicesEvent;
BOOLEAN mAutoEoi;
BOOLEAN mDirectTimerSupported;
LIST_ENTRY mHostVisiblePageList;
UINT64 mSharedGpaBoundary;
UINT64 mCanonicalizationMask;
UINT32 mIsolationType;
VOID* mSvsmCallingArea;

EFI_HV_SINT_CONFIGURATION mSintConfiguration[HV_SYNIC_SINT_COUNT];
UINT8 mVectorSint[256];

EFI_HV_INTERRUPT_HANDLER mDirectTimerInterruptHandlers[256];
HV_X64_MSR_STIMER_CONFIG_CONTENTS mTimerConfiguration[HV_SYNIC_STIMER_COUNT];

#if defined(MDE_CPU_X64)

EFI_CPU_ARCH_PROTOCOL *mCpu;

#elif defined(MDE_CPU_AARCH64)

EFI_HARDWARE_INTERRUPT_PROTOCOL *mHwInt;

#endif

extern EFI_HV_PROTOCOL mHv;
extern EFI_HV_IVM_PROTOCOL mHvIvm;


UINTN
EfiHvpSharedPa(
    VOID* Address
    )
/**
    Given an address, which may be either a VA or a PA, removes any
    canonicalization bits and returns the shared GPA corresponding to the
    address.

    @param Address Input address.

    @returns Shared GPA.

**/
{
    UINTN addr;

    addr = (UINTN)Address;
    addr &= ~mCanonicalizationMask;
    if (addr < mSharedGpaBoundary)
    {
        addr += mSharedGpaBoundary;
    }

    return addr;
}


VOID*
EfiHvpSharedVa(
    VOID* Address
    )
/**
    Given an address, which may be either a VA or a PA, returns a canonicalized
    pointer pointing to the shared GPA alias.

    @param Address Input address.

    @returns Shared VA pointer.

**/
{
    return (VOID*)(EfiHvpSharedPa(Address) | mCanonicalizationMask);
}


UINTN
EfiHvpBasePa(
    UINTN Address
    )
/**
    Given an address, returns the private alias GPA corresponding to that
    address.

    @param Address Input address.

    @returns Shared GPA.

**/
{
    Address &= ~mCanonicalizationMask;
    if (Address >= mSharedGpaBoundary)
    {
        Address -= mSharedGpaBoundary;
    }

    return Address;
}


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
        DEBUG((DEBUG_ERROR, "--- %a: SINT is already registered - %r \n", __FUNCTION__, status));
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
        DEBUG((DEBUG_ERROR, "--- %a: failed to register the interrupt handler - %r \n", __FUNCTION__, status));
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
        __FUNCTION__, TimerIndex, SintIndex, Periodic ? L"TRUE" : L"FALSE",
        DirectMode ? L"TRUE" : L"FALSE", Vector));

    if (TimerIndex >= HV_SYNIC_STIMER_COUNT)
    {
        status = EFI_INVALID_PARAMETER;
        DEBUG((DEBUG_ERROR, "--- %a: invalid timer index - %r \n", __FUNCTION__, status));
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
                DEBUG((DEBUG_ERROR, "--- %a: invalid timer configuration - %r \n", __FUNCTION__, status));
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
                DEBUG((DEBUG_ERROR, "--- %a: failed to register the interrupt handler - %r \n", __FUNCTION__, status));
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
            DEBUG((DEBUG_ERROR, "--- %a: invalid timer configuration (DirectMode) - %r \n", __FUNCTION__, status));
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


HV_STATUS
EfiHvIssueHypercall (
    IN  HV_CALL_CODE CallCode,
    IN  BOOLEAN Fast,
    IN  UINT64 FirstRegister,
    IN  UINT64 SecondRegister
    )
/*++
    Issues a hypercall.

    @param CallCode The hypercall code.

    @param Fast If TRUE, this is a fast hypercall.

    @param FirstRegister The first register value for the hypercall. If a slow hypercall, this must refer
        to the non-shared alias of the GPA.

    @param SecondRegister The second register value for the hypercall. If a slow hypercall, this must
        refer to the non-shared alias of the GPA.

    @returns The hypercall status.

--*/
{
    return
        HvHypercallIssue(
            mUseBypassContext ? &mHvBypassContext : &mHvContext,
            CallCode,
            Fast,
            0,
            FirstRegister,
            SecondRegister,
            NULL);
}


EFI_STATUS
EfiHvConvertStatus (
    IN  HV_STATUS Status
    )
/*++
    Converts a hypervisor status code into an EFI status code.

    @param Status The hypervisor status code.

    @returns EFI status.

--*/
{
    switch (Status)
    {
    case HV_STATUS_SUCCESS:
        return EFI_SUCCESS;

    case HV_STATUS_INVALID_PARAMETER:
        return EFI_INVALID_PARAMETER;

    default:
        return EFI_DEVICE_ERROR;
    }
}


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
        __FUNCTION__, ConnectionId, MessageType, Payload, PayloadSize));

    //
    // A direct hypercall is only valid if we are hardware isolated with a
    // paravisor.
    //

    if (DirectHypercall && (!mUseBypassContext || mBypassOnly))
    {
        return EFI_INVALID_PARAMETER;
    }

    oldTpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);
    input = (PHV_INPUT_POST_MESSAGE)(DirectHypercall ? &mHvPages->HypercallInputPage : mHvInputPage);
    input->ConnectionId = ConnectionId;
    input->Reserved = 0;
    input->MessageType = MessageType;
    input->PayloadSize = PayloadSize;
    CopyMem(input->Payload, Payload, PayloadSize);
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

    return hvStatus;
}

EFI_STATUS
EFIAPI
EfiHvpModifySparseGpaPageHostVisibility(
    IN              HV_MAP_GPA_FLAGS    MapFlags,
    IN              UINT32              PageCount,
    IN              HV_GPA_PAGE_NUMBER  GpaPageBase,
    OUT OPTIONAL    UINT32*             PageCountProcessed
    )
/*++
    Handles the ModifySparseGpaPageHostVisibility hypercall.

    @param MapFlags Access permissions provided to the host.

    @param PageCount The number of pages to modify.

    @param GpaPageBase Supplies the address of the first target GPA to accept. The
                            remaining pages will be modified sequentially from this GPA.

    @param PageCountProcessed If present, the number of pages that are successfully processed
                                will be returned in this.

    @returns EFI status.

--*/
{

    //
    // For this rep call, it's easier to treat the input page as a pointer
    // to this structure.
    PHV_INPUT_MODIFY_SPARSE_GPA_PAGE_HOST_VISIBILITY pInputBuffer;
    HV_STATUS hvStatus;
    EFI_STATUS status;
    EFI_TPL oldTpl;
    UINT32 possibleRepsPerCall;
    UINT32 repsInCurrentCall;
    UINT32 repsProcessedThisCall;
    UINT32 gpaPageBaseIndex = 0;
    UINT32 i;
    UINT32 totalPageCountProcessed = 0;
    BOOLEAN paravisorPresent;

    if (PageCountProcessed)
    {
        *PageCountProcessed = 0;
    }

    paravisorPresent = IsParavisorPresent();

#if defined(MDE_CPU_X64)

    //
    // Check if we are running hardware isolated but do not have a paravisor.
    //
    if (IsHardwareIsolatedNoParavisorEx(mIsolationType, paravisorPresent))
    {

        //
        // If the hypervisor connection has not yet been established, then
        // visibility must be changed without using hypercalls.
        //
        if (!mHvBypassContext.Connected)
        {
            UINT64 pagesProcessed;

            if (MapFlags != 0)
            {
                status = EfiMakePageRangeHostVisible(
                    mIsolationType,
                    mSvsmCallingArea,
                    GpaPageBase,
                    PageCount,
                    &pagesProcessed);
            }
            else
            {
                status = EfiMakePageRangeHostNotVisible(
                    mIsolationType,
                    mSvsmCallingArea,
                    GpaPageBase,
                    PageCount,
                    &pagesProcessed);
            }

            if (EFI_ERROR(status))
            {
                FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
            }

            ASSERT(pagesProcessed <= PageCount);

            if (PageCountProcessed != NULL)
            {
                *PageCountProcessed = (UINT32)pagesProcessed;
            }

            return status;
        }

        //
        // If pages are being made host visible, then revoke page acceptance
        // first.
        //
        if (MapFlags != 0)
        {
            status = EfiUpdatePageRangeAcceptance(
                mIsolationType,
                mSvsmCallingArea,
                GpaPageBase,
                PageCount,
                FALSE);
            if (EFI_ERROR(status))
            {
                FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
            }
        }

        ASSERT(mHvBypassContext.Connected);
    }

#endif

    oldTpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);

    //
    // Simplified version of WinHvpSpecialListRepHypercall with no output parameters
    //
    possibleRepsPerCall = (HV_PAGE_SIZE - sizeof(*pInputBuffer)) / sizeof(HV_GPA_PAGE_NUMBER);

    ASSERT(possibleRepsPerCall <= WINHVP_MAX_REPS_PER_HYPERCALL);

    pInputBuffer = (PHV_INPUT_MODIFY_SPARSE_GPA_PAGE_HOST_VISIBILITY)mHvPages->HypercallInputPage;

    for (;;)
    {
        ASSERT(PageCount > 0);

        repsProcessedThisCall = 0;

        ZeroMem(pInputBuffer, HV_PAGE_SIZE);

        //
        // Build the input.
        //
        repsInCurrentCall = MIN(possibleRepsPerCall, PageCount);

        ASSERT(repsInCurrentCall <= WINHVP_MAX_REPS_PER_HYPERCALL);

        //
        // Fill header
        //
        pInputBuffer->TargetPartitionId = HV_PARTITION_ID_SELF;
        pInputBuffer->HostVisibility = MapFlags;

        //
        // Fill page numbers
        // N.B. instead of copying from an existing list of page numbers, we
        // generate a list of consecutive numbers from GpaPageBase.
        //
        for (i = 0; i < repsInCurrentCall; i++, gpaPageBaseIndex++)
        {
            pInputBuffer->GpaPageList[i] = GpaPageBase + gpaPageBaseIndex;
        }

        //
        // Call the hypervisor.
        //
        hvStatus =
            HvHypercallIssue(
                mBypassOnly ? &mHvBypassContext : &mHvContext,
                HvCallModifySparseGpaPageHostVisibility,
                FALSE, // not fast
                repsInCurrentCall,
                EfiHvpBasePa((UINTN)pInputBuffer),
                0, // no output
                &repsProcessedThisCall);
        status = EfiHvConvertStatus(hvStatus);

        ASSERT(repsProcessedThisCall <= repsInCurrentCall);
        ASSERT(repsProcessedThisCall <= PageCount);

        ASSERT(((repsProcessedThisCall == repsInCurrentCall) &&
                (status == EFI_SUCCESS)) ||
               (status != EFI_SUCCESS));

        //
        // Update the count of reps processed.
        //
        totalPageCountProcessed += repsProcessedThisCall;

        //
        // Check that we haven't overflowed.
        //
        if (repsProcessedThisCall > PageCount)
        {
            status = EFI_BAD_BUFFER_SIZE;
        }

        PageCount -= repsProcessedThisCall;

        if ((status != EFI_SUCCESS) || (PageCount == 0))
        {
            break;
        }
    }

    gBS->RestoreTPL(oldTpl);

#if defined(MDE_CPU_X64)

    if (IsHardwareIsolatedNoParavisorEx(mIsolationType, paravisorPresent))
    {
        //
        // When no paravisor is present, host-generated failure cannot be
        // tolerated.  Fail fast here.
        //
        if (EFI_ERROR(status))
        {
            FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
        }

        //
        // If pages are being made not-visible, then accept the pages in
        // hardware.
        //
        if (MapFlags == 0)
        {
            status = EfiUpdatePageRangeAcceptance(
                mIsolationType,
                mSvsmCallingArea,
                GpaPageBase,
                totalPageCountProcessed,
                TRUE);
            if (EFI_ERROR(status))
            {
                FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
            }
        }
    }

#endif

    if (PageCountProcessed)
    {
        *PageCountProcessed = totalPageCountProcessed;
    }

    return status;
}


EFI_STATUS
EFIAPI
EfiHvMakeAddressRangeHostVisible(
    IN              EFI_HV_IVM_PROTOCOL         *This,
    IN              HV_MAP_GPA_FLAGS            MapFlags,
    IN              VOID                        *BaseAddress,
    IN              UINT32                      ByteCount,
    IN              BOOLEAN                     ZeroPages,
    OUT OPTIONAL    EFI_HV_PROTECTION_HANDLE    *ProtectionHandle
    )
/*++
    Makes a chunk of memory visible to the host.
    Note: Memory visibility changes for hardware-isolated
          systems may change the contents of the pages.

    @param This A pointer to the EFI_HV_PROTOCOL instance.

    @param MapFlags Access permissions provided to the host.

    @param BaseAddress Base address of memory range.

    @param ByteCount Size of memory block in bytes.

    @param ZeroPages If true, memory range is zeroed after making visible to host.

    @param ProtectionHandle Object used to track memory range.

    @returns EFI status.

--*/
{
    UINT32 pageCountProcessed;
    EFI_HV_PROTECTION_OBJECT *protectionObject;
    EFI_STATUS revertStatus;
    EFI_STATUS status;

    if (!IsIsolatedEx(mIsolationType))
    {
        status = EFI_INVALID_PARAMETER;
        DEBUG((DEBUG_ERROR, "--- %a: visibility changes are only permitted on isolated systems - %r \n", __FUNCTION__, status));
        return status;
    }

    //
    // All arguments must be page aligned, and the access must imply host
    // visibility.
    //
    if ((((UINTN)BaseAddress & (EFI_PAGE_SIZE - 1)) != 0) ||
        ((ByteCount & (EFI_PAGE_SIZE - 1)) != 0) ||
        ((MapFlags & HV_MAP_GPA_READABLE) == 0) ||
        ((MapFlags & ~(HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE)) != 0))
    {
        status = EFI_INVALID_PARAMETER;
        DEBUG((DEBUG_ERROR, "--- %a: incorrect alignment or access - %r \n", __FUNCTION__, status));
        return status;
    }

    //
    // Verify that host-read-only is not requested on a system that doesn't
    // support it.
    //
    if (IsHardwareIsolatedEx(mIsolationType) &&
        ((MapFlags & (HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE)) == HV_MAP_GPA_READABLE))
    {
        status = EFI_INVALID_PARAMETER;
        DEBUG((DEBUG_ERROR, "--- %a: invalid host read only request - %r \n", __FUNCTION__, status));
        return status;
    }

    //
    // Allocate memory to use as a tracking object.
    //
    protectionObject = AllocatePool(sizeof(*protectionObject));
    if (protectionObject == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        DEBUG((DEBUG_ERROR, "--- %a: failed to allocate memory - %r \n", __FUNCTION__, status));
        return status;
    }

    protectionObject->GpaPageBase = (UINTN)BaseAddress / EFI_PAGE_SIZE;
    protectionObject->NumberOfPages = ByteCount / EFI_PAGE_SIZE;

    //
    // If this is a software-isolated VM, then memory must be zeroed before it
    // is made visible to the host, since page contents will remain intact
    // following the visibility change.  For a hardware-isolated VM, memory
    // encryption differences will obscure the original contents following the
    // visibility change.
    //
    if (IsSoftwareIsolatedEx(mIsolationType))
    {
        ZeroMem(BaseAddress, ByteCount);
        ZeroPages = FALSE;
    }

    //
    // Update the visibility as requested.
    //
    status =
        EfiHvpModifySparseGpaPageHostVisibility(
            MapFlags,
            protectionObject->NumberOfPages,
            protectionObject->GpaPageBase,
            &pageCountProcessed);

    if (EFI_ERROR(status))
    {

        //
        // If the protection change was partially made, then undo whatever
        // was done.
        //
        if (pageCountProcessed != 0)
        {
            revertStatus =
                EfiHvpModifySparseGpaPageHostVisibility(
                    HV_MAP_GPA_PERMISSIONS_NONE,
                    pageCountProcessed,
                    protectionObject->GpaPageBase,
                    &pageCountProcessed);
            if (EFI_ERROR(revertStatus))
            {

                //
                // This is not allowed to fail - need to fail fast
                //
                FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
            }
        }

        FreePool(protectionObject);
    }
    else
    {
        InsertTailList(&mHostVisiblePageList, &protectionObject->ListEntry);

        //
        // If zeroing was requested and has not already been performed, then
        // zero the buffer now.
        //
        if (ZeroPages)
        {
            ZeroMem(EfiHvpSharedVa(BaseAddress), ByteCount);
        }

        if (ProtectionHandle != NULL)
        {
            *ProtectionHandle = protectionObject;
        }
    }

    return status;
}


VOID
EFIAPI
EfiHvMakeAddressRangeNotHostVisible(
    IN  EFI_HV_IVM_PROTOCOL *This,
    IN  EFI_HV_PROTECTION_HANDLE ProtectionHandle
    )
/*++
    Makes a chunk of memory not visible to the host.
    Note: Memory visibility changes for hardware-isolated
          systems may change the contents of the pages.

    @param This A pointer to the EFI_HV_PROTOCOL instance.

    @param ProtectionHandle Object used to track memory range.

    @returns EFI status.

--*/
{
    EFI_STATUS status;

    RemoveEntryList(&ProtectionHandle->ListEntry);

    status =
        EfiHvpModifySparseGpaPageHostVisibility(
            HV_MAP_GPA_PERMISSIONS_NONE,
            ProtectionHandle->NumberOfPages,
            ProtectionHandle->GpaPageBase,
            NULL);
    if (EFI_ERROR(status))
    {

        //
        // This is not allowed to fail - need to fail fast
        //
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }
}


EFI_STATUS
EfiHvConnectToHypervisor (
    VOID
    )
/*++
    Initializes a connection to the hypervisor.

    @param None.

    @returns EFI status.

--*/
{
    EFI_STATUS status;

#if defined(MDE_CPU_X64)

    HV_CPUID_RESULT cpuidResult;
    BOOLEAN paravisorPresent;

    //
    // Determine the isolation type for this system.  If there is any
    // isolation, then a Microsoft hypervisor can be assumed.
    //
    mIsolationType = GetIsolationType();
    if (!IsIsolatedEx(mIsolationType))
    {

        //
        // Validate that the hypervisor is present, is a Microsoft hypervisor,
        // and has all the required features.
        //
        __cpuid(cpuidResult.AsUINT32, HvCpuIdFunctionVersionAndFeatures);
        if (!cpuidResult.VersionAndFeatures.HypervisorPresent)
        {
            status = EFI_UNSUPPORTED;
            DEBUG((DEBUG_ERROR, "--- %a: no hypervisor present - %r \n", __FUNCTION__, status));
            goto Exit;
        }

        __cpuid(cpuidResult.AsUINT32, HvCpuIdFunctionHvInterface);
        if (cpuidResult.HvInterface.Interface != HvMicrosoftHypervisorInterface)
        {
            status = EFI_UNSUPPORTED;
            DEBUG((DEBUG_ERROR, "--- %a: hypervisor present is not a Microsoft hypervisor - %r \n", __FUNCTION__, status));
            goto Exit;
        }
    }

    mSharedGpaBoundary = PcdGet64(PcdIsolationSharedGpaBoundary);
    mCanonicalizationMask = PcdGet64(PcdIsolationSharedGpaCanonicalizationBitmask);
    paravisorPresent = IsParavisorPresent();

    if ((mIsolationType == UefiIsolationTypeSnp) && !paravisorPresent)
    {
        mSvsmCallingArea = (VOID*)PcdGet64(PcdSvsmCallingArea);
    }

    //
    // Allocate hypervisor communication pages.
    //
    mHypercallPage = NULL;
    mHvPages = AllocatePages(sizeof(*mHvPages) / EFI_PAGE_SIZE);
    if (mHvPages == NULL)
    {
        status =  EFI_OUT_OF_RESOURCES;
        DEBUG((DEBUG_ERROR, "--- %a: failed to allcoate HV pages - %r \n", __FUNCTION__, status));
        goto Exit;
    }
    ZeroMem(mHvPages, sizeof(*mHvPages));

    //
    // If this is a hardware-isolated system with no paravisor, then only the
    // direct, untrusted hypervisor connection is required.
    //
    if (IsHardwareIsolatedNoParavisorEx(mIsolationType, paravisorPresent))
    {

        //
        // Make all of the pages visible to the host.
        //
        status = EfiHvMakeAddressRangeHostVisible(
            NULL,
            HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE,
            mHvPages,
            sizeof(*mHvPages),
            TRUE,
            NULL);

        if (EFI_ERROR(status))
        {
            DEBUG((DEBUG_ERROR, "--- %a: failed to make pages host visible - %r \n", __FUNCTION__, status));
            goto Exit;
        }

        mHvPages = (PEFI_HV_PAGES)EfiHvpSharedVa(mHvPages);
        ZeroMem(mHvPages, sizeof(*mHvPages));
    }
    else
    {
        mHypercallPage = AllocatePages(1);
        if (mHypercallPage == NULL)
        {
            status = EFI_OUT_OF_RESOURCES;
            DEBUG((DEBUG_ERROR, "--- %a: failed to allcoate the hypercall page - %r \n", __FUNCTION__, status));
            goto Exit;
        }

        ZeroMem(mHypercallPage, EFI_PAGE_SIZE);

        HvHypercallConnect(
            mHypercallPage,
            UefiIsolationTypeNone,
            FALSE,
            &mHvContext);

        //
        // Check to see if the hypercall page was mapped. If it wasn't, abort here.
        //
        if (mHypercallPage[0] == 0 &&
            mHypercallPage[1] == 0 &&
            mHypercallPage[2] == 0)
        {
            FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
        }

        //
        // Mark the Hypercall page as executable.
        //
        status = mCpu->SetMemoryAttributes (mCpu, (EFI_PHYSICAL_ADDRESS)mHypercallPage, EFI_PAGE_SIZE, EFI_MEMORY_RO);
        if (EFI_ERROR(status))
        {
            FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
        }
    }

#elif defined(MDE_CPU_AARCH64)

    //
    // Direct timers are always supported on ARM64.
    //
    mDirectTimerSupported = TRUE;

    //
    // Allocate hypervisor communication pages.
    //
    mHvPages = AllocatePages(sizeof(*mHvPages) / EFI_PAGE_SIZE);
    if (mHvPages == NULL)
    {
        status =  EFI_OUT_OF_RESOURCES;
        DEBUG((DEBUG_ERROR, "--- %a: failed to allcoate HV pages - %r \n", __FUNCTION__, status));
        goto Exit;
    }

    HvHypercallConnect(&mHvContext);

    //
    // AutoEoi is not possible on ARM.
    //
    mAutoEoi = FALSE;

#else
#error Unsupported architecture
#endif

    //
    // Initialize the hypercall input page.
    //
    mHvInputPage = mHvPages->HypercallInputPage;

#if defined(MDE_CPU_X64)

    //
    // Determine whether this system uses a hardware isolation architecture
    // that will require a direct connection to the hypervisor that bypasses
    // the paravisor.
    //
    if (IsHardwareIsolatedEx(mIsolationType))
    {
        ASSERT(mSharedGpaBoundary != 0);

        //
        // TDX systems require a host-visible page to use as the hypercall
        // input page when making hypercalls that bypass the paravisor.
        // Allocate such a page if required.  SNP systems always copy
        // hypercall input into the GHCB page so no additional allocation is
        // required for those systems.
        //
        if ((mIsolationType != UefiIsolationTypeSnp) && paravisorPresent)
        {
            VOID* hvInputPage;

            hvInputPage = AllocatePages(1);
            if (hvInputPage == NULL)
            {
                status = EFI_OUT_OF_RESOURCES;
                DEBUG((DEBUG_ERROR, "--- %a: failed to allcoate HV input page - %r \n", __FUNCTION__, status));
                goto Exit;
            }

            //
            // Make this page visible to the hypervisor.  It should not be
            // possible for this to fail.
            //
            status = EfiHvpModifySparseGpaPageHostVisibility(
                HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE,
                1,
                (UINTN)hvInputPage / EFI_PAGE_SIZE,
                NULL);

            if (EFI_ERROR(status))
            {
                FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
            }

            mHvInputPage = EfiHvpSharedVa(hvInputPage);
        }
        else
        {
            mBypassOnly = !paravisorPresent;
        }

        HvHypercallConnect(NULL,
                           mIsolationType,
                           paravisorPresent,
                           &mHvBypassContext);

        mUseBypassContext = TRUE;
    }

    //
    // Cache some enlightenment information.  If this system requires
    // bypassing the paravisor, then assume a set of features that are present
    // instead of asking the hypervisor what it supports.
    //
    if (mUseBypassContext)
    {
        mAutoEoi = FALSE;
        mDirectTimerSupported = TRUE;
    }
    else
    {
        __cpuid(cpuidResult.AsUINT32, HvCpuIdFunctionMsHvEnlightenmentInformation);
        mAutoEoi = !cpuidResult.MsHvEnlightenmentInformation.DeprecateAutoEoi;
        DEBUG((DEBUG_VERBOSE, "--- %a: mAutoEoi 0x%x\n", __FUNCTION__, mAutoEoi));

        __cpuid(cpuidResult.AsUINT32, HvCpuIdFunctionMsHvFeatures);
        if (!(cpuidResult.MsHvFeatures.PartitionPrivileges.AccessPartitionReferenceCounter &&
              cpuidResult.MsHvFeatures.PartitionPrivileges.AccessSynicRegs &&
              cpuidResult.MsHvFeatures.PartitionPrivileges.AccessSyntheticTimerRegs &&
              cpuidResult.MsHvFeatures.PartitionPrivileges.AccessHypercallMsrs))
        {
            status = EFI_UNSUPPORTED;
            DEBUG((DEBUG_ERROR, "--- %a: missing hypervisor features - %r \n", __FUNCTION__, status));
            goto Exit;
        }

        if (cpuidResult.MsHvFeatures.DirectSyntheticTimers)
        {
            mDirectTimerSupported = TRUE;
        }
    }

    if (IsIsolatedEx(mIsolationType))
    {
        DEBUG((EFI_D_INFO, "--- %a: Partition is Isolated\n", __FUNCTION__));
    }

#endif

    status = EFI_SUCCESS;

Exit:

    return status;
}


VOID
EfiHvDisconnectFromHypervisor (
    VOID
    )
/*++
    Tears down a connection to the hypervisor.

    @param None.

    @returns nothing.

--*/
{
    LIST_ENTRY *entry;
    EFI_HV_PROTECTION_OBJECT *protectionObject;
    EFI_STATUS status;

    if (mUseBypassContext)
    {
        HvHypercallDisconnect(&mHvBypassContext);
    }

    //
    // Revoke host visibility for any pages that were made visible.
    //
    while (!IsListEmpty(&mHostVisiblePageList))
    {
        entry = GetFirstNode(&mHostVisiblePageList);
        protectionObject = BASE_CR(entry, EFI_HV_PROTECTION_OBJECT, ListEntry);
        EfiHvMakeAddressRangeNotHostVisible(NULL, protectionObject);
    }

    //
    // Free the bypass input page if required.
    //
    if ((mHvInputPage != mHvPages->HypercallInputPage) && !mBypassOnly)
    {
        mHvInputPage = (VOID*)EfiHvpBasePa((UINTN)mHvInputPage);

        status = EfiHvpModifySparseGpaPageHostVisibility(
            HV_MAP_GPA_PERMISSIONS_NONE,
            1,
            (UINTN)mHvInputPage / EFI_PAGE_SIZE,
            NULL);

        if (EFI_ERROR(status))
        {
            //
            // Failure is not allowed here - need to fail fast.
            //
            FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
        }

        FreePages(mHvInputPage, 1);
    }

    HvHypercallDisconnect(&mHvContext);

    //
    // Free the hypercall communication pages.  If these pages were originally
    // made host-visible, then they were made host-not-visible during the
    // visibility reclaim operation above.
    //
    if (mHvPages != NULL)
    {
        if (mBypassOnly)
        {
            mHvPages = (PEFI_HV_PAGES)EfiHvpBasePa((UINTN)mHvPages);
        }

        FreePages(mHvPages, sizeof(*mHvPages) / EFI_PAGE_SIZE);
        mHvPages = NULL;
    }

#if defined (MDE_CPU_X64)
    if (mHypercallPage != NULL)
    {
        FreePages(mHypercallPage, 1);
        mHypercallPage = NULL;
    }
#endif
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
            component->Page = (VOID*)gpa;
        }
        else
        {
            component->Page = EfiHvpSharedVa((VOID*)gpa);
        }
    }
    else
    {
        ASSERT((mUseBypassContext == FALSE) || mBypassOnly || Direct);
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
        volatile HV_SYNIC_EVENT_FLAGS* flags = EfiHvGetSintEventFlags(&mHv, sintIndex, FALSE);

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


VOID
EFIAPI
EfiHvExitBootServices (
    IN  EFI_EVENT Event,
    IN  VOID *Context
    )
/*++
    Called when ExitBootServices() is called. Tears down the hypervisor
    connection so that the new OS sees a clean state.

    @param Event An EFI event.

    @param Context A pointer to the context.

    @returns nothing.

--*/
{
    EfiHvDisconnectFromSynic();
    EfiHvDisconnectFromHypervisor();
}

EFI_HV_PROTOCOL mHv =
{
    EfiHvConnectSint,
    EfiHvConnectSintToEvent,
    EfiHvDisconnectSint,
    EfiHvGetSintMessage,
    EfiHvCompleteSintMessage,
    EfiHvGetSintEventFlags,
    EfiHvGetReferenceTime,
    EfiHvGetCurrentVpIndex,
    EfiHvDirectTimerSupported,
    EfiHvConfigureTimer,
    EfiHvSetTimer,
    EfiHvPostMessage,
    EfiHvSignalEvent,
    EfiHvStartApplicationProcessor
};

EFI_HV_IVM_PROTOCOL mHvIvm =
{
    EfiHvMakeAddressRangeHostVisible,
    EfiHvMakeAddressRangeNotHostVisible
};


EFI_STATUS
EFIAPI
EfiHvInitialize (
    IN  EFI_HANDLE ImageHandle,
    IN  EFI_SYSTEM_TABLE *SystemTable
    )
/*++
    Entrypoint. Initializes the EfiHv driver.

    @param ImageHandle The handle of the loaded image.

    @param SystemTable A pointer to the system table.

    @returns EFI status.

--*/
{
    EFI_STATUS status;

    InitializeListHead(&mHostVisiblePageList);

#if defined (MDE_CPU_X64)

    //
    // For Intel find the CPU protocol.
    //
    status = gBS->LocateProtocol(&gEfiCpuArchProtocolGuid, NULL, (VOID **)&mCpu);


#elif defined(MDE_CPU_AARCH64)

    //
    // For ARM find the hardware interrupt protocol.
    //
    status = gBS->LocateProtocol(&gHardwareInterruptProtocolGuid, NULL, (VOID **)&mHwInt);

#endif

    if (EFI_ERROR(status))
    {
        DEBUG((DEBUG_ERROR, "--- %a: failed to locate protocol - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    //
    // Register notify function for EVT_SIGNAL_EXIT_BOOT_SERVICES.
    //
    status = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,
                                TPL_CALLBACK,
                                EfiHvExitBootServices,
                                NULL,
                                &gEfiEventExitBootServicesGuid,
                                &mExitBootServicesEvent);
    if (EFI_ERROR(status))
    {
        DEBUG((DEBUG_ERROR, "--- %a: failed to create event - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    //
    // Connect to the hypervisor and synic.
    //
    status = EfiHvConnectToHypervisor();
    if (EFI_ERROR(status))
    {
        DEBUG((DEBUG_ERROR, "--- %a: failed to connect to the hypervisor - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    status = EfiHvConnectToSynic();
    if (EFI_ERROR(status))
    {
        DEBUG((DEBUG_ERROR, "--- %a: failed to connect to Synic - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    //
    // Register the HV protocols.
    //
    status = gBS->InstallMultipleProtocolInterfaces(
                    &mHvHandle,
                    &gEfiHvProtocolGuid, &mHv,
                    &gEfiHvIvmProtocolGuid, &mHvIvm,
                    NULL);

    if (EFI_ERROR(status))
    {
        DEBUG((DEBUG_ERROR, "--- %a: failed to install the protocol - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

Cleanup:
    if (EFI_ERROR(status))
    {
        if (mExitBootServicesEvent != NULL)
        {
            gBS->CloseEvent(mExitBootServicesEvent);
            mExitBootServicesEvent = NULL;
        }

        EfiHvDisconnectFromSynic();
        EfiHvDisconnectFromHypervisor();
    }

    return EFI_SUCCESS;
}

