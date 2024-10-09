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
// Define synthetic interrupt source.
//
typedef union _HV_SYNIC_SINT
{
    UINT64 AsUINT64;
    struct
    {

#if defined(MDE_CPU_AARCH64)

        UINT64 Vector       :10;
        UINT64 ReservedP1   :6;

#else

        UINT64 Vector       :8;
        UINT64 ReservedP1   :8;

#endif

        UINT64 Masked       :1;
        UINT64 AutoEoi      :1;
        UINT64 Polling      :1;
        UINT64 AsIntercept  :1;
        UINT64 Proxy        :1;
        UINT64 ReservedP2   :43;
    };
} HV_SYNIC_SINT, *PHV_SYNIC_SINT;

//
// Define the number of synthetic timers.
//
#define HV_SYNIC_STIMER_COUNT (4)

//
// Define port identifier type.
//
typedef union _HV_PORT_ID
{
    UINT32 AsUINT32;

    struct
    {
        UINT32 Id:24;
        UINT32 Reserved:8;
    };

} HV_PORT_ID, *PHV_PORT_ID;

//
// Define the synthetic interrupt source index type.
//
typedef UINT32 HV_SYNIC_SINT_INDEX, *PHV_SYNIC_SINT_INDEX;

//
// Define the number of synthetic interrupt sources.
//
#define HV_SYNIC_SINT_COUNT (16)

//
// Define synthetic interrupt controller message flags.
//
typedef union _HV_MESSAGE_FLAGS
{
    UINT8 AsUINT8;
    struct
    {
        UINT8 MessagePending:1;
        UINT8 Reserved:7;
    };
} HV_MESSAGE_FLAGS, *PHV_MESSAGE_FLAGS;

//
// Define synthetic interrupt controller message header.
//
typedef struct _HV_MESSAGE_HEADER
{
    HV_MESSAGE_TYPE     MessageType;
    UINT8               PayloadSize;
    HV_MESSAGE_FLAGS    MessageFlags;
    UINT8               Reserved[2];
    union
    {
        HV_PARTITION_ID Sender;
        HV_PORT_ID      Port;
    };

} HV_MESSAGE_HEADER, *PHV_MESSAGE_HEADER;

//
// Define synthetic interrupt controller flag constants.
//
#define HV_EVENT_FLAGS_COUNT        (256 * 8)
#define HV_EVENT_FLAGS_BYTE_COUNT   (256)
#define HV_EVENT_FLAGS_DWORD_COUNT  (256 / sizeof(UINT32))

//
// Define the synthetic interrupt controller event flags format.
//
typedef union
{
    UINT8 Flags8[HV_EVENT_FLAGS_BYTE_COUNT];
    UINT32 Flags32[HV_EVENT_FLAGS_DWORD_COUNT];
} HV_SYNIC_EVENT_FLAGS;

//
// Define the synthetic interrupt flags page layout.
//
typedef struct _HV_SYNIC_EVENT_FLAGS_PAGE
{
    volatile HV_SYNIC_EVENT_FLAGS SintEventFlags[HV_SYNIC_SINT_COUNT];
} HV_SYNIC_EVENT_FLAGS_PAGE, *PHV_SYNIC_EVENT_FLAGS_PAGE;

//
// Define the synthetic timer configuration structure
//
typedef struct _HV_X64_MSR_STIMER_CONFIG_CONTENTS
{
    union
    {
        UINT64 AsUINT64;
        struct
        {
            UINT64 Enable       : 1;
            UINT64 Periodic     : 1;
            UINT64 Lazy         : 1;
            UINT64 AutoEnable   : 1;
            UINT64 ApicVector   : 8;
            UINT64 DirectMode   : 1;
            UINT64 ReservedZ1   : 3;
            UINT64 SINTx        : 4;
            UINT64 ReservedZ2   :44;
        };
    };
} HV_X64_MSR_STIMER_CONFIG_CONTENTS, *PHV_X64_MSR_STIMER_CONFIG_CONTENTS;

//
// Define the format of the SIMP register
//
typedef union _HV_SYNIC_SIMP
{
    UINT64 AsUINT64;
    struct
    {
        UINT64 SimpEnabled : 1;
        UINT64 Preserved   : 11;
        UINT64 BaseSimpGpa : 52;
    };
} HV_SYNIC_SIMP, *PHV_SYNIC_SIMP;

//
// Define the trace buffer index type.
//
typedef UINT32 HV_EVENTLOG_BUFFER_INDEX, *PHV_EVENTLOG_BUFFER_INDEX;

//
// Define all the trace buffer types.
//
typedef enum
{
    HvEventLogTypeGlobalSystemEvents = 0x00000000,
    HvEventLogTypeLocalDiagnostics   = 0x00000001,

    HvEventLogTypeMaximum            = 0x00000001,
} HV_EVENTLOG_TYPE;

