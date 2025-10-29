/** @file
  Azure Integrated HSM BKS3 Key Derivation Interface

  This header defines the interface for deriving BKS3 (Boot Key Set 3)
  secrets using TPM 2.0. It provides functions for:
  - TPM-based secret derivation using platform hierarchy
  - Sealing/unsealing data to TPM Null hierarchy
  - KDF operations for BKS3 key generation
  - TCG event logging for HSM operations

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#ifndef __AZIHSMBKS3_H__
#define __AZIHSMBKS3_H__

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/Tpm2CommandLib.h>
#include <Library/Tpm2DeviceLib.h>
#include <Library/TpmMeasurementLib.h>
#include <IndustryStandard/UefiTcgPlatform.h>
#include <IndustryStandard/Tpm20.h>
#include <Library/PrintLib.h>
#include "AziHsmDxe.h"

//
// Constants for iHSM Key Derivation
//
#define AZIHSM_DEFAULT_KEY_LENGTH             48 // 384 bits for BKS3
#define AZIHSM_HASH_USER_INPUT                "AZIHSM_VM_BKS3_KDF"
#define AZIHSM_PRIMARY_KEY_USER_DATA          "AZIHSM_VM_BKS3_PRIMARY_KEY"
#define AZIHSM_APPLICATION_INFO               "AZIHSM_VM_BKS3_HASH_INFO"
#define AZIHSM_PRIMARY_KEY_USER_DATA_MAX_LEN  64
#define AZIHSM_TCG_PCR_INDEX                  6   // PCR index for Azure Integrated HSM measurements
#define AZIHSM_TCG_EVENT_TYPE                 EV_COMPACT_HASH
#define AZIHSM_TCG_EVENT_MAX_SIZE             128
#define AZIHSM_GUID_SIZE                      16  // Size of GUID in bytes
#define AZIHSM_DERIVED_KEY_SIZE               AZIHSM_DEFAULT_KEY_LENGTH
#define AZIHSM_PCI_IDENTIFIER_MAX_LEN         32  // Max length of PCI Identifier (serial number) in bytes

#define AZIHSM_TPM_CMD_BUFSIZE  1024
#define AZIHSM_TPM_RSP_BUFSIZE  1024

#define MAX_HKDF_BLOCKS           255
#define AZIHSM_HKDF_MAX_INFO_LEN  256
#define KEYBITS_SIZE              2048
#define AES_KEYBITS               128
// Max size for KeyedHash template used in Tpm2 command
#define KEYEDHASH_TEMPLATE_MAX_SIZE  128
#define AZIHSM_SEALED_BLOB_MAX_SIZE  1024
#define AZIHSM_BUFFER_MAX_SIZE       1024
#define AZIHSM_EVENT_DESC_MAX        128
#define AZIHSM_FLUSH_CMD_TMP_SIZE    64
#define FLUSH_CONTEXT_PARAMSIZE      10


// Local copies of TPMA_OBJECT bit definitions if not provided by headers (some edk2 variants
// expose only structure forms). Guard with ifndef to avoid redefinition warnings.
#ifndef TPMA_OBJECT_FIXEDTPM
#define TPMA_OBJECT_FIXEDTPM             0x00000002
#define TPMA_OBJECT_STCLEAR              0x00000004
#define TPMA_OBJECT_FIXEDPARENT          0x00000010
#define TPMA_OBJECT_SENSITIVEDATAORIGIN  0x00000020
#define TPMA_OBJECT_USERWITHAUTH         0x00000040
#define TPMA_OBJECT_ADMINWITHPOLICY      0x00000080
#define TPMA_OBJECT_NO_DA                0x00000400
#define TPMA_OBJECT_RESTRICTED           0x00010000
#define TPMA_OBJECT_DECRYPT              0x00020000
#define TPMA_OBJECT_SIGN_ENCRYPT         0x00040000
#endif

// Forward declared opaque types for TPM sealing outputs.
typedef struct {
  UINT8     Data[AZIHSM_BUFFER_MAX_SIZE]; // Buffer
  UINT32    Size;                         // Size of the buffer
} AZIHSM_BUFFER;

//
// Azure Integrated HSM Device Context for TCG Logging
//
typedef struct _AZIHSM_TCG_CONTEXT {
  UINT8     Guid[AZIHSM_GUID_SIZE];
} AZIHSM_TCG_CONTEXT;

//
// Error handling / logging helper macros (direct DEBUG usage; legacy AZIHSM_PRINT_* removed)
//
#define AZIHSM_CHECK_RC(rc, msg) \
  do { \
    if (EFI_ERROR (rc)) { \
      DEBUG ((DEBUG_ERROR, "AziHsm: %a\n", (msg))); \
      goto Cleanup; \
    } \
  } while (0)

//
// Structure to hold derived key material
//
typedef struct {
  UINT8    KeyData[AZIHSM_DEFAULT_KEY_LENGTH];
  UINTN    KeySize;
} AZIHSM_DERIVED_KEY;

/**

  This function implements the secret derivation from TPM using:
  1. Create Primary KeyedHash based on platform hierarchy
  2. HMAC with KDF Input to generate the PRK

  @param[in, out] TpmPlatformHierarchySecret  Pointer to the structure to hold the derived secret.

  @retval EFI_SUCCESS                         The key derivation workflow completed successfully.
  @retval Others                              An error occurred during the workflow.
**/
EFI_STATUS
EFIAPI
AziHsmGetTpmPlatformSecret (
  IN OUT AZIHSM_DERIVED_KEY  *TpmPlatformHierarchySecret
  );

