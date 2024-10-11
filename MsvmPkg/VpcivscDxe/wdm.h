/** @file
  Certain Windows driver-specific items made it into the VPCI protocol.
  Corral them here.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

#define CmResourceTypeNull                      0
#define CmResourceTypeMemory                    3

//
// Define the bit masks exclusive to type CmResourceTypeMemoryLarge.
//
#define CM_RESOURCE_MEMORY_LARGE_40             0x0200
#define CM_RESOURCE_MEMORY_LARGE_48             0x0400
#define CM_RESOURCE_MEMORY_LARGE_64             0x0800

//
// Define limits for large memory resources
//
#define CM_RESOURCE_MEMORY_LARGE_40_MAXLEN      0x000000FFFFFFFF00
#define CM_RESOURCE_MEMORY_LARGE_48_MAXLEN      0x0000FFFFFFFF0000
#define CM_RESOURCE_MEMORY_LARGE_64_MAXLEN      0xFFFFFFFF00000000

//
// Make sure alignment is made properly by compiler
//
#pragma pack(4)

typedef struct _CM_PARTIAL_RESOURCE_DESCRIPTOR {
    UINT8 Type;
    UINT8 ShareDisposition;
    UINT16 Flags;
    union {
        struct {
            PHYSICAL_ADDRESS Start;
            UINT32 Length;
        } Generic;

        struct {
            UINT64 First;
            UINT64 Second;
        } ForSize;
    } u;
} CM_PARTIAL_RESOURCE_DESCRIPTOR, *PCM_PARTIAL_RESOURCE_DESCRIPTOR;

#pragma pack()

static_assert(sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) == 0x14);
