/** @file
  SEC Platform Type PPI definition.

  This PPI is installed by the SEC phase to communicate the platform type
  to PEI modules.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SEC_PLATFORM_TYPE_H_
#define SEC_PLATFORM_TYPE_H_

///
/// Platform type values passed by the loader.
///
typedef enum {
    MsvmSecPlatformHyperV  = 0,  // Hyper-V with MS extensions
    MsvmSecPlatformGeneric = 1,  // Generic virtualization (no MS extensions)
} MSVM_SEC_PLATFORM_TYPE;

///
/// PPI carrying the platform type from SEC to PEI.
///
typedef struct {
    MSVM_SEC_PLATFORM_TYPE PlatformType;
} MSVM_SEC_PLATFORM_TYPE_PPI;

extern EFI_GUID gMsvmSecPlatformTypePpiGuid;

#endif // SEC_PLATFORM_TYPE_H_
