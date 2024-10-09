/** @file

    Types and definitions for the UEFI boot logging channel
    These are shared between the VM worker process and UEFI.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

//
// Device status code groups
//
typedef enum
{
    DeviceStatusBootGroup       = 0x00010000,
    DeviceStatusSecureBootGroup = 0x00020000,
    DeviceStatusNetworkGroup    = 0x00030000
} BOOT_DEVICE_STATUS_GROUP;

//
// Device failure reason codes
// Status codes are made up of a group ID in the high word and a
// status code in the low word
//
// If items are added to this enum the UEFI string mapping function
// PlatformConsoleDeviceStatusString and the corresponding string table
// in PlatformBdsString.uni must be updated
//
typedef enum
{
    BootPending = 0,
    BootDeviceNoFilesystem              = DeviceStatusBootGroup,
    BootDeviceNoLoader,
    BootDeviceIncompatibleLoader,
    BootDeviceReturnedFailure,
    BootDeviceOsNotLoaded,
    BootDeviceOsLoaded,
    BootDeviceNoDevices,
    BootDeviceLoadError,
    SecureBootFailed                    = DeviceStatusSecureBootGroup,
    SecureBootPolicyDenied,
    SecureBootHashDenied,
    SecureBootCertDenied,
    SecureBootInvalidImage,
    SecureBootUnsignedHashNotInDb,
    SecureBootSignedHashNotFound,
    SecureBootNeitherCertNorHashInDb,
    NetworkBootMediaDisconnected        = DeviceStatusNetworkGroup,
    NetworkBootDhcpFailed,
    NetworkBootNoResponse,
    NetworkBootBufferTooSmall,
    NetworkBootDeviceError,
    NetworkBootNoResources,
    NetworkBootServerTimeout,
    NetworkBootCancelled,
    NetworkBootIcmpError,
    NetworkBootTftpError,
    NetworkBootNoBootFile,
    NetworkBootUnexpectedFailure
} BOOT_DEVICE_STATUS;

//
// Returns the group portion of a BOOT_DEVICE_STATUS
// See BOOT_DEVICE_STATUS_GROUP
//
#define GET_BOOT_DEVICE_STATUS_GROUP(status)    ((status) & 0xFFFF0000)

//
// Event Id for Device Boot Attempts
//
#define BOOT_DEVICE_EVENT_ID        1

//
// Information logged for a boot device.
//
typedef struct
{
    BOOT_DEVICE_STATUS          Status;
    EFI_STATUS                  ExtendedStatus;
    UINT16                      BootVariableNumber;
    UINT32                      DevicePathSize;
    UINT8                       DevicePath[];
} BOOTEVENT_DEVICE_ENTRY;

#define BOOT_EVENT_CHANNEL_GUID \
    {0x8cc6713b, 0x360d, 0x4406, {0x92, 0x68, 0xf6, 0xb0, 0xcf, 0xdf, 0xca, 0x91}}
