/** @file
  Helper definition for PCI BARs defined in the PCI specification.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

#include <Base.h>

// Attribute types for BARs. See PCI Local Bus Specification Revision 3.0, section 6.2.5.1
#pragma warning(disable : 4201)
typedef struct _PCI_BAR_FORMAT
{
    union {
        struct
        {
            UINT32 MemorySpaceIndicator:1;
            UINT32 MemoryType:2;
            UINT32 Prefetchable:1;
            UINT32 Address:28;
        } Memory;

        struct
        {
            UINT32 IoSpaceIndicator:1;
            UINT32 Reserved:1;
            UINT32 Address:30;
        };

        UINT32 AsUINT32;
    };
} PCI_BAR_FORMAT;
#pragma warning(default : 4201)

#define PCI_BAR_MEMORY_TYPE_64BIT 0x2