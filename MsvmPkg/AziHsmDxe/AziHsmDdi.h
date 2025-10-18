/** @file
    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _AZIHSM_DDI_H_
#define _AZIHSM_DDI_H_

#include "AziHsmMbor.h"
#include "AziHsmCp.h"
// DDI Operation Codes
typedef enum _DDI_OPERATION_CODE {
  // Invalid Operation
  DDI_OP_INVALID = 1001,
  // Get API revision
  DDI_OP_GET_API_REV = 1002,
  // Init BKS3
  DDI_OP_INIT_BKS3 = 1111,
  // Get Sealed BKS3
  DDI_OP_GET_SEALED_BKS3 = 1112,
  // Set Sealed BKS3
  DDI_OP_SET_SEALED_BKS3 = 1113,
  // Provision Part
  DDI_OP_PROVISION_PART = 1114,
} DDI_OPERATION_CODE;

// DDI Status Codes
typedef enum _DDI_STATUS {
  /// Operation was successful
  DDI_STATUS_SUCCESS = 0,
  /// Invalid argument
  DDI_STATUS_INVALID_ARG = 134217731,
  /// General failure
  DDI_STATUS_INTERNAL_ERROR = 134217736,
  /// Unsupported Command
  DDI_STATUS_UNSUPPORTED_CMD = 134217737,
  /// CBOR encoding failed
  DDI_STATUS_DDI_ENCODE_FAILED = 141033473,
  /// CBOR decoding failed
  DDI_STATUS_DDI_DECODE_FAILED = 141033474,
} DDI_STATUS;

// Basic DDI API Revision structure
typedef struct _AZIHSM_DDI_API_REV {
  UINT32    Major;
  UINT32    Minor;
} AZIHSM_DDI_API_REV;
#define AZIHSM_DDI_API_REV_FIELD_COUNT  ((UINT8) 2)// Make sure to update the count if the structure changes

// DDI API Revision Response structure
typedef struct _AZIHSM_DDI_API_REV_RESPONSE {
  // Minimum API Revision supported
  AZIHSM_DDI_API_REV    min;

  // Maximum API Revision supported
  AZIHSM_DDI_API_REV    max;
} AZIHSM_DDI_API_REV_RESPONSE;
#define AZIHSM_DDI_API_REV_RESPONSE_FIELD_COUNT  ((UINT8) 2)// Make sure to update the count if the structure changes

// DDI Request Header structure
typedef struct _AZIHSM_DDI_REQ_HDR {
  struct {
    BOOLEAN               Valid;
    AZIHSM_DDI_API_REV    Value;   // Single API revision, not min/max pair
  } Revision;
  // DDI operation code
  DDI_OPERATION_CODE    DdiOp;

  struct {
    BOOLEAN    Valid;
    UINT16     Value;
  } SessionId;
} AZIHSM_DDI_REQ_HDR;
#define AZIHSM_DDI_REQ_HDR_FIELD_COUNT  ((UINT8)3)// Make sure to update the count if the structure changes

// DDI Response Header structure
typedef struct _AZIHSM_DDI_RSP_HDR {
  // API revision, valid only if Valid field is set to TRUE
  struct {
    BOOLEAN               Valid;
    AZIHSM_DDI_API_REV    Value;   // Single API revision, not min/max pair
  } Revision;
  // DDI Operation
  DDI_OPERATION_CODE    DdiOp;
  // Todo: Structures are not aligned? will there be any requirements for alignment? or padding?
  struct {
    BOOLEAN    Valid;
    UINT16     Value;
  } SessionId;
  // Response DDI Status
  DDI_STATUS            DdiStatus;
  BOOLEAN               fips_approved;
} AZIHSM_DDI_RSP_HDR;
#define AZIHSM_DDI_RSP_HDR_FIELD_COUNT  ((UINT8)5)// Make sure to update the count if the structure changes

//
// Function declarations
//

/**
  Encodes a complete API revision command request into MBOR format.

  This function encodes the complete DdiGetApiRevCmdReq structure which contains:
  - Request header (DdiReqHdr) with optional revision, operation code, and optional session ID
  - Request data (DdiGetApiRevReq) - empty structure
  - Request extension (DdiReqExt) - optional, currently not implemented

  @param[in,out] Encoder     Pointer to the MBOR encoder structure.
  @param[in]     ApiRev      Optional API revision for header (NULL if not used).
  @param[in]     SessionId   Optional session ID (NULL if not used).
  @param[out]    EncodedSize Pointer to store the size of encoded data in bytes.

  @retval EFI_SUCCESS           API revision request was successfully encoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval Other                 Error from MBOR encoding functions.
**/
EFI_STATUS
AzihsmEncodeGetApiRevReq (
  IN OUT AZIHSM_MBOR_ENCODER   *Encoder,
  IN CONST AZIHSM_DDI_API_REV  *ApiRev,
  IN CONST UINT16              *SessionId,
  OUT UINTN                    *EncodedSize
  );

