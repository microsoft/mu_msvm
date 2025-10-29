/** @file
    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _AZIHSMDDIAPI_H_
#define _AZIHSMDDIAPI_H_

#include "AziHsmDdi.h"

// Define buffer size constants for debug builds
#define AZIHSM_DDI_MAX_BUFFER_SIZE  256
#define AZIHSM_DDI_MIN_BUFFER_SIZE  16
#define AZIHSM_DDI_DMA_BUFFER_SIZE  SIZE_4KB

/**
  Retrieve the supported API revision range from the Azure Integrated HSM device.

  This function sends a GetApiRevision request to the HSM device and returns
  the minimum and maximum supported DDI API versions. This information is used
  to determine API compatibility between the driver and HSM firmware.

  @param[in]  State           Pointer to the Azure Integrated HSM controller state.
  @param[out] ApiRevisionMin  Pointer to structure to receive the minimum supported API revision.
  @param[out] ApiRevisionMax  Pointer to structure to receive the maximum supported API revision.

  @retval EFI_SUCCESS           API revision retrieved successfully from HSM device.
  @retval EFI_INVALID_PARAMETER One or more input parameters are invalid:
                                - State is NULL
                                - ApiRevisionMin is NULL
                                - ApiRevisionMax is NULL
  @retval EFI_DEVICE_ERROR      HSM device error:
                                - PciIo is not initialized in State
                                - HSM command execution failed
                                - HSM firmware returned error status
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate DMA buffers for HSM communication.
  @retval EFI_PROTOCOL_ERROR    MBOR encoding, decoding, or response validation failed.

  Usage: Call this function during driver initialization to verify API compatibility
         before attempting other HSM operations.
**/
EFI_STATUS
EFIAPI
AziHsmGetApiRevision (
  IN CONST AZIHSM_CONTROLLER_STATE *CONST  State,
  OUT AZIHSM_DDI_API_REV *CONST            ApiRevisionMin,
  OUT AZIHSM_DDI_API_REV *CONST            ApiRevisionMax
  );

