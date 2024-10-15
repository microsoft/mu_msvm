/** @file
 This file implements support routines for GHCB-based calls.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/GhcbLib.h>

EFI_TPL
GhcbpDisableInterrupts(
    VOID
    );


VOID
GhcbpEnableInterrupts(
    EFI_TPL tpl
    );


typedef struct _GHCB_HYPERCALL {
    UINT64 Parameters[511];
    UINT16 Reserved;
    UINT16 Version;
    UINT32 Format;
} GHCB_HYPERCALL, *PGHCB_HYPERCALL;

#define SEV_MSR_GHCB                    0xC0010130

#define GHCB_FIELD_INDEX(Field) ((Field) / 8)
#define GHCB_SET_FIELD_VALID(Ghcb, Field) \
    do { \
        if (Field < GHCB_FIELD_VALID_BITMAP0) { \
            _bittestandset64((UINT64*)((UINT8*)(Ghcb) + GHCB_FIELD_VALID_BITMAP0), GHCB_FIELD_INDEX(Field)); \
        } \
    } while (0)

#define SetGhcbField16(Ghcb, Field, Value) \
    GHCB_SET_FIELD_VALID(Ghcb, Field); \
    (*(UINT16*)((UINT8*)(Ghcb) + (Field)) = (Value))
#define SetGhcbField32(Ghcb, Field, Value) \
    GHCB_SET_FIELD_VALID(Ghcb, Field); \
    (*(UINT32*)((UINT8*)(Ghcb) + (Field)) = (Value))
#define SetGhcbField64(Ghcb, Field, Value) \
    GHCB_SET_FIELD_VALID(Ghcb, Field); \
    (*(UINT64*)((UINT8*)(Ghcb) + (Field)) = (Value))
#define GetGhcbField64(Ghcb, Field) \
    (*(UINT64*)((UINT8*)(Ghcb) + (Field)))
#define SET_GHCB_FORMAT(Ghcb, Format) \
    ((*(UINT32*)(Ghcb) + 0x3FF) = (Format))

#define GHCB_EXITCODE_MSR               0x7C

#define GHCB_FIELD64_RAX                0x1F8
#define GHCB_FIELD64_RCX                0x308
#define GHCB_FIELD64_RDX                0x310
#define GHCB_FIELD64_EXITCODE           0x390
#define GHCB_FIELD64_EXITINFO1          0x398
#define GHCB_FIELD64_EXITINFO2          0x3A0
#define GHCB_FIELD_VALID_BITMAP0        0x3F0
#define GHCB_FIELD_VALID_BITMAP1        0x3F8

VOID*
GhcbInitializeGhcb(
    VOID
    )
{
    UINT64 canonicalizationMask;
    UINT64 ghcbAddress;
    UINT64 sharedGpaBoundary;

    //
    // Obtain the shared GPA boundary.  For isolation architectures that
    // require bypass calls, this must be non-zero.
    //

    sharedGpaBoundary = PcdGet64(PcdIsolationSharedGpaBoundary);
    ASSERT(sharedGpaBoundary != 0);

    canonicalizationMask = PcdGet64(PcdIsolationSharedGpaCanonicalizationBitmask);

    //
    // Obtain the GHCB address.  If this is not above the shared GPA
    // boundary, then it must be incorrectly configured.  If the address
    // is above the shared GPA boundary, then the address can be used
    // without further validation, since only one of four outcomes is
    // possible:
    // 1. The address is non-canonical, which will result in a fatal
    //    exception when it is used.
    // 2. The address is canonical but exceeds the physical address width,
    //    which will result in a fatal exception when it is used.
    // 3. The address is the shared alias for a valid protected page.
    //    When it is used as shared, the hypervisor will revoke the
    //    private copy, resulting in a fatal exception the next time the
    //    protected memory is accessed.
    // 4. The address is legitimate.
    //

    ghcbAddress = AsmReadMsr64(SEV_MSR_GHCB);
    if ((ghcbAddress < sharedGpaBoundary) ||
        ((ghcbAddress & (EFI_PAGE_SIZE - 1)) != 0))
    {
        // If the GHCB is misconfigured, then no further work is possible.
        ASSERT(FALSE);
        CpuDeadLoop();
    }

    return (VOID*)(ghcbAddress | canonicalizationMask);
}


VOID
GhcbWriteMsr(
    IN  VOID    *Ghcb,
        UINT64  MsrNumber,
        UINT64  RegisterValue
    )
{
    EFI_TPL tpl;

    tpl = GhcbpDisableInterrupts();

    //
    // Initialize the GHCB page to indicate a request to set the specified
    // MSR.
    //

    SetGhcbField64(Ghcb, GHCB_FIELD_VALID_BITMAP0, 0);
    SetGhcbField64(Ghcb, GHCB_FIELD_VALID_BITMAP1, 0);

    SetGhcbField64(Ghcb, GHCB_FIELD64_EXITCODE, GHCB_EXITCODE_MSR);
    SetGhcbField64(Ghcb, GHCB_FIELD64_EXITINFO1, 1);
    SetGhcbField64(Ghcb, GHCB_FIELD64_EXITINFO2, 0);
    SetGhcbField64(Ghcb, GHCB_FIELD64_RCX, MsrNumber);
    SetGhcbField64(Ghcb, GHCB_FIELD64_RAX, (UINT32)RegisterValue);
    SetGhcbField64(Ghcb, GHCB_FIELD64_RDX, (RegisterValue >> 32));

    ((PGHCB_HYPERCALL)Ghcb)->Format = 0;
    ((PGHCB_HYPERCALL)Ghcb)->Version = 1;

    _sev_vmgexit();

    GhcbpEnableInterrupts(tpl);
}


VOID
GhcbReadMsr(
    IN  VOID    *Ghcb,
        UINT64  MsrNumber,
    OUT UINT64  *RegisterValue
    )
{
    EFI_TPL tpl;

    tpl = GhcbpDisableInterrupts();

    //
    // Initialize the GHCB page to indicate a request to get the specified
    // MSR.
    //

    SetGhcbField64(Ghcb, GHCB_FIELD_VALID_BITMAP0, 0);
    SetGhcbField64(Ghcb, GHCB_FIELD_VALID_BITMAP1, 0);

    SetGhcbField64(Ghcb, GHCB_FIELD64_EXITCODE, GHCB_EXITCODE_MSR);
    SetGhcbField64(Ghcb, GHCB_FIELD64_EXITINFO1, 0);
    SetGhcbField64(Ghcb, GHCB_FIELD64_EXITINFO2, 0);
    SetGhcbField64(Ghcb, GHCB_FIELD64_RCX, MsrNumber);

    ((PGHCB_HYPERCALL)Ghcb)->Format = 0;
    ((PGHCB_HYPERCALL)Ghcb)->Version = 1;

    _sev_vmgexit();

    //
    // The value is present in EDX:EAX.
    //

    *RegisterValue = GetGhcbField64(Ghcb, GHCB_FIELD64_RDX) << 32;
    *RegisterValue |= (UINT32)GetGhcbField64(Ghcb, GHCB_FIELD64_RAX);

    GhcbpEnableInterrupts(tpl);
}