/**
  Decodes a complete API revision command response from MBOR format.

  This function decodes MBOR format data and extracts the API revision response data
  containing min/max API revision information. The response header is processed
  internally but not exposed to the caller.

  @param[in,out] Decoder     Pointer to the MBOR decoder structure.
  @param[out]    ApiRevData  Pointer to store the decoded API revision response data.
  @param[out]    DecodedSize Pointer to store the size of decoded data in bytes.

  @retval EFI_SUCCESS           API revision response was successfully decoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval EFI_PROTOCOL_ERROR    Field count mismatch or invalid field structure.
  @retval EFI_UNSUPPORTED       Unknown field ID encountered.
  @retval Other                 Error from MBOR decoding functions.
**/
EFI_STATUS
AzihsmDecodeGetApiRevReq (
  IN OUT AZIHSM_MBOR_DECODER       *Decoder,
  OUT AZIHSM_DDI_API_REV_RESPONSE  *ApiRevData,
  OUT UINTN                        *DecodedSize
  );

// define InitBks3 struct request
typedef struct _AZIHSM_DDI_INIT_BKS3_REQ {
  struct {
    UINT8     *Data;  // BKS3 Data
    UINT16    Length; // length of the data
  } Bks3;
} AZIHSM_DDI_INIT_BKS3_REQ;
#define AZIHSM_DDI_INIT_BKS3_REQ_FIELD_COUNT      ((UINT8) 1) // Make sure to update the count if the structure changes
#define AZIHSM_DDI_INIT_BKS3_REQ_MAX_DATA_LENGTH  (48)        // Maximum 48 bytes are supported for now

// define InitBks3 response struct
#define AZIHSM_DDI_INIT_BKS3_RESP_GUID_LENGTH      (16)        // Length of the GUID
#define AZIHSM_DDI_INIT_BKS3_RESP_FIELD_COUNT      ((UINT8) 2) // Make sure to update the count if the structure changes
#define AZIHSM_DDI_INIT_BKS3_RESP_MAX_DATA_LENGTH  (1024)      // Maximum 1024 bytes are supported for now

typedef struct _AZIHSM_DDI_INIT_BKS3_RESP {
  struct {
    UINT8     *Data;  // BKS3 Data
    UINT16    Length; // length of the data
  } Bks3;
  UINT8    Guid[AZIHSM_DDI_INIT_BKS3_RESP_GUID_LENGTH]; // GUID for the BKS3 data
} AZIHSM_DDI_INIT_BKS3_RESP;

/**
  Encodes a complete InitBks3 command request into MBOR format.

  This function encodes the complete DdiInitBks3CmdReq structure which contains:
  - Request header (DdiReqHdr) with optional revision, operation code, and optional session ID
  - Request data (DdiInitBks3Req) containing BKS3 data to be initialized
  - Request extension (DdiReqExt) - optional, currently not implemented

  @param[in,out] Encoder     Pointer to the MBOR encoder structure.
  @param[in]     ApiRev      Optional API revision for header (NULL if not used).
  @param[in]     SessionId   Optional session ID (NULL if not used).
  @param[in]     Request     Pointer to the InitBks3 request data structure containing BKS3 data.
  @param[out]    EncodedSize Pointer to store the size of encoded data in bytes.

  @retval EFI_SUCCESS           InitBks3 request was successfully encoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval Other                 Error from MBOR encoding functions.
**/
EFI_STATUS
AzihsmEncodeInitBks3Req (
  IN OUT AZIHSM_MBOR_ENCODER          *CONST  Encoder,
  IN CONST AZIHSM_DDI_API_REV         *CONST  ApiRev,
  IN CONST UINT16                     *CONST  SessionId,
  IN CONST AZIHSM_DDI_INIT_BKS3_REQ   *CONST  Request,
  OUT UINTN                           *CONST  EncodedSize

  );

