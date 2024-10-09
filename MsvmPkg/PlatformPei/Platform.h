/** @file
  Definitions relating to the Hyper-V "Platform" PEI Module.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Library/CrashLib.h>
#include <Library/HvHypercallLib.h>

#define PEI_FAIL_FAST_IF_FAILED(Status) \
    do \
    { \
        if (EFI_ERROR(Status)) \
        { \
            FAIL_FAST_INITIALIZATION_FAILURE(Status); \
        } \
    } while(0)

#if defined(MDE_CPU_X64)

//
// On X64, the config blob starts after the end of the firmware, and after
// the 6 pages for pagetables, 1 page for GDT entries, and 2 free RW pages.
//

#define MISC_PAGE_COUNT_PAGE_TABLES 6
#define MISC_PAGE_COUNT_GDT_ENTRIES 1
#define MISC_PAGE_COUNT_FREE_RW     2

#define MISC_PAGE_COUNT_TOTAL ( \
    MISC_PAGE_COUNT_PAGE_TABLES + \
    MISC_PAGE_COUNT_GDT_ENTRIES + \
    MISC_PAGE_COUNT_FREE_RW)

#define MISC_PAGE_OFFSET_FREE_RW ( \
    MISC_PAGE_COUNT_PAGE_TABLES + \
    MISC_PAGE_COUNT_GDT_ENTRIES)

#endif

typedef struct _PLATFORM_INIT_CONTEXT
{
    struct _UEFI_CONFIG_HEADER *StartOfConfigBlob;
    HV_HYPERCALL_CONTEXT HvHypercallContext;
    UINT8 PhysicalAddressWidth;

#if defined (MDE_CPU_X64)

    struct _HV_PAGES *HvPages;

#endif
} PLATFORM_INIT_CONTEXT, *PPLATFORM_INIT_CONTEXT;
