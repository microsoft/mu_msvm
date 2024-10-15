/** @file
  This file implements support routines for GHCB-based calls.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Base.h>
#include <Hv/HvGuestHypercall.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HvHypercallLib.h>
#include <Library/GhcbLib.h>

#include <HvHypercallLibP.h>

typedef struct _GHCB_HYPERCALL {
    UINT64 Parameters[509];
    UINT64 Output;
    UINT64 CallCode;
    UINT32 Reserved;
    UINT32 Format;
} GHCB_HYPERCALL, *PGHCB_HYPERCALL;

#define SET_GHCB_FORMAT(Ghcb, Format) \
    ((*(PUINT32)(Ghcb) + 0x3FF) = (Format))

#define GHCB_FIELD64_HYPERCALL_CODE     0xFF0
#define GHCB_FIELD64_HYPERCALL_OUTPUT   0xFE8

#define HV_STATUS_INVALID_HYPERCALL_CODE ((HV_STATUS)0x0002)
#define HV_STATUS_INVALID_PARAMETER      ((HV_STATUS)0x0005)
#define HV_STATUS_TIMEOUT                ((HV_STATUS)0x0078)

HV_STATUS
HvHypercallpIssueGhcbHypercall(
    IN              HV_HYPERCALL_CONTEXT    *Context,
    IN              HV_CALL_CODE            CallCode,
    IN OPTIONAL     VOID                    *InputPage,
                    UINT32                  CountOfElements,
    OUT OPTIONAL    UINT32                  *ElementsProcessed
    )
{
    PGHCB_HYPERCALL ghcb;
    UINT32 headerSize;
    HV_HYPERCALL_INPUT hypercallInput;
    HV_HYPERCALL_OUTPUT hypercallOutput;
    UINT32 inputSize;
    UINT32 repSize;
    HV_STATUS status;

    ghcb = Context->Ghcb;

    //
    // Copy the input page if required.  In order to minimize the amount of
    // data exposed, only the amount of input specified by the call code and
    // rep count are copied to the GHCB.  This means that only specifically
    // approved hypercalls can be made, so the calculation can be done
    // correctly.
    //

    if (InputPage != NULL)
    {
        switch (CallCode)
        {
        case HvCallPostMessage:
            {
                PHV_INPUT_POST_MESSAGE input;
                input = InputPage;
                headerSize = sizeof(*input) + input->PayloadSize;
                repSize = 0;
            }
            break;

        default:
            ASSERT(FALSE);
            return HV_STATUS_INVALID_HYPERCALL_CODE;
        }

        inputSize = headerSize + (repSize * CountOfElements);
        if (inputSize > OFFSET_OF(GHCB_HYPERCALL, Output))
        {
            ASSERT(FALSE);
            return HV_STATUS_INVALID_PARAMETER;
        }
        CopyMem(ghcb, InputPage, inputSize);
    }

    ghcb->Format = 1;

    hypercallInput.AsUINT64 = 0;
    hypercallInput.CallCode = CallCode;
    hypercallInput.CountOfElements = CountOfElements;

    while (TRUE)
    {
        ghcb->CallCode = hypercallInput.AsUINT64;

        _sev_vmgexit();

        //
        // If this was not a rep hypercall, or if the call failed, then no
        // further processing is required.
        //

        hypercallOutput.AsUINT64 = ghcb->CallCode;

        if ((CountOfElements == 0) ||
            (hypercallOutput.CallStatus != HV_STATUS_TIMEOUT))
        {
            break;
        }

        //
        // Continue processing from wherever the hypervisor left off.  The
        // rep start index is not checked for validity, since it is only being
        // used as an input to the untrusted hypervisor.
        //

        hypercallInput.RepStartIndex = hypercallOutput.ElementsProcessed;
    }

    status = (HV_STATUS)hypercallOutput.CallStatus;

    //
    // Ensure that the completed rep count is reasonable.  If not, indicate
    // that the fall failed.
    //

    if ((status == HV_STATUS_SUCCESS) && (hypercallOutput.ElementsProcessed == CountOfElements))
    {
        // NOTHING
    }
    else if ((status != HV_STATUS_SUCCESS) && (hypercallOutput.ElementsProcessed < CountOfElements))
    {
        // NOTHING
    }
    else
    {
        ASSERT(FALSE);
        hypercallOutput.ElementsProcessed = 0;
        status = 0xFFFF;
    }

    if (ElementsProcessed != NULL)
    {
        *ElementsProcessed = hypercallOutput.ElementsProcessed;
    }

    return status;
}
