/** @file

  This file contains declarations and definitions used globally in the
  MsvmPkg AcpiPlatformDxe driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

#include <IndustryStandard/Acpi.h>

#include <AcpiTables.h>
#include <Library/ConfigLib.h>

EFI_STATUS
Oem0InitializeTable(
    IN OUT  EFI_ACPI_DESCRIPTION_HEADER* Table
    );

EFI_STATUS
DsdtInitializeTable(
    IN OUT  EFI_ACPI_DESCRIPTION_HEADER* Dsdt
    );

EFI_STATUS
SpcrInitializeTable(
    IN OUT  EFI_ACPI_DESCRIPTION_HEADER* Spcr
    );

EFI_STATUS
FacpInitializeTable(
    IN OUT  EFI_ACPI_DESCRIPTION_HEADER* Facp
    );

EFI_STATUS
WdatInitializeTable(
    IN OUT  EFI_ACPI_DESCRIPTION_HEADER* Wdat
    );

extern BOOLEAN mHardwareIsolatedNoParavisor;