/**
  Measure Azure Integrated HSM device unique GUID to TPM.

  @param[in]  Context  Azure Integrated HSM TCG context

  @retval EFI_SUCCESS  Measurement completed successfully
  @retval Other        Error occurred during measurement
**/
EFI_STATUS
AziHsmMeasureGuidEvent (
  IN AZIHSM_TCG_CONTEXT  *Context
  );

/**
  Function to seal a data buffer using Null Hierarchy

  @param[in]  DataBuffer  Pointer to the data buffer to seal
  @param[out] SealedBuffer  Pointer to the buffer to hold the sealed blob

  @retval EFI_SUCCESS  The sealing operation completed successfully
  @retval Others       An error occurred during the sealing operation
**/
EFI_STATUS
AziHsmSealToTpmNullHierarchy (
  IN  AZIHSM_BUFFER  *DataBuffer,
  OUT AZIHSM_BUFFER  *SealedBuffer
  );

/**
  Function to unseal a data buffer using Null Hierarchy

  @param[in]  SealedBuffer    Pointer to the sealed blob to unseal
  @param[out] UnsealedBuffer  Pointer to the buffer to hold the unsealed data

  @retval EFI_SUCCESS  The unsealing operation completed successfully
  @retval Others       An error occurred during the unsealing operation
**/
EFI_STATUS
AziHsmUnsealUsingTpmNullHierarchy (
  IN  AZIHSM_BUFFER  *SealedBuffer,
  OUT AZIHSM_BUFFER  *UnsealedBuffer
  );

/**
  Given the HSM PCI Identifier(serial number) and the Unsealed buffer, use manual KDF to derive the BKS3 key

  @param[in]  TpmPlatformSecret   Pointer to the unsealed buffer containing necessary data.
  @param[in]  Id                  Pointer to the PCI Identifier (serial number).
  @param[in]  IdLength            Length of the PCI Identifier in bytes.
  @param[out] BKS3Key             Pointer to the structure to hold the derived key material.

  @retval EFI_SUCCESS  The key derivation completed successfully.
  @retval Others       An error occurred during the key derivation.
**/
EFI_STATUS
AziHsmDeriveBKS3fromId (
  IN  AZIHSM_BUFFER       *TpmPlatformSecret,
  IN  UINT8               *Id,
  IN  UINTN               IdLength,
  OUT AZIHSM_DERIVED_KEY  *BKS3Key
  );

/**
  Function to get random bytes from the TPM

  @param[in]  BytesRequested  Number of random bytes to request
  @param[out] OutputBuffer    Pointer to the buffer to hold the random bytes

  @retval EFI_SUCCESS         The operation completed successfully
  @retval Others              An error occurred during the operation
**/
EFI_STATUS
AziHsmTpmGetRandom (
  IN  UINT16  BytesRequested,
  OUT UINT8   *OutputBuffer
  );

#endif // __AZIHSMBKS3_H__
