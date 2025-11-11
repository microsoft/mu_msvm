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
  @param[out]     Guid            Pointer to buffer to receive the unique GUID associated with the initialized BKS3.
  @param[in, out] GuidSize        On input, pointer to the maximum size of Guid buffer.
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

  if ((Guid == NULL) || (GuidSize == NULL)) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsmDddiApi: Null pointer argument in Guid, or Guid size is invalid\n"
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
  Status = AziHsmMborDecoderInit (&Decoder, OutBuffer.HostAddress, (UINT16)ResponseSize);
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

    ZeroMem (Guid, *GuidSize);
    if (*GuidSize < AZIHSM_DDI_INIT_BKS3_RESP_GUID_LENGTH) {
      DEBUG ((
        DEBUG_ERROR,
        "AziHsmDdiApi: Guid size (%d) is smaller than expected (%d)\n",
        *GuidSize,
        AZIHSM_DDI_INIT_BKS3_RESP_GUID_LENGTH
        ));
      Status = EFI_BUFFER_TOO_SMALL;
      goto Cleanup;
    }

    CopyMem (Guid, InitBks3Resp.Guid, AZIHSM_DDI_INIT_BKS3_RESP_GUID_LENGTH);
    *GuidSize = AZIHSM_DDI_INIT_BKS3_RESP_GUID_LENGTH;

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
  ){

    /*
    Function for BKS3 sealing.
    1. Encode SetSeal request
    2. Fire hsmcommand for setseal
    3. return seal is success or fail 

    */

    // Local variable
    AZIHSM_MBOR_ENCODER         Encoder            = {0};
    AZIHSM_MBOR_DECODER         Decoder            = {0};
    AZIHSM_DMA_BUFFER           InBuffer           = {0};
    AZIHSM_DMA_BUFFER           OutBuffer          = {0};
    DDI_OPERATION_CODE          OpCode             = 0;
    UINTN                       EncodedSize        = 0;
    UINTN                       DecodedSize        = 0;
    EFI_STATUS                  Status             = EFI_SUCCESS;
    UINT32                      FwSts              = 0;
    BOOLEAN                     InBufferAllocated  = FALSE;
    BOOLEAN                     OutBufferAllocated = FALSE;
    UINT32                      ResponseSize       = 0;

    AZIHSM_CP_CMD_SQE_SRC_DATA  SessionData        = {0};
    // Local variables for SetSealedBks3 
    AZIHSM_DDI_SET_SEALED_BKS3_REQ  SetSealedBks3Req  = {0};
    AZIHSM_DDI_SET_SEALED_BKS3_RESP SetSealedBks3Resp = FALSE; // Boolean response
    UINT8 SetSealedBks3Data[AZIHSM_DDI_SET_SEALED_BKS3_REQ_MAX_DATA_LENGTH];
    
    // Initialize SetSealedBks3 data
    ZeroMem(SetSealedBks3Data, sizeof(SetSealedBks3Data));

    //log test started
    DEBUG((DEBUG_INFO, "AzihsmDdiApi: AziHsmSetSealedBks3 started\n"));

    // Validate input parameter
    if (State == NULL) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: Invalid State parameter in AziHsmSetSealedBks3\n"));
        return EFI_INVALID_PARAMETER;
    }

    // Ensure PciIo is available
    if (State->PciIo == NULL) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: PciIo is not initialized in State\n"));
        return EFI_DEVICE_ERROR;
    }
    //check datablob is null or not
    if((DataBlob == NULL) || IsSealSuccess == NULL) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: Invalid DataBlob or IsSealSuccess parameter in AziHsmSetSealedBks3\n"));
        return EFI_INVALID_PARAMETER;
    }
    //check if datasize is valid, else return error
    if((DataSize == 0) || (DataSize > AZIHSM_DDI_SET_SEALED_BKS3_REQ_MAX_DATA_LENGTH)) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: Invalid data size for SetSealedBks3 request\n"));
        return EFI_INVALID_PARAMETER;
    }

    // Init DMA buffers first
    Status = AziHsmDmaBufferAlloc(State->PciIo,  EFI_SIZE_TO_PAGES(AZIHSM_DDI_DMA_BUFFER_SIZE), &InBuffer);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: Failed to initialize InBuffer: %r\n", Status));
        goto Cleanup;
    } else {
        InBufferAllocated = TRUE;
    }

    //Init DMA out buffer
    Status = AziHsmDmaBufferAlloc(State->PciIo,  EFI_SIZE_TO_PAGES(AZIHSM_DDI_DMA_BUFFER_SIZE), &OutBuffer);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: AziHsmSetSealedBks3 Failed to initialize OutBuffer: %r\n", Status));
        goto Cleanup;
    } else {
        OutBufferAllocated = TRUE;
    }

    // Clear the DMA input buffer
    ZeroMem(InBuffer.HostAddress, InBuffer.NumberOfBytes);

    // Init MBOR encoder using DMA buffer directly
    if(InBuffer.NumberOfBytes > MAX_UINT16) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: InBuffer size exceeds MAX_UINT16\n"));
        goto Cleanup;
    }
    Status = AziHsmMborEncoderInit(&Encoder, InBuffer.HostAddress, (UINT16)InBuffer.NumberOfBytes);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: AziHsmSetSealedBks3 Failed to initialize MBOR encoder: %r\n", Status));
        goto Cleanup;
    }

    
    // Use the  response data from tpm  for SetSealedBks3
    CopyMem(SetSealedBks3Data, DataBlob, DataSize);
    SetSealedBks3Req.SealedBks3.Length = (UINT16)DataSize;
    DEBUG((DEBUG_INFO, "AzihsmDdiApi: Copied %d bytes for SetSealedBks3 request\n", SetSealedBks3Req.SealedBks3.Length));
   
    // Prepare SetSealedBks3 request with the data we got from InitBks3 response
    SetSealedBks3Req.SealedBks3.Data = SetSealedBks3Data;

    // Encode SetSealedBks3 request (Session ID is null for now)
    Status = AzihsmEncodeSetSealedBks3Req(&Encoder, &ApiRevision, NULL, &SetSealedBks3Req, &EncodedSize);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: Failed to encode SetSealedBks3 request: %r\n", Status));
        goto Cleanup;
    }
    
    // Set operation code for SetSealedBks3
    OpCode = 0;
    ResponseSize = (UINT32)OutBuffer.NumberOfBytes;
    
    // Fire HSM command for SetSealedBks3
    Status = AziHsmFireHsmCmd((AZIHSM_CONTROLLER_STATE *)State, &InBuffer, (UINT32 *)&EncodedSize, &OutBuffer, &ResponseSize, OpCode, &SessionData, &FwSts);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: Failed to fire SetSealedBks3 HSM command: %r\n", Status));
        goto Cleanup;
    }
    
    // Validate response size doesn't exceed buffer capacity
    if (ResponseSize > OutBuffer.NumberOfBytes) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: Response size (%d) exceeds OutBuffer capacity (%d)\n", ResponseSize, OutBuffer.NumberOfBytes));
        Status = EFI_PROTOCOL_ERROR;
        goto Cleanup;
    }
    
    // Check FwSts for success (0 means success)
    DEBUG((DEBUG_INFO, "AzihsmDdiApi: SetSealedBks3 HSM command FwSts: %d\n", FwSts));
    
    // Handle firmware error status
    if (FwSts != 0) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: SetSealedBks3 HSM command failed with FwSts: %d\n", FwSts));
        Status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }
    
    // Initialize decoder with received buffer for SetSealedBks3
    if (ResponseSize > MAX_UINT16) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: Response size exceeds MAX_UINT16: %d\n", ResponseSize));
        Status = EFI_PROTOCOL_ERROR;
        goto Cleanup;
    }
    Status = AziHsmMborDecoderInit(&Decoder, OutBuffer.HostAddress, (UINT16)ResponseSize);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: Failed to initialize MBOR decoder for SetSealedBks3: %r\n", Status));
        goto Cleanup;
    }
    
    // Decode SetSealedBks3 response (now returns boolean based on DDI status)
    Status = AzihsmDecodeSetSealedBks3Resp(&Decoder, &SetSealedBks3Resp, &DecodedSize);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: Failed to decode SetSealedBks3 response: %r\n", Status));
        goto Cleanup;
    }
    
    // Check if decoded size matches response size
    if (DecodedSize != ResponseSize) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: SetSealedBks3 decoded size (%d) does not match response size (%d)\n", DecodedSize, ResponseSize));
        Status = EFI_PROTOCOL_ERROR;
        goto Cleanup;
    }
    
    // Validate that FwSts and decoded response are consistent
    BOOLEAN ExpectedResponse = (FwSts == 0) ? TRUE : FALSE;
    if (SetSealedBks3Resp == ExpectedResponse) {
        DEBUG((DEBUG_INFO, "AzihsmDdiApi: SetSealedBks3 response  successful - FwSts: %d, Response: %a\n", 
               FwSts, SetSealedBks3Resp ? "TRUE" : "FALSE"));
    } else {
        DEBUG((DEBUG_WARN, "AzihsmDdiApi: SetSealedBks3 response mismatch - FwSts: %d, Response: %a, Expected: %a\n",
               FwSts, SetSealedBks3Resp ? "TRUE" : "FALSE", ExpectedResponse ? "TRUE" : "FALSE"));
    }
    
    // Set the output parameter to indicate sealing success
    *IsSealSuccess = SetSealedBks3Resp;

    DEBUG((DEBUG_INFO, "AzihsmDdiApi: SetSealedBks3  completed - Result: %a\n", SetSealedBks3Resp ? "SUCCESS" : "FAILURE"));

