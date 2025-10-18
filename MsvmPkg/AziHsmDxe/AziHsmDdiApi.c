/** @file
    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "AziHsmDdiApi.h"
#include "AziHsmDma.h"

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
  )
{
  /*
   * function for API revision.
   * To  GetApiRev, we need to send a request and verify the response from the hardware.
   * Main logic for sending request and receiving response is:
   * - Use the provided AZIHSM_CONTROLLER_STATE (State parameter)
   * - Create encoder
   * - Init AZIHSM_DMA_BUFFER in buffer
   * - Init AZIHSM_DMA_BUFFER out buffer
   * - opCode = 0 (Not DdiOpCode)
   * - Init empty AZIHSM_CP_CMD_SQE_SRC_DATA session data
   * - Then call AziHsmFireHsmCmd, it will return the response FwStatus
   * - Decode the AZIHSM_DMA_BUFFER (it should contain the API revision response)
   */

  // Local variable
  AZIHSM_MBOR_ENCODER          Encoder            = { 0 };
  AZIHSM_MBOR_DECODER          Decoder            = { 0 };
  AZIHSM_DDI_API_REV_RESPONSE  ApiRev             = { 0 };
  AZIHSM_DMA_BUFFER            InBuffer           = { 0 };
  AZIHSM_DMA_BUFFER            OutBuffer          = { 0 };
  DDI_OPERATION_CODE           OpCode             = 0;
  AZIHSM_CP_CMD_SQE_SRC_DATA   SessionData        = { 0 };
  UINTN                        EncodedSize        = 0;
  UINTN                        DecodedSize        = 0;
  EFI_STATUS                   Status             = EFI_SUCCESS;
  UINT32                       FwSts              = 0;
  BOOLEAN                      InBufferAllocated  = FALSE;
  BOOLEAN                      OutBufferAllocated = FALSE;
  UINT32                       ResponseSize       = 0;
  // declare MBOR In and out buffer
  UINT8  InBufferData[AZIHSM_DDI_DMA_BUFFER_SIZE];

  // clear input buffer
  ZeroMem (InBufferData, sizeof (InBufferData));

  if ((ApiRevisionMax == NULL) || (ApiRevisionMin == NULL)) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: Invalid ApiRevisionMax/Min parameter in AzihsmGetApiRevision\n"));
    return EFI_INVALID_PARAMETER;
  }

  // Validate input parameter
  if (State == NULL) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: Invalid State parameter in AzihsmApiRev\n"));
    return EFI_INVALID_PARAMETER;
  }

  // Ensure PciIo is available
  if (State->PciIo == NULL) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: PciIo is not initialized in State\n"));
    return EFI_DEVICE_ERROR;
  }

  // Init MBOR encoder
  Status = AziHsmMborEncoderInit (&Encoder, InBufferData, sizeof (InBufferData));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: Failed to initialize MBOR encoder: %r\n", Status));
    goto Cleanup;
  }

  // Init DMA buffers
  Status = AziHsmDmaBufferAlloc (State->PciIo, EFI_SIZE_TO_PAGES (sizeof (InBufferData)), &InBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: Failed to initialize InBuffer: %r\n", Status));
    goto Cleanup;
  } else {
    InBufferAllocated = TRUE;
  }

  // Init DMA out buffer same size as input buffer
  Status = AziHsmDmaBufferAlloc (State->PciIo, EFI_SIZE_TO_PAGES (sizeof (InBufferData)), &OutBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: Failed to initialize OutBuffer: %r\n", Status));
    goto Cleanup;
  } else {
    OutBufferAllocated = TRUE;
  }

  // Encode GetRevAPI
  Status = AzihsmEncodeGetApiRevReq (&Encoder, NULL, NULL, &EncodedSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: Failed to encode GetApiRev request: %r\n", Status));
    goto Cleanup;
  }

  CopyMem (InBuffer.HostAddress, InBufferData, EncodedSize);

  // Set operation code
  OpCode = 0;
  // set expectedResponseSize
  ResponseSize = (UINT32)OutBuffer.NumberOfBytes;
  // Fire command
  Status = AziHsmFireHsmCmd (
             (AZIHSM_CONTROLLER_STATE *)State,
             &InBuffer,
             (UINT32 *)&EncodedSize,
             &OutBuffer,
             &ResponseSize,
             OpCode,
             &SessionData,
             &FwSts
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: Failed to fire HSM command: %r\n", Status));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  // check Firmware status
  if (FwSts != 0) {
    DEBUG ((DEBUG_ERROR, "AziHsmDdiApi: GetApirev failed with firmware status : %d\n", FwSts));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  if (OutBuffer.NumberOfBytes > MAX_UINT16) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: OutBuffer size exceeds Max decode size  : %d\n", OutBuffer.NumberOfBytes));
    Status = EFI_PROTOCOL_ERROR;
    goto Cleanup;
  }

  // Init decoder with received buffer
  Status = AziHsmMborDecoderInit (&Decoder, OutBuffer.HostAddress, (UINT16)OutBuffer.NumberOfBytes);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: Failed to initialize MBOR decoder: %r\n", Status));
    goto Cleanup;
  }

  // decode received buffer
  Status = AzihsmDecodeGetApiRevReq (&Decoder, &ApiRev, &DecodedSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: Failed to decode API revision response: %r\n", Status));
    Status = EFI_PROTOCOL_ERROR;
    goto Cleanup;
  }

  // check if decoded size is same as buffer response size
  if (DecodedSize != ResponseSize) {
    DEBUG ((
      DEBUG_ERROR,
      "AzihsmDdiApi: Decoded size (%d) does not match response size (%d)\n",
      DecodedSize,
      ResponseSize
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto Cleanup;
  }

  // print apirev with min and max
  DEBUG (
    (DEBUG_INFO, "AzihsmDdiApi: API Revision - Min: %d.%d, Max: %d.%d\n",
     ApiRev.min.Major, ApiRev.min.Minor, ApiRev.max.Major, ApiRev.max.Minor)
    );
  // Copy to output
  *ApiRevisionMax = ApiRev.max;
  *ApiRevisionMin = ApiRev.min;

Cleanup:
  // Clean up allocated DMA buffers
  if (OutBufferAllocated) {
    AziHsmDmaBufferFree (&OutBuffer);
  }

  if (InBufferAllocated) {
    AziHsmDmaBufferFree (&InBuffer);
  }

  return Status;
}

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
  IN OUT  UINT16 *CONST                      WrappedKeySize // Should pass max memory, and returns wrapped key size
  )
{
  /*
  function for BKS3 initialization.
  1. Encode InitBks3 request
  2. Fire hsmcommand for initbks3
  3. decode init bks3 , we should see 1024 bytes
  */

  // Local variable
  AZIHSM_MBOR_ENCODER  Encoder            = { 0 };
  AZIHSM_MBOR_DECODER  Decoder            = { 0 };
  AZIHSM_DMA_BUFFER    InBuffer           = { 0 };
  AZIHSM_DMA_BUFFER    OutBuffer          = { 0 };
  DDI_OPERATION_CODE   OpCode             = 0;
  UINTN                EncodedSize        = 0;
  UINTN                DecodedSize        = 0;
  EFI_STATUS           Status             = EFI_SUCCESS;
  UINT32               FwSts              = 0;
  BOOLEAN              InBufferAllocated  = FALSE;
  BOOLEAN              OutBufferAllocated = FALSE;
  UINT32               ResponseSize       = 0;

  AZIHSM_CP_CMD_SQE_SRC_DATA  SessionData  = { 0 };
  AZIHSM_DDI_INIT_BKS3_REQ    InitBks3Req  = { 0 };
  AZIHSM_DDI_INIT_BKS3_RESP   InitBks3Resp = { 0 };
  // declare MBOR In and out buffer
  UINT8  InBufferData[AZIHSM_DDI_DMA_BUFFER_SIZE];

  UINT8  InitBks3Data[AZIHSM_DDI_INIT_BKS3_REQ_MAX_DATA_LENGTH];
  UINT8  InitBks3RespData[AZIHSM_DDI_INIT_BKS3_RESP_MAX_DATA_LENGTH];

  DEBUG ((DEBUG_INFO, "AzihsmDdiApi: AzihsmInitBks3 started\n"));

  // Do null checks
  if ((DerivedKey == NULL) || (KeySize == 0)) {
    DEBUG ((DEBUG_ERROR, "AziHsmDddiApi: Null pointer argument in Derived Key, or Keysize : %d\n", KeySize));
    return EFI_INVALID_PARAMETER;
  }

  if ((WrappedKey == NULL) || (WrappedKeySize == 0)) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsmDddiApi: Null pointer argument in Wrapped Key, or Wrapped Key size : %d\n",
      WrappedKeySize
      ));
    return EFI_INVALID_PARAMETER;
  }

  // Validate input parameter
  if (State == NULL) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: Invalid State parameter in AzihsmInitBks3\n"));
    return EFI_INVALID_PARAMETER;
  }

  // Ensure PciIo is available
  if (State->PciIo == NULL) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: PciIo is not initialized in State\n"));
    return EFI_DEVICE_ERROR;
  }

  if (KeySize > AZIHSM_DDI_INIT_BKS3_REQ_MAX_DATA_LENGTH) {
    DEBUG ((DEBUG_ERROR, "Invalid KeySize (Max %d ) : %d\n", AZIHSM_DDI_INIT_BKS3_REQ_MAX_DATA_LENGTH, KeySize));
    return EFI_INVALID_PARAMETER;
  }

  // clear input buffer
  ZeroMem (InBufferData, sizeof (InBufferData));
  ZeroMem (InitBks3Data, sizeof (InitBks3Data));
  ZeroMem (InitBks3RespData, sizeof (InitBks3RespData));

  InitBks3Req.Bks3.Data    = InitBks3Data;
  InitBks3Req.Bks3.Length  = sizeof (InitBks3Data);
  InitBks3Resp.Bks3.Data   = InitBks3RespData;
  InitBks3Resp.Bks3.Length = sizeof (InitBks3RespData);

  // Init MBOR encoder
  Status = AziHsmMborEncoderInit (&Encoder, InBufferData, AZIHSM_DDI_DMA_BUFFER_SIZE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: AzihsmInitBks3 Failed to initialize MBOR encoder: %r\n", Status));
    Status = EFI_PROTOCOL_ERROR;
    goto Cleanup;
  }

  // Init DMA buffers
  Status = AziHsmDmaBufferAlloc (State->PciIo, EFI_SIZE_TO_PAGES (AZIHSM_DDI_DMA_BUFFER_SIZE), &InBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: Failed to initialize InBuffer: %r\n", Status));
    Status = EFI_OUT_OF_RESOURCES;
    goto Cleanup;
  } else {
    InBufferAllocated = TRUE;
  }

  // Init DMA out buffer
  Status = AziHsmDmaBufferAlloc (State->PciIo, EFI_SIZE_TO_PAGES (AZIHSM_DDI_DMA_BUFFER_SIZE), &OutBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: Failed to initialize OutBuffer: %r\n", Status));
    Status = EFI_OUT_OF_RESOURCES;
    goto Cleanup;
  } else {
    OutBufferAllocated = TRUE;
  }

  // Copy received BKS3 Derived Key
  CopyMem (InitBks3Req.Bks3.Data, DerivedKey, KeySize);
  InitBks3Req.Bks3.Length = (UINT16)KeySize;
  DEBUG ((DEBUG_INFO, "AzihsmDdiApi: Bks3Init Request Data Length: %d\n", InitBks3Req.Bks3.Length));

  // Encode InitBks3 request
  Status = AzihsmEncodeInitBks3Req (&Encoder, &ApiRevision, NULL, &InitBks3Req, &EncodedSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: Failed to encode InitBks3 request: %r\n", Status));
    Status = EFI_PROTOCOL_ERROR;
    goto Cleanup;
  }

  // copy encoded data to inbuffer
  CopyMem (InBuffer.HostAddress, Encoder.Buffer, EncodedSize);

  // Set operation code
  OpCode = 0;
  // set expectedResponseSize
  ResponseSize = (UINT32)OutBuffer.NumberOfBytes;

  Status = AziHsmFireHsmCmd (
             (AZIHSM_CONTROLLER_STATE *)State,
             &InBuffer,
             (UINT32 *)&EncodedSize,
             &OutBuffer,
             &ResponseSize,
             OpCode,
             &SessionData,
             &FwSts
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: Failed to fire HSM command: %r\n", Status));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  // check if command is success or not
  if (FwSts != 0) {
    DEBUG ((DEBUG_ERROR, "AzihsmddiApi: HSM Command returned error : %d\n", FwSts));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  DEBUG ((DEBUG_INFO, "AziHsmDdiApi: HSM InitBks3 Command completed successfully\n"));

  // Init decoder with received buffer
  Status = AziHsmMborDecoderInit (&Decoder, OutBuffer.HostAddress, AZIHSM_DDI_DMA_BUFFER_SIZE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: Failed to initialize MBOR decoder: %r\n", Status));
    Status = EFI_PROTOCOL_ERROR;
    goto Cleanup;
  }

  // decode received buffer
  Status = AzihsmDecodeInitBks3Resp (&Decoder, &InitBks3Resp, &DecodedSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AzihsmDdiApi: Failed to decode InitBKS3 response: %r\n", Status));
    Status = EFI_PROTOCOL_ERROR;
    goto Cleanup;
  }

  // check if decoded size is same as buffer response size
  if (DecodedSize != ResponseSize) {
    DEBUG ((
      DEBUG_ERROR,
      "AzihsmDdiApi: Decoded size (%d) does not match response size (%d)\n",
      DecodedSize,
      ResponseSize
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto Cleanup;
  } else {
    if (InitBks3Resp.Bks3.Length > *WrappedKeySize) {
      DEBUG ((
        DEBUG_ERROR,
        "AziHsmDdiApi: BKS3 response length (%d) exceeds wrapped key size (%d)\n",
        InitBks3Resp.Bks3.Length,
        *WrappedKeySize
        ));
      Status = EFI_BUFFER_TOO_SMALL;
      goto Cleanup;
    }

    // copy data to out buffer
    CopyMem (WrappedKey, InitBks3Resp.Bks3.Data, InitBks3Resp.Bks3.Length);
    *WrappedKeySize = InitBks3Resp.Bks3.Length;

    DEBUG ((DEBUG_INFO, "AziHsmDdiApi: BKS3 response length (%d)\n", InitBks3Resp.Bks3.Length));
  }

Cleanup:
  // Clean up allocated DMA buffers
  if (OutBufferAllocated) {
    AziHsmDmaBufferFree (&OutBuffer);
  }

  if (InBufferAllocated) {
    AziHsmDmaBufferFree (&InBuffer);
  }

  // clear Initbuffer data
  ZeroMem (&InitBks3Data, sizeof (InitBks3Data));
  ZeroMem (&InitBks3RespData, sizeof (InitBks3RespData));

  return Status;
}