/**
  Decodes a complete InitBks3 command response from MBOR format.

  This function decodes MBOR format data into the complete DdiInitBks3CmdResp structure
  which contains:
  - Response header (DdiRespHdr) with optional revision, operation code, optional session ID, status, and FIPS approval
  - Response data (DdiInitBks3Resp) containing processed BKS3 data from the initialization
  - Response extension (DdiRespExt) - optional, currently not implemented

  The function validates the DDI operation code and status before attempting to decode response data.

  @param[in,out] Decoder     Pointer to the MBOR decoder structure.
  @param[out]    Response    Pointer to store the decoded InitBks3 response data structure.
  @param[out]    DecodedSize Pointer to store the size of decoded data in bytes.

  @retval EFI_SUCCESS           InitBks3 response was successfully decoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval EFI_PROTOCOL_ERROR    Field count mismatch, invalid field structure, or wrong operation code.
  @retval EFI_DEVICE_ERROR      DDI operation failed (converted from DDI status codes).
  @retval EFI_UNSUPPORTED       Unknown field ID encountered.
  @retval Other                 Error from MBOR decoding functions.
**/
EFI_STATUS
AzihsmDecodeInitBks3Resp (
  IN OUT AZIHSM_MBOR_DECODER           *CONST  Decoder,
  OUT    AZIHSM_DDI_INIT_BKS3_RESP     *CONST  Response,
  OUT    UINTN                         *CONST  DecodedSize
  );

// Define Set Sealed BKS3 Request struct
typedef struct _AZIHSM_DDI_SET_SEALED_BKS3_REQ {
  struct {
    UINT8     *Data;  // Sealed BKS3 Data, session encryption key
    UINT16    Length; // length of the data
  } SealedBks3;
} AZIHSM_DDI_SET_SEALED_BKS3_REQ;
#define AZIHSM_DDI_SET_SEALED_BKS3_REQ_FIELD_COUNT      ((UINT8) 1) // Make sure to update the count if the structure changes
#define AZIHSM_DDI_SET_SEALED_BKS3_REQ_MAX_DATA_LENGTH  (1024)      // Maximum 1024 bytes are supported for now

// Define Set Sealed BKS3 response either success or failure
typedef BOOLEAN AZIHSM_DDI_SET_SEALED_BKS3_RESP;
#define AZIHSM_DDI_SET_SEALED_BKS3_RESP_FIELD_COUNT  ((UINT8) 0)// Make sure to update the count if the structure changes

/**
  Encodes a complete SetSealedBks3 command request into MBOR format.

  This function encodes the complete DdiSetSealedBks3CmdReq structure which contains:
  - Request header (DdiReqHdr) with optional revision, operation code, and optional session ID
  - Request data (DdiSetSealedBks3Req) containing sealed BKS3 data to be set
  - Request extension (DdiReqExt) - optional, currently not implemented

  The SetSealedBks3 operation stores sealed BKS3 data (session encryption key) in the HSM.
  The sealed data is typically the output from a previous InitBks3 operation.

  @param[in,out] Encoder            Pointer to the MBOR encoder structure.
  @param[in]     ApiRev             Optional API revision for header (NULL if not used).
  @param[in]     SessionId          Optional session ID (NULL if not used).
  @param[in]     SealedBks3Request  Pointer to the SetSealedBks3 request data structure containing sealed BKS3 data.
  @param[out]    EncodedSize        Pointer to store the size of encoded data in bytes.

  @retval EFI_SUCCESS           SetSealedBks3 request was successfully encoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval Other                 Error from MBOR encoding functions.
**/
EFI_STATUS
AzihsmEncodeSetSealedBks3Req (
  IN OUT AZIHSM_MBOR_ENCODER          *CONST      Encoder,
  IN CONST AZIHSM_DDI_API_REV         *CONST      ApiRev,
  IN CONST UINT16                     *CONST      SessionId,
  IN CONST AZIHSM_DDI_SET_SEALED_BKS3_REQ *CONST  SealedBks3Request,
  OUT UINTN                          *CONST       EncodedSize
  );

/**
  Decodes a complete SetSealedBks3 command response from MBOR format.

  This function decodes MBOR format data into the complete DdiSetSealedBks3CmdResp structure
  which contains:
  - Response header (DdiRespHdr) with optional revision, operation code, optional session ID, status, and FIPS approval
  - Response data (DdiSetSealedBks3Resp) - empty structure for this operation
  - Response extension (DdiRespExt) - optional, currently not implemented

  The function validates the DDI operation code and status before attempting to decode response data.
  For SetSealedBks3, the response data is expected to be an empty structure (field count 0).
  The response returns a boolean value based on the DDI status (TRUE for success, FALSE for failure).

  @param[in,out] Decoder     Pointer to the MBOR decoder structure.
  @param[out]    Response    Pointer to store the decoded SetSealedBks3 response data structure (boolean).
  @param[out]    DecodedSize Pointer to store the size of decoded data in bytes.

  @retval EFI_SUCCESS           SetSealedBks3 response was successfully decoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval EFI_PROTOCOL_ERROR    Field count mismatch, invalid field structure, or wrong operation code.
  @retval EFI_DEVICE_ERROR      DDI operation failed (converted from DDI status codes).
  @retval Other                 Error from MBOR decoding functions.
**/
EFI_STATUS
AzihsmDecodeSetSealedBks3Resp (
  IN OUT AZIHSM_MBOR_DECODER           *CONST    Decoder,
  OUT    AZIHSM_DDI_SET_SEALED_BKS3_RESP *CONST  Response,
  OUT    UINTN                         *CONST    DecodedSize
  );

