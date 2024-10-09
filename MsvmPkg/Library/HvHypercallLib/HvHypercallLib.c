/** @file
  This file implements hypercall support routines.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Base.h>
#include <Hv/HvGuest.h>
#include <Hv/HvGuestMsr.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HvHypercallLib.h>
#include <Library/GhcbLib.h>
#include <IsolationTypes.h>

#include <HvHypercallLibP.h>

#define SEV_MSR_GHCB                    0xC0010130

#if defined(MDE_CPU_X64)

HV_HYPERCALL_OUTPUT
HvHypercallpIssueTdxHypercall(
    IN  HV_HYPERCALL_INPUT  Control,
        UINT64              InputPhysicalAddress,
        UINT64              OutputPhysicalAddress
    );


VOID
HvHypercallConnect(
    IN  VOID                    *HypercallPage,
        UINT32                  IsolationType,
        BOOLEAN                 ParavisorPresent,
    OUT HV_HYPERCALL_CONTEXT    *Context
    )
/*++

Routine Description:

    Sets up hypercall context by connecting to the hypervisor.

Arguments:

    HypercallPage - An address at which to place the hypercall page.

    IsolationType - Supplies an isolation architecture which must be used to
                    connect to the hypervisor, or UefiIsolationTypeNone for
                    non-isolated hypercalls (also used for calls to the
                    paravisor from an isolated VM).

    ParavisorPresent - For hardware isolation types, whether a paravisor is present.

    Context - Receives the hypercall context.

Return Value:

    None.

--*/
{
    HV_GUEST_OS_ID_CONTENTS guestOsId;
    HV_X64_MSR_HYPERCALL_CONTENTS hypercallMsr;

    ZeroMem(Context, sizeof(*Context));

    //
    // Choose a value for the guest ID.
    //

    guestOsId.AsUINT64 = 0;
    guestOsId.OsId = 1;

    if (IsolationType > UefiIsolationTypeVbs)
    {
#if defined(MDE_CPU_X64)

        //
        // Obtain the shared GPA boundary.  For isolation architectures that
        // require bypass calls, this must be non-zero.
        //

        Context->SharedGpaBoundary = PcdGet64(PcdIsolationSharedGpaBoundary);
        ASSERT(Context->SharedGpaBoundary != 0);

        Context->CanonicalizationMask = PcdGet64(PcdIsolationSharedGpaCanonicalizationBitmask);
        Context->ParavisorPresent = ParavisorPresent;

        //
        // Determine how the isolation boundary will be penetrated.
        //
        if (IsolationType == UefiIsolationTypeTdx)
        {
            Context->IsTdx = TRUE;
        }
        else
        {
            Context->Ghcb = GhcbInitializeGhcb();
        }

        //
        // Set the guest OS ID via a direct GHCB-based MSR write, since
        // GHCB-based hypercalls are not permitted until the guest OS MSR is
        // set.
        //

        if (Context->IsTdx)
        {
            _tdx_vmcall_wrmsr(HV_X64_MSR_GUEST_OS_ID, guestOsId.AsUINT64);
        }
        else
        {
            GhcbWriteMsr(Context->Ghcb, HV_X64_MSR_GUEST_OS_ID, guestOsId.AsUINT64);
        }

#else

        //
        // Isolation bypass is not permitted on 32-bit systems.
        //

        ASSERT(FALSE);

#endif
    }
    else
    {
        //
        // Set the guest ID before enabling hypercalls.
        //

        HvHypercallSetVpRegister64Self(Context, HvRegisterGuestOsId, guestOsId.AsUINT64);

        hypercallMsr.AsUINT64 = HvHypercallGetVpRegister64Self(Context, HvX64RegisterHypercall);
        ASSERT(hypercallMsr.Enable == 0);
        hypercallMsr.Enable = 1;
        hypercallMsr.GpaPageNumber = (UINTN)HypercallPage / HV_PAGE_SIZE;
        HvHypercallSetVpRegister64Self(Context, HvX64RegisterHypercall, hypercallMsr.AsUINT64);

        Context->HypercallPage = HypercallPage;
    }

    Context->Connected = TRUE;
}

