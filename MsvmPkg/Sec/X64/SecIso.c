/** @file
  Routines to support hardware isolation of the SEC driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Hv/HvGuestCpuid.h>
#include <Hv/HvGuestMsr.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/CpuLib.h>
#include <Register/Intel/ArchitecturalMsr.h>
#include <BiosInterface.h>
#include <IsolationTypes.h>
#include "SecP.h"

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

#define GHCB_INFO_REGISTER_REQUEST      0x012
#define GHCB_INFO_REGISTER_RESPONSE     0x013

#define GHCB_EXITCODE_MSR               0x7C

#define GHCB_FIELD64_RAX                0x1F8
#define GHCB_FIELD64_RBX                0x318
#define GHCB_FIELD64_RCX                0x308
#define GHCB_FIELD64_RDX                0x310
#define GHCB_FIELD64_EXITCODE           0x390
#define GHCB_FIELD64_EXITINFO1          0x398
#define GHCB_FIELD64_EXITINFO2          0x3A0
#define GHCB_FIELD_VALID_BITMAP0        0x3F0
#define GHCB_FIELD_VALID_BITMAP1        0x3F8
#define GHCB_FIELD16_VERSION            0xFFA
#define GHCB_FIELD32_FORMAT             0xFFC

VOID *Ghcb;
UINT64 TscMultiplier;
UINT64 TscDivisor;
HV_PSP_CPUID_PAGE *CpuidPage;
SEC_CPUID_INFO CpuidInfo;
SEC_CPUID_INFO ExtendedCpuidInfo;

//
// Access to ioports should be restricted in TDX scenario.
//
BOOLEAN FilterIoPortAccesses = TRUE;

BOOLEAN
SecIsPortAccessAllowed(
    UINT16 PortNumber
    )
{
    BOOLEAN retValue = FALSE;
    UINT32 com1Register;
    UINT32 com2Register;
    UINT32 biosPort;

    if (FilterIoPortAccesses)
    {
        //
        // Access is allowed only to COM1, COM2 registers and BIOS ports.
        //
        com1Register = FixedPcdGet32(PcdCom1RegisterBase);
        com2Register = FixedPcdGet32(PcdCom2RegisterBase);
        //
        //
        // Although, Bios ports are enabled for hardware isolated scenarios,
        // the BiosWatchdog is not.
        //
        // biosPort = port for BiosAddress
        // biosPort+4 = port for BiosData
        //
        biosPort = PcdGet32(PcdBiosBaseAddress);

        if ((PortNumber >= com1Register) && (PortNumber < (com1Register + 8)))
        {
            retValue = TRUE;
        }
        else if ((PortNumber >= com2Register) && (PortNumber < (com2Register + 8)))
        {
            retValue = TRUE;
        }
        else if ((PortNumber == (biosPort)) || (PortNumber == (biosPort + 4)))
        {
            retValue = TRUE;
        }
        else
        {
            retValue = FALSE;
        }
    }
    else
    {
        retValue = FALSE;
    }

    return retValue;
}

UINT64
SecReadMsrWithGhcb(
    UINT64 MsrNumber
    )
{
    UINT64 msrValue;

    //
    // Initialize the GHCB page to indicate a request to set the specified
    // MSR.
    //

    SetGhcbField64(Ghcb, GHCB_FIELD_VALID_BITMAP0, 0);
    SetGhcbField64(Ghcb, GHCB_FIELD_VALID_BITMAP1, 0);

    SetGhcbField64(Ghcb, GHCB_FIELD64_EXITCODE, GHCB_EXITCODE_MSR);
    SetGhcbField64(Ghcb, GHCB_FIELD64_EXITINFO1, 0);
    SetGhcbField64(Ghcb, GHCB_FIELD64_EXITINFO2, 0);
    SetGhcbField64(Ghcb, GHCB_FIELD64_RCX, MsrNumber);
    SetGhcbField32(Ghcb, GHCB_FIELD32_FORMAT, 0);
    SetGhcbField16(Ghcb, GHCB_FIELD16_VERSION, 1);

    SecVmgexit();

    msrValue = (UINT32)GetGhcbField64(Ghcb, GHCB_FIELD64_RAX);
    msrValue |= GetGhcbField64(Ghcb, GHCB_FIELD64_RDX) << 32;

    return msrValue;
}

VOID
SecWriteMsrWithGhcb(
    UINT64 MsrNumber,
    UINT64 Value
    )
{
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
    SetGhcbField64(Ghcb, GHCB_FIELD64_RAX, (UINT32)Value);
    SetGhcbField64(Ghcb, GHCB_FIELD64_RDX, Value >> 32);
    SetGhcbField32(Ghcb, GHCB_FIELD32_FORMAT, 0);
    SetGhcbField16(Ghcb, GHCB_FIELD16_VERSION, 1);

    SecVmgexit();
}

VOID
SecInitializeReferenceTime (
    UINT32 ClockFrequency,
    UINT32 TscNumerator,
    UINT32 TscDenominator
    )
{
    //
    // The TSC frequency is (clock * numerator) / (denominator).
    // From a given TSC value, the reference time in 100ns units will be
    // (TSC / TscFrequency) * (100ns-frequency).  This is equivalent to
    // TSC * (denominator * 100ns-frequency) / (clock * numerator).  Since all
    // of these components are 32-bit values, they can be multiplied in pairs
    // to produce a 64-bit multiplier and divisor for a 64-bit MulDiv to
    // calculate reference time from TSC.
    //

    TscMultiplier = (UINT64)TscDenominator * 10000000;
    TscDivisor = ClockFrequency * (UINT64)TscNumerator;
    if (TscDivisor == 0)
    {
        TscDivisor = 1;
    }
}

BOOLEAN
SecInitializeHardwareIsolation (
    UINT32 IsolationType,
    UEFI_IGVM_PARAMETER_INFO *ParameterInfo
    )
{
    UINT32 clockFrequency;
    SEC_CPUID_INFO *cpuidInfo;
    UINT64 ghcbAddress;
    UINT64 ghcbMsr;
    UINT32 gpaWidth;
    HV_GUEST_OS_ID_CONTENTS guestOsId;
    UINT32 index;
    UINT32 leafNumber;
    UINT32 leafType;
    UINT64 sharedGpaBoundary;
    UINT32 tscDenominator;
    UINT32 tscNumerator;

    if (IsolationType == UefiIsolationTypeSnp)
    {
        //
        // Select a GHCB address as the first page before the parameter info
        // block.
        //

        if (mIsolationConfiguration.SharedGpaBoundaryActive)
        {
            sharedGpaBoundary = (1UI64 << mIsolationConfiguration.SharedGpaBoundaryBits);
        }
        else
        {
            sharedGpaBoundary = 0;
        }

        ghcbAddress = (UINTN)ParameterInfo - EFI_PAGE_SIZE + sharedGpaBoundary;

        //
        // Attempt to register the GHCB at the selected address.
        //

        AsmWriteMsr64(MSR_GHCB, ghcbAddress | GHCB_INFO_REGISTER_REQUEST);
        SecVmgexit();
        ghcbMsr = AsmReadMsr64(MSR_GHCB);
        if (ghcbMsr != (ghcbAddress | GHCB_INFO_REGISTER_RESPONSE))
        {
            return FALSE;
        }

        //
        // Configure the GHCB for further use.
        //

        AsmWriteMsr64(MSR_GHCB, ghcbAddress);
        Ghcb = (VOID*)ghcbAddress;

        //
        // Capture the location of CPUID information.
        //

        CpuidPage = (HV_PSP_CPUID_PAGE *)((UINTN)ParameterInfo + ParameterInfo->CpuidPagesOffset * EFI_PAGE_SIZE);

        //
        // Capture the set of CPUID information that is present.
        //

        CpuidInfo.SupportedLeaves |= 1;
        ExtendedCpuidInfo.SupportedLeaves |= 1;

        for (index = 0; index < HV_PSP_CPUID_LEAF_COUNT_MAX; index += 1)
        {
            leafNumber = CpuidPage->CpuidLeafInfo[index].EaxIn & 0x0FFFFFFF;
            leafType = (CpuidPage->CpuidLeafInfo[index].EaxIn >> 28);
            if (leafType == 0)
            {
                cpuidInfo = &CpuidInfo;
            }
            else if (leafType == 8)
            {
                cpuidInfo = &ExtendedCpuidInfo;
            }
            else
            {
                cpuidInfo = NULL;
            }

            if ((cpuidInfo != NULL) && (leafNumber < 0x40))
            {
                if (leafNumber > cpuidInfo->MaximumLeafIndex)
                {
                    cpuidInfo->MaximumLeafIndex = leafNumber;
                }
                cpuidInfo->SupportedLeaves |= (1UI64 << leafNumber);
            }
        }
    }

    if (IsolationType == UefiIsolationTypeTdx)
    {
        //
        // Query the shared GPA boundary from hardware and ensure that it
        // matches the software configuration.
        //

        if (SecGetTdInfo(&gpaWidth) != 0)
        {
            return FALSE;
        }

        if (gpaWidth != mIsolationConfiguration.SharedGpaBoundaryBits + 1)
        {
            return FALSE;
        }
    }

    //
    // Capture the TSC frequency for scaling.
    //

    if (IsolationType == UefiIsolationTypeSnp)
    {
        clockFrequency = (UINT32)SecReadMsrWithGhcb(HvSyntheticMsrTscFrequency);
        tscNumerator = 1;
        tscDenominator = 1;
    }
    else
    {
        AsmCpuid(0x15, &tscDenominator, &tscNumerator, &clockFrequency, NULL);
    }

    SecInitializeReferenceTime(clockFrequency, tscNumerator, tscDenominator);

    //
    // Set the guest OS ID so that hypercalls are possible.
    //

    guestOsId.AsUINT64 = 0;
    guestOsId.BuildNumber = 1;
    guestOsId.ServiceVersion = 1;
    guestOsId.MinorVersion = 1;
    guestOsId.MajorVersion = 1;
    guestOsId.OsId = HvGuestOsMicrosoftUndefined;
    guestOsId.VendorId = HvGuestOsVendorMicrosoft;

    if (IsolationType == UefiIsolationTypeSnp)
    {
        SecWriteMsrWithGhcb(HV_X64_MSR_GUEST_OS_ID, guestOsId.AsUINT64);
    }
    else
    {
        SecTdCallWrmsr(HV_X64_MSR_GUEST_OS_ID, guestOsId.AsUINT64);
    }

    return TRUE;
}

BOOLEAN
SecProcessVirtualMsrRead (
    IN PTRAP_FRAME TrapFrame
    )
{
    UINT64 value;

    DEBUG((DEBUG_VERBOSE, "#VE - MsrRead Index 0x%x\n", TrapFrame->Rcx));

    switch (TrapFrame->Rcx)
    {
    case HvSyntheticMsrTimeRefCount:
        value = MulDiv64(AsmReadTsc(), TscMultiplier, TscDivisor);
        break;

    case HvSyntheticMsrDebugDeviceOptions:

        //
        // This must be read directly from the hypervisor.
        //

        if (Ghcb != NULL)
        {
            value = SecReadMsrWithGhcb(TrapFrame->Rcx);
        }
        else
        {
            value = SecTdCallRdmsr(TrapFrame->Rcx);
        }

        break;

    case HvSyntheticMsrVpIndex:

        //
        // UEFI always runs on the BSP only.
        //

        value = 0;
        break;

    case MSR_IA32_MTRRCAP:

        //
        // CPUID advertises that MTRRs are available, but they are not usable.
        // Advertise that there are zero variable MTRRs and no fixed MTRRs to
        // prevent their use.
        //

        value = 0;
        break;

    case MSR_IA32_APIC_BASE:

        //
        // This register is queried to determine APIC mode. Always return the following:
        // 1. BSP (0x100)
        // 2. X2APIC mode (0x400)
        // 3. Global Enabled (0x800)
        //

        value = 0xD00;
        break;

    default:
        return FALSE;
    }

    TrapFrame->Rax = (UINT32)value;
    TrapFrame->Rdx = value >> 32;
    return TRUE;
}

BOOLEAN
SecProcessVirtualMsrWrite (
    IN PTRAP_FRAME TrapFrame
    )
{
    UINT64 value;

    value = (TrapFrame->Rdx << 32) | (UINT32)TrapFrame->Rax;
    DEBUG((DEBUG_VERBOSE,
           "#VE - MsrWrite Index 0x%x, value 0x%lx\n",
           TrapFrame->Rcx,
           value));

    switch (TrapFrame->Rcx)
    {
    case MSR_IA32_EFER:

        //
        // EFER cannot be modified if direct MSR writes cause an intercept, so
        // simply verify that the requested change has no effect.  If the
        // value being written is the current EFER value, then ignore the
        // write.
        //

        if (value == AsmReadMsr64(MSR_IA32_EFER))
        {
            return TRUE;
        }
        return FALSE;

    default:
        return FALSE;
    }
}

BOOLEAN
SecProcessVirtualCpuid (
    IN PTRAP_FRAME TrapFrame
    )
{
    SEC_CPUID_INFO *cpuidInfo;
    HV_CPUID_RESULT cpuidResult;
    UINT32 index;
    UINT32 leaf;
    UINT32 leafNumber;
    BOOLEAN matchEcx;

    DEBUG((DEBUG_VERBOSE,
           "#VE - CPUID leaf 0x%x subleaf 0x%x\n",
           (UINT32)TrapFrame->Rax,
           (UINT32)TrapFrame->Rcx));

    ZeroMem(&cpuidResult, sizeof(HV_CPUID_RESULT));

    //
    // Only support architectural and hypervisor CPUID leaves.
    //

    matchEcx = FALSE;
    leaf = (UINT32)TrapFrame->Rax;
    switch (leaf >> 28)
    {
    case 0:

        //
        // Determine whether this CPUID leaf has any sub-leaves.
        //

        if ((0x890 & (1 << TrapFrame->Rax)) != 0)
        {
            matchEcx = TRUE;
        }

        cpuidInfo = &CpuidInfo;
        break;

    case 4:
        cpuidInfo = NULL;
        break;

    case 8:
        cpuidInfo = &ExtendedCpuidInfo;
        break;

    default:
        return FALSE;
    }

    if ((cpuidInfo != NULL) && (CpuidPage != NULL))
    {
        //
        // See if the requested leaf can be found in the table.  If not, then fail.
        //

        leafNumber = leaf & 0x0FFFFFFF;
        if ((cpuidInfo == NULL) ||
            (leafNumber > cpuidInfo->MaximumLeafIndex) ||
            ((cpuidInfo->SupportedLeaves & (1UI64 << leafNumber)) == 0))
        {
            return FALSE;
        }

        for (index = 0; index < HV_PSP_CPUID_LEAF_COUNT_MAX; index += 1)
        {
            if (leaf != CpuidPage->CpuidLeafInfo[index].EaxIn)
            {
                continue;
            }

            if (!matchEcx || (TrapFrame->Rcx == CpuidPage->CpuidLeafInfo[index].EcxIn))
            {
                cpuidResult.Eax = CpuidPage->CpuidLeafInfo[index].EaxOut;
                cpuidResult.Ebx = CpuidPage->CpuidLeafInfo[index].EbxOut;
                cpuidResult.Ecx = CpuidPage->CpuidLeafInfo[index].EcxOut;
                cpuidResult.Edx = CpuidPage->CpuidLeafInfo[index].EdxOut;
                break;
            }
        }
    }

    //
    // Customize output as required, including for hypervisor leaves.
    //

    switch (leaf)
    {
    case 0:
    case 0x80000000:

        if (CpuidPage != NULL)
        {
            //
            // These leaves are not normally present in the table, so the value
            // must be calculated here.  Since SNP is the only platform that
            // uses a CPUID table, the AMD value can be inserted here.
            //

            cpuidResult.Eax = cpuidInfo->MaximumLeafIndex | (leaf & 0x80000000);
            cpuidResult.Ebx = 'htuA';
            cpuidResult.Edx = 'itne';
            cpuidResult.Ecx = 'DMAc';
        }
        break;

    case 1:

        //
        // Indicate the presence of a hypervisor.
        //

        cpuidResult.Ecx |= 0x80000000;
        break;

    case HvCpuIdFunctionHvVendorAndMaxFunction:
        cpuidResult.HvVendorAndMaxFunction.MaxFunction = HvCpuidFunctionMsHvIsolationConfiguration;
        CopyMem(cpuidResult.HvVendorAndMaxFunction.VendorName,
                "Microsoft Hv",
                sizeof(cpuidResult.HvVendorAndMaxFunction.VendorName));
        break;

    case HvCpuIdFunctionHvInterface:
        cpuidResult.HvInterface.Interface = '1#vH';
        break;

    case HvCpuIdFunctionMsHvFeatures:
        cpuidResult.MsHvFeatures.PartitionPrivileges.Isolation = 1;
        cpuidResult.MsHvFeatures.PartitionPrivileges.AccessPartitionReferenceCounter = 1;
        cpuidResult.MsHvFeatures.PartitionPrivileges.AccessSynicRegs = 1;
        cpuidResult.MsHvFeatures.PartitionPrivileges.AccessSyntheticTimerRegs = 1;
        cpuidResult.MsHvFeatures.PartitionPrivileges.AccessIntrCtrlRegs = 1;
        cpuidResult.MsHvFeatures.PartitionPrivileges.AccessHypercallMsrs = 1;
        cpuidResult.MsHvFeatures.PartitionPrivileges.AccessVpIndex = 1;
        cpuidResult.MsHvFeatures.DirectSyntheticTimers = 1;
        cpuidResult.MsHvFeatures.DebugRegsAvailable = 1;
        break;

    case HvCpuidFunctionMsHvIsolationConfiguration:
        cpuidResult.MsHvIsolationConfiguration = mIsolationConfiguration;
        break;

    default:

        //
        // Fail on any unhandled hypervisor leaves.
        //

        if (cpuidInfo == NULL)
        {
            return FALSE;
        }
    }

    TrapFrame->Rax = cpuidResult.Eax;
    TrapFrame->Rbx = cpuidResult.Ebx;
    TrapFrame->Rcx = cpuidResult.Ecx;
    TrapFrame->Rdx = cpuidResult.Edx;

    return TRUE;
}


BOOLEAN
SecProcessHlt(
    IN PTRAP_FRAME TrapFrame
    )
{
    //
    // TDX only. This is an automatic exit on SNP.
    //

    SecTdCallHlt();

    return TRUE;
}

BOOLEAN
SecProcessIoPortRead(
    IN  PTRAP_FRAME TrapFrame,
        UINT16 PortNumber,
        UINT32 AccessSize
    )
{
    UINT64 mask;
    UINT64 rax;
    UINT32 value;

    //
    // Currently TDX-only.
    //

    if (Ghcb != NULL)
    {
        return FALSE;
    }

    if (!SecIsPortAccessAllowed(PortNumber))
    {
        return FALSE;
    }

    value = SecTdCallReadIoPort(PortNumber, AccessSize);
    rax = TrapFrame->Rax;
    mask = ((1UI64 << (AccessSize * 8)) - 1);
    rax = (rax & ~mask) | (value & mask);
    if (AccessSize == 4)
    {
        rax = (UINT32)rax;
    }

    TrapFrame->Rax = rax;
    return TRUE;
}

BOOLEAN
SecProcessIoPortWrite(
    IN  PTRAP_FRAME TrapFrame,
        UINT16 PortNumber,
        UINT32 AccessSize
    )
{
    UINT64 mask;
    UINT32 value;

    //
    // Currently TDX-only.
    //

    if (Ghcb != NULL)
    {
        return FALSE;
    }

    if (!SecIsPortAccessAllowed(PortNumber))
    {
        return FALSE;
    }

    mask = ((1UI64 << (AccessSize * 8)) - 1);
    value =  (UINT32)(TrapFrame->Rax & mask);
    SecTdCallWriteIoPort(PortNumber, AccessSize, value);

    return TRUE;
}

BOOLEAN
SecProcessVirtualCommunicationException (
    IN PTRAP_FRAME TrapFrame
    )
{
    UINT32 InstructionLength;

    switch (TrapFrame->ErrorCode)
    {
    case VC_EXIT_CODE_MSR:

        //
        // Examine the instruction to determine whether it is a read or write.
        //

        if (*(UINT8*)(TrapFrame->Rip + 1) == 0x30)
        {
            //
            // WRMSR.
            //

            return FALSE;
        }

        if (!SecProcessVirtualMsrRead(TrapFrame))
        {
            return FALSE;
        }

        InstructionLength = 2;
        break;

    case VC_EXIT_CODE_CPUID:
        if (!SecProcessVirtualCpuid(TrapFrame))
        {
            return FALSE;
        }

        InstructionLength = 2;
        break;

    default:
        return FALSE;
    }

    TrapFrame->Rip += InstructionLength;

    return TRUE;
}

BOOLEAN
SecProcessVirtualizationException (
    IN PTRAP_FRAME TrapFrame
    )
{
    UINT32 accessSize;
    TDX_VE_EXIT_QUALIFICATION_IO ioQual;
    TDX_VE_INFO veInfo;

    //
    // Attempt to obtain the #VE information.  If this is not possible, then
    // the exception cannot be handled.
    //

    if (SecGetTdxVeInfo(&veInfo) < 0)
    {
        DEBUG((DEBUG_VERBOSE, "#VE - Unable to obtain VEInfo\n"));
        goto FailVe;
    }

    //
    // Handle the intercept if possible.
    //

    switch (veInfo.ExitReason)
    {
    case VE_EXIT_CODE_RDMSR:
        if (!SecProcessVirtualMsrRead(TrapFrame))
        {
            goto FailVe;
        }
        break;

    case VE_EXIT_CODE_WRMSR:
        if (!SecProcessVirtualMsrWrite(TrapFrame))
        {
            goto FailVe;
        }
        break;

    case VE_EXIT_CODE_CPUID:
        if (!SecProcessVirtualCpuid(TrapFrame))
        {
            goto FailVe;
        }
        break;

    case VE_EXIT_CODE_HLT:
        if (!SecProcessHlt(TrapFrame))
        {
            goto FailVe;
        }
        break;

    case VE_EXIT_CODE_IO:
        ioQual.AsUINT64 = veInfo.ExitQualification;
        if (ioQual.String)
        {
            goto FailVe;
        }

        accessSize = ioQual.AccessSize + 1;
        if (ioQual.In)
        {
            if (!SecProcessIoPortRead(TrapFrame, (UINT16)ioQual.PortNumber, accessSize))
            {
                goto FailVe;
            }
        }
        else
        {
            if (!SecProcessIoPortWrite(TrapFrame, (UINT16)ioQual.PortNumber, accessSize))
            {
                goto FailVe;
            }
        }
        break;

    default:
        DEBUG((DEBUG_VERBOSE, "#VE - Unknown exit reason 0x%x\n", veInfo.ExitReason));
        goto FailVe;
    }

    TrapFrame->Rip += veInfo.InstructionLength;

    return TRUE;

FailVe:

    DEBUG((DEBUG_VERBOSE, "#VE - Handling failed\n"));

    return FALSE;
}
