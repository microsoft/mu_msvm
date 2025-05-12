/** @file
  Type definitions for the hypervisor guest interface.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

#define HV_MAXIMUM_PROCESSORS       2048

//
// Memory Types
//
// Guest virtual addresses (GVAs) are used within the guest when it enables address
// translation and provides a valid guest page table.
//
// Guest physical addresses (GPAs) define the guest's view of physical memory.
// GPAs can be mapped to underlying SPAs. There is one guest physical address space per
// partition.
//
typedef UINT64 HV_GVA, *PHV_GVA;

typedef UINT64 HV_GPA, *PHV_GPA;
typedef UINT64 HV_GPA_PAGE_NUMBER, *PHV_GPA_PAGE_NUMBER;

#if defined(MDE_CPU_AARCH64)

#define HV_ARM64_PAGE_SIZE              4096
#define HV_ARM64_LARGE_PAGE_SIZE        0x200000
#define HV_ARM64_LARGE_PAGE_SIZE_1GB    0x40000000
#define HV_PAGE_SIZE                    HV_ARM64_PAGE_SIZE
#define HV_LARGE_PAGE_SIZE              HV_ARM64_LARGE_PAGE_SIZE
#define HV_LARGE_PAGE_SIZE_1GB          HV_ARM64_LARGE_PAGE_SIZE_1GB

#define HV_ARM64_HVC_IMM16              1
#define HV_ARM64_HVC_VTLENTRY_IMM16     2
#define HV_ARM64_HVC_VTLEXIT_IMM16      3
#define HV_ARM64_HVC_LAUNCH_IMM16       4
#define HV_ARM64_HVC_LAUNCH_SL_IMM16    5

//
// The HVC immediate below is handled by the Microvisor
// for GICv3 support in the absence of the full Hypervisor.
//
#define HV_ARM64_ENABLE_SRE             2

//
// Vendor-specific reset types
//
#define HV_ARM64_SYSTEM_RESET2_FIRMWARE_CRASH 0x80000001

#elif defined(MDE_CPU_X64)

#define HV_X64_PAGE_SIZE                4096
#define HV_X64_LARGE_PAGE_SIZE          0x200000
#define HV_X64_LARGE_PAGE_SIZE_1GB      0x40000000
#define HV_PAGE_SIZE                    HV_X64_PAGE_SIZE
#define HV_LARGE_PAGE_SIZE              HV_X64_LARGE_PAGE_SIZE
#define HV_LARGE_PAGE_SIZE_1GB          HV_X64_LARGE_PAGE_SIZE_1GB

#else

#error Unknown/Unsupported architecture

#endif

typedef UINT64 HV_PARTITION_ID, *PHV_PARTITION_ID;

//
// Define invalid partition identifier and "self" partition identifier
//
#define HV_PARTITION_ID_INVALID ((HV_PARTITION_ID) 0x0)
#define HV_PARTITION_ID_SELF    ((HV_PARTITION_ID) -1)

//
// Time in the hypervisor is measured in 100 nanosecond units
//
typedef UINT64 HV_NANO100_TIME,     *PHV_NANO100_TIME;
typedef UINT64 HV_NANO100_DURATION, *PHV_NANO100_DURATION;

#define HV_NANO100_TIME_NEVER ((HV_NANO100_TIME)-1)

typedef UINT32 HV_IOMMU_ID, *PHV_IOMMU_ID;

//
// Define the intercept access types.
//
typedef UINT8 HV_INTERCEPT_ACCESS_TYPE;

//
// Virtual Processor Indices
//
typedef UINT32 HV_VP_INDEX, *PHV_VP_INDEX;

//
// VSM Definitions
//
// Define a virtual trust level (VTL)
//
typedef UINT8 HV_VTL, *PHV_VTL;

//
// Flags to describe the access a partition has to a GPA page.
//
typedef UINT32 HV_MAP_GPA_FLAGS, *PHV_MAP_GPA_FLAGS;

#if defined(MDE_CPU_X64)

typedef struct _HV_X64_SEGMENT_REGISTER
{
    UINT64 Base;
    UINT32 Limit;
    UINT16 Selector;
#pragma warning(disable : 4201)
    union
    {
        struct
        {
            UINT16 SegmentType:4;
            UINT16 NonSystemSegment:1;
            UINT16 DescriptorPrivilegeLevel:2;
            UINT16 Present:1;
            UINT16 Reserved:4;
            UINT16 Available:1;
            UINT16 Long:1;
            UINT16 Default:1;
            UINT16 Granularity:1;
        };
        UINT16 Attributes;
    };
#pragma warning(default : 4201)

} HV_X64_SEGMENT_REGISTER, *PHV_X64_SEGMENT_REGISTER;

typedef struct _HV_X64_TABLE_REGISTER
{
    UINT16     Pad[3];
    UINT16     Limit;
    UINT64     Base;
} HV_X64_TABLE_REGISTER, *PHV_X64_TABLE_REGISTER;

#endif

//
// Initial X64 VP context for a newly enabled VTL
//
typedef struct _HV_INITIAL_VP_CONTEXT
{

#if defined(MDE_CPU_AARCH64)

    UINT64 Pc;
    UINT64 Sp_ELh;
    UINT64 SCTLR_EL1;
    UINT64 MAIR_EL1;
    UINT64 TCR_EL1;
    UINT64 VBAR_EL1;
    UINT64 TTBR0_EL1;
    UINT64 TTBR1_EL1;
    UINT64 X18;

#else

    UINT64 Rip;
    UINT64 Rsp;
    UINT64 Rflags;

    //
    // Segment selector registers together with their hidden state.
    //

    HV_X64_SEGMENT_REGISTER Cs;
    HV_X64_SEGMENT_REGISTER Ds;
    HV_X64_SEGMENT_REGISTER Es;
    HV_X64_SEGMENT_REGISTER Fs;
    HV_X64_SEGMENT_REGISTER Gs;
    HV_X64_SEGMENT_REGISTER Ss;
    HV_X64_SEGMENT_REGISTER Tr;
    HV_X64_SEGMENT_REGISTER Ldtr;

    //
    // Global and Interrupt Descriptor tables
    //

    HV_X64_TABLE_REGISTER Idtr;
    HV_X64_TABLE_REGISTER Gdtr;

    //
    // Control registers and MSR's
    //

    UINT64 Efer;
    UINT64 Cr0;
    UINT64 Cr3;
    UINT64 Cr4;
    UINT64 MsrCrPat;

#endif

} HV_INITIAL_VP_CONTEXT, *PHV_INITIAL_VP_CONTEXT;

//
// HV Map GPA (Guest Physical Address) Flags
//
// Definitions of flags to describe the access a partition has to a GPA page.
// (used with HV_MAP_GPA_FLAGS).
//

//
// The first byte is reserved for permissions.
//
#define HV_MAP_GPA_PERMISSIONS_NONE     0x0
#define HV_MAP_GPA_READABLE             0x1
#define HV_MAP_GPA_WRITABLE             0x2
