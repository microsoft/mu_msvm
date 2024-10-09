/** @file
  Type definitions for the hypervisor guest MSR interface.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

#pragma warning(disable : 4201)

//
// Version info reported by guest OS's
//
typedef enum _HV_GUEST_OS_VENDOR
{
    HvGuestOsVendorMicrosoft        = 0x0001

} HV_GUEST_OS_VENDOR, *PHV_GUEST_OS_VENDOR;

typedef enum _HV_GUEST_OS_MICROSOFT_IDS
{
    HvGuestOsMicrosoftUndefined     = 0x00,
    HvGuestOsMicrosoftMSDOS         = 0x01,
    HvGuestOsMicrosoftWindows3x     = 0x02,
    HvGuestOsMicrosoftWindows9x     = 0x03,
    HvGuestOsMicrosoftWindowsNT     = 0x04,
    HvGuestOsMicrosoftWindowsCE     = 0x05

} HV_GUEST_OS_MICROSOFT_IDS, *PHV_GUEST_OS_MICROSOFT_IDS;

#if defined(MDE_CPU_X64)

//
// This enumeration collates MSR indices for ease of maintainability.
//
typedef enum _HV_X64_SYNTHETIC_MSR
{
    HvSyntheticMsrGuestOsId                 =    0x40000000,
    HvSyntheticMsrHypercall                 =    0x40000001,
    HvSyntheticMsrVpIndex                   =    0x40000002,
    HvSyntheticMsrReset                     =    0x40000003,
    HvSyntheticMsrCpuMgmtVersion            =    0x40000004,
    XbSyntheticMsrTbFlushHost               =    0x40000006,
    XbSyntheticMsrTbFlush                   =    0x40000007,
    XbSyntheticMsrCrash                     =    0x40000008,
    XbSyntheticMsrGuestOsType               =    0x40000009,
    HvSyntheticMsrVpRuntime                 =    0x40000010,
    XbSyntheticMsrRefTimeOffset             =    0x40000011,
    XbSyntheticMsrRefTscScale               =    0x40000012,
    XbSyntheticMsrVpCount                   =    0x40000013,
    XbSyntheticMsrWbinvd                    =    0x40000014,
    XbSyntheticMsrFlushWb                   =    0x40000015,
    XbSyntheticMsrFlushTbCurrent            =    0x40000016,
    XbSyntheticMsrKcfgDone                  =    0x40000017,
    HvSyntheticMsrTimeRefCount              =    0x40000020,
    HvSyntheticMsrReferenceTsc              =    0x40000021,
    HvSyntheticMsrTscFrequency              =    0x40000022,
    HvSyntheticMsrApicFrequency             =    0x40000023,
    HvSyntheticMsrNpiepConfig               =    0x40000040,
    HvSyntheticMsrEoi                       =    0x40000070,
    HvSyntheticMsrIcr                       =    0x40000071,
    HvSyntheticMsrTpr                       =    0x40000072,
    HvSyntheticMsrVpAssistPage              =    0x40000073,
    HvSyntheticMsrSControl                  =    0x40000080,
    HvSyntheticMsrSVersion                  =    0x40000081,
    HvSyntheticMsrSiefp                     =    0x40000082,
    HvSyntheticMsrSimp                      =    0x40000083,
    HvSyntheticMsrEom                       =    0x40000084,
    HvSyntheticMsrSirb                      =    0x40000085,
    HvSyntheticMsrSint0                     =    0x40000090,
    HvSyntheticMsrSint1                     =    0x40000091,
    HvSyntheticMsrSint2                     =    0x40000092,
    HvSyntheticMsrSint3                     =    0x40000093,
    HvSyntheticMsrSint4                     =    0x40000094,
    HvSyntheticMsrSint5                     =    0x40000095,
    HvSyntheticMsrSint6                     =    0x40000096,
    HvSyntheticMsrSint7                     =    0x40000097,
    HvSyntheticMsrSint8                     =    0x40000098,
    HvSyntheticMsrSint9                     =    0x40000099,
    HvSyntheticMsrSint10                    =    0x4000009A,
    HvSyntheticMsrSint11                    =    0x4000009B,
    HvSyntheticMsrSint12                    =    0x4000009C,
    HvSyntheticMsrSint13                    =    0x4000009D,
    HvSyntheticMsrSint14                    =    0x4000009E,
    HvSyntheticMsrSint15                    =    0x4000009F,
    HvSyntheticMsrSTimer0Config             =    0x400000B0,
    HvSyntheticMsrSTimer0Count              =    0x400000B1,
    HvSyntheticMsrSTimer1Config             =    0x400000B2,
    HvSyntheticMsrSTimer1Count              =    0x400000B3,
    HvSyntheticMsrSTimer2Config             =    0x400000B4,
    HvSyntheticMsrSTimer2Count              =    0x400000B5,
    HvSyntheticMsrSTimer3Config             =    0x400000B6,
    HvSyntheticMsrSTimer3Count              =    0x400000B7,
    HvSyntheticMsrPerfFeedbackTrigger       =    0x400000C1,
    HvSyntheticMsrGuestSchedulerEvent       =    0x400000C2,
    HvSyntheticMsrGuestIdle                 =    0x400000F0,
    HvSyntheticMsrSynthDebugControl         =    0x400000F1,
    HvSyntheticMsrSynthDebugStatus          =    0x400000F2,
    HvSyntheticMsrSynthDebugSendBuffer      =    0x400000F3,
    HvSyntheticMsrSynthDebugReceiveBuffer   =    0x400000F4,
    HvSyntheticMsrSynthDebugPendingBuffer   =    0x400000F5,
    XbSyntheticMsrSynthDebugTransition      =    0x400000F6,
    HvSyntheticMsrDebugDeviceOptions        =    0x400000FF,
    HvSyntheticMsrCrashP0                   =    0x40000100,
    HvSyntheticMsrCrashP1                   =    0x40000101,
    HvSyntheticMsrCrashP2                   =    0x40000102,
    HvSyntheticMsrCrashP3                   =    0x40000103,
    HvSyntheticMsrCrashP4                   =    0x40000104,
    HvSyntheticMsrCrashCtl                  =    0x40000105,
    HvSyntheticMsrReenlightenmentControl    =    0x40000106,
    HvSyntheticMsrTscEmulationControl       =    0x40000107,
    HvSyntheticMsrTscEmulationStatus        =    0x40000108,
    HvSyntheticMsrSWatchdogConfig           =    0x40000110,
    HvSyntheticMsrSWatchdogCount            =    0x40000111,
    HvSyntheticMsrSWatchdogStatus           =    0x40000112,
    HvSyntheticMsrSTimeUnhaltedTimerConfig  =    0x40000114,
    HvSyntheticMsrSTimeUnhaltedTimerCount   =    0x40000115,
    HvSyntheticMsrMemoryZeroingControl      =    0x40000116,
    XbSyntheticMsrFsBase                    =    0x40000122,
    XbSyntheticMsrXOnly                     =    0x40000123,
    HvSyntheticMsrBelow1MbPage              =    0x40000200,
    HvSyntheticMsrNestedVpIndex             =    0x40001002,
    HvSyntheticMsrNestedSControl            =    0x40001080,
    HvSyntheticMsrNestedSVersion            =    0x40001081,
    HvSyntheticMsrNestedSiefp               =    0x40001082,
    HvSyntheticMsrNestedSimp                =    0x40001083,
    HvSyntheticMsrNestedEom                 =    0x40001084,
    HvSyntheticMsrNestedSirb                =    0x40001085,
    HvSyntheticMsrNestedSint0               =    0x40001090,
    HvSyntheticMsrNestedSint1               =    0x40001091,
    HvSyntheticMsrNestedSint2               =    0x40001092,
    HvSyntheticMsrNestedSint3               =    0x40001093,
    HvSyntheticMsrNestedSint4               =    0x40001094,
    HvSyntheticMsrNestedSint5               =    0x40001095,
    HvSyntheticMsrNestedSint6               =    0x40001096,
    HvSyntheticMsrNestedSint7               =    0x40001097,
    HvSyntheticMsrNestedSint8               =    0x40001098,
    HvSyntheticMsrNestedSint9               =    0x40001099,
    HvSyntheticMsrNestedSint10              =    0x4000109A,
    HvSyntheticMsrNestedSint11              =    0x4000109B,
    HvSyntheticMsrNestedSint12              =    0x4000109C,
    HvSyntheticMsrNestedSint13              =    0x4000109D,
    HvSyntheticMsrNestedSint14              =    0x4000109E,
    HvSyntheticMsrNestedSint15              =    0x4000109F,

} HV_X64_SYNTHETIC_MSR, *PHV_X64_SYNTHETIC_MSR;

//
// Declare the MSR used to identify the guest OS.
//
#define HV_X64_MSR_GUEST_OS_ID HvSyntheticMsrGuestOsId

#endif

typedef union _HV_GUEST_OS_ID_CONTENTS
{
    UINT64 AsUINT64;
    struct
    {
        UINT64 BuildNumber    : 16;
        UINT64 ServiceVersion : 8; // Service Pack, etc.
        UINT64 MinorVersion   : 8;
        UINT64 MajorVersion   : 8;
        UINT64 OsId           : 8; // HV_GUEST_OS_MICROSOFT_IDS (If Vendor=MS)
        UINT64 VendorId       : 16; // HV_GUEST_OS_VENDOR. We only support using
                                    // the least significant 15 bits, the top
                                    // bit must be zero. If 1, refers to the
                                    // open source format.
    };

    struct
    {
        UINT64 VendorSpecific1 : 16;
        UINT64 Version         : 32;
        UINT64 VendorSpecific2 : 8;
        UINT64 OsId            : 7;
        UINT64 IsOpenSource    : 1;
    } OpenSource;

} HV_GUEST_OS_ID_CONTENTS, *PHV_GUEST_OS_ID_CONTENTS;

//
// Declare the MSR used to setup pages used to communicate with the hypervisor.
//
#define HV_X64_MSR_HYPERCALL HvSyntheticMsrHypercall

typedef union _HV_X64_MSR_HYPERCALL_CONTENTS
{
    UINT64 AsUINT64;
    struct
    {
        UINT64 Enable               : 1;
        UINT64 Locked               : 1;
        UINT64 ReservedP            : 10;
        UINT64 GpaPageNumber        : 52;
    };
} HV_X64_MSR_HYPERCALL_CONTENTS, *PHV_X64_MSR_HYPERCALL_CONTENTS;

#define HV_CRASH_MAXIMUM_MESSAGE_SIZE     4096ull

typedef union _HV_CRASH_CTL_REG_CONTENTS
{
    UINT64 AsUINT64;
    struct
    {
        UINT64 Reserved      : 58; // Reserved bits
        UINT64 PreOSId       : 3;  // Crash occurred in the preOS environment
        UINT64 NoCrashDump   : 1;  // Crash dump will not be captured
        UINT64 CrashMessage  : 1;  // P3 is the PA of the message, P4 is the length in bytes
        UINT64 CrashNotify   : 1;  // Log contents of crash parameter system registers
    };
} HV_CRASH_CTL_REG_CONTENTS, *PHV_CRASH_CTL_REG_CONTENTS;

#pragma warning(default : 4201)
