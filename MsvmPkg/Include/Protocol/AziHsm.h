/** @file
  Provides the protocol definition for EFI_AZI_HSM_PROTOCOL, which provides
  UEFI access to the Azure Integrated HSM.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#ifndef __AZIHSM_H__
#define __AZIHSM_H__

// AziHsmProtocol GUID macro definition
#define MSVM_AZIHSM_PROTOCOL_GUID \
  { \
    0x38976D4E, 0x7454, 0x40CF, {0x9E, 0x12, 0x95, 0xCE, 0x61, 0xA4, 0xCD, 0x6C} \
  };

// External declaration of the global GUID variable  
extern EFI_GUID gMsvmAziHsmProtocolGuid;

// AziHsmProtocol definition
typedef struct _AZIHSM_PROTOCOL {
  UINT64    _Reserved;
} AZIHSM_PROTOCOL;

#endif //__AZIHSM_H__
