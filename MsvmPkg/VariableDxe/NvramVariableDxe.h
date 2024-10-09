/** @file
  Declarations for the NVRAM Variable Services driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

#include <BiosInterface.h>

extern
EFI_STATUS
NvramInitialize();

extern
VOID
NvramAddressChangeHandler();

extern
VOID
NvramExitBootServicesHandler(
    BOOLEAN VsmAware
    );

extern
EFI_STATUS
NvramQueryInfo(
        UINT32  Attributes,
    OUT UINT64* MaximumVariableStorageSize,
    OUT UINT64* RemainingVariableStorageSize,
    OUT UINT64* MaximumVariableSize
    );

extern
EFI_STATUS
NvramSetVariable(
    IN CHAR16*   VariableName,
    IN EFI_GUID* VendorGuid,
    IN UINT32    Attributes,
    IN UINTN     DataSize,
    IN void*     Data
    );

extern
EFI_STATUS
NvramGetVariable(
    IN              CHAR16*   VariableName,
    IN              EFI_GUID* VendorGuid,
    OUT OPTIONAL    UINT32*   Attributes,
    IN OUT          UINTN*    DataSize,
    OUT             void*     Data
    );

extern
EFI_STATUS
NvramGetFirstVariableName(
    OUT UINTN*    VariableNameSize,
    OUT CHAR16*   VariableName,
    OUT EFI_GUID* VendorGuid
    );

extern
EFI_STATUS
NvramGetNextVariableName(
    IN OUT  UINTN*    VariableNameSize,
    IN OUT  CHAR16*   VariableName,
    IN OUT  EFI_GUID* VendorGuid
    );

extern
VOID
NvramDebugLog(
    IN CONST CHAR8 *Format,
    ...
    );


