/** @file
  This file contains declarations and definitions shared between the
  ACPI table ASLC files and the ACPI platform DXE driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <BiosInterface.h>
#include <IndustryStandard/SerialPortConsoleRedirectionTable.h>
#include <IndustryStandard/WatchdogActionTable.h>

#define STANDARD_HEADER(sig, length, rev)                   \
    {                                                       \
        sig,                                                \
        length,                                             \
        rev,                                                \
        0,                                                  \
        "VRTUAL",                                           \
        SIGNATURE_64('M','I','C','R','O','S','F','T'),      \
        1,                                                  \
        SIGNATURE_32('M','S','F','T'),                      \
        1                                                   \
    },

#pragma pack(push, 1)

typedef struct _VM_ACPI_ENTROPY_TABLE
{
    EFI_ACPI_DESCRIPTION_HEADER Header;
    UINT8 Data[BiosInterfaceEntropyTableSize];
} VM_ACPI_ENTROPY_TABLE;

#define VM_ACPI_ENTROPY_TABLE_SIGNATURE SIGNATURE_32('O','E','M','0')

#define AMD_ACPI_ASPT_TABLE_SIGNATURE SIGNATURE_32('A', 'S', 'P', 'T')

//
// WDAT table.
//

#define VM_HARDWARE_WATCHDOG_ACTION_COUNT 18

typedef struct {
    EFI_ACPI_WATCHDOG_ACTION_1_0_TABLE header;
    EFI_ACPI_WATCHDOG_ACTION_1_0_WATCHDOG_ACTION_INSTRUCTION_ENTRY action[VM_HARDWARE_WATCHDOG_ACTION_COUNT];
} VM_HARDWARE_WATCHDOG_ACTION_TABLE;

#pragma pack(pop)

