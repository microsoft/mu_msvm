/** @file

    Definitions, types and structures needed by the BIOS vdev to process events logs
    from the UEFI Event log driver.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

#include <MsvmBase.h>

//
//  Indicates that the event is pending and the associated
//  data may be updated at later time.
//
#define EVENT_FLAG_PENDING      0x00000001

//
//  Indicates that the event is potentially incomplete
//  because it was committed as result of another action
//  e.g. forced commit because of a channel flush.
//
#define EVENT_FLAG_INCOMPLETE   0x00000002

//
//  Describes an event log entry
//  Event specific data may follow immediately after the header
//
typedef struct
{
    //
    // Optional GUID identifing the producer of the event
    //
    EFI_GUID    Producer;
    //
    // Optional GUID used to correlate an event entry with another
    // event entry
    //
    EFI_GUID    CorrelationId;
    //
    // Timestamp when the event was created
    //
    UINT64      CreateTime;
    //
    // Timestamp when the event was committed (may be the same as CreateTime)
    //
    UINT64      CommitTime;
    //
    // Producer specific identifier
    //
    UINT32      EventId;
    //
    // See EVENT_FLAG_nnnnn
    //
    UINT32      Flags;
    //
    // Size of this header structure
    //
    UINT32      HeaderSize;
    //
    // Associated Data Size
    //
    UINT32      DataSize;
    //
    // New fields should be added here.
    //
} EFI_EVENT_DESCRIPTOR;

#define SIZEOF_EFI_EVENT_DESCRIPTOR_REVISION_1  (SIZEOF_THROUGH_FIELD(EFI_EVENT_DESCRIPTOR, DataSize))

//
// Represents an event channel plus data.
// This is used when flushing a UEFI event channel to the BIOS device.
// Data is series of EFI_EVENT_DESCRIPTORs with variable sized data.
//
typedef struct
{
    GUID    Channel;
    UINT32  EventsWritten;
    UINT32  EventsLost;
    UINT32  DataSize;
    UINT8   Data[];
} BIOS_EVENT_CHANNEL;
