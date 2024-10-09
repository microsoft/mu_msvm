/** @file
  A library to simplify access to virtual machine configuration information.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <BiosInterface.h>
#include <UefiConstants.h>

typedef
UINT32
(CONFIG_GET_UINT32)(
    void
    );

typedef
UINT64
(CONFIG_GET_UINT64)(
    void
    );

typedef
void*
(CONFIG_GET_PTR)(
    void
    );

typedef
CHAR8*
(CONFIG_GET_STRING)(
    void
    );

typedef
EFI_GUID*
(CONFIG_GET_GUID)(
    void
    );

typedef
BOOLEAN
(CONFIG_GET_BOOLEAN)(
    void
    );

typedef
void
(CONFIG_SET_UINT64)(
    UINT64 Value
    );

enum
{
    ConfigLibEntropyDataSize =      BiosInterfaceEntropyTableSize,
    ConfigLibSmbiosStringMax =      BiosInterfaceSmbiosStringMax,
    ConfigLibSmbiosV24CpuInfoSize = sizeof(SMBIOS_CPU_INFORMATION)
};

CONFIG_GET_UINT32   GetNfitSize;
CONFIG_SET_UINT64   GetNfit;
CONFIG_SET_UINT64   SetVpmemACPIBuffer;
CONFIG_SET_UINT64   SetGenerationIdAddress;