// Define get sealed BKS3 request struct
#define AZIHSM_DDI_GET_SEALED_BKS3_REQ_FIELD_COUNT      ((UINT8) 0)                                    // Make sure to update the count if the structure changes
#define AZIHSM_DDI_GET_SEALED_BKS3_REQ_MAX_DATA_LENGTH  AZIHSM_DDI_SET_SEALED_BKS3_REQ_MAX_DATA_LENGTH // Maximum 1024 bytes are supported for now

/**
  Encodes a complete GetSealedBks3 command request into MBOR format.

  This function encodes the complete DdiGetSealedBks3CmdReq structure which contains:
  - Request header (DdiReqHdr) with optional revision, operation code, and optional session ID
  - Request data (DdiGetSealedBks3Req) - empty structure for this operation
  - Request extension (DdiReqExt) - optional, currently not implemented

  The GetSealedBks3 operation retrieves previously sealed BKS3 data from the HSM.
  No input data is required for this operation as it retrieves the currently stored sealed BKS3.

  @param[in,out] Encoder     Pointer to the MBOR encoder structure.
  @param[in]     ApiRev      Optional API revision for header (NULL if not used).
  @param[in]     SessionId   Optional session ID (NULL if not used).
  @param[out]    EncodedSize Pointer to store the size of encoded data in bytes.

  @retval EFI_SUCCESS           GetSealedBks3 request was successfully encoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval Other                 Error from MBOR encoding functions.
**/
EFI_STATUS
AzihsmEncodeGetSealedBks3Req (
  IN OUT AZIHSM_MBOR_ENCODER          *CONST  Encoder,
  IN CONST AZIHSM_DDI_API_REV         *CONST  ApiRev,
  IN CONST UINT16                     *CONST  SessionId,
  OUT UINTN                           *CONST  EncodedSize
  );

// define Get sealed BKS3 response struct
typedef struct _AZIHSM_DDI_GET_SEALED_BKS3_RESP {
  struct {
    UINT8     *Data;  // Sealed BKS3 Data
    UINT16    Length; // length of the data
  } SealedBks3;
} AZIHSM_DDI_GET_SEALED_BKS3_RESP;
#define AZIHSM_DDI_GET_SEALED_BKS3_RESP_FIELD_COUNT  ((UINT8) 1)// Make sure to update the count if the structure changes

/**
  Decodes a complete GetSealedBks3 command response from MBOR format.

  This function decodes MBOR format data into the complete DdiGetSealedBks3CmdResp structure
  which contains:
  - Response header (DdiRespHdr) with optional revision, operation code, optional session ID, status, and FIPS approval
  - Response data (DdiGetSealedBks3Resp) containing the sealed BKS3 data retrieved from the HSM
  - Response extension (DdiRespExt) - optional, currently not implemented

  The function validates the DDI operation code and status before attempting to decode response data.
  The sealed BKS3 data is extracted and stored in the provided response structure.

  @param[in,out] Decoder     Pointer to the MBOR decoder structure.
  @param[out]    Response    Pointer to store the decoded GetSealedBks3 response data structure.
  @param[out]    DecodedSize Pointer to store the size of decoded data in bytes.

  @retval EFI_SUCCESS           GetSealedBks3 response was successfully decoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval EFI_PROTOCOL_ERROR    Field count mismatch, invalid field structure, or wrong operation code.
  @retval EFI_DEVICE_ERROR      DDI operation failed (converted from DDI status codes).
  @retval EFI_UNSUPPORTED       Unknown field ID encountered.
  @retval Other                 Error from MBOR decoding functions.
**/
EFI_STATUS
AzihsmDecodeGetSealedBks3Resp (
  IN OUT AZIHSM_MBOR_DECODER           *CONST    Decoder,
  OUT    AZIHSM_DDI_GET_SEALED_BKS3_RESP *CONST  Response,
  OUT    UINTN                         *CONST    DecodedSize
  );

#endif // _AZIHSM_DDI_H_