#elif defined(MDE_CPU_AARCH64)

VOID
HvHypercallConnect(
    OUT HV_HYPERCALL_CONTEXT *Context
    )
/*++

Routine Description:

    Sets up hypercall context by connecting to the hypervisor.

Arguments:

    Context - Receives the hypercall context.

Return Value:

    None.

--*/
{
    HV_GUEST_OS_ID_CONTENTS guestOsId;

    ZeroMem(Context, sizeof(*Context));

    //
    // Set the guest ID.
    //

    guestOsId.AsUINT64 = 0;
    guestOsId.OsId = 4;     // Windows NT
    guestOsId.VendorId = 1; // Microsoft
    HvHypercallSetVpRegister64Self(Context, HvRegisterGuestOsId, guestOsId.AsUINT64);

    guestOsId.AsUINT64 = 0;
    guestOsId.AsUINT64 = HvHypercallGetVpRegister64Self(Context, HvRegisterGuestOsId);
    ASSERT(guestOsId.VendorId == 1 && guestOsId.OsId == 4);

    Context->Connected = TRUE;
}

#endif


VOID
HvHypercallDisconnect(
    IN OUT HV_HYPERCALL_CONTEXT *Context
    )
/*++

Routine Description:

    Tears down hypercall context by disconnecting from the hypervisor.

Arguments:

    Context - The hypercall context.

Return Value:

    None.

--*/
{
    if (Context->Connected)
    {
#if defined(MDE_CPU_X64)

        HV_X64_MSR_HYPERCALL_CONTENTS hypercallMsr;

        if ((Context->Ghcb == NULL) && !Context->IsTdx)
        {
            hypercallMsr.AsUINT64 = HvHypercallGetVpRegister64Self(Context, HvX64RegisterHypercall);
            hypercallMsr.Enable = 0;
            hypercallMsr.GpaPageNumber = 0;
            HvHypercallSetVpRegister64Self(Context, HvX64RegisterHypercall, hypercallMsr.AsUINT64);
        }

#endif

        HvHypercallSetVpRegister64Self(Context, HvRegisterGuestOsId, 0);

        Context->Connected = FALSE;
    }
}

#ifndef HV_STATUS_TIMEOUT

//
// MessageId: HV_STATUS_TIMEOUT
//
// MessageText:
//
// The specified timeout expired before the operation completed.
//
#define HV_STATUS_TIMEOUT                ((HV_STATUS)0x0078)

#endif

HV_STATUS
HvHypercallIssue(
    IN              HV_HYPERCALL_CONTEXT    *Context,
    IN              HV_CALL_CODE            CallCode,
                    BOOLEAN                 Fast,
                    UINT32                  CountOfElements,
                    UINT64                  FirstRegister,
                    UINT64                  SecondRegister,
    OUT OPTIONAL    UINT32                  *ElementsProcessed
    )
