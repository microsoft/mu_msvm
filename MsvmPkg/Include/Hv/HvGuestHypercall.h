/** @file
  This file declares public structures for the Hypercall component of the
  hypervisor for the guest interface.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

#include <Hv/HvGuest.h>

#pragma warning(disable : 4201)

//
// Define a 128bit type.
//
typedef union __declspec(align(16)) _HV_UINT128
{
    struct
    {
        UINT64  Low64;
        UINT64  High64;
    };

    UINT32  Dword[4];

    UINT8 AsUINT8[16];
} HV_UINT128, *PHV_UINT128;

//
// Define an alignment for structures passed via hypercall.
//
#define HV_CALL_ALIGNMENT   8
#define HV_CALL_ATTRIBUTES __declspec(align(HV_CALL_ALIGNMENT))

//
// Address spaces presented by the guest.
//
typedef UINT64 HV_ADDRESS_SPACE_ID, *PHV_ADDRESS_SPACE_ID;

//
// Definition of the HvCallSwitchVirtualAddressSpace hypercall input
// structure.  This call switches the guest's virtual address space.
//
typedef struct HV_CALL_ATTRIBUTES _HV_INPUT_SWITCH_VIRTUAL_ADDRESS_SPACE
{
    HV_ADDRESS_SPACE_ID AddressSpace;
} HV_INPUT_SWITCH_VIRTUAL_ADDRESS_SPACE, *PHV_INPUT_SWITCH_VIRTUAL_ADDRESS_SPACE;

//
// Define connection identifier type.
//
typedef union _HV_CONNECTION_ID
{
    UINT32 AsUINT32;

    struct
    {
        UINT32 Id:24;
        UINT32 Reserved:6;
        UINT32 Scope:2;
    };

} HV_CONNECTION_ID, *PHV_CONNECTION_ID;

//
// Declare the various hypercall operations.
//
typedef enum _HV_CALL_CODE
{
    //
    // Reserved Feature Code
    //

    HvCallReserved0000                  = 0x0000,

    //
    // V1 Address space enlightment IDs
    //

    HvCallSwitchVirtualAddressSpace     = 0x0001,
    HvCallFlushVirtualAddressSpace      = 0x0002,
    HvCallFlushVirtualAddressList       = 0x0003,

    //
    // V1 Power Management and Run time metrics IDs
    //

    HvCallGetLogicalProcessorRunTime    = 0x0004,

    //
    // Deprectated index, now repurposed for updating processor features.
    //

    HvCallUpdateHvProcessorFeatures     = 0x0005,

    //
    //  Deprecated index, now repurposed for switching to and from alias map.
    //

    HvCallSwitchAliasMap                = 0x0006,

    //
    // Deprecated index, now repurposed for dynamic microcode updates.
    //
    HvCallUpdateMicrocode               = 0x0007,

    //
    // V1 Spinwait enlightenment IDs
    //

    HvCallNotifyLongSpinWait            = 0x0008,

    //
    // V2 Core parking IDs.
    // Reused Hypercall code: Previously was
    // HvCallParkLogicalProcessors.
    //

    HvCallParkedVirtualProcessors       = 0x0009,

    //
    // V2 Invoke Hypervisor debugger
    //

    HvCallInvokeHypervisorDebugger      = 0x000a,

    //
    // V4 Send Synthetic Cluster Ipi
    //

    HvCallSendSyntheticClusterIpi       = 0x000b,

    //
    // V5 Virtual Secure Mode (VSM) hypercalls.
    //

    HvCallModifyVtlProtectionMask       = 0x000c,
    HvCallEnablePartitionVtl            = 0x000d,
    HvCallDisablePartitionVtl           = 0x000e,
    HvCallEnableVpVtl                   = 0x000f,
    HvCallDisableVpVtl                  = 0x0010,
    HvCallVtlCall                       = 0x0011,
    HvCallVtlReturn                     = 0x0012,

    //
    // V5 Extended Flush Routines and Cluster IPIs.
    //

    HvCallFlushVirtualAddressSpaceEx    = 0x0013,
    HvCallFlushVirtualAddressListEx     = 0x0014,
    HvCallSendSyntheticClusterIpiEx     = 0x0015,

    //
    // V1 enlightenment name space reservation.
    //

    HvCallQueryImageInfo                = 0x0016,
    HvCallMapImagePages                 = 0x0017,
    HvCallCommitPatch                   = 0x0018,
    HvCallReserved0019                  = 0x0019,
    HvCallReserved001a                  = 0x001a,
    HvCallReserved001b                  = 0x001b,
    HvCallReserved001c                  = 0x001c,
    HvCallReserved001d                  = 0x001d,
    HvCallReserved001e                  = 0x001e,
    HvCallReserved001f                  = 0x001f,
    HvCallReserved0020                  = 0x0020,
    HvCallReserved0021                  = 0x0021,
    HvCallReserved0022                  = 0x0022,
    HvCallReserved0023                  = 0x0023,
    HvCallReserved0024                  = 0x0024,
    HvCallReserved0025                  = 0x0025,
    HvCallReserved0026                  = 0x0026,
    HvCallReserved0027                  = 0x0027,
    HvCallReserved0028                  = 0x0028,
    HvCallReserved0029                  = 0x0029,
    HvCallReserved002a                  = 0x002a,
    HvCallReserved002b                  = 0x002b,
    HvCallReserved002c                  = 0x002c,
    HvCallReserved002d                  = 0x002d,
    HvCallReserved002e                  = 0x002e,
    HvCallReserved002f                  = 0x002f,
    HvCallReserved0030                  = 0x0030,
    HvCallReserved0031                  = 0x0031,
    HvCallReserved0032                  = 0x0032,
    HvCallReserved0033                  = 0x0033,
    HvCallReserved0034                  = 0x0034,
    HvCallReserved0035                  = 0x0035,
    HvCallReserved0036                  = 0x0036,
    HvCallReserved0037                  = 0x0037,
    HvCallReserved0038                  = 0x0038,
    HvCallReserved0039                  = 0x0039,
    HvCallReserved003a                  = 0x003a,
    HvCallReserved003b                  = 0x003b,
    HvCallReserved003c                  = 0x003c,
    HvCallReserved003d                  = 0x003d,
    HvCallReserved003e                  = 0x003e,
    HvCallReserved003f                  = 0x003f,

    //
    // V1 Partition Management IDs
    //

    HvCallCreatePartition               = 0x0040,
    HvCallInitializePartition           = 0x0041,
    HvCallFinalizePartition             = 0x0042,
    HvCallDeletePartition               = 0x0043,
    HvCallGetPartitionProperty          = 0x0044,
    HvCallSetPartitionProperty          = 0x0045,
    HvCallGetPartitionId                = 0x0046,
    HvCallGetNextChildPartition         = 0x0047,

    //
    // V1 Resource Management IDs
    //

    HvCallDepositMemory                 = 0x0048,
    HvCallWithdrawMemory                = 0x0049,
    HvCallGetMemoryBalance              = 0x004a,

    //
    // V1 Guest Physical Address Space Management IDs
    //

    HvCallMapGpaPages                   = 0x004b,
    HvCallUnmapGpaPages                 = 0x004c,

    //
    // V1 Intercept Management IDs
    //

    HvCallInstallIntercept              = 0x004d,

    //
    // V1 Virtual Processor Management IDs
    //

    HvCallCreateVp                      = 0x004e,
    HvCallDeleteVp                      = 0x004f,
    HvCallGetVpRegisters                = 0x0050,
    HvCallSetVpRegisters                = 0x0051,

    //
    // V1 Virtual TLB IDs
    //

    HvCallTranslateVirtualAddress       = 0x0052,
    HvCallReadGpa                       = 0x0053,
    HvCallWriteGpa                      = 0x0054,

    //
    // V1 Interrupt Management IDs
    //

    HvCallAssertVirtualInterruptDeprecated = 0x0055,
    HvCallClearVirtualInterrupt            = 0x0056,

    //
    // V1 Port IDs
    //

    HvCallCreatePortDeprecated          = 0x0057,
    HvCallDeletePort                    = 0x0058,
    HvCallConnectPortDeprecated         = 0x0059,
    HvCallGetPortProperty               = 0x005a,
    HvCallDisconnectPort                = 0x005b,
    HvCallPostMessage                   = 0x005c,
    HvCallSignalEvent                   = 0x005d,

    //
    // V1 Partition State IDs
    //

    HvCallSavePartitionState            = 0x005e,
    HvCallRestorePartitionState         = 0x005f,

    //
    // V1 Trace IDs
    //

    HvCallInitializeEventLogBufferGroup = 0x0060,
    HvCallFinalizeEventLogBufferGroup   = 0x0061,
    HvCallCreateEventLogBuffer          = 0x0062,
    HvCallDeleteEventLogBuffer          = 0x0063,
    HvCallMapEventLogBuffer             = 0x0064,
    HvCallUnmapEventLogBuffer           = 0x0065,
    HvCallSetEventLogGroupSources       = 0x0066,
    HvCallReleaseEventLogBuffer         = 0x0067,
    HvCallFlushEventLogBuffer           = 0x0068,

    //
    // V1 Dbg Call IDs
    //

    HvCallPostDebugData                 = 0x0069,
    HvCallRetrieveDebugData             = 0x006a,
    HvCallResetDebugSession             = 0x006b,

    //
    // V1 Stats IDs
    //

    HvCallMapStatsPage                  = 0x006c,
    HvCallUnmapStatsPage                = 0x006d,

    //
    // V2 Guest Physical Address Space Management IDs
    //

    HvCallMapSparseGpaPages             = 0x006e,

    //
    // V2 Set System Property
    //

    HvCallSetSystemProperty             = 0x006f,

    //
    // V2 Port Ids.
    //

    HvCallSetPortProperty               = 0x0070,

    //
    // V2 Test IDs
    //

    HvCallOutputDebugCharacter          = 0x0071,
    HvCallEchoIncrement                 = 0x0072,

    //
    // V2 Performance IDs
    //

    HvCallPerfNop                       = 0x0073,
    HvCallPerfNopInput                  = 0x0074,
    HvCallPerfNopOutput                 = 0x0075,

    //
    // V3 Logical Processor Management IDs
    //

    HvCallAddLogicalProcessor           = 0x0076,
    HvCallRemoveLogicalProcessor        = 0x0077,

    HvCallQueryNumaDistance             = 0x0078,

    HvCallSetLogicalProcessorProperty   = 0x0079,
    HvCallGetLogicalProcessorProperty   = 0x007a,

    //
    // V3 Get System Property
    //

    HvCallGetSystemProperty             = 0x007b,

    //
    // V3 IOMMU hypercalls
    //

    HvCallMapDeviceInterrupt            = 0x007c,
    HvCallUnmapDeviceInterrupt          = 0x007d,

    //
    // V5 IOMMU hypercalls
    //

    HvCallRetargetDeviceInterrupt       = 0x007e,
    HvCallRetargetRootDeviceInterrupt   = 0x007f,

    //
    // V6 IOMMU hypercalls
    //

    HvCallAssertDeviceInterrupt         = 0x0080,

    //
    // V3 IOMMU hypercalls
    //

    HvCallReserved0081                  = 0x0081,
    HvCallAttachDevice                  = 0x0082,
    HvCallDetachDevice                  = 0x0083,

    //
    // V3 Sleep state transition hypercall
    //

    HvCallEnterSleepState              = 0x0084,
    HvCallNotifyStandbyTransition      = 0x0085,
    HvCallPrepareForHibernate          = 0x0086,

    HvCallNotifyPartitionEvent         = 0x0087,

    //
    // V3 Logical Processor State IDs
    //

    HvCallGetLogicalProcessorRegisters  = 0x0088,
    HvCallSetLogicalProcessorRegisters  = 0x0089,

    //
    // V3 MCA specific Hypercall IDs
    //

    HvCallQueryAssociatedLpsForMca      = 0x008A,

    //
    // V3 Event ring flush.
    //

    HvCallNotifyPortRingEmpty           = 0x008B,

    //
    // V3 Synthetic machine check injection
    //

    HvCallInjectSyntheticMachineCheck   = 0x008C,

    //
    // V4 Parition Management IDs
    //

    HvCallScrubPartition                = 0x008D,

    //
    // V4 Debugger and livedump hypercalls.
    //

    HvCallCollectLivedump               = 0x008E,

    //
    // V4 Turn off virtualization.
    //

    HvCallDisableHypervisor             = 0x008F,

    //
    // V4 Guest Physical Address Space Management IDs
    //

    HvCallModifySparseGpaPages          = 0x0090,

    //
    // V4 Intercept result registration hypercalls.
    //

    HvCallRegisterInterceptResult       = 0x0091,
    HvCallUnregisterInterceptResult     = 0x0092,

    //
    // V5 Test Only Coverage Hypercall
    //
    HvCallGetCoverageData               = 0x0093,

    //
    // V5 Synic Hypercalls (slightly extended from V1)
    //

    HvCallAssertVirtualInterrupt        = 0x0094,
    HvCallCreatePort                    = 0x0095,
    HvCallConnectPort                   = 0x0096,

    //
    // V5 Resource Management Hypercalls.
    //

    HvCallGetSpaPageList                = 0x0097,

    //
    // V5 ARM64 Startup Stub interface
    //

    HvCallArm64GetStartStub             = 0x0098,

    //
    // V5 VP Startup Enlightment
    //

    HvCallStartVirtualProcessor         = 0x0099,

    //
    // V5 VP Index retrieval for VP startup
    //

    HvCallGetVpIndexFromApicId          = 0x009A,

    //
    // V5 Power management
    //

    HvCallGetPowerProperty              = 0x009B,
    HvCallSetPowerProperty              = 0x009C,

    //
    // V5 SVM hypercalls
    //

    HvCallCreatePasidSpace              = 0x009D,
    HvCallDeletePasidSpace              = 0x009E,
    HvCallSetPasidAddressSpace          = 0x009F,
    HvCallFlushPasidAddressSpace        = 0x00A0,
    HvCallFlushPasidAddressList         = 0x00A1,
    HvCallAttachPasidSpace              = 0x00A2,
    HvCallDetachPasidSpace              = 0x00A3,
    HvCallEnablePasid                   = 0x00A4,
    HvCallDisablePasid                  = 0x00A5,
    HvCallAcknowledgeDevicePageRequest  = 0x00A6,
    HvCallCreateDevicePrQueue           = 0x00A7,
    HvCallDeleteDevicePrQueue           = 0x00A8,
    HvCallSetDevicePrqProperty          = 0x00A9,
    HvCallGetPhysicalDeviceProperty     = 0x00AA,
    HvCallSetPhysicalDeviceProperty     = 0x00AB,

    //
    // V5 Virtual TLB IDs
    //

    HvCallTranslateVirtualAddressEx     = 0x00AC,

    //
    // V5 I/O port intercepts
    //

    HvCallCheckForIoIntercept           = 0x00AD,

    //
    // V5 GPA page attributes.
    //

    HvCallSetGpaPageAttributes          = 0x00AE,

    //
    // V5 Enlightened nested page table flush.
    //

    HvCallFlushGuestPhysicalAddressSpace = 0x00AF,
    HvCallFlushGuestPhysicalAddressList  = 0x00B0,

    //
    // V5 Device Domains.
    //

    HvCallCreateDeviceDomain            = 0x00B1,
    HvCallAttachDeviceDomain            = 0x00B2,
    HvCallMapDeviceGpaPages             = 0x00B3,
    HvCallUnmapDeviceGpaPages           = 0x00B4,

    //
    // V5 CPU group management.
    //

    HvCallCreateCpuGroup                = 0x00B5,
    HvCallDeleteCpuGroup                = 0x00B6,
    HvCallGetCpuGroupProperty           = 0x00B7,
    HvCallSetCpuGroupProperty           = 0x00B8,
    HvCallGetCpuGroupAffinity           = 0x00B9,
    HvCallGetNextCpuGroup               = 0x00BA,
    HvCallGetNextCpuGroupPartition      = 0x00BB,

    //
    // V5 Nested Dynamic Memory.
    //

    HvCallAddPhysicalMemory             = 0x00BC,

    //
    // V6 Intercept Completion.
    //

    HvCallCompleteIntercept             = 0x00BD,

    //
    // V6 Guest Physical Address Space Management IDs
    //

    HvCallPrecommitGpaPages             = 0x00BE,
    HvCallUncommitGpaPages              = 0x00BF,

    //
    // ARM64 Parent asserted interrupt hypercalls.
    //

    HvCallConfigureVirtualInterruptLine = 0x00C0,
    HvCallSetVirtualInterruptLineState  = 0x00C1,

    //
    // V6 Integrated (Root) Scheduler
    //

    HvCallDispatchVp                    = 0x00C2,
    HvCallProcessIommuPrq               = 0x00C3,

    //
    // V6 Device Domains.
    //

    HvCallDetachDeviceDomain            = 0x00C4,
    HvCallDeleteDeviceDomain            = 0x00C5,
    HvCallQueryDeviceDomain             = 0x00C6,
    HvCallMapSparseDeviceGpaPages       = 0x00C7,
    HvCallUnmapSparseDeviceGpaPages     = 0x00C8,

    //
    // V6 Page Access Tracking.
    //

    HvCallGetGpaPagesAccessState        = 0x00C9,
    HvCallGetSparseGpaPagesAccessState  = 0x00CA,

    //
    // V6 TestFramework hypercall
    //

    HvCallInvokeTestFramework           = 0x00CB,

    //
    // V7 Virtual Secure Mode (VSM) hypercalls.
    //

    HvCallQueryVtlProtectionMaskRange   = 0x00CC,
    HvCallModifyVtlProtectionMaskRange  = 0x00CD,

    //
    // V7 Device Domains.
    //

    HvCallConfigureDeviceDomain         = 0x00CE,
    HvCallQueryDeviceDomainProperties   = 0x00CF,
    HvCallFlushDeviceDomain             = 0x00D0,
    HvCallFlushDeviceDomainList         = 0x00D1,

    //
    // V7 Virtual Secure Mode (VSM) hypercalls.
    //

    HvCallAcquireSparseGpaPageHostAccess = 0x00D2,
    HvCallReleaseSparseGpaPageHostAccess = 0x00D3,
    HvCallCheckSparseGpaPageVtlAccess    = 0x00D4,

    //
    // V7 Device Domains.
    //

    HvCallEnableDeviceInterrupt          = 0x00D5,

    //
    // V7 ARM enlightened TLB flush.
    //

    HvCallFlushTlb                       = 0x00D6,

    //
    // V8 Isolated VM (IVM)
    //

    HvCallAcquireSparseSpaPageHostAccess    = 0x00D7,
    HvCallReleaseSparseSpaPageHostAccess    = 0x00D8,
    HvCallAcceptGpaPages                    = 0x00D9,
    HvCallUnacceptGpaPages                  = 0x00DA,
    HvCallModifySparseGpaPageHostVisibility = 0x00DB,
    HvCallLockSparseGpaPageMapping          = 0x00DC,
    HvCallUnlockSparseGpaPageMapping        = 0x00DD,

    //
    // V8 Hypervisor Idle State Management hypercalls.
    //

    HvCallRequestProcessorHalt = 0x00DE,

    //
    // V8 Intercept Completion.
    //

    HvCallGetInterceptData              = 0x00DF,

    //
    // Total of all hypercalls
    //

    HvCallCount

} HV_CALL_CODE, *PHV_CALL_CODE;

//
// Declare constants and structures for submitting hypercalls.
//
#define HV_X64_MAX_HYPERCALL_ELEMENTS ((1<<12) - 1)

typedef union _HV_HYPERCALL_INPUT
{
    //
    // Input: The call code, argument sizes and calling convention
    //
    struct
    {
        UINT32 CallCode        : 16; // Least significant bits
        UINT32 IsFast          : 1;  // Uses the register based form
        UINT32 Reserved1       : 14;
        UINT32 IsNested        : 1;  // The outer hypervisor handles this call.
        UINT32 CountOfElements : 12;
        UINT32 Reserved2       : 4;
        UINT32 RepStartIndex   : 12;
        UINT32 Reserved3       : 4;  // Most significant bits
    };

    UINT64 AsUINT64;

} HV_HYPERCALL_INPUT, *PHV_HYPERCALL_INPUT;

typedef union _HV_HYPERCALL_OUTPUT
{
    //
    // Output: The result and returned data size
    //
    struct
    {
        UINT16 CallStatus;             // Least significant bits
        UINT16 Reserved1;
        UINT32 ElementsProcessed : 12;
        UINT32 Reserved2         : 20; // Most significant bits
    };
    UINT64 AsUINT64;

} HV_HYPERCALL_OUTPUT, *PHV_HYPERCALL_OUTPUT;

//
// Define synthetic interrupt controller message constants.
//
#define HV_MESSAGE_SIZE                 (256)
#define HV_MESSAGE_PAYLOAD_BYTE_COUNT   (240)
#define HV_MESSAGE_PAYLOAD_QWORD_COUNT  (30)

//
// Define hypervisor message types.
//
typedef enum _HV_MESSAGE_TYPE
{
    HvMessageTypeNone = 0x00000000,

    //
    // Memory access messages.
    //
    HvMessageTypeUnmappedGpa = 0x80000000,
    HvMessageTypeGpaIntercept = 0x80000001,

#if defined(MDE_CPU_AARCH64)
    HvMessageTypeMmioIntercept = 0x80000002,
#endif

    HvMessageTypeUnacceptedGpa = 0x80000003,
    HvMessageTypeGpaAttributeIntercept = 0x80000004,

    //
    // Timer notification messages.
    //
    HvMessageTimerExpired = 0x80000010,

    //
    // Error messages.
    //
    HvMessageTypeInvalidVpRegisterValue = 0x80000020,
    HvMessageTypeUnrecoverableException = 0x80000021,
    HvMessageTypeUnsupportedFeature = 0x80000022,
    HvMessageTypeTlbPageSizeMismatch = 0x80000023,
    HvMessageTypeIommuFault = 0x80000024,

    //
    // Trace buffer complete messages.
    //
    HvMessageTypeEventLogBufferComplete = 0x80000040,

    //
    // Hypercall intercept.
    //
    HvMessageTypeHypercallIntercept = 0x80000050,

    //
    // Synic intercepts.
    //
    HvMessageTypeSynicEventIntercept = 0x80000060,

    //
    // Integrated (root) scheduler signal VP-backing thread(s) messages.
    //
    // N.B. Message id range [0x80000100, 0x800001FF] inclusively is reserved for
    //      the integrated (root) scheduler messages.
    //
    HvMessageTypeSchedulerIdRangeStart = 0x80000100,
    HvMessageTypeSchedulerVpSignalBitset = 0x80000100,
    HvMessageTypeSchedulerVpSignalPair = 0x80000101,
    HvMessageTypeSchedulerIdRangeEnd = 0x800001FF,

    //
    // Platform-specific processor intercept messages.
    //

    HvMessageTypeMsrIntercept = 0x80010001,
    HvMessageTypeExceptionIntercept = 0x80010003,
    HvMessageTypeRegisterIntercept = 0x80010006,

#if defined(MDE_CPU_X64)

    //
    // (AMD64/X86).
    //
    HvMessageTypeX64IoPortIntercept = 0x80010000,
    HvMessageTypeX64CpuidIntercept = 0x80010002,
    HvMessageTypeX64ApicEoi = 0x80010004,
    HvMessageTypeX64IommuPrq = 0x80010006,
    HvMessageTypeX64Halt = 0x80010007,
    HvMessageTypeX64InterruptionDeliverable = 0x80010008,
    HvMessageTypeX64SipiIntercept = 0x80010009,

#elif defined(MDE_CPU_AARCH64)

    HvMessageTypeArm64ResetIntercept = 0x80010000,

#endif

} HV_MESSAGE_TYPE, *PHV_MESSAGE_TYPE;

//
// Definition of the HvPostMessage hypercall input structure.
//
typedef struct HV_CALL_ATTRIBUTES _HV_INPUT_POST_MESSAGE
{
    HV_CONNECTION_ID    ConnectionId;
    UINT32              Reserved;
    HV_MESSAGE_TYPE     MessageType;
    UINT32              PayloadSize;
    UINT64              Payload[HV_MESSAGE_PAYLOAD_QWORD_COUNT];
} HV_INPUT_POST_MESSAGE, *PHV_INPUT_POST_MESSAGE;

//
// Definition of the HvSignalEvent hypercall input structure.
//
typedef struct HV_CALL_ATTRIBUTES _HV_INPUT_SIGNAL_EVENT
{
    HV_CONNECTION_ID ConnectionId;
    UINT16           FlagNumber;
    UINT16           RsvdZ;
} HV_INPUT_SIGNAL_EVENT, *PHV_INPUT_SIGNAL_EVENT;

//
// Hypervisor register names for accessing a virtual processor's registers.
//
typedef enum _HV_REGISTER_NAME
{
    // Suspend Registers
    HvRegisterExplicitSuspend           = 0x00000000,
    HvRegisterInterceptSuspend          = 0x00000001,
    HvRegisterInstructionEmulationHints = 0x00000002,
    HvRegisterDispatchSuspend           = 0x00000003,

    // Version
    HvRegisterHypervisorVersion = 0x00000100,   // 128-bit result same as CPUID 0x40000002

    // Feature Access (registers are 128 bits)
    HvRegisterPrivilegesAndFeaturesInfo = 0x00000200,   // 128-bit result same as CPUID 0x40000003
    HvRegisterFeaturesInfo              = 0x00000201,   // 128-bit result same as CPUID 0x40000004
    HvRegisterImplementationLimitsInfo  = 0x00000202,   // 128-bit result same as CPUID 0x40000005
    HvRegisterHardwareFeaturesInfo      = 0x00000203,   // 128-bit result same as CPUID 0x40000006
    HvRegisterCpuManagementFeaturesInfo = 0x00000204,   // 128-bit result same as CPUID 0x40000007
    HvRegisterSvmFeaturesInfo           = 0x00000205,   // 128-bit result same as CPUID 0x40000008
    HvRegisterSkipLevelFeaturesInfo     = 0x00000206,   // 128-bit result same as CPUID 0x40000009
    HvRegisterNestedVirtFeaturesInfo    = 0x00000207,   // 128-bit result same as CPUID 0x4000000A
    HvRegisterIsolationConfiguration    = 0x00000209,   // 128-bit result same as CPUID 0x4000000C

    // Guest Crash Registers
    HvRegisterGuestCrashP0  = 0x00000210,
    HvRegisterGuestCrashP1  = 0x00000211,
    HvRegisterGuestCrashP2  = 0x00000212,
    HvRegisterGuestCrashP3  = 0x00000213,
    HvRegisterGuestCrashP4  = 0x00000214,
    HvRegisterGuestCrashCtl = 0x00000215,

    // Power State Configuration
    HvRegisterPowerStateConfigC1  = 0x00000220,
    HvRegisterPowerStateTriggerC1 = 0x00000221,
    HvRegisterPowerStateConfigC2  = 0x00000222,
    HvRegisterPowerStateTriggerC2 = 0x00000223,
    HvRegisterPowerStateConfigC3  = 0x00000224,
    HvRegisterPowerStateTriggerC3 = 0x00000225,

    // Frequency Registers
    HvRegisterProcessorClockFrequency = 0x00000240,
    HvRegisterInterruptClockFrequency = 0x00000241,

    // Idle Register
    HvRegisterGuestIdle = 0x00000250,

    // Guest Debug
    HvRegisterDebugDeviceOptions = 0x00000260,

    // Memory Zeroing Conrol Register
    HvRegisterMemoryZeroingControl = 0x00000270,

    // Pending Interruption Register
    HvRegisterPendingInterruption = 0x00010002,

    // Interrupt State register
    HvRegisterInterruptState = 0x00010003,

    // Pending Event Register
    HvRegisterPendingEvent0 = 0x00010004,
    HvRegisterPendingEvent1 = 0x00010005,

    // Misc
    HvRegisterVpRuntime            = 0x00090000,
    HvRegisterGuestOsId            = 0x00090002,
    HvRegisterVpIndex              = 0x00090003,
    HvRegisterTimeRefCount         = 0x00090004,
    HvRegisterCpuManagementVersion = 0x00090007,
    HvRegisterVpAssistPage         = 0x00090013,
    HvRegisterVpRootSignalCount    = 0x00090014,

    // Performance statistics Registers
    HvRegisterStatsPartitionRetail   = 0x00090020,
    HvRegisterStatsPartitionInternal = 0x00090021,
    HvRegisterStatsVpRetail          = 0x00090022,
    HvRegisterStatsVpInternal        = 0x00090023,

    HvRegisterNestedVpIndex          = 0x00091003,

    // Hypervisor-defined Registers (Synic)
    HvRegisterSint0    = 0x000A0000,
    HvRegisterSint1    = 0x000A0001,
    HvRegisterSint2    = 0x000A0002,
    HvRegisterSint3    = 0x000A0003,
    HvRegisterSint4    = 0x000A0004,
    HvRegisterSint5    = 0x000A0005,
    HvRegisterSint6    = 0x000A0006,
    HvRegisterSint7    = 0x000A0007,
    HvRegisterSint8    = 0x000A0008,
    HvRegisterSint9    = 0x000A0009,
    HvRegisterSint10   = 0x000A000A,
    HvRegisterSint11   = 0x000A000B,
    HvRegisterSint12   = 0x000A000C,
    HvRegisterSint13   = 0x000A000D,
    HvRegisterSint14   = 0x000A000E,
    HvRegisterSint15   = 0x000A000F,
    HvRegisterScontrol = 0x000A0010,
    HvRegisterSversion = 0x000A0011,
    HvRegisterSifp     = 0x000A0012,
    HvRegisterSipp     = 0x000A0013,
    HvRegisterEom      = 0x000A0014,
    HvRegisterSirbp    = 0x000A0015,

    HvRegisterNestedSint0    = 0x000A1000,
    HvRegisterNestedSint1    = 0x000A1001,
    HvRegisterNestedSint2    = 0x000A1002,
    HvRegisterNestedSint3    = 0x000A1003,
    HvRegisterNestedSint4    = 0x000A1004,
    HvRegisterNestedSint5    = 0x000A1005,
    HvRegisterNestedSint6    = 0x000A1006,
    HvRegisterNestedSint7    = 0x000A1007,
    HvRegisterNestedSint8    = 0x000A1008,
    HvRegisterNestedSint9    = 0x000A1009,
    HvRegisterNestedSint10   = 0x000A100A,
    HvRegisterNestedSint11   = 0x000A100B,
    HvRegisterNestedSint12   = 0x000A100C,
    HvRegisterNestedSint13   = 0x000A100D,
    HvRegisterNestedSint14   = 0x000A100E,
    HvRegisterNestedSint15   = 0x000A100F,
    HvRegisterNestedScontrol = 0x000A1010,
    HvRegisterNestedSversion = 0x000A1011,
    HvRegisterNestedSifp     = 0x000A1012,
    HvRegisterNestedSipp     = 0x000A1013,
    HvRegisterNestedEom      = 0x000A1014,
    HvRegisterNestedSirbp    = 0x000A1015,


    // Hypervisor-defined Registers (Synthetic Timers)
    HvRegisterStimer0Config            = 0x000B0000,
    HvRegisterStimer0Count             = 0x000B0001,
    HvRegisterStimer1Config            = 0x000B0002,
    HvRegisterStimer1Count             = 0x000B0003,
    HvRegisterStimer2Config            = 0x000B0004,
    HvRegisterStimer2Count             = 0x000B0005,
    HvRegisterStimer3Config            = 0x000B0006,
    HvRegisterStimer3Count             = 0x000B0007,
    HvRegisterStimeUnhaltedTimerConfig = 0x000B0100,
    HvRegisterStimeUnhaltedTimerCount  = 0x000B0101,

    //
    // Synthetic VSM registers
    //

    // 0x000D0000-1 are available for future use.
    HvRegisterVsmCodePageOffsets     = 0x000D0002,
    HvRegisterVsmVpStatus            = 0x000D0003,
    HvRegisterVsmPartitionStatus     = 0x000D0004,
    HvRegisterVsmVina                = 0x000D0005,
    HvRegisterVsmCapabilities        = 0x000D0006,
    HvRegisterVsmPartitionConfig     = 0x000D0007,

    HvRegisterVsmVpSecureConfigVtl0  = 0x000D0010,
    HvRegisterVsmVpSecureConfigVtl1  = 0x000D0011,
    HvRegisterVsmVpSecureConfigVtl2  = 0x000D0012,
    HvRegisterVsmVpSecureConfigVtl3  = 0x000D0013,
    HvRegisterVsmVpSecureConfigVtl4  = 0x000D0014,
    HvRegisterVsmVpSecureConfigVtl5  = 0x000D0015,
    HvRegisterVsmVpSecureConfigVtl6  = 0x000D0016,
    HvRegisterVsmVpSecureConfigVtl7  = 0x000D0017,
    HvRegisterVsmVpSecureConfigVtl8  = 0x000D0018,
    HvRegisterVsmVpSecureConfigVtl9  = 0x000D0019,
    HvRegisterVsmVpSecureConfigVtl10 = 0x000D001A,
    HvRegisterVsmVpSecureConfigVtl11 = 0x000D001B,
    HvRegisterVsmVpSecureConfigVtl12 = 0x000D001C,
    HvRegisterVsmVpSecureConfigVtl13 = 0x000D001D,
    HvRegisterVsmVpSecureConfigVtl14 = 0x000D001E,

    HvRegisterVsmVpWaitForTlbLock    = 0x000D0020,

    HvRegisterIsolationCapabilities  = 0x000D0100,

#if defined(MDE_CPU_X64)

    // Interruptible notification register
    HvX64RegisterDeliverabilityNotifications = 0x00010006,

    // X64 User-Mode Registers
    HvX64RegisterRax                = 0x00020000,
    HvX64RegisterRcx                = 0x00020001,
    HvX64RegisterRdx                = 0x00020002,
    HvX64RegisterRbx                = 0x00020003,
    HvX64RegisterRsp                = 0x00020004,
    HvX64RegisterRbp                = 0x00020005,
    HvX64RegisterRsi                = 0x00020006,
    HvX64RegisterRdi                = 0x00020007,
    HvX64RegisterR8                 = 0x00020008,
    HvX64RegisterR9                 = 0x00020009,
    HvX64RegisterR10                = 0x0002000A,
    HvX64RegisterR11                = 0x0002000B,
    HvX64RegisterR12                = 0x0002000C,
    HvX64RegisterR13                = 0x0002000D,
    HvX64RegisterR14                = 0x0002000E,
    HvX64RegisterR15                = 0x0002000F,
    HvX64RegisterRip                = 0x00020010,
    HvX64RegisterRflags             = 0x00020011,

    // X64 Floating Point and Vector Registers
    HvX64RegisterXmm0               = 0x00030000,
    HvX64RegisterXmm1               = 0x00030001,
    HvX64RegisterXmm2               = 0x00030002,
    HvX64RegisterXmm3               = 0x00030003,
    HvX64RegisterXmm4               = 0x00030004,
    HvX64RegisterXmm5               = 0x00030005,
    HvX64RegisterXmm6               = 0x00030006,
    HvX64RegisterXmm7               = 0x00030007,
    HvX64RegisterXmm8               = 0x00030008,
    HvX64RegisterXmm9               = 0x00030009,
    HvX64RegisterXmm10              = 0x0003000A,
    HvX64RegisterXmm11              = 0x0003000B,
    HvX64RegisterXmm12              = 0x0003000C,
    HvX64RegisterXmm13              = 0x0003000D,
    HvX64RegisterXmm14              = 0x0003000E,
    HvX64RegisterXmm15              = 0x0003000F,
    HvX64RegisterFpMmx0             = 0x00030010,
    HvX64RegisterFpMmx1             = 0x00030011,
    HvX64RegisterFpMmx2             = 0x00030012,
    HvX64RegisterFpMmx3             = 0x00030013,
    HvX64RegisterFpMmx4             = 0x00030014,
    HvX64RegisterFpMmx5             = 0x00030015,
    HvX64RegisterFpMmx6             = 0x00030016,
    HvX64RegisterFpMmx7             = 0x00030017,
    HvX64RegisterFpControlStatus    = 0x00030018,
    HvX64RegisterXmmControlStatus   = 0x00030019,

    // X64 Control Registers
    HvX64RegisterCr0                = 0x00040000,
    HvX64RegisterCr2                = 0x00040001,
    HvX64RegisterCr3                = 0x00040002,
    HvX64RegisterCr4                = 0x00040003,
    HvX64RegisterCr8                = 0x00040004,
    HvX64RegisterXfem               = 0x00040005,

    // X64 Intermediate Control Registers
    HvX64RegisterIntermediateCr0    = 0x00041000,
    HvX64RegisterIntermediateCr4    = 0x00041003,
    HvX64RegisterIntermediateCr8    = 0x00041004,

    // X64 Debug Registers
    HvX64RegisterDr0                = 0x00050000,
    HvX64RegisterDr1                = 0x00050001,
    HvX64RegisterDr2                = 0x00050002,
    HvX64RegisterDr3                = 0x00050003,
    HvX64RegisterDr6                = 0x00050004,
    HvX64RegisterDr7                = 0x00050005,

    // X64 Segment Registers
    HvX64RegisterEs                 = 0x00060000,
    HvX64RegisterCs                 = 0x00060001,
    HvX64RegisterSs                 = 0x00060002,
    HvX64RegisterDs                 = 0x00060003,
    HvX64RegisterFs                 = 0x00060004,
    HvX64RegisterGs                 = 0x00060005,
    HvX64RegisterLdtr               = 0x00060006,
    HvX64RegisterTr                 = 0x00060007,

    // X64 Table Registers
    HvX64RegisterIdtr               = 0x00070000,
    HvX64RegisterGdtr               = 0x00070001,

    // X64 Virtualized MSRs
    HvX64RegisterTsc                = 0x00080000,
    HvX64RegisterEfer               = 0x00080001,
    HvX64RegisterKernelGsBase       = 0x00080002,
    HvX64RegisterApicBase           = 0x00080003,
    HvX64RegisterPat                = 0x00080004,
    HvX64RegisterSysenterCs         = 0x00080005,
    HvX64RegisterSysenterEip        = 0x00080006,
    HvX64RegisterSysenterEsp        = 0x00080007,
    HvX64RegisterStar               = 0x00080008,
    HvX64RegisterLstar              = 0x00080009,
    HvX64RegisterCstar              = 0x0008000A,
    HvX64RegisterSfmask             = 0x0008000B,
    HvX64RegisterInitialApicId      = 0x0008000C,

    //
    // X64 Cache control MSRs
    //
    HvX64RegisterMsrMtrrCap         = 0x0008000D,
    HvX64RegisterMsrMtrrDefType     = 0x0008000E,
    HvX64RegisterMsrMtrrPhysBase0   = 0x00080010,
    HvX64RegisterMsrMtrrPhysBase1   = 0x00080011,
    HvX64RegisterMsrMtrrPhysBase2   = 0x00080012,
    HvX64RegisterMsrMtrrPhysBase3   = 0x00080013,
    HvX64RegisterMsrMtrrPhysBase4   = 0x00080014,
    HvX64RegisterMsrMtrrPhysBase5   = 0x00080015,
    HvX64RegisterMsrMtrrPhysBase6   = 0x00080016,
    HvX64RegisterMsrMtrrPhysBase7   = 0x00080017,
    HvX64RegisterMsrMtrrPhysBase8   = 0x00080018,
    HvX64RegisterMsrMtrrPhysBase9   = 0x00080019,
    HvX64RegisterMsrMtrrPhysBaseA   = 0x0008001A,
    HvX64RegisterMsrMtrrPhysBaseB   = 0x0008001B,
    HvX64RegisterMsrMtrrPhysBaseC   = 0x0008001C,
    HvX64RegisterMsrMtrrPhysBaseD   = 0x0008001D,
    HvX64RegisterMsrMtrrPhysBaseE   = 0x0008001E,
    HvX64RegisterMsrMtrrPhysBaseF   = 0x0008001F,
    HvX64RegisterMsrMtrrPhysMask0   = 0x00080040,
    HvX64RegisterMsrMtrrPhysMask1   = 0x00080041,
    HvX64RegisterMsrMtrrPhysMask2   = 0x00080042,
    HvX64RegisterMsrMtrrPhysMask3   = 0x00080043,
    HvX64RegisterMsrMtrrPhysMask4   = 0x00080044,
    HvX64RegisterMsrMtrrPhysMask5   = 0x00080045,
    HvX64RegisterMsrMtrrPhysMask6   = 0x00080046,
    HvX64RegisterMsrMtrrPhysMask7   = 0x00080047,
    HvX64RegisterMsrMtrrPhysMask8   = 0x00080048,
    HvX64RegisterMsrMtrrPhysMask9   = 0x00080049,
    HvX64RegisterMsrMtrrPhysMaskA   = 0x0008004A,
    HvX64RegisterMsrMtrrPhysMaskB   = 0x0008004B,
    HvX64RegisterMsrMtrrPhysMaskC   = 0x0008004C,
    HvX64RegisterMsrMtrrPhysMaskD   = 0x0008004D,
    HvX64RegisterMsrMtrrPhysMaskE   = 0x0008004E,
    HvX64RegisterMsrMtrrPhysMaskF   = 0x0008004F,
    HvX64RegisterMsrMtrrFix64k00000 = 0x00080070,
    HvX64RegisterMsrMtrrFix16k80000 = 0x00080071,
    HvX64RegisterMsrMtrrFix16kA0000 = 0x00080072,
    HvX64RegisterMsrMtrrFix4kC0000  = 0x00080073,
    HvX64RegisterMsrMtrrFix4kC8000  = 0x00080074,
    HvX64RegisterMsrMtrrFix4kD0000  = 0x00080075,
    HvX64RegisterMsrMtrrFix4kD8000  = 0x00080076,
    HvX64RegisterMsrMtrrFix4kE0000  = 0x00080077,
    HvX64RegisterMsrMtrrFix4kE8000  = 0x00080078,
    HvX64RegisterMsrMtrrFix4kF0000  = 0x00080079,
    HvX64RegisterMsrMtrrFix4kF8000  = 0x0008007A,

    HvX64RegisterTscAux             = 0x0008007B,
    HvX64RegisterBndcfgs            = 0x0008007C,
    HvX64RegisterDebugCtl           = 0x0008007D,

    //
    // Available
    //
    HvX64RegisterAvailable0008007E  = 0x0008007E,
    HvX64RegisterAvailable0008007F  = 0x0008007F,

    HvX64RegisterSgxLaunchControl0  = 0x00080080,
    HvX64RegisterSgxLaunchControl1  = 0x00080081,
    HvX64RegisterSgxLaunchControl2  = 0x00080082,
    HvX64RegisterSgxLaunchControl3  = 0x00080083,
    HvX64RegisterSpecCtrl           = 0x00080084,
    HvX64RegisterPredCmd            = 0x00080085,

    //
    // Other MSRs
    //
    HvX64RegisterMsrIa32MiscEnable  = 0x000800A0,
    HvX64RegisterIa32FeatureControl = 0x000800A1,
    HvX64RegisterIa32VmxBasic          = 0x000800A2,
    HvX64RegisterIa32VmxPinbasedCtls   = 0x000800A3,
    HvX64RegisterIa32VmxProcbasedCtls  = 0x000800A4,
    HvX64RegisterIa32VmxExitCtls       = 0x000800A5,
    HvX64RegisterIa32VmxEntryCtls      = 0x000800A6,
    HvX64RegisterIa32VmxMisc           = 0x000800A7,
    HvX64RegisterIa32VmxCr0Fixed0      = 0x000800A8,
    HvX64RegisterIa32VmxCr0Fixed1      = 0x000800A9,
    HvX64RegisterIa32VmxCr4Fixed0      = 0x000800AA,
    HvX64RegisterIa32VmxCr4Fixed1      = 0x000800AB,
    HvX64RegisterIa32VmxVmcsEnum       = 0x000800AC,
    HvX64RegisterIa32VmxProcbasedCtls2 = 0x000800AD,
    HvX64RegisterIa32VmxEptVpidCap     = 0x000800AE,
    HvX64RegisterIa32VmxTruePinbasedCtls   = 0x000800AF,
    HvX64RegisterIa32VmxTrueProcbasedCtls  = 0x000800B0,
    HvX64RegisterIa32VmxTrueExitCtls       = 0x000800B1,
    HvX64RegisterIa32VmxTrueEntryCtls      = 0x000800B2,

    //
    // Performance monitoring MSRs
    //
    HvX64RegisterPerfGlobalCtrl     = 0x00081000,
    HvX64RegisterPerfGlobalStatus   = 0x00081001,
    HvX64RegisterPerfGlobalInUse    = 0x00081002,
    HvX64RegisterFixedCtrCtrl       = 0x00081003,
    HvX64RegisterDsArea             = 0x00081004,
    HvX64RegisterPebsEnable         = 0x00081005,
    HvX64RegisterPebsLdLat          = 0x00081006,
    HvX64RegisterPebsFrontend       = 0x00081007,
    HvX64RegisterPerfEvtSel0        = 0x00081100,
    HvX64RegisterPmc0               = 0x00081200,
    HvX64RegisterFixedCtr0          = 0x00081300,

    HvX64RegisterLbrTos             = 0x00082000,
    HvX64RegisterLbrSelect          = 0x00082001,
    HvX64RegisterLerFromLip         = 0x00082002,
    HvX64RegisterLerToLip           = 0x00082003,
    HvX64RegisterLbrFrom0           = 0x00082100,
    HvX64RegisterLbrTo0             = 0x00082200,
    HvX64RegisterLbrInfo0           = 0x00083300,

    //
    // Hypervisor-defined registers (Misc)
    //
    HvX64RegisterHypercall          = 0x00090001,

    //
    // X64 Virtual APIC registers MSRs
    //
    HvX64RegisterEoi                = 0x00090010,
    HvX64RegisterIcr                = 0x00090011,
    HvX64RegisterTpr                = 0x00090012,

    //
    // Partition Timer Assist Registers
    //
    HvX64RegisterEmulatedTimerPeriod    = 0x00090030,
    HvX64RegisterEmulatedTimerControl   = 0x00090031,
    HvX64RegisterPmTimerAssist          = 0x00090032,

    //
    // Intercept Control Registers
    //

    HvX64RegisterCrInterceptControl            = 0x000E0000,
    HvX64RegisterCrInterceptCr0Mask            = 0x000E0001,
    HvX64RegisterCrInterceptCr4Mask            = 0x000E0002,
    HvX64RegisterCrInterceptIa32MiscEnableMask = 0x000E0003,

#elif defined(MDE_CPU_AARCH64)
    // ARM64 Registers

    HvArm64RegisterX0 = 0x00020000,
    HvArm64RegisterX1 = 0x00020001,
    HvArm64RegisterX2 = 0x00020002,
    HvArm64RegisterX3 = 0x00020003,
    HvArm64RegisterX4 = 0x00020004,
    HvArm64RegisterX5 = 0x00020005,
    HvArm64RegisterX6 = 0x00020006,
    HvArm64RegisterX7 = 0x00020007,
    HvArm64RegisterX8 = 0x00020008,
    HvArm64RegisterX9 = 0x00020009,
    HvArm64RegisterX10 = 0x0002000A,
    HvArm64RegisterX11 = 0x0002000B,
    HvArm64RegisterX12 = 0x0002000C,
    HvArm64RegisterX13 = 0x0002000D,
    HvArm64RegisterX14 = 0x0002000E,
    HvArm64RegisterX15 = 0x0002000F,
    HvArm64RegisterX16 = 0x00020010,
    HvArm64RegisterX17 = 0x00020011,
    HvArm64RegisterX18 = 0x00020012,
    HvArm64RegisterX19 = 0x00020013,
    HvArm64RegisterX20 = 0x00020014,
    HvArm64RegisterX21 = 0x00020015,
    HvArm64RegisterX22 = 0x00020016,
    HvArm64RegisterX23 = 0x00020017,
    HvArm64RegisterX24 = 0x00020018,
    HvArm64RegisterX25 = 0x00020019,
    HvArm64RegisterX26 = 0x0002001A,
    HvArm64RegisterX27 = 0x0002001B,
    HvArm64RegisterX28 = 0x0002001C,
    HvArm64RegisterXFp = 0x0002001D,
    HvArm64RegisterXLr = 0x0002001E,
    HvArm64RegisterXSp = 0x0002001F, // alias for either El0/x depending on Cpsr.SPSel
    HvArm64RegisterXSpEl0 = 0x00020020,
    HvArm64RegisterXSpElx = 0x00020021,
    HvArm64RegisterXPc = 0x00020022,
    HvArm64RegisterCpsr = 0x00020023,

    HvArm64RegisterQ0 = 0x00030000,
    HvArm64RegisterQ1 = 0x00030001,
    HvArm64RegisterQ2 = 0x00030002,
    HvArm64RegisterQ3 = 0x00030003,
    HvArm64RegisterQ4 = 0x00030004,
    HvArm64RegisterQ5 = 0x00030005,
    HvArm64RegisterQ6 = 0x00030006,
    HvArm64RegisterQ7 = 0x00030007,
    HvArm64RegisterQ8 = 0x00030008,
    HvArm64RegisterQ9 = 0x00030009,
    HvArm64RegisterQ10 = 0x0003000A,
    HvArm64RegisterQ11 = 0x0003000B,
    HvArm64RegisterQ12 = 0x0003000C,
    HvArm64RegisterQ13 = 0x0003000D,
    HvArm64RegisterQ14 = 0x0003000E,
    HvArm64RegisterQ15 = 0x0003000F,
    HvArm64RegisterQ16 = 0x00030010,
    HvArm64RegisterQ17 = 0x00030011,
    HvArm64RegisterQ18 = 0x00030012,
    HvArm64RegisterQ19 = 0x00030013,
    HvArm64RegisterQ20 = 0x00030014,
    HvArm64RegisterQ21 = 0x00030015,
    HvArm64RegisterQ22 = 0x00030016,
    HvArm64RegisterQ23 = 0x00030017,
    HvArm64RegisterQ24 = 0x00030018,
    HvArm64RegisterQ25 = 0x00030019,
    HvArm64RegisterQ26 = 0x0003001A,
    HvArm64RegisterQ27 = 0x0003001B,
    HvArm64RegisterQ28 = 0x0003001C,
    HvArm64RegisterQ29 = 0x0003001D,
    HvArm64RegisterQ30 = 0x0003001E,
    HvArm64RegisterQ31 = 0x0003001F,
    HvArm64RegisterFpControl = 0x00030020,
    HvArm64RegisterFpStatus = 0x00030021,

    // Debug Registers
    HvArm64RegisterBcr0 = 0x00050000,
    HvArm64RegisterBcr1 = 0x00050001,
    HvArm64RegisterBcr2 = 0x00050002,
    HvArm64RegisterBcr3 = 0x00050003,
    HvArm64RegisterBcr4 = 0x00050004,
    HvArm64RegisterBcr5 = 0x00050005,
    HvArm64RegisterBcr6 = 0x00050006,
    HvArm64RegisterBcr7 = 0x00050007,
    HvArm64RegisterBcr8 = 0x00050008,
    HvArm64RegisterBcr9 = 0x00050009,
    HvArm64RegisterBcr10 = 0x0005000A,
    HvArm64RegisterBcr11 = 0x0005000B,
    HvArm64RegisterBcr12 = 0x0005000C,
    HvArm64RegisterBcr13 = 0x0005000D,
    HvArm64RegisterBcr14 = 0x0005000E,
    HvArm64RegisterBcr15 = 0x0005000F,
    HvArm64RegisterWcr0 = 0x00050010,
    HvArm64RegisterWcr1 = 0x00050011,
    HvArm64RegisterWcr2 = 0x00050012,
    HvArm64RegisterWcr3 = 0x00050013,
    HvArm64RegisterWcr4 = 0x00050014,
    HvArm64RegisterWcr5 = 0x00050015,
    HvArm64RegisterWcr6 = 0x00050016,
    HvArm64RegisterWcr7 = 0x00050017,
    HvArm64RegisterWcr8 = 0x00050018,
    HvArm64RegisterWcr9 = 0x00050019,
    HvArm64RegisterWcr10 = 0x0005001A,
    HvArm64RegisterWcr11 = 0x0005001B,
    HvArm64RegisterWcr12 = 0x0005001C,
    HvArm64RegisterWcr13 = 0x0005001D,
    HvArm64RegisterWcr14 = 0x0005001E,
    HvArm64RegisterWcr15 = 0x0005001F,
    HvArm64RegisterBvr0 = 0x00050020,
    HvArm64RegisterBvr1 = 0x00050021,
    HvArm64RegisterBvr2 = 0x00050022,
    HvArm64RegisterBvr3 = 0x00050023,
    HvArm64RegisterBvr4 = 0x00050024,
    HvArm64RegisterBvr5 = 0x00050025,
    HvArm64RegisterBvr6 = 0x00050026,
    HvArm64RegisterBvr7 = 0x00050027,
    HvArm64RegisterBvr8 = 0x00050028,
    HvArm64RegisterBvr9 = 0x00050029,
    HvArm64RegisterBvr10 = 0x0005002A,
    HvArm64RegisterBvr11 = 0x0005002B,
    HvArm64RegisterBvr12 = 0x0005002C,
    HvArm64RegisterBvr13 = 0x0005002D,
    HvArm64RegisterBvr14 = 0x0005002E,
    HvArm64RegisterBvr15 = 0x0005002F,
    HvArm64RegisterWvr0 = 0x00050030,
    HvArm64RegisterWvr1 = 0x00050031,
    HvArm64RegisterWvr2 = 0x00050032,
    HvArm64RegisterWvr3 = 0x00050033,
    HvArm64RegisterWvr4 = 0x00050034,
    HvArm64RegisterWvr5 = 0x00050035,
    HvArm64RegisterWvr6 = 0x00050036,
    HvArm64RegisterWvr7 = 0x00050037,
    HvArm64RegisterWvr8 = 0x00050038,
    HvArm64RegisterWvr9 = 0x00050039,
    HvArm64RegisterWvr10 = 0x0005003A,
    HvArm64RegisterWvr11 = 0x0005003B,
    HvArm64RegisterWvr12 = 0x0005003C,
    HvArm64RegisterWvr13 = 0x0005003D,
    HvArm64RegisterWvr14 = 0x0005003E,
    HvArm64RegisterWvr15 = 0x0005003F,

    // Control Registers
    HvArm64RegisterMidr = 0x00040000,
    HvArm64RegisterMpidr = 0x00040001,
    HvArm64RegisterSctlr = 0x00040002,
    HvArm64RegisterActlr = 0x00040003,
    HvArm64RegisterCpacr = 0x00040004,
    HvArm64RegisterTtbr0 = 0x00040005,
    HvArm64RegisterTtbr1 = 0x00040006,
    HvArm64RegisterTcr = 0x00040007,
    HvArm64RegisterEsrEl1 = 0x00040008,
    HvArm64RegisterFarEl1 = 0x00040009,
    HvArm64RegisterParEl1 = 0x0004000A,
    HvArm64RegisterMair = 0x0004000B,
    HvArm64RegisterVbar = 0x0004000C,
    HvArm64RegisterContextIdr = 0x0004000D,
    HvArm64RegisterTpidr = 0x0004000E,
    HvArm64RegisterCntkctl = 0x0004000F,
    HvArm64RegisterTpidrroEl0 = 0x00040010,
    HvArm64RegisterTpidrEl0 = 0x00040011,
    HvArm64RegisterFpcrEl1 = 0x00040012,
    HvArm64RegisterFpsrEl1 = 0x00040013,
    HvArm64RegisterSpsrEl1 = 0x00040014,
    HvArm64RegisterElrEl1 = 0x00040015,
    HvArm64RegisterAfsr0 = 0x00040016,
    HvArm64RegisterAfsr1 = 0x00040017,
    HvArm64RegisterAMair = 0x00040018,
    HvArm64RegisterMdscr = 0x00040019,

    // Trap control
    HvArm64RegisterMdcr = 0x00040101,
    HvArm64RegisterCptr = 0x00040102,
    HvArm64RegisterHstr = 0x00040103,
    HvArm64RegisterHacr = 0x00040104,

    // GIT Registers
    HvArm64RegisterCnthCtl = 0x000B0400,
    HvArm64RegisterCntkCtl = 0x000B0401,
    HvArm64RegisterCntpCtl = 0x000B0402,
    HvArm64RegisterCntpCval = 0x000B0403,
    HvArm64RegisterCntvOffset = 0x000B0404,
    HvArm64RegisterCntvCtl = 0x000B0405,
    HvArm64RegisterCntvCval = 0x000B0406,

    // Status Registers
    HvArm64RegisterCtr = 0x00040300,
    HvArm64RegisterDczid = 0x00040301,
    HvArm64RegisterRevidr = 0x00040302,
    HvArm64RegisterIdAa64pfr0 = 0x00040303,
    HvArm64RegisterIdAa64pfr1 = 0x00040304,
    HvArm64RegisterIdAa64dfr0 = 0x00040305,
    HvArm64RegisterIdAa64dfr1 = 0x00040306,
    HvArm64RegisterIdAa64afr0 = 0x00040307,
    HvArm64RegisterIdAa64afr1 = 0x00040308,
    HvArm64RegisterIdAa64isar0 = 0x00040309,
    HvArm64RegisterIdAa64isar1 = 0x0004030A,
    HvArm64RegisterIdAa64mmfr0 = 0x0004030B,
    HvArm64RegisterIdAa64mmfr1 = 0x0004030C,
    HvArm64RegisterClidr = 0x0004030D,
    HvArm64RegisterAidr = 0x0004030E,
    HvArm64RegisterCsselr = 0x0004030F,
    HvArm64RegisterCcsidr = 0x00040310,

    // Address to use for synthetic exceptions
    HvArm64RegisterSyntheticException = 0x00040400,

    // Misc
    HvArm64RegisterInterfaceVersion = 0x00090006,      // low 32 bits result same as CPUID 0x40000001
    HvArm64RegisterPartitionInfoPage = 0x00090015,
    HvArm64RegisterTlbiControl = 0x00090016,

#else

#error Unsupported Architecture

#endif

} HV_REGISTER_NAME, *PHV_REGISTER_NAME;

typedef union _HV_REGISTER_VALUE
{
    HV_UINT128                                  Reg128;
    UINT64                                      Reg64;
    UINT32                                      Reg32;
    UINT16                                      Reg16;
    UINT8                                       Reg8;
} HV_REGISTER_VALUE, *PHV_REGISTER_VALUE;

//
// Definition for HvStartVirtualProcessor hypercall input structure.
// This sets the values provided in VpContext and makes the said Vp runnable.
//
typedef struct HV_CALL_ATTRIBUTES _HV_INPUT_START_VIRTUAL_PROCESSOR
{
    HV_PARTITION_ID             PartitionId;
    HV_VP_INDEX                 VpIndex;
    HV_VTL                      TargetVtl;
    UINT8                       ReservedZ0;
    UINT16                      ReservedZ1;
    HV_INITIAL_VP_CONTEXT       VpContext;
} HV_INPUT_START_VIRTUAL_PROCESSOR, *PHV_INPUT_START_VIRTUAL_PROCESSOR;

//
// Definitions for the HvCallModifySparseGpaPageHostVisibility hypercall.
//
typedef struct HV_CALL_ATTRIBUTES _HV_INPUT_MODIFY_SPARSE_GPA_PAGE_HOST_VISIBILITY
{
    //
    // Supplies the partition ID of the partition this request is for.
    //

    HV_PARTITION_ID TargetPartitionId;

    //
    // Supplies the new host visibility.
    //

    UINT32 HostVisibility : 2;

    UINT32 Reserved0 : 30;
    UINT32 Reserved1;

    //
    // Supplies an array of GPA page numbers to modify.
    //

    HV_CALL_ATTRIBUTES HV_GPA_PAGE_NUMBER GpaPageList[];

} HV_INPUT_MODIFY_SPARSE_GPA_PAGE_HOST_VISIBILITY, *PHV_INPUT_MODIFY_SPARSE_GPA_PAGE_HOST_VISIBILITY;

#pragma warning(default : 4201)
