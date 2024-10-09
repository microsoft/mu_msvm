/** @file
  Hypervisor interactions during PEI.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

extern BOOLEAN mParavisorPresent;
extern UINT32 mIsolationType;
extern UINT32 mSharedGpaBit;

VOID
HvDetectIsolation(
    VOID
    );

VOID
HvDetectSvsm(
    IN  VOID    *SecretsPage,
    OUT UINT64  *SvsmBase,
    OUT UINT64  *SvsmSize
    );

typedef struct _SNP_SECRETS {
    UINT8   Reserved[0x140];
    UINT64  SvsmBase;
    UINT64  SvsmSize;
    UINT64  SvsmCallingArea;
} SNP_SECRETS, *PSNP_SECRETS;