/*++

Routine Description:

    Issues a hypercall.

Arguments:

    Context - The hypercall context (set up via HvHypercallConnect).

    CallCode - The hypercall code.

    Fast - If TRUE, this is a fast hypercall.

    CountOfElements - The number of elements to process, or 0 if not a rep
        hypercall.

    FirstRegister - The first register value for the hypercall. If a slow hypercall, this must refer
        to the non-shared alias of the GPA.

    SecondRegister - The second register value for the hypercall. If a slow hypercall, this must
        refer to the non-shared alias of the GPA.

    ElementsProcessed - Receives the number of elements that were successfully
        processed.

Return Value:

    The hypercall status.

--*/
{
    HV_HYPERCALL_INPUT callInput;
    HV_HYPERCALL_OUTPUT callOutput;

    ASSERT(Context->Connected);
// TODO: temporary until exception handling can reboot
#if defined(MDE_CPU_AARCH64)
    if (!Context->Connected)
    {
        return HV_STATUS_INVALID_PARTITION_STATE;
    }
#endif
    ASSERT(CountOfElements <= HV_X64_MAX_HYPERCALL_ELEMENTS);

    // DEBUG((DEBUG_VERBOSE, ">>> %a\n", __FUNCTION__));

    callInput.AsUINT64 = 0;
    callInput.CallCode = CallCode;
    callInput.IsFast = (Fast != FALSE);
    callInput.CountOfElements = CountOfElements;

#if defined(MDE_CPU_X64)

    if (Context->Ghcb != NULL)
    {
        HvHypercallpDisableInterrupts();

        //
        // When a GHCB is present, it means that the call must be made via
        // VMGEXIT directly.
        //

        if (Fast)
        {
            //
            // No input page copy is required; just fill the GHCB with the
            // input parameters.
            //

            ((UINT64*)Context->Ghcb)[0] = FirstRegister;
            ((UINT64*)Context->Ghcb)[1] = SecondRegister;

            FirstRegister = 0;
        }
        else
        {
            ASSERT(SecondRegister == 0);

            if (!Context->ParavisorPresent && FirstRegister != 0)
            {
                //
                // FirstRegister supplies the Input Page PA, below the shared gpa
                // boundary.
                // GHCB based calls do not directly specify this page, rather the
                // data is copied over from it into the GHCB. Convert it to a VA
                // to make this possible.
                //

                ASSERT(FirstRegister < Context->SharedGpaBoundary);
                FirstRegister += Context->SharedGpaBoundary;
                FirstRegister |= Context->CanonicalizationMask;
            }
        }

        callOutput.CallStatus = HvHypercallpIssueGhcbHypercall(Context,
                                                               CallCode,
                                                               (VOID *)FirstRegister,
                                                               CountOfElements,
                                                               ElementsProcessed);

        HvHypercallpEnableInterrupts();

        return callOutput.CallStatus;
    }
    else
    {
        typedef HV_HYPERCALL_OUTPUT HYPERCALL_ROUTINE(
            IN  HV_HYPERCALL_INPUT  Control,
                UINT64              InputPhysicalAddress,
                UINT64              OutputPhysicalAddress
            );

        HYPERCALL_ROUTINE* hypercallRoutine;
        if (Context->IsTdx)
        {
            hypercallRoutine = HvHypercallpIssueTdxHypercall;

            if (!Fast)
            {
                //
                // FirstRegister and SecondRegister supply the Input Page and Output Page PAs,
                // below the shared gpa boundary. Convert them to the shared GPA.
                //

                if (FirstRegister != 0)
                {
                    ASSERT(FirstRegister < Context->SharedGpaBoundary);
                    FirstRegister += Context->SharedGpaBoundary;
                }

                if (SecondRegister != 0)
                {
                    ASSERT(SecondRegister < Context->SharedGpaBoundary);
                    SecondRegister += Context->SharedGpaBoundary;
                }
            }

            while (TRUE)
            {
                callOutput = hypercallRoutine(callInput, FirstRegister, SecondRegister);

                if ((CountOfElements == 0) ||
                    (callOutput.CallStatus != HV_STATUS_TIMEOUT))
                {
                    break;
                }

                //
                // Continue processing from wherever the hypervisor left off.  The
                // rep start index is not checked for validity, since it is only being
                // used as an input to the untrusted hypervisor.
                //

                callInput.RepStartIndex = callOutput.ElementsProcessed;
            }

            if ((callOutput.CallStatus == HV_STATUS_SUCCESS) &&
                (callOutput.ElementsProcessed == CountOfElements))
            {
                // NOTHING
            }
            else if ((callOutput.CallStatus != HV_STATUS_SUCCESS) &&
                     (callOutput.ElementsProcessed < CountOfElements))
            {
                // NOTHING
            }
            else
            {
                ASSERT(FALSE);
                callOutput.ElementsProcessed = 0;
                callOutput.CallStatus = 0xFFFF;
            }
        }
        else
        {
            hypercallRoutine = (HYPERCALL_ROUTINE *)Context->HypercallPage;
            callOutput = hypercallRoutine(callInput,
                                          FirstRegister,
                                          SecondRegister);
        }

    }

#elif defined(MDE_CPU_AARCH64)

    callOutput = AsmHyperCall(callInput, FirstRegister, SecondRegister);

#else
#error Unsupported Architecture
#endif
    // DEBUG((DEBUG_VERBOSE, "<<< %a\n", __FUNCTION__));

    if (ElementsProcessed != NULL)
    {
        *ElementsProcessed = callOutput.ElementsProcessed;
    }

    return callOutput.CallStatus;
}


