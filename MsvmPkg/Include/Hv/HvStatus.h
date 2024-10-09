/** @file
  This file declares HV_STATUS codes.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

//
// Status codes for hypervisor operations.
//
typedef UINT16 HV_STATUS, *PHV_STATUS;

//
// MessageId: HV_STATUS_SUCCESS
//
// MessageText:
//
// The specified hypercall succeeded
//
#define HV_STATUS_SUCCESS                ((HV_STATUS)0x0000)

//
// MessageId: HV_STATUS_INVALID_HYPERCALL_CODE
//
// MessageText:
//
// The hypervisor does not support the operation because the specified hypercall code is not supported.
//
#define HV_STATUS_INVALID_HYPERCALL_CODE ((HV_STATUS)0x0002)

//
// MessageId: HV_STATUS_INVALID_HYPERCALL_INPUT
//
// MessageText:
//
// The hypervisor does not support the operation because the encoding for the hypercall input register is not supported.
//
#define HV_STATUS_INVALID_HYPERCALL_INPUT ((HV_STATUS)0x0003)

//
// MessageId: HV_STATUS_INVALID_ALIGNMENT
//
// MessageText:
//
// The hypervisor could not perform the operation because a parameter has an invalid alignment.
//
#define HV_STATUS_INVALID_ALIGNMENT      ((HV_STATUS)0x0004)

//
// MessageId: HV_STATUS_INVALID_PARAMETER
//
// MessageText:
//
// The hypervisor could not perform the operation because an invalid parameter was specified.
//
#define HV_STATUS_INVALID_PARAMETER      ((HV_STATUS)0x0005)

//
// MessageId: HV_STATUS_ACCESS_DENIED
//
// MessageText:
//
// Access to the specified object was denied.
//
#define HV_STATUS_ACCESS_DENIED          ((HV_STATUS)0x0006)

//
// MessageId: HV_STATUS_INVALID_PARTITION_STATE
//
// MessageText:
//
// The hypervisor could not perform the operation because the partition is entering or in an invalid state.
//
#define HV_STATUS_INVALID_PARTITION_STATE ((HV_STATUS)0x0007)

//
// MessageId: HV_STATUS_OPERATION_DENIED
//
// MessageText:
//
// The operation is not allowed in the current state.
//
#define HV_STATUS_OPERATION_DENIED       ((HV_STATUS)0x0008)

//
// MessageId: HV_STATUS_UNKNOWN_PROPERTY
//
// MessageText:
//
// The hypervisor does not recognize the specified partition property.
//
#define HV_STATUS_UNKNOWN_PROPERTY       ((HV_STATUS)0x0009)

//
// MessageId: HV_STATUS_PROPERTY_VALUE_OUT_OF_RANGE
//
// MessageText:
//
// The specified value of a partition property is out of range or violates an invariant.
//
#define HV_STATUS_PROPERTY_VALUE_OUT_OF_RANGE ((HV_STATUS)0x000A)

//
// MessageId: HV_STATUS_INSUFFICIENT_MEMORY
//
// MessageText:
//
// There is not enough memory in the hypervisor pool to complete the operation.
//
#define HV_STATUS_INSUFFICIENT_MEMORY    ((HV_STATUS)0x000B)

//
// MessageId: HV_STATUS_PARTITION_TOO_DEEP
//
// MessageText:
//
// The maximum partition depth has been exceeded for the partition hierarchy.
//
#define HV_STATUS_PARTITION_TOO_DEEP     ((HV_STATUS)0x000C)

//
// MessageId: HV_STATUS_INVALID_PARTITION_ID
//
// MessageText:
//
// A partition with the specified partition Id does not exist.
//
#define HV_STATUS_INVALID_PARTITION_ID   ((HV_STATUS)0x000D)

//
// MessageId: HV_STATUS_INVALID_VP_INDEX
//
// MessageText:
//
// The hypervisor could not perform the operation because the specified VP index is invalid.
//
#define HV_STATUS_INVALID_VP_INDEX       ((HV_STATUS)0x000E)

//
// MessageId: HV_STATUS_NOT_FOUND
//
// MessageText:
//
// The iteration is complete; no addition items in the iteration could be found.
//
#define HV_STATUS_NOT_FOUND              ((HV_STATUS)0x0010)

//
// MessageId: HV_STATUS_INVALID_PORT_ID
//
// MessageText:
//
// The hypervisor could not perform the operation because the specified port identifier is invalid.
//
#define HV_STATUS_INVALID_PORT_ID        ((HV_STATUS)0x0011)

//
// MessageId: HV_STATUS_INVALID_CONNECTION_ID
//
// MessageText:
//
// The hypervisor could not perform the operation because the specified connection identifier is invalid.
//
#define HV_STATUS_INVALID_CONNECTION_ID  ((HV_STATUS)0x0012)

//
// MessageId: HV_STATUS_INSUFFICIENT_BUFFERS
//
// MessageText:
//
// You did not supply enough message buffers to send a message.
//
#define HV_STATUS_INSUFFICIENT_BUFFERS   ((HV_STATUS)0x0013)

//
// MessageId: HV_STATUS_NOT_ACKNOWLEDGED
//
// MessageText:
//
// The previous virtual interrupt has not been acknowledged.
//
#define HV_STATUS_NOT_ACKNOWLEDGED       ((HV_STATUS)0x0014)

//
// MessageId: HV_STATUS_INVALID_VP_STATE
//
// MessageText:
//
// A virtual processor is not in the correct state for the performance of the indicated operation.
//
#define HV_STATUS_INVALID_VP_STATE       ((HV_STATUS)0x0015)

//
// MessageId: HV_STATUS_ACKNOWLEDGED
//
// MessageText:
//
// The previous virtual interrupt has already been acknowledged.
//
#define HV_STATUS_ACKNOWLEDGED           ((HV_STATUS)0x0016)

//
// MessageId: HV_STATUS_INVALID_SAVE_RESTORE_STATE
//
// MessageText:
//
// The indicated partition is not in a valid state for saving or restoring.
//
#define HV_STATUS_INVALID_SAVE_RESTORE_STATE ((HV_STATUS)0x0017)

//
// MessageId: HV_STATUS_INVALID_SYNIC_STATE
//
// MessageText:
//
// The hypervisor could not complete the operation because a required feature of the synthetic interrupt controller (SynIC) was disabled.
//
#define HV_STATUS_INVALID_SYNIC_STATE    ((HV_STATUS)0x0018)

//
// MessageId: HV_STATUS_OBJECT_IN_USE
//
// MessageText:
//
// The hypervisor could not perform the operation because the object or value was either already in use or being used for a purpose that would not permit completing the operation.
//
#define HV_STATUS_OBJECT_IN_USE          ((HV_STATUS)0x0019)

//
// MessageId: HV_STATUS_INVALID_PROXIMITY_DOMAIN_INFO
//
// MessageText:
//
// The proximity domain information is invalid.
//
#define HV_STATUS_INVALID_PROXIMITY_DOMAIN_INFO ((HV_STATUS)0x001A)

//
// MessageId: HV_STATUS_NO_DATA
//
// MessageText:
//
// An attempt to retrieve debugging data failed because none was available.
//
#define HV_STATUS_NO_DATA                ((HV_STATUS)0x001B)

//
// MessageId: HV_STATUS_INACTIVE
//
// MessageText:
//
// The physical connection being used for debuggging has not recorded any receive activity since the last operation.
//
#define HV_STATUS_INACTIVE               ((HV_STATUS)0x001C)

//
// MessageId: HV_STATUS_NO_RESOURCES
//
// MessageText:
//
// There are not enough resources to complete the operation.
//
#define HV_STATUS_NO_RESOURCES           ((HV_STATUS)0x001D)

//
// MessageId: HV_STATUS_FEATURE_UNAVAILABLE
//
// MessageText:
//
// A hypervisor feature is not available to the user.
//
#define HV_STATUS_FEATURE_UNAVAILABLE    ((HV_STATUS)0x001E)

//
// MessageId: HV_STATUS_PARTIAL_PACKET
//
// MessageText:
//
// The debug packet returned is only a partial packet due to an io error.
//
#define HV_STATUS_PARTIAL_PACKET         ((HV_STATUS)0x001F)

//
// MessageId: HV_STATUS_PROCESSOR_FEATURE_NOT_SUPPORTED
//
// MessageText:
//
// The supplied restore state requires an unsupported processor feature.
//
#define HV_STATUS_PROCESSOR_FEATURE_NOT_SUPPORTED ((HV_STATUS)0x0020)

//
// MessageId: HV_STATUS_PROCESSOR_CACHE_LINE_FLUSH_SIZE_INCOMPATIBLE
//
// MessageText:
//
// The supplied restore state requires requires a processor with a different
// cache line flush size.
//
#define HV_STATUS_PROCESSOR_CACHE_LINE_FLUSH_SIZE_INCOMPATIBLE ((HV_STATUS)0x0030)

//
// MessageId: HV_STATUS_INSUFFICIENT_BUFFER
//
// MessageText:
//
// The specified buffer was too small to contain all of the requested data.
//
#define HV_STATUS_INSUFFICIENT_BUFFER    ((HV_STATUS)0x0033)

//
// MessageId: HV_STATUS_INCOMPATIBLE_PROCESSOR
//
// MessageText:
//
// The supplied restore state is for an incompatible processor
// vendor.
//
#define HV_STATUS_INCOMPATIBLE_PROCESSOR ((HV_STATUS)0x0037)

//
// MessageId: HV_STATUS_INSUFFICIENT_DEVICE_DOMAINS
//
// MessageText:
//
// The maximum number of domains supported by the platform I/O remapping
// hardware is currently in use. No domains are available to assign this device
// to this partition.
//
#define HV_STATUS_INSUFFICIENT_DEVICE_DOMAINS ((HV_STATUS)0x0038)

//
// MessageId: HV_STATUS_CPUID_FEATURE_VALIDATION_ERROR
//
// MessageText:
//
// Generic logical processor CPUID feature set validation error.
//
#define HV_STATUS_CPUID_FEATURE_VALIDATION_ERROR ((HV_STATUS)0x003C)

//
// MessageId: HV_STATUS_CPUID_XSAVE_FEATURE_VALIDATION_ERROR
//
// MessageText:
//
// CPUID XSAVE feature validation error.
//
#define HV_STATUS_CPUID_XSAVE_FEATURE_VALIDATION_ERROR ((HV_STATUS)0x003D)

//
// MessageId: HV_STATUS_PROCESSOR_STARTUP_TIMEOUT
//
// MessageText:
//
// Processor startup timed out.
//
#define HV_STATUS_PROCESSOR_STARTUP_TIMEOUT ((HV_STATUS)0x003E)

//
// MessageId: HV_STATUS_SMX_ENABLED
//
// MessageText:
//
// SMX enabled by the BIOS.
//
#define HV_STATUS_SMX_ENABLED ((HV_STATUS)0x003F)

//
// MessageId: HV_STATUS_INVALID_LP_INDEX
//
// MessageText:
//
// The hypervisor could not perform the operation because the specified LP index is invalid.
//
#define HV_STATUS_INVALID_LP_INDEX ((HV_STATUS)0x0041)

//
// MessageId: HV_STATUS_INVALID_REGISTER_VALUE
//
// MessageText:
//
// The supplied register value is invalid.
//
#define HV_STATUS_INVALID_REGISTER_VALUE ((HV_STATUS)0x0050)

//
// MessageId: HV_STATUS_INVALID_VTL_STATE
//
// MessageText:
//
// The supplied virtual trust level is not in the correct state to perform the requested operation.
//
#define HV_STATUS_INVALID_VTL_STATE ((HV_STATUS)0x0051)

//
// MessageId: HV_STATUS_NX_NOT_DETECTED
//
// MessageText:
//
// NX not detected on the machine.
//
#define HV_STATUS_NX_NOT_DETECTED ((HV_STATUS)(0x0055))

//
// MessageId: HV_STATUS_INVALID_DEVICE_ID
//
// MessageText:
//
// The supplied device ID is invalid.
//
#define HV_STATUS_INVALID_DEVICE_ID ((HV_STATUS)0x0057)

//
// MessageId: HV_STATUS_INVALID_DEVICE_STATE
//
// MessageText:
//
// The operation is not allowed in the current device state.
//
#define HV_STATUS_INVALID_DEVICE_STATE ((HV_STATUS)0x0058)

//
// MessageId: HV_STATUS_PENDING_PAGE_REQUESTS
//
// MessageText:
//
// The device had pending page requests which were discarded.
//
#define HV_STATUS_PENDING_PAGE_REQUESTS ((HV_STATUS)0x0059)

//
// MessageId: HV_STATUS_PAGE_REQUEST_INVALID
//
// MessageText:
//
// The supplied page request specifies a memory access that the guest does not
// have permissions to perform.
//
#define HV_STATUS_PAGE_REQUEST_INVALID ((HV_STATUS)0x0060)

//
// MessageId: HV_STATUS_KEY_ALREADY_EXISTS
//
// MessageText:
//
// The entry cannot be added as another entry with the same key already exists.
//
#define HV_STATUS_KEY_ALREADY_EXISTS     ((HV_STATUS)0x0065)

//
// MessageId: HV_STATUS_DEVICE_ALREADY_IN_DOMAIN
//
// MessageText:
//
// The device is already attached to the device domain.
//
#define HV_STATUS_DEVICE_ALREADY_IN_DOMAIN     ((HV_STATUS)0x0066)

//
// MessageId: HV_STATUS_INVALID_CPU_GROUP_ID
//
// MessageText:
//
// A CPU group with the specified CPU group Id does not exist.
//
#define HV_STATUS_INVALID_CPU_GROUP_ID ((HV_STATUS)0x006F)

//
// MessageId: HV_STATUS_INVALID_CPU_GROUP_STATE
//
// MessageText:
//
// The hypervisor could not perform the operation because the CPU group is entering or in an invalid state.
//
#define HV_STATUS_INVALID_CPU_GROUP_STATE ((HV_STATUS)0x0070)

//
// MessageId: HV_STATUS_OPERATION_FAILED
//
// MessageText:
//
// The requested operation failed.
//
#define HV_STATUS_OPERATION_FAILED       ((HV_STATUS)0x0071)

//
// MessageId: HV_STATUS_NOT_ALLOWED_WITH_NESTED_VIRT_ACTIVE
//
// MessageText:
//
// The requested operation is not allowed due to one or more virtual processors
// having nested virtualization active.
//
#define HV_STATUS_NOT_ALLOWED_WITH_NESTED_VIRT_ACTIVE  ((HV_STATUS)0x0072)

//
// MessageId: HV_STATUS_INSUFFICIENT_ROOT_MEMORY
//
// MessageText:
//
// There is not enough memory in the root partition's pool to complete the operation.
//
#define HV_STATUS_INSUFFICIENT_ROOT_MEMORY ((HV_STATUS)0x0073)