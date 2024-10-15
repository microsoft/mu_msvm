/** @file
  Definitions relating to X64 version of the SEC driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

extern HV_HYPERVISOR_ISOLATION_CONFIGURATION mIsolationConfiguration;

typedef struct _TRAP_FRAME {
    UINT64 P1;
    UINT64 P2;
    UINT64 P3;
    UINT64 P4;
    UINT64 XmmRegisters[12];
    UINT64 Rax;
    UINT64 Rcx;
    UINT64 Rdx;
    UINT64 Rbx;
    UINT64 R8;
    UINT64 R9;
    UINT64 R10;
    UINT64 R11;
    UINT64 ErrorCode;
    UINT64 Rip;
    UINT64 SegCs;
    UINT64 Rflags;
    UINT64 Rsp;
    UINT64 SegSs;
} TRAP_FRAME, *PTRAP_FRAME;

BOOLEAN
SecInitializeHardwareIsolation (
        UINT32 IsolationType,
    IN  UEFI_IGVM_PARAMETER_INFO *ParameterInfo
    );

#define MSR_GHCB        0xC0010130

VOID
SecVirtualCommunicationExceptionHandler (
    VOID
    );

#define VC_EXIT_CODE_CPUID      0x72
#define VC_EXIT_CODE_MSR        0x7C

VOID
SecVmgexit (
    VOID
    );

typedef struct _HV_PSP_CPUID_LEAF
{
    UINT32 EaxIn;
    UINT32 EcxIn;
    UINT64 XfemIn;
    UINT64 XssIn;
    UINT32 EaxOut;
    UINT32 EbxOut;
    UINT32 EcxOut;
    UINT32 EdxOut;
    UINT64 ReservedZ;
} HV_PSP_CPUID_LEAF, *PHV_PSP_CPUID_LEAF;

#define HV_PSP_CPUID_LEAF_COUNT_MAX     64

typedef struct _HV_PSP_CPUID_PAGE
{
    UINT32 Count;
    UINT32 ReservedZ1;
    UINT64 ReservedZ2;
    HV_PSP_CPUID_LEAF CpuidLeafInfo[HV_PSP_CPUID_LEAF_COUNT_MAX];
} HV_PSP_CPUID_PAGE, *PHV_PSP_CPUID_PAGE;


typedef struct _SEC_CPUID_INFO
{
    UINT64 SupportedLeaves;
    UINT32 MaximumLeafIndex;
} SEC_CPUID_INFO;

VOID
SecVirtualizationExceptionHandler (
    VOID
    );

typedef struct _TDX_VE_INFO {
    UINT32 ExitReason;
    UINT32 Valid;
    UINT64 ExitQualification;
    UINT64 GuestLinearAddress;
    UINT64 GuestPhysicalAddress;
    UINT32 InstructionLength;
    UINT32 InstructionInfo;
} TDX_VE_INFO, *PTDX_VE_INFO;

#define VE_EXIT_CODE_CPUID      10
#define VE_EXIT_CODE_HLT        12
#define VE_EXIT_CODE_IO         30
#define VE_EXIT_CODE_RDMSR      31
#define VE_EXIT_CODE_WRMSR      32

//
// VM Exit qualification for IO instructions and IO SMIs.
//

typedef union _TDX_VE_EXIT_QUALIFICATION_IO
{
    UINT64 AsUINT64;
    UINT32 AsUINT32;

#pragma warning(disable : 4201)
    struct
    {
        UINT32 AccessSize:2;
        UINT32 Reserved1:1;
        UINT32 In:1;
        UINT32 String:1;
        UINT32 RepPrefix:1;
        UINT32 ImmediateOperand:1;
        UINT32 Reserved2:9;
        UINT32 PortNumber:16;
    };
#pragma warning(default : 4201)

} TDX_VE_EXIT_QUALIFICATION_IO, *PTDX_VE_EXIT_QUALIFICATION_IO;

long long
SecGetTdxVeInfo(
    OUT PTDX_VE_INFO VeInfo
    );

long long
SecGetTdInfo(
    OUT UINT32 *GpaWidth
    );

UINT64
SecTdCallRdmsr(
    UINT64 MsrNumber
    );

VOID
SecTdCallWrmsr(
    UINT64 MsrNumber,
    UINT64 MsrValue
    );

VOID
SecTdCallHlt(
    VOID
    );

UINT32
SecTdCallReadIoPort(
    UINT32 PortNumber,
    UINT32 AccessSize
    );

VOID
SecTdCallWriteIoPort(
    UINT32 PortNumber,
    UINT32 AccessSize,
    UINT32 Value
    );

UINT64
MulDiv64 (
    UINT64 Value,
    UINT64 Multiplier,
    UINT64 Divisor
    );