#if !defined(MDEPKG_NDEBUG)
CHAR16*
HvHypercallpRegisterNameToString(
    IN  HV_REGISTER_NAME RegisterName
    )
{
    switch (RegisterName)
    {
    case HvRegisterSint0:
        return L"HvRegisterSint0";
    case HvRegisterSint1:
        return L"HvRegisterSint1";
    case HvRegisterSint2:
        return L"HvRegisterSint2";
    case HvRegisterSint3:
        return L"HvRegisterSint3";
    case HvRegisterSint4:
        return L"HvRegisterSint4";
    case HvRegisterSint5:
        return L"HvRegisterSint5";
    case HvRegisterSint6:
        return L"HvRegisterSint6";
    case HvRegisterSint7:
        return L"HvRegisterSint7";
    case HvRegisterSint8:
        return L"HvRegisterSint8";
    case HvRegisterSint9:
        return L"HvRegisterSint9";
    case HvRegisterSint10:
        return L"HvRegisterSint10";
    case HvRegisterSint11:
        return L"HvRegisterSint11";
    case HvRegisterSint12:
        return L"HvRegisterSint12";
    case HvRegisterSint13:
        return L"HvRegisterSint13";
    case HvRegisterSint14:
        return L"HvRegisterSint14";
    case HvRegisterSint15:
        return L"HvRegisterSint15";

    case HvRegisterScontrol:
        return L"HvRegisterScontrol";
    case HvRegisterSversion:
        return L"HvRegisterSversion";
    case HvRegisterSifp:
        return L"HvRegisterSifp";
    case HvRegisterSipp:
        return L"HvRegisterSipp";
    case HvRegisterEom:
        return L"HvRegisterEom";
    case HvRegisterSirbp:
        return L"HvRegisterSirbp";

    case HvRegisterNestedSint0:
        return L"HvRegisterNestedSint0";
    case HvRegisterNestedSint1:
        return L"HvRegisterNestedSint1";
    case HvRegisterNestedSint2:
        return L"HvRegisterNestedSint2";
    case HvRegisterNestedSint3:
        return L"HvRegisterNestedSint3";
    case HvRegisterNestedSint4:
        return L"HvRegisterNestedSint4";
    case HvRegisterNestedSint5:
        return L"HvRegisterNestedSint5";
    case HvRegisterNestedSint6:
        return L"HvRegisterNestedSint6";
    case HvRegisterNestedSint7:
        return L"HvRegisterNestedSint7";
    case HvRegisterNestedSint8:
        return L"HvRegisterNestedSint8";
    case HvRegisterNestedSint9:
        return L"HvRegisterNestedSint9";
    case HvRegisterNestedSint10:
        return L"HvRegisterNestedSint10";
    case HvRegisterNestedSint11:
        return L"HvRegisterNestedSint11";
    case HvRegisterNestedSint12:
        return L"HvRegisterNestedSint12";
    case HvRegisterNestedSint13:
        return L"HvRegisterNestedSint13";
    case HvRegisterNestedSint14:
        return L"HvRegisterNestedSint14";
    case HvRegisterNestedSint15:
        return L"HvRegisterNestedSint15";

    case HvRegisterNestedScontrol:
        return L"HvRegisterNestedScontrol";
    case HvRegisterNestedSversion:
        return L"HvRegisterNestedSversion";
    case HvRegisterNestedSifp:
        return L"HvRegisterNestedSifp";
    case HvRegisterNestedSipp:
        return L"HvRegisterNestedSipp";
    case HvRegisterNestedEom:
        return L"HvRegisterNestedEom";
    case HvRegisterNestedSirbp:
        return L"HvRegisterNestedSirbp";

    case HvRegisterVpIndex:
        return L"HvRegisterVpIndex";
    case HvRegisterGuestOsId:
        return L"HvRegisterGuestOsId";
    case HvRegisterTimeRefCount:
        return L"HvRegisterTimeRefCount";
    case HvRegisterNestedVpIndex:
        return L"HvRegisterNestedVpIndex";

    case HvRegisterStimer0Config:
        return L"HvRegisterStimer0Config";
    case HvRegisterStimer0Count:
        return L"HvRegisterStimer0Count";
    case HvRegisterStimer1Config:
        return L"HvRegisterStimer1Config";
    case HvRegisterStimer1Count:
        return L"HvRegisterStimer1Count";
    case HvRegisterStimer2Config:
        return L"HvRegisterStimer2Config";
    case HvRegisterStimer2Count:
        return L"HvRegisterStimer2Count";
    case HvRegisterStimer3Config:
        return L"HvRegisterStimer3Config";
    case HvRegisterStimer3Count:
        return L"HvRegisterStimer3Count";

#if defined (MDE_CPU_X64)
    case HvX64RegisterHypercall:
        return L"HvX64RegisterHypercall";
#endif

    default:
        return L"*** Unknown Register Name ***";
    }
}
#endif


