/** @file

    This file contains types and constants shared between
    the PowerManagementDevice virtual device and the UEFI firmware.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

enum
{
    // Power management I/O offsets from base port address
    PowerMgmtStatusRegOffset                = 0x00,       // two-byte value
        PowerMgmtTimerOverflowMask          = 0x0001,     // The PM timer overflowed
        PowerMgmtWakeStatusMask             = 0x8000,     // Indicates hardware is ready for OSPM to Resume from Sx
    PowerMgmtResumeEnableRegOffset          = 0x02,       // two-byte value
        PowerMgmtEnableTimerOverflowMask    = 0x0001,     // Timer overflow should interrupt
    PowerMgmtControlRegOffset               = 0x04,       // two-byte value
        PowerMgmtControlSuspendEnableMask   = 0x2000,     // Enable the specified suspend type
        PowerMgmtControlSuspendTypeMask     = 0x1C00,     // Suspend type field
        PowerMgmtControlGlobalRlsMask       = 0x0004,     // Generate SMI
        PowerMgmtControlSciEnableMask       = 0x0001,     // Events should cause SCI =  not SMI
    PowerMgmtTimerRegOffset                 = 0x08,       // four-byte value (read only,
    PowerMgmtGenPurposeStatusRegOffset      = 0x0C,       // two-byte value
    PowerMgmtGenPurposeEnableRegOffset      = 0x0E,       // two-byte value
    PowerMgmtProcControlRegOffset           = 0x10,       // four-byte value
    PowerMgmtProcL2RegOffset                = 0x14,       // one-byte value
    PowerMgmtProcL3RegOffset                = 0x15,       // one-byte value
    PowerMgmtGlobalStatusRegOffset          = 0x18,       // two-byte value
        PowerMgmtStatusGpMask               = 0x0080,     // One of the GP event flags is set
        PowerMgmtStatusPmMask               = 0x0040,     // One of the PM event flags is set
        PowerMgmtStatusApmcWriteMask        = 0x0020,     // APMC register (B2, was written
        PowerMgmtStatusDeviceMask           = 0x0010,     // One device event flags is set
        PowerMgmtStatusGlobalRlsWriteMask   = 0x0001,     // GlbRls bit of control reg written
    PowerMgmtDeviceStatusRegOffset          = 0x1C,       // four-byte value
    PowerMgmtGlobalEnableRegOffset          = 0x20,       // two-byte value
        PowerMgmtGlobalEnableBiosEnableMask = 0x0002,     // Will GlbRls write cause SMI?
    PowerMgmtGlobalControlRegOffset         = 0x28,       // four-byte value
        PowerMgmtGlobalControlEndOfSmiMask  = 0x00010000, // Is SMI already pending?
        PowerMgmtGlobalControlBiosRlsMask   = 0x00000002, // Generate SCI?
        PowerMgmtGlobalControlSmiEnableMask = 0x00000001, // Is SMI generation enabled?
    PowerMgmtDeviceControlRegOffset         = 0x2C,       // four-byte value
    PowerMgmtGeneralInput1RegOffset         = 0x30,       // one-byte value (read only,
    PowerMgmtGeneralInput2RegOffset         = 0x31,       // one-byte value (read only,
    PowerMgmtGeneralInput3RegOffset         = 0x32,       // one-byte value (read only,
    PowerMgmtResetRegister                  = 0x33,       // one-byte value
        PowerMgmtResetValue                 = 0x01,       // Reset the VM
    PowerMgmtGeneralOutput0RegOffset        = 0x34,       // one-byte value
    PowerMgmtGeneralOutput2RegOffset        = 0x35,       // one-byte value
    PowerMgmtGeneralOutput3RegOffset        = 0x36,       // one-byte value
    PowerMgmtGeneralOutput4RegOffset        = 0x37,       // one-byte value
};

enum
{
    //
    // This is the default base register when running in ACPI mode
    // (i.e. for UEFI VMs).
    //

    PowerMgmtDefaultBaseRegister = 0x400,
};