Cleanup:
    // Clear sensitive data from DMA buffers before freeing
    if (InBufferAllocated) {
        ZeroMem(InBuffer.HostAddress, InBuffer.NumberOfBytes);
        AziHsmDmaBufferFree(&InBuffer);
    }
    
    if (OutBufferAllocated) {
        ZeroMem(OutBuffer.HostAddress, OutBuffer.NumberOfBytes);
        AziHsmDmaBufferFree(&OutBuffer);
    }
    
    // Clear local sensitive data buffers
    ZeroMem(SetSealedBks3Data, sizeof(SetSealedBks3Data));
    
    return Status;
}

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
  ){
    // Local variable
    AZIHSM_MBOR_ENCODER         Encoder            = {0};
    AZIHSM_MBOR_DECODER         Decoder            = {0};
    AZIHSM_DMA_BUFFER           InBuffer           = {0};
    AZIHSM_DMA_BUFFER           OutBuffer          = {0};
    DDI_OPERATION_CODE          OpCode             = 0;
    UINTN                       EncodedSize        = 0;
    UINTN                       DecodedSize        = 0;
    EFI_STATUS                  Status             = EFI_SUCCESS;
    UINT32                      FwSts              = 0;
    BOOLEAN                     InBufferAllocated  = FALSE;
    BOOLEAN                     OutBufferAllocated = FALSE;
    UINT32                      ResponseSize       = 0;

    AZIHSM_CP_CMD_SQE_SRC_DATA  SessionData        = {0};
    // Local variables for GetSealedBks3 
    AZIHSM_DDI_GET_SEALED_BKS3_RESP GetSealedBks3Resp = {0};
    UINT8 GetSealedBks3Data[AZIHSM_DDI_GET_SEALED_BKS3_REQ_MAX_DATA_LENGTH];
    
    // Initialize GetSealedBks3 data
    ZeroMem(GetSealedBks3Data, sizeof(GetSealedBks3Data));

    DEBUG((DEBUG_INFO, "AzihsmDdiApi: GetSealedBks3 started\n"));

    // Validate input parameters
    if (State == NULL) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: Invalid State parameter in AziHsmGetSealedBks3\n"));
        return EFI_INVALID_PARAMETER;
    }

    // Ensure PciIo is available
    if (State->PciIo == NULL) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: PciIo is not initialized in State\n"));
        return EFI_DEVICE_ERROR;
    }
    
    // Validate output parameters
    if((DataBlob == NULL) || (DataSize == NULL) || (DataBlobSize == 0)) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: Invalid output parameter in AziHsmGetSealedBks3\n"));
        return EFI_INVALID_PARAMETER;
    }
    // Init DMA buffers first
    Status = AziHsmDmaBufferAlloc(State->PciIo,  EFI_SIZE_TO_PAGES(AZIHSM_DDI_DMA_BUFFER_SIZE), &InBuffer);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: Failed to initialize InBuffer: %r\n", Status));
        goto Cleanup;
    } else {
        InBufferAllocated = TRUE;
    }

    //Init DMA out buffer
    Status = AziHsmDmaBufferAlloc(State->PciIo,  EFI_SIZE_TO_PAGES(AZIHSM_DDI_DMA_BUFFER_SIZE), &OutBuffer);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: AziHsmGetSealedBks3 Failed to initialize OutBuffer: %r\n", Status));
        goto Cleanup;
    } else {
        OutBufferAllocated = TRUE;
    }

    // Clear the DMA input buffer
    ZeroMem(InBuffer.HostAddress, InBuffer.NumberOfBytes);

    // Init MBOR encoder using DMA buffer directly
    if(InBuffer.NumberOfBytes > MAX_UINT16) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: InBuffer size exceeds MAX_UINT16\n"));
        Status = EFI_PROTOCOL_ERROR;
        goto Cleanup;
    }
    Status = AziHsmMborEncoderInit(&Encoder, InBuffer.HostAddress, (UINT16)InBuffer.NumberOfBytes);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "AzihsmDdiApi: AziHsmGetSealedBks3 Failed to initialize MBOR encoder: %r\n", Status));
        goto Cleanup;
    }
    //  // Encode GetSealedBks3 request (no data needed, just header)
    // Function now properly handles NULL ApiRev and SessionId
    Status = AzihsmEncodeGetSealedBks3Req(&Encoder, &ApiRevision, NULL, &EncodedSize);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "AziHsmDdi: Failed to encode GetSealedBks3 request: %r\n", Status));
        goto Cleanup;
    }
    
    // Copy encoded data to InBuffer
    CopyMem(InBuffer.HostAddress, Encoder.Buffer, EncodedSize);
    
    // Set operation code for GetSealedBks3
    OpCode = 0;
    ResponseSize = (UINT32)OutBuffer.NumberOfBytes;
    
    // Fire HSM command for GetSealedBks3
    Status = AziHsmFireHsmCmd(((AZIHSM_CONTROLLER_STATE *)State), &InBuffer, (UINT32 *)&EncodedSize, &OutBuffer, &ResponseSize, OpCode, &SessionData, &FwSts);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "AziHsmDdi: Failed to fire GetSealedBks3 HSM command: %r\n", Status));
        goto Cleanup;
    }
     // Validate that FwSts indicates success for a proper response
    if (FwSts != 0) {
        DEBUG((DEBUG_ERROR, "AziHsmDdi: GetSealedBks3 FW operation failed - FwSts: %d\n", FwSts));
        Status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }
    
    // Initialize decoder with received buffer for GetSealedBks3
    Status = AziHsmMborDecoderInit(&Decoder, OutBuffer.HostAddress,  (UINT16)ResponseSize);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "AziHsmDdi: Failed to initialize MBOR decoder for GetSealedBks3: %r\n", Status));
        goto Cleanup;
    }
    
    // Decode GetSealedBks3 response
    GetSealedBks3Resp.SealedBks3.Data = GetSealedBks3Data;
    GetSealedBks3Resp.SealedBks3.Length = sizeof(GetSealedBks3Data);
    Status = AzihsmDecodeGetSealedBks3Resp(&Decoder, &GetSealedBks3Resp, &DecodedSize);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "AziHsmDdi: Failed to decode GetSealedBks3 response: %r\n", Status));
        goto Cleanup;
    }
    
    // Check if decoded size matches response size
    if (DecodedSize != ResponseSize) {
        DEBUG((DEBUG_ERROR, "AziHsmDdi: GetSealedBks3 decoded size (%d) does not match response size (%d)\n", DecodedSize, ResponseSize));
        Status = EFI_PROTOCOL_ERROR;
        goto Cleanup;
    }
    
    // Validate response data size and copy to output buffer
    if (GetSealedBks3Resp.SealedBks3.Length > DataBlobSize) {
        DEBUG((DEBUG_ERROR, "AziHsmDdi: GetSealedBks3 response length (%d) exceeds output buffer size (%d)\n", 
               GetSealedBks3Resp.SealedBks3.Length, DataBlobSize));
        Status = EFI_BUFFER_TOO_SMALL;
        goto Cleanup;
    }
    
    // Copy retrieved sealed data to output buffer
    CopyMem(DataBlob, GetSealedBks3Resp.SealedBks3.Data, GetSealedBks3Resp.SealedBks3.Length);
    *DataSize = GetSealedBks3Resp.SealedBks3.Length;
    
    DEBUG((DEBUG_INFO, "AziHsmDdi: GetSealedBks3 completed successfully - Retrieved %d bytes\n", 
           GetSealedBks3Resp.SealedBks3.Length));
    
    Status = EFI_SUCCESS;

   Cleanup:
    // Clear sensitive data from DMA buffers before freeing
    if (InBufferAllocated) {
        ZeroMem(InBuffer.HostAddress, InBuffer.NumberOfBytes);
        AziHsmDmaBufferFree(&InBuffer);
    }
    
    if (OutBufferAllocated) {
        ZeroMem(OutBuffer.HostAddress, OutBuffer.NumberOfBytes);
        AziHsmDmaBufferFree(&OutBuffer);
    }
    
    // Clear local sensitive data buffers
    ZeroMem(GetSealedBks3Data, sizeof(GetSealedBks3Data));
    
    return Status;
}