#if defined (MDE_CPU_X64)
static
UINT32
HvHypercallpGetMsrNameFromRegisterName(
    IN  HV_REGISTER_NAME RegisterName
    )
/*++

Routine Description:

    Maps a register name to an msr index.

Arguments:

    RegisterName - Supplies the register name to be mapped.

Return Value:

    Returns the msr index.

--*/
{
    UINT32 msrIndex;

    switch (RegisterName)
    {
    case HvRegisterSint0:
    case HvRegisterSint1:
    case HvRegisterSint2:
    case HvRegisterSint3:
    case HvRegisterSint4:
    case HvRegisterSint5:
    case HvRegisterSint6:
    case HvRegisterSint7:
    case HvRegisterSint8:
    case HvRegisterSint9:
    case HvRegisterSint10:
    case HvRegisterSint11:
    case HvRegisterSint12:
    case HvRegisterSint13:
    case HvRegisterSint14:
    case HvRegisterSint15:
        msrIndex = HvSyntheticMsrSint0 + (RegisterName - HvRegisterSint0);
        break;

    case HvRegisterScontrol:
    case HvRegisterSversion:
    case HvRegisterSifp:
    case HvRegisterSipp:
    case HvRegisterEom:
    case HvRegisterSirbp:
        msrIndex = HvSyntheticMsrSControl + (RegisterName - HvRegisterScontrol);
        break;

    case HvRegisterNestedSint0:
    case HvRegisterNestedSint1:
    case HvRegisterNestedSint2:
    case HvRegisterNestedSint3:
    case HvRegisterNestedSint4:
    case HvRegisterNestedSint5:
    case HvRegisterNestedSint6:
    case HvRegisterNestedSint7:
    case HvRegisterNestedSint8:
    case HvRegisterNestedSint9:
    case HvRegisterNestedSint10:
    case HvRegisterNestedSint11:
    case HvRegisterNestedSint12:
    case HvRegisterNestedSint13:
    case HvRegisterNestedSint14:
    case HvRegisterNestedSint15:
        msrIndex = HvSyntheticMsrNestedSint0 +
            (RegisterName - HvRegisterNestedSint0);

        break;

    case HvRegisterNestedScontrol:
    case HvRegisterNestedSversion:
    case HvRegisterNestedSifp:
    case HvRegisterNestedSipp:
    case HvRegisterNestedEom:
    case HvRegisterNestedSirbp:
        msrIndex = HvSyntheticMsrNestedSControl +
            (RegisterName - HvRegisterNestedScontrol);

        break;

    case HvRegisterVpIndex:
        msrIndex = HvSyntheticMsrVpIndex;
        break;

    case HvRegisterGuestOsId:
        msrIndex = HvSyntheticMsrGuestOsId;
        break;

    case HvRegisterTimeRefCount:
        msrIndex = HvSyntheticMsrTimeRefCount;
        break;

    case HvRegisterNestedVpIndex:
        msrIndex = HvSyntheticMsrNestedVpIndex;
        break;

    case HvRegisterStimer0Config:
    case HvRegisterStimer0Count:
    case HvRegisterStimer1Config:
    case HvRegisterStimer1Count:
    case HvRegisterStimer2Config:
    case HvRegisterStimer2Count:
    case HvRegisterStimer3Config:
    case HvRegisterStimer3Count:
        msrIndex = HvSyntheticMsrSTimer0Config +
            (RegisterName - HvRegisterStimer0Config);
        break;

    case HvX64RegisterHypercall:
        msrIndex = HvSyntheticMsrHypercall;
        break;

    default:
        ASSERT(FALSE);
        __assume(0);
    }

    return msrIndex;
}
#endif


