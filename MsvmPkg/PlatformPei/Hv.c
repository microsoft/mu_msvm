/** @file
  Hypervisor interactions during PEI.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>

#include <Platform.h>
#include <Hv.h>
#include <IsolationTypes.h>

#include <Hv/HvGuestCpuid.h>
#include <Library/DebugLib.h>
#include <Library/CrashDumpAgentLib.h>

BOOLEAN mParavisorPresent = FALSE;
UINT32 mIsolationType = UefiIsolationTypeNone;
UINT32 mSharedGpaBit = 0;

VOID
HvDetectIsolation(
    VOID
    )
/*++

Routine Description:

    Determines whether UEFI is running in an isolated VM.

Arguments:

    None.

Return Value:

    None.

--*/
{
#if defined(MDE_CPU_X64)

    HV_CPUID_RESULT cpuidResult;
    UINT64 sharedGpaBoundary;
    UINT64 sharedGpaCanonicalizationBitmask;
    EFI_STATUS status = EFI_SUCCESS;
    UINT32 virtualAddressBits;

    __cpuid(cpuidResult.AsUINT32, HvCpuIdFunctionVersionAndFeatures);
    if (!cpuidResult.VersionAndFeatures.HypervisorPresent)
    {
        DEBUG((DEBUG_INFO, __FUNCTION__" - Hypervisor is not present \n"));
        return;
    }

    __cpuid(cpuidResult.AsUINT32, HvCpuIdFunctionHvInterface);
    if (cpuidResult.HvInterface.Interface != HvMicrosoftHypervisorInterface)
    {
        DEBUG((DEBUG_INFO, __FUNCTION__" - Hypervisor interface is not present \n"));
        return;
    }

    __cpuid(cpuidResult.AsUINT32, HvCpuIdFunctionMsHvFeatures);
    if (!cpuidResult.MsHvFeatures.PartitionPrivileges.Isolation)
    {
        DEBUG((DEBUG_INFO, __FUNCTION__" - Isolation is not present \n"));
        return;
    }

    __cpuid(cpuidResult.AsUINT32, HvCpuidFunctionMsHvIsolationConfiguration);
    switch (cpuidResult.MsHvIsolationConfiguration.IsolationType)
    {
    case HV_PARTITION_ISOLATION_TYPE_VBS:
        static_assert(HV_PARTITION_ISOLATION_TYPE_VBS == UefiIsolationTypeVbs);
        mIsolationType = UefiIsolationTypeVbs;
        break;
    case HV_PARTITION_ISOLATION_TYPE_SNP:
        static_assert(HV_PARTITION_ISOLATION_TYPE_SNP == UefiIsolationTypeSnp);
        mIsolationType = UefiIsolationTypeSnp;
        break;
    case HV_PARTITION_ISOLATION_TYPE_TDX:
        static_assert(HV_PARTITION_ISOLATION_TYPE_TDX == UefiIsolationTypeTdx);
        mIsolationType = UefiIsolationTypeTdx;
        break;
    case HV_PARTITION_ISOLATION_TYPE_NONE:
        static_assert(HV_PARTITION_ISOLATION_TYPE_NONE == UefiIsolationTypeNone);
        return;
    default:
        ASSERT(FALSE);
        return;
    }

    status = PcdSet32S(PcdIsolationArchitecture, mIsolationType);
    if (EFI_ERROR(status))
    {
        DEBUG((DEBUG_ERROR, "Failed to set the PCD PcdIsolationArchitecture::0x%x \n", status));
        PEI_FAIL_FAST_IF_FAILED(status);
    }

    if (cpuidResult.MsHvIsolationConfiguration.ParavisorPresent)
    {
        mParavisorPresent = TRUE;
        status = PcdSetBoolS(PcdIsolationParavisorPresent, TRUE);
        if (EFI_ERROR(status))
        {
            DEBUG((DEBUG_ERROR, "Failed to set the PCD PcdIsolationParavisorPresent::0x%x \n", status));
            PEI_FAIL_FAST_IF_FAILED(status);
        }
    }

    if (cpuidResult.MsHvIsolationConfiguration.SharedGpaBoundaryActive)
    {
        mSharedGpaBit = cpuidResult.MsHvIsolationConfiguration.SharedGpaBoundaryBits;
        sharedGpaBoundary = 1UI64 << mSharedGpaBit;
        sharedGpaCanonicalizationBitmask = 0;
        virtualAddressBits = 48;
        if (cpuidResult.MsHvIsolationConfiguration.SharedGpaBoundaryBits == (virtualAddressBits - 1))
        {
            sharedGpaCanonicalizationBitmask = ~((1UI64 << virtualAddressBits) - 1);
        }
        else if (cpuidResult.MsHvIsolationConfiguration.SharedGpaBoundaryBits > (virtualAddressBits - 1))
        {
            FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
        }

        DEBUG((DEBUG_VERBOSE,
               "%a: SharedGpaBoundary: 0x%lx, CanonicalizationMask 0x%lx\n",
               __FUNCTION__,
               sharedGpaBoundary,
               sharedGpaCanonicalizationBitmask));

        status = PcdSet64S(PcdIsolationSharedGpaBoundary, sharedGpaBoundary);
        if (!EFI_ERROR(status))
        {
            status = PcdSet64S(PcdIsolationSharedGpaCanonicalizationBitmask,
                               sharedGpaCanonicalizationBitmask);
        }

        if (EFI_ERROR(status))
        {
            DEBUG((DEBUG_ERROR, "Failed to set the PCD PcdIsolationSharedGpaBoundary::0x%x \n", status));
            PEI_FAIL_FAST_IF_FAILED(status);
        }
    }

#endif
    return;
}

VOID
HvDetectSvsm(
    IN  PSNP_SECRETS    SecretsPage,
    OUT UINT64          *SvsmBase,
    OUT UINT64          *SvsmSize
    )
/*++

Routine Description:

    Determines whether an SVSM is present.

Arguments:

    SecretsPage - A pointer to the SNP secrets page, if this is a no-paravisor
                  SNP system.

    SvsmBase - Receives the base of the SVSM area.

    SvsmSize - Receives the size of the SVSM area.

Return Value:

    None.

--*/
{
    EFI_STATUS status;

    //
    // Examine the secrets page to determine whether any SVSM has declared its
    // presence.
    //

    if (SecretsPage->SvsmSize != 0)
    {
        *SvsmBase = SecretsPage->SvsmBase;
        *SvsmSize = SecretsPage->SvsmSize;
        status = PcdSet64S(PcdSvsmCallingArea, SecretsPage->SvsmCallingArea);
        if (EFI_ERROR(status))
        {
            DEBUG((DEBUG_ERROR, "Failed to set the SVSM calling area address::0x%x \n", status));
            PEI_FAIL_FAST_IF_FAILED(status);
        }
    }
    else
    {
        *SvsmBase = 0;
        *SvsmSize = 0;
    }
}