/**
  Initialize BKS3 (Boot Key Store 3) with derived key material.

  This function encodes an InitBks3 request with the provided derived key,
  sends it to the Azure Integrated HSM device via HSM command interface,
  and decodes the response to return the wrapped key from the device.

  @param[in]      State           Pointer to the Azure Integrated HSM controller state.
  @param[in]      ApiRevision     DDI API revision structure containing major and minor version.
  @param[in]      DerivedKey      Pointer to the derived key material to initialize BKS3.
  @param[in]      KeySize         Size of the derived key in bytes. Must not exceed
                                  AZIHSM_DDI_INIT_BKS3_REQ_MAX_DATA_LENGTH.
  @param[out]     WrappedKey      Pointer to buffer to receive the wrapped key from HSM.
  @param[in, out] WrappedKeySize  On input, pointer to the maximum size of WrappedKey buffer.
                                  On output, pointer to the actual size of wrapped key returned.
  @param[out]     Guid            Pointer to buffer to receive the unique GUID associated with the initialized BKS3.
  @param[in,out]  GuidSize        On input, pointer to the maximum size of Guid buffer.
                                  On output, pointer to receive the size of the GUID returned.

  @retval EFI_SUCCESS           BKS3 initialization completed successfully and wrapped key returned.
  @retval EFI_INVALID_PARAMETER One or more input parameters are invalid:
                                - State is NULL
                                - DerivedKey is NULL or KeySize is 0
                                - WrappedKey is NULL or WrappedKeySize is 0
                                - KeySize exceeds maximum allowed length
  @retval EFI_DEVICE_ERROR      HSM device error:
                                - PciIo is not initialized in State
                                - HSM command execution failed
                                - HSM firmware returned error status
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate DMA buffers for HSM communication.
  @retval EFI_PROTOCOL_ERROR    MBOR encoding or decoding operation failed.
  @retval EFI_BUFFER_TOO_SMALL  WrappedKey buffer is too small to hold the response data.

  Usage: Call this function to initialize BKS3 after deriving the key material.
         Ensure WrappedKey buffer is large enough to hold the wrapped key response.
**/
EFI_STATUS
EFIAPI
AziHsmInitBks3 (
  IN CONST AZIHSM_CONTROLLER_STATE   *CONST  State,
  IN CONST AZIHSM_DDI_API_REV                ApiRevision,
  IN CONST UINT8 *CONST                      DerivedKey,
  IN CONST UINTN                             KeySize,
  OUT  UINT8 *CONST                          WrappedKey,
  IN OUT  UINT16 *CONST                      WrappedKeySize, // Should pass max memory, and returns wrapped key size
  OUT UINT8 *CONST                           Guid,
  IN OUT UINT16 *CONST                       GuidSize
  );

  /**
  Set sealed BKS3 (Boot Key Store 3) data in the Azure Integrated HSM device.

  This function encodes a SetSealedBks3 request with the provided sealed data blob,
  sends it to the Azure Integrated HSM device via HSM command interface, and
  returns whether the sealing operation was successful.

  @param[in]  State           Pointer to the Azure Integrated HSM controller state.
  @param[in]  ApiRevision     DDI API revision structure containing major and minor version.
  @param[in]  DataBlob        Pointer to the sealed data blob to be set in HSM.
  @param[in]  DataSize        Size of the data blob in bytes. Must not exceed
                              AZIHSM_DDI_SET_SEALED_BKS3_REQ_MAX_DATA_LENGTH and must be greater than 0.
  @param[out] IsSealSuccess   Pointer to boolean to receive the sealing operation result.
                              TRUE if sealing was successful, FALSE otherwise.

  @retval EFI_SUCCESS           SetSealedBks3 operation completed successfully.
  @retval EFI_INVALID_PARAMETER One or more input parameters are invalid:
                                - State is NULL
                                - DataBlob is NULL or DataSize is 0
                                - IsSealSuccess is NULL
                                - DataSize exceeds maximum allowed length
  @retval EFI_DEVICE_ERROR      HSM device error:
                                - PciIo is not initialized in State
                                - HSM command execution failed
                                - HSM firmware returned error status
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate DMA buffers for HSM communication.
  @retval EFI_PROTOCOL_ERROR    MBOR encoding, decoding, or response validation failed.

  Usage: Call this function to seal BKS3 data after obtaining wrapped key from InitBks3.
         Check IsSealSuccess to determine if the sealing operation completed successfully.
**/
EFI_STATUS
EFIAPI
AziHsmSetSealedBks3 (
  IN CONST AZIHSM_CONTROLLER_STATE   *CONST  State,
  IN CONST AZIHSM_DDI_API_REV                ApiRevision,
  IN CONST UINT8 *CONST                      DataBlob,
  IN CONST UINTN                             DataSize,
  OUT  BOOLEAN *CONST                        IsSealSuccess
  );

  /**
  Retrieve sealed BKS3 (Boot Key Store 3) data from the Azure Integrated HSM device.

  This function encodes a GetSealedBks3 request to retrieve previously sealed BKS3 data
  from the Azure Integrated HSM device via HSM command interface, decodes the response,
  and returns the sealed data blob to the caller.

  @param[in]      State           Pointer to the Azure Integrated HSM controller state.
  @param[in]      ApiRevision     DDI API revision structure containing major and minor version.
  @param[out]     DataBlob        Pointer to buffer to receive the sealed BKS3 data from HSM.
  @param[in]      DataBlobSize    Maximum size of the DataBlob buffer in bytes.
  @param[out]     DataSize        Pointer to receive the actual size of sealed data returned.

  @retval EFI_SUCCESS           GetSealedBks3 operation completed successfully and data returned.
  @retval EFI_INVALID_PARAMETER One or more input parameters are invalid:
                                - State is NULL
                                - DataBlob is NULL
                                - DataSize is NULL
                                - DataBlobSize is 0
  @retval EFI_DEVICE_ERROR      HSM device error:
                                - PciIo is not initialized in State
                                - HSM command execution failed
                                - HSM firmware returned error status
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate DMA buffers for HSM communication.
  @retval EFI_PROTOCOL_ERROR    MBOR encoding, decoding, or response validation failed.
  @retval EFI_BUFFER_TOO_SMALL  DataBlob buffer is too small to hold the sealed data.

  Usage: Call this function to retrieve previously sealed BKS3 data from the HSM device.
         Ensure DataBlob buffer is large enough to hold the expected sealed data.
**/
EFI_STATUS
EFIAPI
AziHsmGetSealedBks3 (
  IN CONST AZIHSM_CONTROLLER_STATE   *CONST  State,
  IN CONST AZIHSM_DDI_API_REV                ApiRevision,
  OUT  UINT8 *CONST                           DataBlob,
  IN CONST UINTN                              DataBlobSize,
  OUT UINTN *CONST                            DataSize
);

#endif // Endof _AZIHSMDDIAPI_H