UINT64
HvHypercallGetVpRegister64Self(
    IN  HV_HYPERCALL_CONTEXT *Context,
    IN  HV_REGISTER_NAME RegisterName
    )
/*++

Routine Description:

    Gets a 64 bit register value on the current virtual processor.

Arguments:

    Context - The hypercall context (set up via HvHypercallConnect).

    RegisterName - Supplies the register name to be mapped.

Return Value:

    The register value.

--*/
{
    UINT64 registerValue;

#if defined (MDE_CPU_X64)
    UINT32 msr = HvHypercallpGetMsrNameFromRegisterName(RegisterName);

    // DEBUG((DEBUG_VERBOSE, ">>> %a: Name 0x%x %s MSR 0x%x\n", __FUNCTION__,
    //     RegisterName, HvHypercallpRegisterNameToString(RegisterName), msr));

#if defined(MDE_CPU_X64)

    if (Context->Ghcb != NULL)
    {
        GhcbReadMsr(Context->Ghcb, msr, &registerValue);
    }
    else if (Context->IsTdx)
    {
        registerValue = _tdx_vmcall_rdmsr(msr);
    }
    else

#endif
    {
        registerValue = AsmReadMsr64(msr);
    }

#elif defined(MDE_CPU_AARCH64)

    HV_STATUS status;

    ASSERT(Context->Ghcb == NULL);
    status = AsmGetVpRegister64(RegisterName, &registerValue);
    ASSERT(status == 0);

#else
#error Unsupported Architecture
#endif

    // DEBUG((DEBUG_VERBOSE, "<<< %a: Value 0x%lx\n", __FUNCTION__, registerValue));
    return registerValue;
}


VOID
HvHypercallSetVpRegister64Self(
    IN  HV_HYPERCALL_CONTEXT *Context,
    IN  HV_REGISTER_NAME RegisterName,
        UINT64 RegisterValue
    )
/*++

Routine Description:

    Sets a 64 bit register on the current virtual processor.

Arguments:

    Context - The hypercall context (set up via HvHypercallConnect).

    RegisterName - Supplies the register name to be mapped.

    RegisterValue - Supplies the register value.

Return Value:

    None.

--*/
{

#if defined (MDE_CPU_X64)

    UINT32 msr = HvHypercallpGetMsrNameFromRegisterName(RegisterName);

    // DEBUG((DEBUG_VERBOSE, ">>> %a: Name 0x%x %s MSR 0x%x Value 0x%lx\n", __FUNCTION__,
    //     RegisterName, HvHypercallpRegisterNameToString(RegisterName), msr, RegisterValue));

#if defined(MDE_CPU_X64)

    if (Context->Ghcb != NULL)
    {
        GhcbWriteMsr(Context->Ghcb, msr, RegisterValue);
    }
    else if (Context->IsTdx)
    {
        _tdx_vmcall_wrmsr(msr, RegisterValue);
    }
    else

#endif
    {
        AsmWriteMsr64(msr, RegisterValue);
    }

#elif defined(MDE_CPU_AARCH64)

    // DEBUG((DEBUG_VERBOSE, ">>> %a: Name 0x%x %s Value 0x%lx\n", __FUNCTION__,
    //     RegisterName, HvHypercallpRegisterNameToString(RegisterName),
    //     RegisterValue));

    ASSERT(Context->Ghcb == NULL);
    AsmSetVpRegister64(RegisterName, RegisterValue);

#else
#error Unsupported Architecture
#endif

    // DEBUG((DEBUG_VERBOSE, "<<< %a\n", __FUNCTION__));
}
