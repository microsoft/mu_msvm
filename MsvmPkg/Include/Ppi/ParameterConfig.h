/** @file
  SEC parameter configuration PPI definition.

  This PPI carries the optional parameter configuration header supplied in
  the initial VP context from SEC to PEI.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PARAMETER_CONFIG_H_
#define PARAMETER_CONFIG_H_

///
/// PPI carrying an optional parameter configuration header from SEC to PEI.
/// A NULL header means that the legacy UEFI configuration blob is in use.
///
typedef struct {
    VOID *ParameterConfigHeader;
} MSVM_PARAMETER_CONFIG_PPI;

extern EFI_GUID gMsvmParameterConfigPpiGuid;

#endif // PARAMETER_CONFIG_H_