//
// Define trace message header structure.
//
typedef struct _HV_EVENTLOG_MESSAGE_PAYLOAD
{

    HV_EVENTLOG_TYPE EventLogType;
    HV_EVENTLOG_BUFFER_INDEX BufferIndex;

} HV_EVENTLOG_MESSAGE_PAYLOAD, *PHV_EVENTLOG_MESSAGE_PAYLOAD;

//
// Define timer message payload structure.
//
typedef struct _HV_TIMER_MESSAGE_PAYLOAD
{
    UINT32          TimerIndex;
    UINT32          Reserved;
    HV_NANO100_TIME ExpirationTime;     // When the timer expired
    HV_NANO100_TIME DeliveryTime;       // When the message was delivered
} HV_TIMER_MESSAGE_PAYLOAD, *PHV_TIMER_MESSAGE_PAYLOAD;

//
// Define IOMMU PRQ message payload structure.
//
typedef struct _HV_IOMMU_PRQ_MESSAGE_PAYLOAD
{
    HV_IOMMU_ID IommuId;
} HV_IOMMU_PRQ_MESSAGE_PAYLOAD, *PHV_IOMMU_PRQ_MESSAGE_PAYLOAD;

//
// Define IOMMU fault message payload structure.
//
typedef enum _HV_IOMMU_FAULT_TYPE
{
    //
    // The IOMMU did not obtain a translation for a DMA transaction.
    //
    HvIommuTranslationFault,

    //
    // Translation request, translated request or untranslated request
    // explicitly blocked.
    //
    HvIommuTranslationBlocked,

    //
    // Hardware blocked an interrupt request.
    //
    HvIommuInterruptFault,

#if defined(MDE_CPU_AARCH64)

    //
    // The IOMMU retrieved a transation for a DMA transaction, but the
    // transaction has insufficient privileges.
    //
    HvIommuPermissionFault,

    //
    // An output address contained an unexpected number of bits.
    //
    HvIommuAddressSizeFault,

    //
    // A TLB match conflict was detected.
    //
    HvIommuTlbMatchConflict,

    //
    // An external abort / unsupported upstream transaction was reported to
    // the IOMMU during transaction processing.
    //
    HvIommuExternalFault,
    HvIommuUnsupportedUpstreamTransaction,

#endif

} HV_IOMMU_FAULT_TYPE, *PHV_IOMMU_FAULT_TYPE;

typedef struct _HV_IOMMU_FAULT_MESSAGE_PAYLOAD
{
    //
    // Indicates the type of the fault.
    //
    HV_IOMMU_FAULT_TYPE Type;

    //
    // Access type of the DMA transaction.
    //
    HV_INTERCEPT_ACCESS_TYPE AccessType;

    //
    // Fault flags.
    //
    struct
    {
        //
        // Indicates that the fault address is valid.
        //
        UINT32 FaultAddressValid : 1;

        //
        // Indicates that the logical device ID is valid.
        //
        UINT32 DeviceIdValid : 1;

    } Flags;

    //
    // Logical ID of the device that caused the fault.
    //
    UINT64 LogicalDeviceId;

    //
    // Device virtual address that caused the fault (if known).
    //
    HV_GVA FaultAddress;

} HV_IOMMU_FAULT_MESSAGE_PAYLOAD, *PHV_IOMMU_FAULT_MESSAGE_PAYLOAD;

//
// Define synthetic interrupt controller message format.
// N.B. The Payload may contain XMM registers that the compiler might expect to
// to be aligned. Therefore, this structure must be 16-byte aligned. The header
// is 16B already.
//
typedef struct __declspec(align(16)) _HV_MESSAGE
{
    HV_MESSAGE_HEADER Header;
    union
    {
        UINT64 Payload[HV_MESSAGE_PAYLOAD_QWORD_COUNT];
        HV_TIMER_MESSAGE_PAYLOAD TimerPayload;
        HV_EVENTLOG_MESSAGE_PAYLOAD TracePayload;
        HV_IOMMU_PRQ_MESSAGE_PAYLOAD IommuPrqPayload;
        HV_IOMMU_FAULT_MESSAGE_PAYLOAD IommuFaultPayload;
    };
} HV_MESSAGE, *PHV_MESSAGE;

//
// Define the synthetic interrupt message page layout.
//

typedef struct _HV_MESSAGE_PAGE
{
    volatile HV_MESSAGE SintMessage[HV_SYNIC_SINT_COUNT];
} HV_MESSAGE_PAGE, *PHV_MESSAGE_PAGE;

//
// Define the format of the SIEFP register
//
typedef union _HV_SYNIC_SIEFP
{
    UINT64 AsUINT64;
    struct
    {
        UINT64 SiefpEnabled : 1;
        UINT64 ReservedP    : 11;
        UINT64 BaseSiefpGpa : 52;
    };
} HV_SYNIC_SIEFP, *PHV_SYNIC_SIEFP;

#pragma warning(default : 4201)
