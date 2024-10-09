/** @file
  This file declares public structures for the CPUID hypercalls

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

#pragma warning(disable : 4201)

//
// Microsoft hypervisor interface signature.
//
typedef enum _HV_HYPERVISOR_INTERFACE
{
    HvMicrosoftHypervisorInterface = '1#vH'

} HV_HYPERVISOR_INTERFACE, *PHV_HYPERVISOR_INTERFACE;

//
// Version info reported by hypervisors
//
typedef struct _HV_HYPERVISOR_VERSION_INFO
{
    UINT32 BuildNumber;

    UINT32 MinorVersion:16;
    UINT32 MajorVersion:16;

    UINT32 ServicePack;

    UINT32 ServiceNumber:24;
    UINT32 ServiceBranch:8; // Type is HV_SERVICE_BRANCH

} HV_HYPERVISOR_VERSION_INFO, *PHV_HYPERVISOR_VERSION_INFO;

//
// VM Partition Privileges mask. Please also update the bitmask along
// with the below union.
//
typedef union _HV_PARTITION_PRIVILEGE_MASK
{
    UINT64 AsUINT64;
    struct
    {
        //
        // Access to virtual MSRs
        //
        UINT64  AccessVpRunTimeReg:1;
        UINT64  AccessPartitionReferenceCounter:1;
        UINT64  AccessSynicRegs:1;
        UINT64  AccessSyntheticTimerRegs:1;
        UINT64  AccessIntrCtrlRegs:1;
        UINT64  AccessHypercallMsrs:1;
        UINT64  AccessVpIndex:1;
        UINT64  AccessResetReg:1;
        UINT64  AccessStatsReg:1;
        UINT64  AccessPartitionReferenceTsc:1;
        UINT64  AccessGuestIdleReg:1;
        UINT64  AccessFrequencyRegs:1;
        UINT64  AccessDebugRegs:1;
        UINT64  AccessReenlightenmentControls:1;
        UINT64  AccessRootSchedulerReg:1;
        UINT64  Reserved1:17;

        //
        // Access to hypercalls
        //
        UINT64  CreatePartitions:1;
        UINT64  AccessPartitionId:1;
        UINT64  AccessMemoryPool:1;
        UINT64  AdjustMessageBuffers:1;
        UINT64  PostMessages:1;
        UINT64  SignalEvents:1;
        UINT64  CreatePort:1;
        UINT64  ConnectPort:1;
        UINT64  AccessStats:1;
        UINT64  Reserved2:2;
        UINT64  Debugging:1;
        UINT64  CpuManagement:1;
        UINT64  ConfigureProfiler:1;
        UINT64  AccessVpExitTracing:1;
        UINT64  EnableExtendedGvaRangesForFlushVirtualAddressList:1;
        UINT64  AccessVsm:1;
        UINT64  AccessVpRegisters:1;
        UINT64  UnusedBit:1;
        UINT64  FastHypercallOutput:1;
        UINT64  EnableExtendedHypercalls:1;
        UINT64  StartVirtualProcessor:1;
        UINT64  Isolation:1;
        UINT64  Reserved3:9;

    };

} HV_PARTITION_PRIVILEGE_MASK, *PHV_PARTITION_PRIVILEGE_MASK;

#if defined(MDE_CPU_X64)

typedef union _HV_X64_PLATFORM_CAPABILITIES {
    UINT64 AsUINT64[2];
    struct {

        //
        // Eax
        //

        UINT32 AllowRedSignedCode : 1;
        UINT32 AllowKernelModeDebugging : 1;
        UINT32 AllowUserModeDebugging : 1;
        UINT32 AllowTelnetServer : 1;
        UINT32 AllowIOPorts : 1;
        UINT32 AllowFullMsrSpace : 1;
        UINT32 AllowPerfCounters : 1;
        UINT32 AllowHost512MB : 1;
        UINT32 ReservedEax1 : 1;
        UINT32 AllowRemoteRecovery : 1;
        UINT32 AllowStreaming : 1;
        UINT32 AllowPushDeployment : 1;
        UINT32 AllowPullDeployment : 1;
        UINT32 AllowProfiling : 1;
        UINT32 AllowJsProfiling : 1;
        UINT32 AllowCrashDump : 1;
        UINT32 AllowVsCrashDump : 1;
        UINT32 AllowToolFileIO : 1;
        UINT32 AllowConsoleMgmt : 1;
        UINT32 AllowTracing : 1;
        UINT32 AllowXStudio : 1;
        UINT32 AllowGestureBuilder : 1;
        UINT32 AllowSpeechLab : 1;
        UINT32 AllowSmartglassStudio : 1;
        UINT32 AllowNetworkTools : 1;
        UINT32 AllowTcrTool : 1;
        UINT32 AllowHostNetworkStack : 1;
        UINT32 AllowSystemUpdateTest : 1;
        UINT32 AllowOffChipPerfCtrStreaming : 1;
        UINT32 AllowToolingMemory : 1;
        UINT32 AllowSystemDowngrade : 1;
        UINT32 AllowGreenDiskLicenses : 1;

        //
        // Ebx
        //

        UINT32 IsLiveConnected : 1;
        UINT32 IsMteBoosted : 1;
        UINT32 IsQaSlt : 1;
        UINT32 IsStockImage : 1;
        UINT32 IsMsTestLab : 1;
        UINT32 IsRetailDebugger : 1;
        UINT32 IsXvdSrt : 1;
        UINT32 IsGreenDebug : 1;
        UINT32 IsHwDevTest : 1;
        UINT32 AllowDiskLicenses : 1;
        UINT32 AllowInstrumentation : 1;
        UINT32 AllowWifiTester : 1;
        UINT32 AllowWifiTesterDFS : 1;
        UINT32 IsHwTest : 1;
        UINT32 AllowHostOddTest : 1;
        UINT32 IsLiveUnrestricted : 1;
        UINT32 AllowDiscLicensesWithoutMediaAuth : 1;
        UINT32 ReservedEbx : 15;

        //
        // Ecx
        //

        UINT32 ReservedEcx;

        //
        // Edx
        //

        UINT32 ReservedEdx : 31;
        UINT32 UseAlternateXvd : 1;
    };

} HV_X64_PLATFORM_CAPABILITIES, *PHV_X64_PLATFORM_CAPABILITIES;

#endif

//
// Typedefs for CPUID leaves on HvMicrosoftHypercallInterface-supporting
// hypervisors.
// =====================================================================
//
// The below CPUID leaves are present if VersionAndFeatures.HypervisorPresent
// is set by CPUID(HvCpuIdFunctionVersionAndFeatures).
// =====================================================================
//

typedef enum _HV_CPUID_FUNCTION
{
    HvCpuIdFunctionVersionAndFeatures           = 0x00000001,
    HvCpuIdFunctionHvVendorAndMaxFunction       = 0x40000000,
    HvCpuIdFunctionHvInterface                  = 0x40000001,

    //
    // The remaining functions depend on the value of HvCpuIdFunctionInterface
    //
    HvCpuIdFunctionMsHvVersion                  = 0x40000002,
    HvCpuIdFunctionMsHvFeatures                 = 0x40000003,
    HvCpuIdFunctionMsHvEnlightenmentInformation = 0x40000004,
    HvCpuIdFunctionMsHvImplementationLimits     = 0x40000005,
    HvCpuIdFunctionMsHvHardwareFeatures         = 0x40000006,
    HvCpuIdFunctionMsHvCpuManagementFeatures    = 0x40000007,
    HvCpuIdFunctionMsHvSvmFeatures              = 0x40000008,
    HvCpuIdFunctionMsHvSkipLevelFeatures        = 0x40000009,
    HvCpuidFunctionMsHvNestedVirtFeatures       = 0x4000000A,
    HvCpuidFunctionMsHvIsolationConfiguration   = 0x4000000C,
    HvCpuIdFunctionMaxReserved                  = 0x4000000C

} HV_CPUID_FUNCTION, *PHV_CPUID_FUNCTION;


//
// Hypervisor Vendor Info - HvCpuIdFunctionHvVendorAndMaxFunction Leaf
//

typedef struct _HV_VENDOR_AND_MAX_FUNCTION
{
    UINT32 MaxFunction;
    UINT8 VendorName[12];
} HV_VENDOR_AND_MAX_FUNCTION, *PHV_VENDOR_AND_MAX_FUNCTION;


//
// Hypervisor Interface Info - HvCpuIdFunctionHvInterface Leaf
//

typedef struct _HV_HYPERVISOR_INTERFACE_INFO
{
    UINT32 Interface; // HV_HYPERVISOR_INTERFACE
    UINT32 Reserved1;
    UINT32 Reserved2;
    UINT32 Reserved3;
} HV_HYPERVISOR_INTERFACE_INFO, *PHV_HYPERVISOR_INTERFACE_INFO;


//
// Hypervisor Feature Information
//

#if defined(MDE_CPU_X64)

// CPUID Information - HvCpuIdFunctionMsHvFeatures Leaf

typedef struct _HV_X64_HYPERVISOR_FEATURES
{
    //
    // Eax-Ebx
    //
    HV_PARTITION_PRIVILEGE_MASK PartitionPrivileges;

    //
    // Ecx - this indicates the power configuration for the current VP.
    //
    UINT32 MaxSupportedCState:4;
    UINT32 HpetNeededForC3PowerState_Deprecated:1;
    UINT32 Reserved:27;

    //
    // Edx
    //
    UINT32 MwaitAvailable_Deprecated:1;
    UINT32 GuestDebuggingAvailable:1;
    UINT32 PerformanceMonitorsAvailable:1;
    UINT32 CpuDynamicPartitioningAvailable:1;
    UINT32 XmmRegistersForFastHypercallAvailable:1;
    UINT32 GuestIdleAvailable:1;
    UINT32 HypervisorSleepStateSupportAvailable:1;
    UINT32 NumaDistanceQueryAvailable:1;
    UINT32 FrequencyRegsAvailable:1;
    UINT32 SyntheticMachineCheckAvailable:1;
    UINT32 GuestCrashRegsAvailable:1;
    UINT32 DebugRegsAvailable:1;
    UINT32 Npiep1Available:1;
    UINT32 DisableHypervisorAvailable:1;
    UINT32 ExtendedGvaRangesForFlushVirtualAddressListAvailable:1;

    UINT32 FastHypercallOutputAvailable:1;
    UINT32 SvmFeaturesAvailable:1;
    UINT32 SintPollingModeAvailable:1;
    UINT32 HypercallMsrLockAvailable:1;
    UINT32 DirectSyntheticTimers:1;
    UINT32 RegisterPatAvailable:1;
    UINT32 RegisterBndcfgsAvailable:1;
    UINT32 WatchdogTimerAvailable:1;
    UINT32 SyntheticTimeUnhaltedTimerAvailable:1;
    UINT32 DeviceDomainsAvailable:1; // HDK only.
    UINT32 S1DeviceDomainsAvailable:1; // HDK only.
    UINT32 Reserved1:6;

} HV_X64_HYPERVISOR_FEATURES, *PHV_X64_HYPERVISOR_FEATURES;

#define _HV_HYPERVISOR_FEATURES     _HV_X64_HYPERVISOR_FEATURES
#define HV_HYPERVISOR_FEATURES      HV_X64_HYPERVISOR_FEATURES
#define PHV_HYPERVISOR_FEATURES     PHV_X64_HYPERVISOR_FEATURES

#endif

#if defined(MDE_CPU_AARCH64)

typedef struct _HV_ARM64_HYPERVISOR_FEATURES
{
    HV_PARTITION_PRIVILEGE_MASK PartitionPrivileges;

    UINT32 GuestDebuggingAvailable:1;
    UINT32 PerformanceMonitorsAvailable:1;
    UINT32 CpuDynamicPartitioningAvailable:1;
    UINT32 GuestIdleAvailable:1;
    UINT32 HypervisorSleepStateSupportAvailable:1;
    UINT32 NumaDistanceQueryAvailable:1;
    UINT32 FrequencyRegsAvailable:1;
    UINT32 SyntheticMachineCheckAvailable:1;
    UINT32 GuestCrashRegsAvailable:1;
    UINT32 DebugRegsAvailable:1;
    UINT32 DisableHypervisorAvailable:1;
    UINT32 ExtendedGvaRangesForFlushVirtualAddressListAvailable:1;
    UINT32 SintPollingModeAvailable:1;
    UINT32 DirectSyntheticTimers:1;
    UINT32 DeviceDomainsAvailable:1; // HDK only.
    UINT32 S1DeviceDomainsAvailable:1; // HDK only.
    UINT32 Reserved1:16;


} HV_ARM64_HYPERVISOR_FEATURES, *PHV_ARM64_HYPERVISOR_FEATURES;

#define _HV_HYPERVISOR_FEATURES     _HV_ARM64_HYPERVISOR_FEATURES
#define HV_HYPERVISOR_FEATURES      HV_ARM64_HYPERVISOR_FEATURES
#define PHV_HYPERVISOR_FEATURES     PHV_ARM64_HYPERVISOR_FEATURES

#endif


//
// Enlightenment Info
//

#if defined(MDE_CPU_X64)

// CPUID Information - HvCpuIdFunctionMsHvEnlightenmentInformation Leaf

typedef struct _HV_X64_ENLIGHTENMENT_INFORMATION
{
    //
    // Eax
    //
    UINT32 UseHypercallForAddressSpaceSwitch:1;
    UINT32 UseHypercallForLocalFlush:1;
    UINT32 UseHypercallForRemoteFlushAndLocalFlushEntire:1;
    UINT32 UseApicMsrs:1;
    UINT32 UseHvRegisterForReset:1;
    UINT32 UseRelaxedTiming:1;
    UINT32 UseDmaRemapping_Deprecated:1;
    UINT32 UseInterruptRemapping_Deprecated:1;
    UINT32 UseX2ApicMsrs:1;
    UINT32 DeprecateAutoEoi:1;
    UINT32 UseSyntheticClusterIpi:1;
    UINT32 UseExProcessorMasks:1;
    UINT32 Nested:1;
    UINT32 UseIntForMbecSystemCalls:1;
    UINT32 UseVmcsEnlightenments:1;
    UINT32 UseSyncedTimeline:1;
    UINT32 Available:1;  // Was UseReferencePageForSyncedTimeline but never consumed.
    UINT32 UseDirectLocalFlushEntire:1;
    UINT32 Reserved:14;

    //
    // Ebx
    //
    UINT32 LongSpinWaitCount;

    //
    // Ecx
    //
    UINT32 ReservedEcx;

    //
    // Edx
    //
    UINT32 ReservedEdx;

} HV_X64_ENLIGHTENMENT_INFORMATION, *PHV_X64_ENLIGHTENMENT_INFORMATION;

#define _HV_ENLIGHTENMENT_INFORMATION   _HV_X64_ENLIGHTENMENT_INFORMATION
#define HV_ENLIGHTENMENT_INFORMATION    HV_X64_ENLIGHTENMENT_INFORMATION
#define PHV_ENLIGHTENMENT_INFORMATION   PHV_X64_ENLIGHTENMENT_INFORMATION

#endif

#if defined(MDE_CPU_AARCH64)

typedef struct _HV_ARM64_ENLIGHTENMENT_INFORMATION
{
    UINT32 UseHvRegisterForReset:1;
    UINT32 UseRelaxedTiming:1;
    UINT32 UseSyntheticClusterIpi:1;
    UINT32 UseExProcessorMasks:1;
    UINT32 Nested:1;
    UINT32 UseSyncedTimeline:1;
    UINT32 Reserved:26;

    UINT32 LongSpinWaitCount;

    UINT32 Reserved0;
    UINT32 Reserved1;

} HV_ARM64_ENLIGHTENMENT_INFORMATION, *PHV_ARM64_ENLIGHTENMENT_INFORMATION;

#define _HV_ENLIGHTENMENT_INFORMATION   _HV_ARM64_ENLIGHTENMENT_INFORMATION
#define HV_ENLIGHTENMENT_INFORMATION    HV_ARM64_ENLIGHTENMENT_INFORMATION
#define PHV_ENLIGHTENMENT_INFORMATION   PHV_ARM64_ENLIGHTENMENT_INFORMATION

#endif


//
// Implementation Limits - HvCpuIdFunctionMsHvImplementationLimits Leaf
//

typedef struct _HV_IMPLEMENTATION_LIMITS
{
    //
    // Eax
    //
    UINT32 MaxVirtualProcessorCount;

    //
    // Ebx
    //
    UINT32 MaxLogicalProcessorCount;

    //
    // Ecx
    //
    UINT32 MaxInterruptMappingCount;

    //
    // Edx
    //
    UINT32 Reserved;

} HV_IMPLEMENTATION_LIMITS, *PHV_IMPLEMENTATION_LIMITS;


#if defined(MDE_CPU_X64)

//
// Hypervisor Hardware Features Info - HvCpuIdFunctionMsHvHardwareFeatures Leaf
//

typedef struct _HV_X64_HYPERVISOR_HARDWARE_FEATURES
{
    //
    // Eax
    //
    UINT32 ApicOverlayAssistInUse:1;
    UINT32 MsrBitmapsInUse:1;
    UINT32 ArchitecturalPerformanceCountersInUse:1;
    UINT32 SecondLevelAddressTranslationInUse:1;
    UINT32 DmaRemappingInUse:1;
    UINT32 InterruptRemappingInUse:1;
    UINT32 MemoryPatrolScrubberPresent:1;
    UINT32 DmaProtectionInUse:1;
    UINT32 HpetRequested:1;
    UINT32 SyntheticTimersVolatile:1;
    UINT32 HypervisorLevel:4;
    UINT32 PhysicalDestinationModeRequired:1;
    UINT32 UseVmfuncForAliasMapSwitch:1;
    UINT32 HvRegisterForMemoryZeroingSupported:1;
    UINT32 UnrestrictedGuestSupported:1;
    UINT32 L3CachePartitioningSupported:1;
    UINT32 L3CacheMonitoringSupported:1;
    UINT32 Reserved:12;

    //
    // Ebx
    //
    UINT32 ReservedEbx;

    //
    // Ecx
    //
    UINT32 ReservedEcx;

    //
    // Edx
    //
    UINT32 ReservedEdx;

} HV_X64_HYPERVISOR_HARDWARE_FEATURES, *PHV_X64_HYPERVISOR_HARDWARE_FEATURES;

#define _HV_HYPERVISOR_HARDWARE_FEATURES    _HV_X64_HYPERVISOR_HARDWARE_FEATURES
#define HV_HYPERVISOR_HARDWARE_FEATURES     HV_X64_HYPERVISOR_HARDWARE_FEATURES
#define PHV_HYPERVISOR_HARDWARE_FEATURES    PHV_X64_HYPERVISOR_HARDWARE_FEATURES

//
// Hypervisor Cpu Management features - HvCpuIdFunctionMsHvCpuManagementFeatures
// leaf.
//

typedef struct _HV_X64_HYPERVISOR_CPU_MANAGEMENT_FEATURES
{
    //
    // Eax
    //
    UINT32 StartLogicalProcessor:1;
    UINT32 CreateRootVirtualProcessor:1;
    UINT32 PerformanceCounterSync:1;
    UINT32 Reserved0:28;
    UINT32 ReservedIdentityBit:1;

    //
    // Ebx
    //
    UINT32 ProcessorPowerManagement:1;
    UINT32 MwaitIdleStates:1;
    UINT32 LogicalProcessorIdling:1;
    UINT32 Reserved1:29;

    //
    // Ecx
    //
    UINT32 RemapGuestUncached : 1;
    UINT32 ReservedZ2 : 31;

    //
    // Edx
    //
    UINT32 ReservedEdx;

} HV_X64_HYPERVISOR_CPU_MANAGEMENT_FEATURES, *PHV_X64_HYPERVISOR_CPU_MANAGEMENT_FEATURES;

#define _HV_HYPERVISOR_CPU_MANAGEMENT_FEATURES _HV_X64_HYPERVISOR_CPU_MANAGEMENT_FEATURES
#define HV_HYPERVISOR_CPU_MANAGEMENT_FEATURES HV_X64_HYPERVISOR_CPU_MANAGEMENT_FEATURES
#define PHV_HYPERVISOR_CPU_MANAGEMENT_FEATURES PHV_X64_HYPERVISOR_CPU_MANAGEMENT_FEATURES

#endif

#if defined(MDE_CPU_AARCH64)

typedef struct _HV_ARM64_HYPERVISOR_HARDWARE_FEATURES
{
    UINT32 ArchitecturalPerformanceCountersInUse:1;
    UINT32 SecondLevelAddressTranslationInUse:1;
    UINT32 DmaRemappingInUse:1;
    UINT32 InterruptRemappingInUse:1;
    UINT32 MemoryPatrolScrubberPresent:1;
    UINT32 DmaProtectionInUse:1;
    UINT32 SyntheticTimersVolatile:1;
    UINT32 HvRegisterForMemoryZeroingSupported:1;
    UINT32 Reserved:24;

    UINT32 Reserved0;
    UINT32 Reserved1;
    UINT32 Reserved2;

} HV_ARM64_HYPERVISOR_HARDWARE_FEATURES, *PHV_ARM64_HYPERVISOR_HARDWARE_FEATURES;

#define _HV_HYPERVISOR_HARDWARE_FEATURES    _HV_ARM64_HYPERVISOR_HARDWARE_FEATURES
#define HV_HYPERVISOR_HARDWARE_FEATURES     HV_ARM64_HYPERVISOR_HARDWARE_FEATURES
#define PHV_HYPERVISOR_HARDWARE_FEATURES    PHV_ARM64_HYPERVISOR_HARDWARE_FEATURES

//
// Hypervisor Cpu Management features - HvCpuIdFunctionMsHvCpuManagementFeatures
// leaf.
//

typedef struct _HV_ARM64_HYPERVISOR_CPU_MANAGEMENT_FEATURES
{
    //
    // Eax
    //
    UINT32 StartLogicalProcessor:1;
    UINT32 CreateRootVirtualProcessor:1;
    UINT32 PerformanceCounterSync:1;
    UINT32 Reserved0:28;
    UINT32 ReservedIdentityBit:1;

    //
    // Ebx
    //
    UINT32 ProcessorPowerManagement:1;
    UINT32 RootManagedIdleStates:1;
    UINT32 Reserved1:30;

    //
    // Ecx
    //
    UINT32 ReservedEcx;

    //
    // Edx
    //
    UINT32 ReservedEdx;

} HV_ARM64_HYPERVISOR_CPU_MANAGEMENT_FEATURES, *PHV_ARM64_HYPERVISOR_CPU_MANAGEMENT_FEATURES;

#define _HV_HYPERVISOR_CPU_MANAGEMENT_FEATURES _HV_ARM64_HYPERVISOR_CPU_MANAGEMENT_FEATURES
#define HV_HYPERVISOR_CPU_MANAGEMENT_FEATURES HV_ARM64_HYPERVISOR_CPU_MANAGEMENT_FEATURES
#define PHV_HYPERVISOR_CPU_MANAGEMENT_FEATURES PHV_ARM64_HYPERVISOR_CPU_MANAGEMENT_FEATURES

#endif

//
// SVM features - HvCpuIdFunctionMsHvSvmFeatures leaf.
//

typedef struct _HV_HYPERVISOR_SVM_FEATURES
{
    // Eax
    UINT32 SvmSupported : 1;
    UINT32 Reserved0 : 10;
    UINT32 MaxPasidSpacePasidCount : 21;

    // Ebx
    UINT32 MaxPasidSpaceCount;

    // Ecx
    UINT32 MaxDevicePrqSize;

    // Edx
    UINT32 Reserved1;

} HV_HYPERVISOR_SVM_FEATURES, *PHV_HYPERVISOR_SVM_FEATURES;

#if defined(MDE_CPU_X64)

//
// Nested virtualization features (Vmx) -
//

typedef struct _HV_HYPERVISOR_NESTED_VIRT_FEATURES
{
    // Eax
    UINT32 EnlightenedVmcsVersionLow                : 8;
    UINT32 EnlightenedVmcsVersionHigh               : 8;
    UINT32 FlushGuestPhysicalHypercall_Deprecated   : 1;
    UINT32 NestedFlushVirtualHypercall              : 1;
    UINT32 FlushGuestPhysicalHypercall              : 1;
    UINT32 MsrBitmap                                : 1;
    UINT32 VirtualizationException                  : 1;
    UINT32 Reserved0                                : 11;

    // Ebx
    UINT32 ReservedEbx;

    // Ecx
    UINT32 ReservedEcx;

    // Edx
    UINT32 ReservedEdx;

} HV_HYPERVISOR_NESTED_VIRT_FEATURES, *PHV_HYPERVISOR_NESTED_VIRT_FEATURES;

#endif

//
// Isolated VM configuration - HvCpuidFunctionMsHvIsolationConfiguration leaf.
//

typedef struct _HV_HYPERVISOR_ISOLATION_CONFIGURATION
{
    // Eax
    UINT32 ParavisorPresent : 1;
    UINT32 Reserved0 : 31;

    // Ebx
    UINT32 IsolationType : 4;
    UINT32 Reserved11 : 1;
    UINT32 SharedGpaBoundaryActive : 1;
    UINT32 SharedGpaBoundaryBits : 6;
    UINT32 Reserved12 : 20;

    // Ecx
    UINT32 Reserved2;

    // Edx
    UINT32 Reserved3;

} HV_HYPERVISOR_ISOLATION_CONFIGURATION, *PHV_HYPERVISOR_ISOLATION_CONFIGURATION;

#define HV_PARTITION_ISOLATION_TYPE_NONE            0
#define HV_PARTITION_ISOLATION_TYPE_VBS             1
#define HV_PARTITION_ISOLATION_TYPE_SNP             2
#define HV_PARTITION_ISOLATION_TYPE_TDX             3

//
// Typedefs for CPUID leaves on HvMicrosoftHypercallInterface-supporting
// hypervisors.
//
typedef union _HV_CPUID_RESULT
{

    UINT32 AsUINT32[4];

#if defined(MDE_CPU_X64)

    struct
    {
        UINT32 Eax;
        UINT32 Ebx;
        UINT32 Ecx;
        UINT32 Edx;
    };

    struct
    {
        //
        // Eax
        //
        UINT32 ReservedEax;

        //
        // Ebx
        //
        UINT32 ReservedEbx:24;
        UINT32 InitialApicId:8;

        //
        // Ecx
        //
        UINT32 ReservedEcx:31;
        UINT32 HypervisorPresent:1;

        //
        // Edx
        //
        UINT32 ReservedEdx;

    } VersionAndFeatures;

    HV_X64_PLATFORM_CAPABILITIES MsHvPlatformCapabilities;

    HV_HYPERVISOR_NESTED_VIRT_FEATURES MsHvNestedVirtFeatures;

#endif

    //
    // Eax-Edx on x86/x64
    //

    HV_VENDOR_AND_MAX_FUNCTION HvVendorAndMaxFunction;

    HV_HYPERVISOR_INTERFACE_INFO HvInterface;

    HV_HYPERVISOR_VERSION_INFO MsHvVersion;

    HV_HYPERVISOR_FEATURES MsHvFeatures;

    HV_ENLIGHTENMENT_INFORMATION MsHvEnlightenmentInformation;

    HV_IMPLEMENTATION_LIMITS MsHvImplementationLimits;

    HV_HYPERVISOR_HARDWARE_FEATURES MsHvHardwareFeatures;

    HV_HYPERVISOR_CPU_MANAGEMENT_FEATURES MsHvCpuManagementFeatures;

    HV_HYPERVISOR_SVM_FEATURES MsHvSvmFeatures;

    HV_HYPERVISOR_ISOLATION_CONFIGURATION MsHvIsolationConfiguration;

} HV_CPUID_RESULT, *PHV_CPUID_RESULT;

#pragma warning(default : 4201)