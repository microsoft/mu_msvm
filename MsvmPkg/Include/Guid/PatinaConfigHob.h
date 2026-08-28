/** @file -- PatinaConfigHob.h

Contains definitions for the HOB to pass config to Patina.
This must be kept in sync with msvm-patina-bins.

Copyright (c) Microsoft Corporation.

**/

#ifndef PATINA_CONFIG_HOB_H_
#define PATINA_CONFIG_HOB_H_

#include <Uefi.h>

#pragma pack(push, 1)

//
// This HOB is produced to pass configuration to the MSVM Patina bin wrapper. The Rust code must
// have this #[repr(C, packed)].
//
typedef struct _MSVM_PATINA_CONFIG {
  UINT32                  VersionMajor;
  UINT32                  VersionMinor;
 #if defined (MDE_CPU_AARCH64)
  EFI_PHYSICAL_ADDRESS    GicDistributorBase;
  EFI_PHYSICAL_ADDRESS    GicRedistributorBase;
 #endif
} MSVM_PATINA_CONFIG;

#pragma pack(pop)

#define MSVM_CONFIG_HOB_GUID \
  { 0x1feb5a86, 0x7695, 0x4a5c, { 0x93, 0x49, 0x7d, 0x24, 0x3a, 0x43, 0x99, 0x49 } }

extern EFI_GUID  gMsvmPatinaConfigHobGuid;

#define MSVM_CONFIG_HOB_VERSION_MAJOR  1
#define MSVM_CONFIG_HOB_VERSION_MINOR  0

#endif // PATINA_CONFIG_HOB_H_
