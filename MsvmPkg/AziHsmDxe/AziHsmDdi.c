/** @file
    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "AziHsmDdi.h"
#include "AziHsmDma.h"

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>

// Static definitions of field IDs for Request Header : // Readjust constants if field count changes
#define REV_FIELD_ID         ((UINT8)1)
#define DDI_OP_FIELD_ID      ((UINT8)2)
#define SESSION_ID_FIELD_ID  ((UINT8)3)
// Define minimum required field count for request header
#define MIN_REQ_HDR_FIELD_COUNT  ((UINT8)1)

// Define field IDs for response header
#define RSP_REV_FIELD_ID            ((UINT8)1)
#define RSP_DDI_OP_FIELD_ID         ((UINT8)2)
#define RSP_SESSION_ID_FIELD_ID     ((UINT8)3)
#define RSP_DDI_STATUS_FIELD_ID     ((UINT8)4)
#define RSP_FIPS_APPROVED_FIELD_ID  ((UINT8)5)
// Define minimum required field count for response header
#define MIN_RSP_HDR_FIELD_COUNT  ((UINT8)3)

// Define field IDs for general command request/response structures
#define REQ_CMD_HDR_FIELD_ID   ((UINT8)0)
#define REQ_CMD_DATA_FIELD_ID  ((UINT8)1)
#define REQ_CMD_EXT_FIELD_ID   ((UINT8)2)

// Define field IDs for API revision response data structure
#define API_REV_RESP_MIN_FIELD_ID  ((UINT8)1)
#define API_REV_RESP_MAX_FIELD_ID  ((UINT8)2)

// Define field IDs for single API revision structure
#define API_REV_MAJOR_FIELD_ID  ((UINT8)1)
#define API_REV_MINOR_FIELD_ID  ((UINT8)2)

// Define field count limits for API revision command response structure
#define API_REV_CMD_RESP_MIN_FIELD_COUNT  ((UINT8)2)    // hdr + data (minimum required)
#define API_REV_CMD_RESP_MAX_FIELD_COUNT  ((UINT8)3)    // hdr + data + ext (if extension is present)

// define BKS3 INIT request command fields
#define API_INIT_BKS3_CMD_REQ_DATA_FIELD_ID  ((UINT8)1)
// define BKS3 INIT response command fields
#define API_INIT_BKS3_CMD_RESP_DATA_FIELD_ID  ((UINT8)1)
// Define BKS3 INIT response GUID field
#define API_INIT_BKS3_CMD_RESP_GUID_FIELD_ID  ((UINT8)2)

// define BKS3 set sealed request command fields
#define API_SET_SEALED_BKS3_CMD_REQ_DATA_FIELD_ID  ((UINT8)1)
// define BKS3 set sealed response command fields
#define API_SET_SEALED_BKS3_CMD_RESP_DATA_FIELD_ID  ((UINT8)0)

// define BKS3 get sealed request command fields
#define API_GET_SEALED_BKS3_CMD_REQ_DATA_FIELD_ID  ((UINT8)0)
// define BKS3 get sealed response command fields
#define API_GET_SEALED_BKS3_CMD_RESP_DATA_FIELD_ID  ((UINT8)1)

typedef struct _AZIHSM_DDI_GET_API_REV_CMD_REQ {
  AZIHSM_DDI_REQ_HDR    Hdr;           // Field ID 0

  // Field ID 1 - DdiGetApiRevReq (empty struct)
  // Empty struct - no fields needed, just a placeholder
  UINT8                 data_placeholder; // Placeholder for empty struct

  struct {
    // Field ID 2 - DdiReqExt (optional)
    BOOLEAN    Valid;
    UINT32     Reserved;
  } ext;
} AZIHSM_DDI_GET_API_REV_CMD_REQ;
#define AZIHSM_DDI_GET_API_REV_CMD_REQ_FIELDS  ((UINT8) 2)// hdr + data (ext is optional)

typedef struct _AZIHSM_DDI_GET_API_REV_CMD_RESP {
  AZIHSM_DDI_RSP_HDR             Hdr;          // Field ID 0 - Response header
  AZIHSM_DDI_API_REV_RESPONSE    data;         // Field ID 1 - Actual API revision data

  struct {
    // Field ID 2 - DdiRespExt (optional)
    BOOLEAN    Valid;
    UINT32     Reserved;
  } ext;
} AZIHSM_DDI_GET_API_REV_CMD_RESP;
#define AZIHSM_DDI_GET_API_REV_CMD_RESP_FIELDS  ((UINT8) 2)// hdr + data (ext is optional)

// Static functions
STATIC EFI_STATUS
EncodeRequestHeader (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN  AZIHSM_DDI_REQ_HDR      *ReqHdr,
  OUT UINTN                   *EncodedSize
  );

STATIC EFI_STATUS
EncodeCommandRequestHeader (
  IN OUT AZIHSM_MBOR_ENCODER           *const  Encoder,
  IN     DDI_OPERATION_CODE                    DdiOp,
  IN     CONST AZIHSM_DDI_API_REV      *const  ApiRev,
  IN     CONST UINT16                  *const  SessionId,
  OUT    UINTN                         *const  EncodedSize
  );

STATIC EFI_STATUS
DecodeRequestHeader (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  IN OUT  AZIHSM_DDI_REQ_HDR  *ReqHdr,
  OUT UINTN                   *DecodedSize
  );

STATIC EFI_STATUS
EncodeResponseHeader (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN  AZIHSM_DDI_RSP_HDR      *RspHdr,
  OUT UINTN                   *EncodedSize
  );

STATIC EFI_STATUS
DecodeResponseHeader (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  IN OUT  AZIHSM_DDI_RSP_HDR  *RspHdr,
  OUT UINTN                   *DecodedSize
  );

STATIC EFI_STATUS
DecodeCommandResponseHeader (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  IN OUT AZIHSM_DDI_RSP_HDR   *RspHdr,
  OUT    UINTN                *DecodedSize
  );

STATIC EFI_STATUS
DecodeApiRevisionCommandResponseData (
  IN OUT AZIHSM_MBOR_DECODER          *Decoder,
  IN OUT AZIHSM_DDI_API_REV_RESPONSE  *ApiRevData,
  OUT    UINTN                        *DecodedSize
  );

STATIC EFI_STATUS
DecodeApiRevisionResponse (
  IN OUT AZIHSM_MBOR_DECODER           *Decoder,
  IN OUT  AZIHSM_DDI_API_REV_RESPONSE  *ApiRev,
  OUT UINTN                            *DecodedSize
  );

/**
  Converts DDI status codes to EFI status codes.

  This function maps DDI-specific status codes to appropriate EFI status codes
  for consistent error handling in the UEFI environment.

  @param[in] DdiStatus  The DDI status code to convert.

  @retval EFI_SUCCESS           DDI operation was successful.
  @retval EFI_INVALID_PARAMETER DDI reported invalid argument.
  @retval EFI_DEVICE_ERROR      DDI reported internal error.
  @retval EFI_UNSUPPORTED       DDI reported unsupported command.
  @retval EFI_PROTOCOL_ERROR    DDI reported encoding/decoding failure.
**/
STATIC EFI_STATUS
ConvertDdiStatusToEfiStatus (
  IN DDI_STATUS  DdiStatus
  )
{
  switch (DdiStatus) {
    case DDI_STATUS_SUCCESS:
      return EFI_SUCCESS;

    case DDI_STATUS_INVALID_ARG:
      return EFI_INVALID_PARAMETER;

    case DDI_STATUS_INTERNAL_ERROR:
      return EFI_DEVICE_ERROR;

    case DDI_STATUS_UNSUPPORTED_CMD:
      return EFI_UNSUPPORTED;

    case DDI_STATUS_DDI_ENCODE_FAILED:
    case DDI_STATUS_DDI_DECODE_FAILED:
      return EFI_PROTOCOL_ERROR;

    default:
      DEBUG ((DEBUG_WARN, "AziHsmDdi: Unknown DDI status code: %d\n", DdiStatus));
      return EFI_DEVICE_ERROR;  // Default to device error for unknown codes
  }
}

/**
  Encodes a field ID into the MBOR encoder.

  This helper function encodes a field identifier into the given MBOR encoder.
  Field IDs are used to identify different fields in the MBOR data structure.

  @param[in,out] Encoder  Pointer to the MBOR encoder structure.
  @param[in]     FieldId  The field ID to encode.

  @retval EFI_SUCCESS           Field ID was successfully encoded.
  @retval EFI_INVALID_PARAMETER Encoder is NULL.
  @retval Other                 Error from AziHsmMborEncodeU8.
**/
STATIC EFI_STATUS
AziHsmMborEncodeFieldId (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN UINT8                    FieldId
  )
{
  if (Encoder == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Encode the field ID
  return AziHsmMborEncodeU8 (Encoder, FieldId);
}

/**
  Decodes a field ID from the MBOR decoder.

  This helper function decodes a field identifier from the given MBOR decoder.
  Field IDs are used to identify different fields in the MBOR data structure.

  @param[in,out] Decoder  Pointer to the MBOR decoder structure.
  @param[out]    FieldId  Pointer to store the decoded field ID.

  @retval EFI_SUCCESS           Field ID was successfully decoded.
  @retval EFI_INVALID_PARAMETER Decoder is NULL.
  @retval Other                 Error from AziHsmMborDecodeU8.
**/
STATIC EFI_STATUS
AziHsmMborDecodeFieldId (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  OUT UINT8                   *FieldId
  )
{
  if ((Decoder == NULL) || (FieldId == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  // TODO: Peek first to see if we have valid U8 or not, may be its better if we have helper function to get next element type
  return AziHsmMborDecodeU8 (Decoder, FieldId);
}

/**
  Encodes a command request header into MBOR format.

  This helper function encodes the common structure for DDI command requests:
  - Field count (always 2: header + data)
  - Header field ID (always 0)
  - Request header with specified operation, optional revision, and optional session ID

  @param[in,out] Encoder     Pointer to the MBOR encoder structure.
  @param[in]     DdiOp       The DDI operation code for this command.
  @param[in]     ApiRev      Optional API revision for header (NULL if not used).
  @param[in]     SessionId   Optional session ID (NULL if not used).
  @param[out]    EncodedSize Pointer to store the size of encoded data in bytes.

  @retval EFI_SUCCESS           Command header was successfully encoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval Other                 Error from MBOR encoding functions.
**/
STATIC EFI_STATUS
EncodeCommandRequestHeader (
  IN OUT AZIHSM_MBOR_ENCODER           *const  Encoder,
  IN     DDI_OPERATION_CODE                    DdiOp,
  IN     CONST AZIHSM_DDI_API_REV      *const  ApiRev,
  IN     CONST UINT16                  *const  SessionId,
  OUT    UINTN                         *const  EncodedSize
  )
{
  EFI_STATUS          Status           = EFI_SUCCESS;
  CONST UINT8         FIELD_COUNT      = 2;  // hdr (id=0) + data (id=1), ext (id=2) is optional and not implemented
  UINT32              EncoderStartMark = 0;
  UINTN               TempEncodedSize  = 0;
  AZIHSM_DDI_REQ_HDR  Hdr              = { 0 };

  if ((Encoder == NULL) || (EncodedSize == NULL)) {
    *EncodedSize = 0;
    return EFI_INVALID_PARAMETER;
  }

  EncoderStartMark = Encoder->Position;

  // Initialize request header
  Hdr.DdiOp           = DdiOp;
  Hdr.Revision.Valid  = FALSE;
  Hdr.SessionId.Valid = FALSE;

  // Prepare header - optional revision can be provided
  if (ApiRev != NULL) {
    Hdr.Revision.Valid = TRUE;
    Hdr.Revision.Value = *ApiRev;
  }

  if (SessionId != NULL) {
    Hdr.SessionId.Valid = TRUE;
    Hdr.SessionId.Value = *SessionId;
  }

  // Encode command structure: [FieldCount:U8:2] - only hdr and data, ext is optional and not implemented
  Status = AziHsmMborEncodeMap (Encoder, FIELD_COUNT);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Field 0: Request header (DdiReqHdr)
  Status = AziHsmMborEncodeFieldId (Encoder, REQ_CMD_HDR_FIELD_ID);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  Status = EncodeRequestHeader (Encoder, &Hdr, &TempEncodedSize);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

ExitFunction:
  *EncodedSize = Encoder->Position - EncoderStartMark;
  return Status;
}

/**
  Encodes a single API revision structure into MBOR format.

  This helper function encodes a single API revision (DdiApiRev) containing
  major and minor version numbers. This is used for encoding the revision
  field in request/response headers.

  @param[in,out] Encoder      Pointer to the MBOR encoder structure.
  @param[in]     ApiRev       Pointer to the API revision structure to encode.
  @param[out]    EncodedSize  Pointer to store the size of encoded data in bytes.

  @retval EFI_SUCCESS           API revision was successfully encoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval Other                 Error from MBOR encoding functions.
**/
STATIC EFI_STATUS
EncodeApiRevision (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN  AZIHSM_DDI_API_REV      *ApiRev,
  OUT UINTN                   *EncodedSize
  )
{
  EFI_STATUS   Status           = EFI_SUCCESS;
  CONST UINT8  FIELD_COUNT      = AZIHSM_DDI_API_REV_FIELD_COUNT;  // Number of fields in single API revision
  UINT32       EncoderStartMark = 0;

  if ((ApiRev == NULL) || (Encoder == NULL) || (EncodedSize == NULL)) {
    *EncodedSize = 0;
    return EFI_INVALID_PARAMETER;
  }

  EncoderStartMark = Encoder->Position;

  // Encode single API revision structure
  // [FieldCount:U8:2][FIELD_ID:U8:1][MAJOR:U32][FIELD_ID:U8:2][MINOR:U32]
  Status = AziHsmMborEncodeMap (Encoder, FIELD_COUNT);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Encode major version
  Status = AziHsmMborEncodeFieldId (Encoder, API_REV_MAJOR_FIELD_ID);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  Status = AziHsmMborEncodeU32 (Encoder, ApiRev->Major);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Encode minor version
  Status = AziHsmMborEncodeFieldId (Encoder, API_REV_MINOR_FIELD_ID);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  Status = AziHsmMborEncodeU32 (Encoder, ApiRev->Minor);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

ExitFunction:
  *EncodedSize = Encoder->Position - EncoderStartMark;
  return Status;
}

/**
  Decodes a single API revision structure from MBOR format.

  This helper function decodes a single API revision (DdiApiRev) containing
  major and minor version numbers. This is used for decoding the revision
  field in request/response headers.

  @param[in,out] Decoder     Pointer to the MBOR decoder structure.
  @param[in,out] ApiRev      Pointer to the API revision structure to populate.
  @param[out]    DecodedSize Pointer to store the size of decoded data in bytes.

  @retval EFI_SUCCESS           API revision was successfully decoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval EFI_PROTOCOL_ERROR    Field count mismatch or invalid field structure.
  @retval EFI_UNSUPPORTED       Unknown field ID encountered.
  @retval Other                 Error from MBOR decoding functions.
**/
STATIC EFI_STATUS
DecodeApiRevision (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  IN OUT  AZIHSM_DDI_API_REV  *ApiRev,
  OUT UINTN                   *DecodedSize
  )
{
  EFI_STATUS   Status               = EFI_SUCCESS;
  UINT8        FieldId              = 0;
  CONST UINT8  EXPECTED_FIELD_COUNT = AZIHSM_DDI_API_REV_FIELD_COUNT;
  UINT8        FieldCount           = 0;
  UINT32       DecoderStartMark     = 0;

  if ((ApiRev == NULL) || (Decoder == NULL) || (DecodedSize == NULL)) {
    *DecodedSize = 0;
    return EFI_INVALID_PARAMETER;
  }

  DecoderStartMark = Decoder->Position;

  // Decode single API revision structure
  // [FieldCount:U8:2][FIELD_ID:U8:1][MAJOR:U32][FIELD_ID:U8:2][MINOR:U32]
  Status = AziHsmMborDecodeMap (Decoder, &FieldCount);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  if (FieldCount != EXPECTED_FIELD_COUNT) {
    DEBUG ((
      DEBUG_WARN,
      "AziHsmDdi: Unexpected field count for single API revision %d expected %d\n",
      FieldCount,
      EXPECTED_FIELD_COUNT
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Decode each field based on its ID
  for (UINT8 i = 0; i < FieldCount; i++) {
    Status = AziHsmMborDecodeFieldId (Decoder, &FieldId);
    if (EFI_ERROR (Status)) {
      goto ExitFunction;
    }

    switch (FieldId) {
      case API_REV_MAJOR_FIELD_ID:
        Status = AziHsmMborDecodeU32 (Decoder, &ApiRev->Major);
        break;
      case API_REV_MINOR_FIELD_ID:
        Status = AziHsmMborDecodeU32 (Decoder, &ApiRev->Minor);
        break;
      default:
        Status = EFI_UNSUPPORTED;
        break;
    }

    if (EFI_ERROR (Status)) {
      goto ExitFunction;
    }
  }

ExitFunction:
  *DecodedSize = Decoder->Position - DecoderStartMark;
  return Status;
}

/**
  Encodes an API revision response structure into MBOR format.

  This function encodes the API revision response structure containing minimum
  and maximum API revision information into MBOR format. Each revision contains
  major and minor version numbers.

  @param[in,out] Encoder      Pointer to the MBOR encoder structure.
  @param[in]     ApiRevResponse Pointer to the API revision response structure to encode.
  @param[out]    EncodedSize  Pointer to store the size of encoded data in bytes.

  @retval EFI_SUCCESS           API revision response was successfully encoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval Other                 Error from MBOR encoding functions.
**/
STATIC EFI_STATUS
EncodeApiRevisionResponse (
  IN OUT AZIHSM_MBOR_ENCODER       *Encoder,
  IN  AZIHSM_DDI_API_REV_RESPONSE  *ApiRevResponse,
  OUT UINTN                        *EncodedSize
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  // Number of fields in the API revision response structure
  CONST UINT8  MAX_FIELD_COUNT  = AZIHSM_DDI_API_REV_RESPONSE_FIELD_COUNT;
  UINT32       EncoderStartMark = 0;
  UINTN        TempEncodedSize  = 0;

  if ((ApiRevResponse == NULL) || (Encoder == NULL) || (EncodedSize == NULL)) {
    *EncodedSize = 0;     // mark encoded size to 0
    return EFI_INVALID_PARAMETER;
  }

  // Calculate the size of the encoded data
  EncoderStartMark = Encoder->Position;

  // Encode the API revision response structure
  // [FieldCount:U8:2]
  // [FIELD_ID:U8:1][MIN_API_REV:AZIHSM_DDI_API_REV]
  // [FIELD_ID:U8:2][MAX_API_REV:AZIHSM_DDI_API_REV]
  // where AZIHSM_DDI_API_REV is [FieldCount:U8:2][FIELD_ID:U8:1][MAJOR:U32][FIELD_ID:U8:2][MINOR:U32]

  Status = AziHsmMborEncodeMap (Encoder, MAX_FIELD_COUNT);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Encode field 1: min API revision
  Status = AziHsmMborEncodeFieldId (Encoder, API_REV_RESP_MIN_FIELD_ID);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // EncodeApiRevision function for min API revision
  Status = EncodeApiRevision (Encoder, &ApiRevResponse->min, &TempEncodedSize);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Encode field 2: max API revision
  Status = AziHsmMborEncodeFieldId (Encoder, API_REV_RESP_MAX_FIELD_ID);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // EncodeApiRevision function for max API revision
  Status = EncodeApiRevision (Encoder, &ApiRevResponse->max, &TempEncodedSize);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

ExitFunction:
  *EncodedSize = Encoder->Position - EncoderStartMark;
  return Status;
}

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
  )
{
  EFI_STATUS  Status            = EFI_SUCCESS;
  UINT32      EncoderStartMark  = 0;
  UINTN       HeaderEncodedSize = 0;

  if ((Encoder == NULL) || (EncodedSize == NULL)) {
    *EncodedSize = 0;
    return EFI_INVALID_PARAMETER;
  }

  EncoderStartMark = Encoder->Position;

  // Use helper function to encode command header
  Status = EncodeCommandRequestHeader (Encoder, DDI_OP_GET_API_REV, ApiRev, SessionId, &HeaderEncodedSize);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Field 1: Request data (DdiGetApiRevReq) - empty struct
  Status = AziHsmMborEncodeFieldId (Encoder, REQ_CMD_DATA_FIELD_ID);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Encode empty struct (DdiGetApiRevReq has no fields)
  Status = AziHsmMborEncodeMap (Encoder, 0);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Field 2: Request extension (DdiReqExt) - optional, not implemented yet
  // TODO: Implement extension field if needed

ExitFunction:
  *EncodedSize = Encoder->Position - EncoderStartMark;
  return Status;
}

/**
  Decodes an API revision response structure from MBOR format.

  This function decodes MBOR format data into an API revision response structure
  containing minimum and maximum API revision information. Each revision contains
  major and minor version numbers. The function validates field count and field
  identifiers during decoding.

  @param[in,out] Decoder     Pointer to the MBOR decoder structure.
  @param[in,out] ApiRev      Pointer to the API revision response structure to populate.
  @param[out]    DecodedSize Pointer to store the size of decoded data in bytes.

  @retval EFI_SUCCESS           API revision response was successfully decoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval EFI_PROTOCOL_ERROR    Field count mismatch or invalid field structure.
  @retval EFI_UNSUPPORTED       Unknown field ID encountered.
  @retval Other                 Error from MBOR decoding functions.
**/
STATIC EFI_STATUS
DecodeApiRevisionResponse (
  IN OUT AZIHSM_MBOR_DECODER           *Decoder,
  IN OUT  AZIHSM_DDI_API_REV_RESPONSE  *ApiRev,
  OUT UINTN                            *DecodedSize
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  UINT8        FieldId          = 0;                                       // local field id
  CONST UINT8  MAX_FIELD_COUNT  = AZIHSM_DDI_API_REV_RESPONSE_FIELD_COUNT; // Number of fields in the API revision response structure
  UINT8        FieldCount       = 0;                                       // field count read from the decoder
  UINT32       DecoderStartMark = 0;
  UINTN        TempDecodedSize  = 0;

  if ((ApiRev == NULL) || (Decoder == NULL) || (DecodedSize == NULL)) {
    *DecodedSize = 0;
    return EFI_INVALID_PARAMETER;
  }

  // Calculate the size of the decoded data
  DecoderStartMark = Decoder->Position;

  // Decode the API revision response,
  // Decode struct is in the format :
  // [FieldCount:U8:2]
  // [FIELD_ID:U8:1][MIN_API_REV:AZIHSM_DDI_API_REV]
  // [FIELD_ID:U8:2][MAX_API_REV:AZIHSM_DDI_API_REV]
  // where AZIHSM_DDI_API_REV is [FieldCount:U8:2][FIELD_ID:U8:1][MAJOR:U32][FIELD_ID:U8:2][MINOR:U32]

  // Decode field count
  Status = AziHsmMborDecodeMap (Decoder, &FieldCount);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Check if the field count is as expected
  if (FieldCount != MAX_FIELD_COUNT) {
    DEBUG ((
      DEBUG_WARN,
      "AziHsmDdi: Unexpected field count for API revision response %d expected %d\n",
      FieldCount,
      MAX_FIELD_COUNT
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Decode each field based on its ID
  for (UINT8 i = 0; i < FieldCount; i++) {
    Status = AziHsmMborDecodeFieldId (Decoder, &FieldId);
    if (EFI_ERROR (Status)) {
      goto ExitFunction;
    }

    switch (FieldId) {
      case API_REV_RESP_MIN_FIELD_ID:       // min API revision
        // Use DecodeApiRevision helper function for min API revision
        Status = DecodeApiRevision (Decoder, &ApiRev->min, &TempDecodedSize);
        break;

      case API_REV_RESP_MAX_FIELD_ID:       // max API revision
        // Use DecodeApiRevision helper function for max API revision
        Status = DecodeApiRevision (Decoder, &ApiRev->max, &TempDecodedSize);
        break;

      default:
        Status = EFI_UNSUPPORTED;
        break;
    }

    if (EFI_ERROR (Status)) {
      goto ExitFunction;
    }
  }

ExitFunction:

  *DecodedSize = Decoder->Position - DecoderStartMark;
  return Status;
}

/**
  Decodes the header portion of a DDI command response from MBOR format.

  This function decodes the initial structure and extracts the response header,
  validating the field structure. It positions the decoder at the data field
  for subsequent processing. This is a generic function that can be used for
  any DDI command response.

  @param[in,out] Decoder     Pointer to the MBOR decoder structure.
  @param[in,out] RspHdr      Pointer to store the decoded response header.
  @param[out]    DecodedSize Pointer to store the size of decoded data in bytes.

  @retval EFI_SUCCESS           Response header was successfully decoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval EFI_PROTOCOL_ERROR    Field count mismatch or invalid field structure.
  @retval EFI_UNSUPPORTED       Unknown field ID encountered.
  @retval Other                 Error from MBOR decoding functions.
**/
STATIC EFI_STATUS
DecodeCommandResponseHeader (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  IN OUT AZIHSM_DDI_RSP_HDR   *RspHdr,
  OUT    UINTN                *DecodedSize
  )
{
  EFI_STATUS   Status           = EFI_SUCCESS;
  UINT8        FieldId          = 0;
  CONST UINT8  MIN_FIELD_COUNT  = API_REV_CMD_RESP_MIN_FIELD_COUNT;
  CONST UINT8  MAX_FIELD_COUNT  = API_REV_CMD_RESP_MAX_FIELD_COUNT;
  UINT8        FieldCount       = 0;
  UINT32       DecoderStartMark = 0;
  UINTN        TempDecodedSize  = 0;

  if ((RspHdr == NULL) || (Decoder == NULL) || (DecodedSize == NULL)) {
    *DecodedSize = 0;
    return EFI_INVALID_PARAMETER;
  }

  DecoderStartMark = Decoder->Position;

  // Decode the outer structure field count
  Status = AziHsmMborDecodeMap (Decoder, &FieldCount);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Check if the field count is within expected range
  if ((FieldCount < MIN_FIELD_COUNT) || (FieldCount > MAX_FIELD_COUNT)) {
    DEBUG ((
      DEBUG_WARN,
      "AziHsmDdi: Unexpected field count for API revision response %d expected %d-%d\n",
      FieldCount,
      MIN_FIELD_COUNT,
      MAX_FIELD_COUNT
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Look for the header field (field ID 0)
  Status = AziHsmMborDecodeFieldId (Decoder, &FieldId);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  if (FieldId != REQ_CMD_HDR_FIELD_ID) {
    DEBUG ((DEBUG_WARN, "AziHsmDdi: Expected header field ID %d, got %d\n", REQ_CMD_HDR_FIELD_ID, FieldId));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Decode the response header
  Status = DecodeResponseHeader (Decoder, RspHdr, &TempDecodedSize);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Note: DDI operation validation should be done by the caller
  // since this is a generic function used by multiple command types

ExitFunction:
  *DecodedSize = Decoder->Position - DecoderStartMark;
  return Status;
}

/**
  Decodes the data portion of an API revision command response from MBOR format.

  This function assumes the decoder is positioned at the data field and extracts
  the API revision response data. It expects the next field to be the data field
  with ID 1 containing min/max API revision information.

  @param[in,out] Decoder     Pointer to the MBOR decoder structure.
  @param[in,out] ApiRevData  Pointer to store the decoded API revision response data.
  @param[out]    DecodedSize Pointer to store the size of decoded data in bytes.

  @retval EFI_SUCCESS           API revision data was successfully decoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval EFI_PROTOCOL_ERROR    Wrong field ID or invalid structure.
  @retval EFI_UNSUPPORTED       Unknown field ID encountered.
  @retval Other                 Error from MBOR decoding functions.
**/
STATIC EFI_STATUS
DecodeApiRevisionCommandResponseData (
  IN OUT AZIHSM_MBOR_DECODER          *Decoder,
  IN OUT AZIHSM_DDI_API_REV_RESPONSE  *ApiRevData,
  OUT    UINTN                        *DecodedSize
  )
{
  EFI_STATUS  Status           = EFI_SUCCESS;
  UINT8       FieldId          = 0;
  UINT32      DecoderStartMark = 0;
  UINTN       TempDecodedSize  = 0;

  if ((ApiRevData == NULL) || (Decoder == NULL) || (DecodedSize == NULL)) {
    *DecodedSize = 0;
    return EFI_INVALID_PARAMETER;
  }

  DecoderStartMark = Decoder->Position;

  // Decode the data field ID
  Status = AziHsmMborDecodeFieldId (Decoder, &FieldId);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  if (FieldId != REQ_CMD_DATA_FIELD_ID) {
    DEBUG ((DEBUG_WARN, "AziHsmDdi: Expected data field ID %d, got %d\n", REQ_CMD_DATA_FIELD_ID, FieldId));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Decode the API revision response data
  Status = DecodeApiRevisionResponse (Decoder, ApiRevData, &TempDecodedSize);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

ExitFunction:
  *DecodedSize = Decoder->Position - DecoderStartMark;
  return Status;
}

/**
  Decodes a complete API revision command response from MBOR format.

  This function decodes MBOR format data into the complete DdiGetApiRevCmdResp structure
  which contains:
  - Response header (DdiRespHdr) with optional revision, operation code, optional session ID, status, and FIPS approval
  - Response data (DdiGetApiRevResp) containing min/max API revision information
  - Response extension (DdiRespExt) - optional, currently not implemented

  @param[in,out] Decoder     Pointer to the MBOR decoder structure.
  @param[in,out] RspHdr      Pointer to the response header structure to populate.
  @param[in,out] ApiRevData  Pointer to the API revision response data structure to populate.
  @param[out]    DecodedSize Pointer to store the size of decoded data in bytes.

  @retval EFI_SUCCESS           API revision response was successfully decoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval EFI_PROTOCOL_ERROR    Field count mismatch or invalid field structure.
  @retval EFI_UNSUPPORTED       Unknown field ID encountered.
  @retval Other                 Error from MBOR decoding functions.
**/
STATIC EFI_STATUS
DecodeApiRevisionCommandResponse (
  IN OUT AZIHSM_MBOR_DECODER           *Decoder,
  IN OUT  AZIHSM_DDI_RSP_HDR           *RspHdr,
  IN OUT  AZIHSM_DDI_API_REV_RESPONSE  *ApiRevData,
  OUT UINTN                            *DecodedSize
  )
{
  EFI_STATUS  Status            = EFI_SUCCESS;
  UINTN       HeaderDecodedSize = 0;
  UINTN       DataDecodedSize   = 0;
  UINT32      DecoderStartMark  = 0;

  if ((RspHdr == NULL) || (ApiRevData == NULL) || (Decoder == NULL) || (DecodedSize == NULL)) {
    *DecodedSize = 0;
    return EFI_INVALID_PARAMETER;
  }

  DecoderStartMark = Decoder->Position;

  // Step 1: Decode and validate the response header
  Status = DecodeCommandResponseHeader (Decoder, RspHdr, &HeaderDecodedSize);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Step 2: Decode the API revision response data
  Status = DecodeApiRevisionCommandResponseData (Decoder, ApiRevData, &DataDecodedSize);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // TODO: Handle optional extension field if present (field ID 2)
  // For now, we assume only 2 fields (header + data) are present

ExitFunction:
  *DecodedSize = Decoder->Position - DecoderStartMark;
  return Status;
}

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
  IN OUT AZIHSM_MBOR_DECODER          *Decoder,
  OUT    AZIHSM_DDI_API_REV_RESPONSE  *ApiRevData,
  OUT    UINTN                        *DecodedSize
  )
{
  EFI_STATUS          Status = EFI_SUCCESS;
  AZIHSM_DDI_RSP_HDR  RspHdr;
  UINTN               HeaderDecodedSize = 0;
  UINTN               DataDecodedSize   = 0;
  UINT32              DecoderStartMark  = 0;

  if ((Decoder == NULL) || (ApiRevData == NULL) || (DecodedSize == NULL)) {
    *DecodedSize = 0;
    return EFI_INVALID_PARAMETER;
  }

  DecoderStartMark = Decoder->Position;

  // Step 1: Decode and validate the response header
  Status = DecodeCommandResponseHeader (Decoder, &RspHdr, &HeaderDecodedSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsmDdi: Failed to decode API revision response header: %r\n", Status));
    goto ExitFunction;
  }

  // Validate that this is indeed an API revision response
  if (RspHdr.DdiOp != DDI_OP_GET_API_REV) {
    DEBUG ((DEBUG_WARN, "AziHsmDdi: Expected DDI_OP_GET_API_REV (%d), got %d\n", DDI_OP_GET_API_REV, RspHdr.DdiOp));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Step 2: Check if the operation was successful
  if (RspHdr.DdiStatus != DDI_STATUS_SUCCESS) {
    DEBUG ((DEBUG_WARN, "AziHsmDdi: API revision request failed with DDI status: %d\n", RspHdr.DdiStatus));
    Status = ConvertDdiStatusToEfiStatus (RspHdr.DdiStatus);
    goto ExitFunction;
  }

  // Step 3: Decode the API revision response data
  Status = DecodeApiRevisionCommandResponseData (Decoder, ApiRevData, &DataDecodedSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsmDdi: Failed to decode API revision response data: %r\n", Status));
    goto ExitFunction;
  }

ExitFunction:
  *DecodedSize = Decoder->Position - DecoderStartMark;
  return Status;
}

/**
  Encodes a DDI request header structure into MBOR format.

  This function encodes a DDI (Device Driver Interface) request header containing
  optional revision information, DDI operation code, and optional session ID into
  MBOR format. The function dynamically determines which fields to include based
  on their validity flags.

  @param[in,out] Encoder     Pointer to the MBOR encoder structure.
  @param[in]     ReqHdr      Pointer to the DDI request header structure to encode.
  @param[out]    EncodedSize Pointer to store the size of encoded data in bytes.

  @retval EFI_SUCCESS           Request header was successfully encoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval EFI_PROTOCOL_ERROR    Field count exceeds maximum allowed.
  @retval Other                 Error from MBOR encoding functions.
**/
STATIC
EFI_STATUS
EncodeRequestHeader (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN  AZIHSM_DDI_REQ_HDR      *ReqHdr,
  OUT UINTN                   *EncodedSize
  )
{
  EFI_STATUS  Status           = EFI_SUCCESS;
  UINT32      EncoderStartMark = 0;
  UINT8       FieldCount       = 0;
  UINTN       EncodedSizeTmp   = 0;

  // Request header is encoded with one of the following structures:
  // [FieldCount:U8:1||2||3]
  // [FIELD_ID:U8:1] [Revision:AZIHSM_DDI_API_REV]
  // [FIELD_ID:U8:2][DdiOp:DDI_OPERATION_CODE]
  // [FIELD_ID:U8:3][SessionId:UINT16]

  if ((Encoder == NULL) || (ReqHdr == NULL) || (EncodedSize == NULL)) {
    *EncodedSize = 0;
    return EFI_INVALID_PARAMETER;
  }

  EncoderStartMark = Encoder->Position;

  // check if Revision field is valid or not
  FieldCount = (ReqHdr->Revision.Valid == TRUE) ? (FieldCount + 1) : FieldCount;

  // Check if SessionId field is valid or not
  FieldCount = (ReqHdr->SessionId.Valid == TRUE) ? (FieldCount + 1) : FieldCount;

  // DdiOp is always valid
  FieldCount++;

  // check if we exceeded the expected field count
  if (FieldCount > AZIHSM_DDI_REQ_HDR_FIELD_COUNT) {
    DEBUG ((
      DEBUG_WARN,
      "AziHsmDdi: Unexpected field count for request header %d expected %d\n",
      FieldCount,
      AZIHSM_DDI_REQ_HDR_FIELD_COUNT
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Encode the request header
  // [FieldCount:U8:1||2||3]
  Status = AziHsmMborEncodeMap (Encoder, FieldCount);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Encode Revision only if revision field is valid
  if (ReqHdr->Revision.Valid == TRUE) {
    // [FIELD_ID:U8:1] [Revision:AZIHSM_DDI_API_REV]
    Status = AziHsmMborEncodeFieldId (Encoder, REV_FIELD_ID);
    if (EFI_ERROR (Status)) {
      goto ExitFunction;
    }

    Status = EncodeApiRevision (Encoder, &ReqHdr->Revision.Value, &EncodedSizeTmp);
    if (EFI_ERROR (Status)) {
      goto ExitFunction;
    }
  }

  // [FIELD_ID:U8:2][DdiOp:DDI_OPERATION_CODE]
  Status = AziHsmMborEncodeFieldId (Encoder, DDI_OP_FIELD_ID);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  Status = AziHsmMborEncodeU32 (Encoder, (UINT32)ReqHdr->DdiOp);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  if (ReqHdr->SessionId.Valid == TRUE) {
    // [FIELD_ID:U8:3][SessionId:UINT16]
    Status = AziHsmMborEncodeFieldId (Encoder, SESSION_ID_FIELD_ID);
    if (EFI_ERROR (Status)) {
      goto ExitFunction;
    }

    Status = AziHsmMborEncodeU16 (Encoder, ReqHdr->SessionId.Value);
    if (EFI_ERROR (Status)) {
      goto ExitFunction;
    }
  }

ExitFunction:

  *EncodedSize = Encoder->Position - EncoderStartMark;
  return Status;
}

/**
  Decodes a DDI request header structure from MBOR format.

  This function decodes MBOR format data into a DDI request header structure.
  It validates field counts, handles optional fields (revision, session ID),
  and ensures required fields (DDI operation) are present. The function sets
  validity flags for optional fields based on their presence in the data.

  @param[in,out] Decoder     Pointer to the MBOR decoder structure.
  @param[in,out] ReqHdr      Pointer to the DDI request header structure to populate.
  @param[out]    DecodedSize Pointer to store the size of decoded data in bytes.

  @retval EFI_SUCCESS           Request header was successfully decoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval EFI_PROTOCOL_ERROR    Invalid field count or missing required fields.
  @retval EFI_UNSUPPORTED       Unknown field ID encountered.
  @retval Other                 Error from MBOR decoding functions.
**/
STATIC
EFI_STATUS
DecodeRequestHeader (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  IN OUT  AZIHSM_DDI_REQ_HDR  *ReqHdr,
  OUT UINTN                   *DecodedSize
  )
{
  // Decode the request header
  EFI_STATUS  Status = EFI_SUCCESS;

  UINT8   FieldCount       = 0;
  UINT8   FieldId          = 0;
  UINT32  DecoderStartMark = Decoder->Position;
  UINTN   DecodedSizeTmp   = 0;

  if ((Decoder == NULL) || (ReqHdr == NULL) || (DecodedSize == NULL)) {
    *DecodedSize = 0;
    return EFI_INVALID_PARAMETER;
  }

  // Clear validity flags
  ReqHdr->Revision.Valid  = FALSE;
  ReqHdr->SessionId.Valid = FALSE;

  // Decode field count
  Status = AziHsmMborDecodeMap (Decoder, &FieldCount);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  if ((FieldCount > AZIHSM_DDI_REQ_HDR_FIELD_COUNT) || (FieldCount < MIN_REQ_HDR_FIELD_COUNT)) {
    DEBUG ((
      DEBUG_WARN,
      "AziHsmDdi: Unexpected field count for request header %d expected <= %d\n",
      FieldCount,
      AZIHSM_DDI_REQ_HDR_FIELD_COUNT
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Decode fields based on field count and expected logic
  UINT32   DdiOpVal       = 0;
  BOOLEAN  DdiOpFound     = FALSE;
  BOOLEAN  RevisionFound  = FALSE;
  BOOLEAN  SessionIdFound = FALSE;

  for (UINT8 i = 0; i < FieldCount; i++) {
    Status = AziHsmMborDecodeFieldId (Decoder, &FieldId);
    if (EFI_ERROR (Status)) {
      goto ExitFunction;
    }

    switch (FieldId) {
      case REV_FIELD_ID:
        RevisionFound          = TRUE;
        ReqHdr->Revision.Valid = TRUE;
        Status                 = DecodeApiRevision (Decoder, &ReqHdr->Revision.Value, &DecodedSizeTmp);
        break;
      case DDI_OP_FIELD_ID:
        DdiOpFound = TRUE;
        Status     = AziHsmMborDecodeU32 (Decoder, &DdiOpVal);
        if (!EFI_ERROR (Status)) {
          ReqHdr->DdiOp = (DDI_OPERATION_CODE)DdiOpVal;
        }

        break;
      case SESSION_ID_FIELD_ID:
        SessionIdFound = TRUE;
        {
          UINT16  SessionIdVal = 0;
          Status = AziHsmMborDecodeU16 (Decoder, &SessionIdVal);
          if (!EFI_ERROR (Status)) {
            ReqHdr->SessionId.Valid = TRUE;
            ReqHdr->SessionId.Value = SessionIdVal;
          }
        }
        break;
      default:
        Status = EFI_UNSUPPORTED;
        break;
    }

    if (EFI_ERROR (Status)) {
      goto ExitFunction;
    }
  }

  // Validate field presence according to logic
  if (FieldCount == 1) {
    if (!DdiOpFound) {
      DEBUG ((DEBUG_WARN, "AziHsmDdi: FieldCount %d but DDI_OP_FIELD_ID not present\n", FieldCount));
      Status = EFI_PROTOCOL_ERROR;
      goto ExitFunction;
    }
  } else if (FieldCount == 2) {
    if (!DdiOpFound || (!RevisionFound && !SessionIdFound)) {
      DEBUG ((DEBUG_WARN, "AziHsmDdi: FieldCount %d but missing required fields\n", FieldCount));
      Status = EFI_PROTOCOL_ERROR;
      goto ExitFunction;
    }
  } else if (FieldCount == 3) {
    if (!DdiOpFound || !RevisionFound || !SessionIdFound) {
      DEBUG ((DEBUG_WARN, "AziHsmDdi: FieldCount %d but not all fields present\n", FieldCount));
      Status = EFI_PROTOCOL_ERROR;
      goto ExitFunction;
    }
  }

ExitFunction:
  *DecodedSize = Decoder->Position - DecoderStartMark;
  return Status;
}

/**
  Encodes a DDI response header structure into MBOR format.

  This function encodes a DDI response header containing optional revision and
  session ID, required DDI operation code, status code, and FIPS approval flag
  into MBOR format. The function includes all required fields and optional fields
  based on their validity flags.

  @param[in,out] Encoder     Pointer to the MBOR encoder structure.
  @param[in]     RspHdr      Pointer to the DDI response header structure to encode.
  @param[out]    EncodedSize Pointer to store the size of encoded data in bytes.

  @retval EFI_SUCCESS           Response header was successfully encoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval EFI_PROTOCOL_ERROR    Field count exceeds maximum allowed.
  @retval Other                 Error from MBOR encoding functions.
**/
STATIC EFI_STATUS
EncodeResponseHeader (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN  AZIHSM_DDI_RSP_HDR      *RspHdr,
  OUT UINTN                   *EncodedSize
  )
{
  EFI_STATUS  Status           = EFI_SUCCESS;
  UINT32      EncoderStartMark = 0;
  UINT8       FieldCount       = 0;
  UINTN       EncodedSizeTmp   = 0;

  // Response header is encoded with one or more of the following fields:
  // [FieldCount:U8:3/4/5]
  // [RSP_HDR_FIELD_ID:U8:1] [Revision:AZIHSM_DDI_API_REV] : Optional
  // [RSP_HDR_FIELD_ID:U8:2][DdiOp:DDI_OPERATION_CODE]
  // [RSP_HDR_FIELD_ID:U8:3][SessionId:UINT16] : Optional
  // [RSP_HDR_FIELD_ID:u8:4] [DdiStatus:UINT32]
  // [RSP_HDR_FIELD_ID:u8:5] [fips_approved:BOOLEAN]

  if ((Encoder == NULL) || (RspHdr == NULL) || (EncodedSize == NULL)) {
    *EncodedSize = 0;
    return EFI_INVALID_PARAMETER;
  }

  EncoderStartMark = Encoder->Position;

  // Count valid fields
  FieldCount = 0;
  if (RspHdr->Revision.Valid == TRUE) {
    FieldCount++;
  }

  if (RspHdr->SessionId.Valid == TRUE) {
    FieldCount++;
  }

  FieldCount++;   // DdiOp is always present
  FieldCount++;   // DdiStatus is always present
  FieldCount++;   // FipsApproved is always present

  // Check if we exceeded the expected field count
  if (FieldCount > AZIHSM_DDI_RSP_HDR_FIELD_COUNT) {
    DEBUG ((
      DEBUG_WARN,
      "AziHsmDdi: Unexpected field count for response header %d expected <= %d\n",
      FieldCount,
      AZIHSM_DDI_RSP_HDR_FIELD_COUNT
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Encode the response header
  Status = AziHsmMborEncodeMap (Encoder, FieldCount);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Encode Revision if valid
  if (RspHdr->Revision.Valid == TRUE) {
    Status = AziHsmMborEncodeFieldId (Encoder, RSP_REV_FIELD_ID);
    if (EFI_ERROR (Status)) {
      goto ExitFunction;
    }

    Status = EncodeApiRevision (Encoder, &RspHdr->Revision.Value, &EncodedSizeTmp);
    if (EFI_ERROR (Status)) {
      goto ExitFunction;
    }
  }

  // Encode DdiOp (always present)
  Status = AziHsmMborEncodeFieldId (Encoder, RSP_DDI_OP_FIELD_ID);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  Status = AziHsmMborEncodeU32 (Encoder, (UINT32)RspHdr->DdiOp);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Encode SessionId if valid
  if (RspHdr->SessionId.Valid == TRUE) {
    Status = AziHsmMborEncodeFieldId (Encoder, RSP_SESSION_ID_FIELD_ID);
    if (EFI_ERROR (Status)) {
      goto ExitFunction;
    }

    Status = AziHsmMborEncodeU16 (Encoder, RspHdr->SessionId.Value);
    if (EFI_ERROR (Status)) {
      goto ExitFunction;
    }
  }

  // Encode DdiStatus (always present)
  Status = AziHsmMborEncodeFieldId (Encoder, RSP_DDI_STATUS_FIELD_ID);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  Status = AziHsmMborEncodeU32 (Encoder, RspHdr->DdiStatus);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Encode FipsApproved (always present)
  Status = AziHsmMborEncodeFieldId (Encoder, RSP_FIPS_APPROVED_FIELD_ID);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  Status = AziHsmMborEncodeBoolean (Encoder, RspHdr->fips_approved);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

ExitFunction:
  *EncodedSize = Encoder->Position - EncoderStartMark;
  return Status;
}

/**
  Decodes a DDI response header structure from MBOR format.

  This function decodes MBOR format data into a DDI response header structure.
  It validates field counts, handles optional fields (revision, session ID),
  ensures required fields (DDI operation, status, FIPS approval) are present,
  and prevents duplicate fields. The function sets validity flags for optional
  fields based on their presence in the data.

  @param[in,out] Decoder     Pointer to the MBOR decoder structure.
  @param[in,out] RspHdr      Pointer to the DDI response header structure to populate.
  @param[out]    DecodedSize Pointer to store the size of decoded data in bytes.

  @retval EFI_SUCCESS           Response header was successfully decoded.
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL.
  @retval EFI_PROTOCOL_ERROR    Invalid field count, missing required fields, or duplicate fields.
  @retval EFI_UNSUPPORTED       Unknown field ID encountered.
  @retval Other                 Error from MBOR decoding functions.
**/
STATIC EFI_STATUS
DecodeResponseHeader (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  IN OUT  AZIHSM_DDI_RSP_HDR  *RspHdr,
  OUT UINTN                   *DecodedSize
  )
{
  EFI_STATUS  Status            = EFI_SUCCESS;
  UINT8       FieldCount        = 0;
  UINT8       FieldId           = 0;
  UINT32      DecoderStartMark  = Decoder->Position;
  UINTN       DecodedSizeTmp    = 0;
  UINT32      DdiOpVal          = 0;
  UINT32      DdiStatusVal      = 0;
  BOOLEAN     DdiOpFound        = FALSE;
  BOOLEAN     RevisionFound     = FALSE;
  BOOLEAN     SessionIdFound    = FALSE;
  BOOLEAN     DdiStatusFound    = FALSE;
  BOOLEAN     FipsApprovedFound = FALSE;

  // Check valid params
  if ((Decoder == NULL) || (RspHdr == NULL) || (DecodedSize == NULL)) {
    if (DecodedSize != NULL) {
      *DecodedSize = 0;
    }

    return EFI_INVALID_PARAMETER;
  }

  // Clear validity flags
  RspHdr->Revision.Valid  = FALSE;
  RspHdr->SessionId.Valid = FALSE;

  // Decode field count
  Status = AziHsmMborDecodeMap (Decoder, &FieldCount);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Return error if expected field count is not present
  if ((FieldCount > AZIHSM_DDI_RSP_HDR_FIELD_COUNT) || (FieldCount < MIN_RSP_HDR_FIELD_COUNT)) {
    DEBUG ((
      DEBUG_WARN,
      "AziHsmDdi: Unexpected field count for response header %d expected >= %d and <= %d\n",
      FieldCount,
      MIN_RSP_HDR_FIELD_COUNT,
      AZIHSM_DDI_RSP_HDR_FIELD_COUNT
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  for (UINT8 i = 0; i < FieldCount; i++) {
    Status = AziHsmMborDecodeFieldId (Decoder, &FieldId);
    if (EFI_ERROR (Status)) {
      goto ExitFunction;
    }

    // Check for invalid field ID
    if (FieldId > AZIHSM_DDI_RSP_HDR_FIELD_COUNT) {
      DEBUG ((DEBUG_WARN, "AziHsmDdi: Invalid field ID %d in response header\n", FieldId));
      Status = EFI_PROTOCOL_ERROR;
      goto ExitFunction;
    }

    switch (FieldId) {
      case RSP_REV_FIELD_ID:
        if (RevisionFound) {
          DEBUG ((DEBUG_WARN, "AziHsmDdi: Duplicate Revision field in response header\n"));
          Status = EFI_PROTOCOL_ERROR;
          goto ExitFunction;
        }

        RevisionFound          = TRUE;
        RspHdr->Revision.Valid = TRUE;
        Status                 = DecodeApiRevision (Decoder, &RspHdr->Revision.Value, &DecodedSizeTmp);
        break;
      case RSP_DDI_OP_FIELD_ID:
        if (DdiOpFound) {
          DEBUG ((DEBUG_WARN, "AziHsmDdi: Duplicate DdiOp field in response header\n"));
          Status = EFI_PROTOCOL_ERROR;
          goto ExitFunction;
        }

        DdiOpFound = TRUE;
        Status     = AziHsmMborDecodeU32 (Decoder, &DdiOpVal);
        if (!EFI_ERROR (Status)) {
          RspHdr->DdiOp = (DDI_OPERATION_CODE)DdiOpVal;
        }

        break;
      case RSP_SESSION_ID_FIELD_ID:
        if (SessionIdFound) {
          DEBUG ((DEBUG_WARN, "AziHsmDdi: Duplicate SessionId field in response header\n"));
          Status = EFI_PROTOCOL_ERROR;
          goto ExitFunction;
        }

        SessionIdFound = TRUE;
        {
          UINT16  SessionIdVal = 0;
          Status = AziHsmMborDecodeU16 (Decoder, &SessionIdVal);
          if (!EFI_ERROR (Status)) {
            RspHdr->SessionId.Valid = TRUE;
            RspHdr->SessionId.Value = SessionIdVal;
          }
        }
        break;
      case RSP_DDI_STATUS_FIELD_ID:
        if (DdiStatusFound) {
          DEBUG ((DEBUG_WARN, "AziHsmDdi: Duplicate DdiStatus field in response header\n"));
          Status = EFI_PROTOCOL_ERROR;
          goto ExitFunction;
        }

        DdiStatusFound = TRUE;
        Status         = AziHsmMborDecodeU32 (Decoder, &DdiStatusVal);
        if (!EFI_ERROR (Status)) {
          RspHdr->DdiStatus = DdiStatusVal;
        }

        break;
      case RSP_FIPS_APPROVED_FIELD_ID:
        if (FipsApprovedFound) {
          DEBUG ((DEBUG_WARN, "AziHsmDdi: Duplicate FipsApproved field in response header\n"));
          Status = EFI_PROTOCOL_ERROR;
          goto ExitFunction;
        }

        FipsApprovedFound = TRUE;
        Status            = AziHsmMborDecodeBoolean (Decoder, &RspHdr->fips_approved);
        break;
      default:
        Status = EFI_UNSUPPORTED;
        break;
    }

    if (EFI_ERROR (Status)) {
      goto ExitFunction;
    }
  }

  // Validate required fields
  if (!DdiOpFound || !DdiStatusFound || !FipsApprovedFound) {
    DEBUG ((DEBUG_WARN, "AziHsmDdi: Missing required fields in response header\n"));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

ExitFunction:
  *DecodedSize = Decoder->Position - DecoderStartMark;
  return Status;
}

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
  )
{
  EFI_STATUS  Status            = EFI_SUCCESS;
  UINT32      EncoderStartMark  = 0;
  UINTN       HeaderEncodedSize = 0;

  if ((Encoder == NULL) || (EncodedSize == NULL) || (Request == NULL)) {
    if (EncodedSize != NULL) {
      *EncodedSize = 0;
    }

    return EFI_INVALID_PARAMETER;
  }

  EncoderStartMark = Encoder->Position;

  // encode command header
  Status = EncodeCommandRequestHeader (Encoder, DDI_OP_INIT_BKS3, ApiRev, SessionId, &HeaderEncodedSize);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Field 1: Request data (InitBks3) - AZIHSM_DDI_INIT_BKS3_REQ
  Status = AziHsmMborEncodeFieldId (Encoder, REQ_CMD_DATA_FIELD_ID);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Encode BKS3 data
  Status = AziHsmMborEncodeMap (Encoder, 1);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Encode field ID of data
  Status = AziHsmMborEncodeFieldId (Encoder, API_INIT_BKS3_CMD_REQ_DATA_FIELD_ID);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Encode actual BKS3 data
  Status = AziHsmMborEncodeBytes (Encoder, Request->Bks3.Data, Request->Bks3.Length);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

ExitFunction:
  *EncodedSize = Encoder->Position - EncoderStartMark;
  return Status;
}

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
  )
{
  EFI_STATUS          Status            = EFI_SUCCESS;
  UINT32              DecoderStartMark  = 0;
  UINT8               FieldId           = 0;
  AZIHSM_DDI_RSP_HDR  RspHdr            = { 0 };
  UINTN               HeaderDecodedSize = 0;
  UINT16              DataDecodedSize   = 0;
  UINT8               DataFieldCount    = 0;

  if ((Decoder == NULL) || (Response == NULL) || (DecodedSize == NULL)) {
    if (DecodedSize != NULL) {
      *DecodedSize = 0;
    }

    return EFI_INVALID_PARAMETER;
  }

  DecoderStartMark = Decoder->Position;

  // Decode the response header
  Status = DecodeCommandResponseHeader (Decoder, &RspHdr, &HeaderDecodedSize);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Validate that this is indeed an InitBks3 response
  if (RspHdr.DdiOp != DDI_OP_INIT_BKS3) {
    DEBUG ((DEBUG_WARN, "AziHsmDdi: Expected DDI_OP_INIT_BKS3, got %d\n", RspHdr.DdiOp));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Check if the operation was successful
  if (RspHdr.DdiStatus != DDI_STATUS_SUCCESS) {
    DEBUG ((DEBUG_WARN, "AziHsmDdi: InitBks3 request failed with DDI status: %d\n", RspHdr.DdiStatus));
    Status = ConvertDdiStatusToEfiStatus (RspHdr.DdiStatus);
    goto ExitFunction;
  }

  // Decode the data field ID
  Status = AziHsmMborDecodeFieldId (Decoder, &FieldId);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  if (FieldId != REQ_CMD_DATA_FIELD_ID) {
    DEBUG ((DEBUG_WARN, "AziHsmDdi: Expected data field ID %d, got %d\n", REQ_CMD_DATA_FIELD_ID, FieldId));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Decode the InitBks3 response data structure

  Status = AziHsmMborDecodeMap (Decoder, &DataFieldCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsmDdi: Failed to decode InitBks3 response map: %r\n", Status));
    goto ExitFunction;
  }

  if (DataFieldCount != AZIHSM_DDI_INIT_BKS3_RESP_FIELD_COUNT) {
    DEBUG ((
      DEBUG_WARN,
      "AziHsmDdi: Expected %d fields in InitBks3 response data, got %d\n",
      AZIHSM_DDI_INIT_BKS3_RESP_FIELD_COUNT,
      DataFieldCount
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Decode the response data field ID
  Status = AziHsmMborDecodeFieldId (Decoder, &FieldId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsmDdi: Failed to decode InitBks3 response field ID: %r\n", Status));
    goto ExitFunction;
  }

  if (FieldId != API_INIT_BKS3_CMD_RESP_DATA_FIELD_ID) {
    DEBUG ((
      DEBUG_WARN,
      "AziHsmDdi: Expected BKS3 response data field ID %d, got %d\n",
      API_INIT_BKS3_CMD_RESP_DATA_FIELD_ID,
      FieldId
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Decode the actual BKS3 response data
  Status = AziHsmMborDecodePaddedBytes (Decoder, Response->Bks3.Data, &DataDecodedSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsmDdi: Failed to decode InitBks3 response data: %r\n", Status));
    goto ExitFunction;
  }

  Response->Bks3.Length = DataDecodedSize;

  // Start decoding GUID
  Status = AziHsmMborDecodeFieldId (Decoder, &FieldId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsmDdi: Failed to decode InitBks3 response field ID for GUID: %r\n", Status));
    goto ExitFunction;
  }

  // check GUID field id
  if (FieldId != API_INIT_BKS3_CMD_RESP_GUID_FIELD_ID) {
    DEBUG ((
      DEBUG_WARN,
      "AziHsmDdi: Expected BKS3 response GUID field ID %d, got %d\n",
      API_INIT_BKS3_CMD_RESP_GUID_FIELD_ID,
      FieldId
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // get next byte array
  Status = AziHsmMborDecodeBytes (Decoder, Response->Guid, &DataDecodedSize);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsmDdi: Failed to decode InitBks3 response GUID data: %r\n", Status));
    goto ExitFunction;
  } else if (DataDecodedSize != AZIHSM_DDI_INIT_BKS3_RESP_GUID_LENGTH) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsmDdi: Expected GUID length %d, got %d\n",
      AZIHSM_DDI_INIT_BKS3_RESP_GUID_LENGTH,
      DataDecodedSize
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

ExitFunction:
  *DecodedSize = Decoder->Position - DecoderStartMark;
  return Status;
}

/**
  Encodes a complete SetSealedBks3 command request into MBOR format.

  This function encodes the complete DdiSetSealedBks3CmdReq structure which contains:
  - Request header (DdiReqHdr) with optional revision, operation code, and optional session ID
  - Request data (DdiSetSealedBks3Req) containing sealed BKS3 data to be set
  - Request extension (DdiReqExt) - optional, currently not implemented

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
  OUT UINTN                           *CONST      EncodedSize
  )
{
  EFI_STATUS  Status            = EFI_SUCCESS;
  UINT32      EncoderStartMark  = 0;
  UINTN       HeaderEncodedSize = 0;
  UINT8       FieldId           = 0;

  EncoderStartMark = Encoder->Position;
  // Check params are null and return error
  if ((Encoder == NULL) || (SealedBks3Request == NULL) || (EncodedSize == NULL)) {
    if (EncodedSize != NULL) {
      *EncodedSize = 0;
    }

    DEBUG ((DEBUG_WARN, "AziHsmDdi: Invalid parameter(s) for SetSealedBks3 request\n"));
    return EFI_INVALID_PARAMETER;
  }

  // Encode the request header
  Status = EncodeCommandRequestHeader (Encoder, DDI_OP_SET_SEALED_BKS3, ApiRev, SessionId, &HeaderEncodedSize);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Encode the data field ID
  FieldId = API_SET_SEALED_BKS3_CMD_REQ_DATA_FIELD_ID;
  Status  = AziHsmMborEncodeFieldId (Encoder, FieldId);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Encode the Set Sealed BKS3 request data structure
  Status = AziHsmMborEncodeMap (Encoder, AZIHSM_DDI_SET_SEALED_BKS3_REQ_FIELD_COUNT);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Encode field id
  Status = AziHsmMborEncodeFieldId (Encoder, API_SET_SEALED_BKS3_CMD_REQ_DATA_FIELD_ID);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Encode the actual BKS3 request data
  Status = AziHsmMborEncodeBytes (Encoder, SealedBks3Request->SealedBks3.Data, SealedBks3Request->SealedBks3.Length);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

ExitFunction:
  *EncodedSize = Encoder->Position - EncoderStartMark;
  return Status;
}

/**
  Decodes a complete SetSealedBks3 command response from MBOR format.

  This function decodes MBOR format data into the complete DdiSetSealedBks3CmdResp structure
  which contains:
  - Response header (DdiRespHdr) with optional revision, operation code, optional session ID, status, and FIPS approval
  - Response data (DdiSetSealedBks3Resp) - empty structure for this operation
  - Response extension (DdiRespExt) - optional, currently not implemented

  The function validates the DDI operation code and status before attempting to decode response data.
  For SetSealedBks3, the response data is expected to be an empty structure (field count 0).

  @param[in,out] Decoder     Pointer to the MBOR decoder structure.
  @param[out]    Response    Pointer to store the decoded SetSealedBks3 response data structure.
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
  )
{
  EFI_STATUS          Status            = EFI_SUCCESS;
  UINT32              DecoderStartMark  = 0;
  UINT8               FieldId           = 0;
  AZIHSM_DDI_RSP_HDR  RspHdr            = { 0 };
  UINTN               HeaderDecodedSize = 0;
  UINT8               DataFieldCount    = 0;

  if ((Decoder == NULL) || (Response == NULL) || (DecodedSize == NULL)) {
    if (DecodedSize != NULL) {
      *DecodedSize = 0;
    }

    return EFI_INVALID_PARAMETER;
  }

  DecoderStartMark = Decoder->Position;

  // Decode the response header
  Status = DecodeCommandResponseHeader (Decoder, &RspHdr, &HeaderDecodedSize);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // Validate that this is indeed a SetSealedBks3 response
  if (RspHdr.DdiOp != DDI_OP_SET_SEALED_BKS3) {
    DEBUG ((DEBUG_WARN, "AziHsmDdi: Expected DDI_OP_SET_SEALED_BKS3, got %d\n", RspHdr.DdiOp));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Set the response based on DDI status
  // If DDI status is success (0), set response to TRUE, otherwise FALSE
  if (RspHdr.DdiStatus == DDI_STATUS_SUCCESS) {
    *Response = TRUE;
    DEBUG ((DEBUG_INFO, "AziHsmDdi: SetSealedBks3 operation successful, response set to TRUE\n"));
  } else {
    *Response = FALSE;
    DEBUG ((
      DEBUG_WARN,
      "AziHsmDdi: SetSealedBks3 request failed with DDI status: %d, response set to FALSE\n",
      RspHdr.DdiStatus
      ));
  }

  // Decode the data field ID
  Status = AziHsmMborDecodeFieldId (Decoder, &FieldId);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  if (FieldId != REQ_CMD_DATA_FIELD_ID) {
    DEBUG ((DEBUG_WARN, "AziHsmDdi: Expected data field ID %d, got %d\n", REQ_CMD_DATA_FIELD_ID, FieldId));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Decode the SetSealedBks3 response data structure (should be empty)
  Status = AziHsmMborDecodeMap (Decoder, &DataFieldCount);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  // For SetSealedBks3, the response data should be an empty struct (field count 0)
  if (DataFieldCount != 0) {
    DEBUG ((
      DEBUG_WARN,
      "AziHsmDdi: Expected 0 fields in SetSealedBks3 response data (empty struct), got %d\n",
      DataFieldCount
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Success - response decoding completed
  DEBUG ((
    DEBUG_INFO,
    "AziHsmDdi: SetSealedBks3 response successfully decoded, boolean result: %s\n",
    *Response ? "TRUE" : "FALSE"
    ));

ExitFunction:
  *DecodedSize = Decoder->Position - DecoderStartMark;
  return Status;
}

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
  )
{
  EFI_STATUS  Status            = EFI_SUCCESS;
  UINTN       HeaderEncodedSize = 0;

  if ((Encoder == NULL) || (EncodedSize == NULL)) {
    if (EncodedSize != NULL) {
      *EncodedSize = 0;
    }

    return EFI_INVALID_PARAMETER;
  }

  // Encode the request header
  Status = EncodeCommandRequestHeader (Encoder, DDI_OP_GET_SEALED_BKS3, ApiRev, SessionId, &HeaderEncodedSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsmDdi: EncodeCommandRequestHeader failed with status: %r\n", Status));
    goto ExitFunction;
  }

  // Encode field ID
  Status = AziHsmMborEncodeFieldId (Encoder, REQ_CMD_DATA_FIELD_ID);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsmDdi: AziHsmMborEncodeFieldId failed with status: %r\n", Status));
    goto ExitFunction;
  }

  // Encode the request data (empty for GetSealedBks3)
  Status = AziHsmMborEncodeMap (Encoder, 0);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsmDdi: AziHsmMborEncodeMap failed with status: %r\n", Status));
    goto ExitFunction;
  }

ExitFunction:
  *EncodedSize = Encoder->Position;
  return Status;
}

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
  IN OUT AZIHSM_MBOR_DECODER               *CONST  Decoder,
  OUT    AZIHSM_DDI_GET_SEALED_BKS3_RESP   *CONST  Response,
  OUT    UINTN                             *CONST  DecodedSize
  )
{
  EFI_STATUS          Status            = EFI_SUCCESS;
  UINT32              DecoderStartMark  = 0;
  UINT8               FieldId           = 0;
  AZIHSM_DDI_RSP_HDR  RspHdr            = { 0 };
  UINTN               HeaderDecodedSize = 0;
  UINT8               DataFieldCount    = 0;

  if ((Decoder == NULL) || (Response == NULL) || (DecodedSize == NULL)) {
    if (DecodedSize != NULL) {
      *DecodedSize = 0;
    }

    return EFI_INVALID_PARAMETER;
  }

  DecoderStartMark = Decoder->Position;

  // Decode the response header
  Status = DecodeCommandResponseHeader (Decoder, &RspHdr, &HeaderDecodedSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsmDdi: AzihsmDecodeGetSealedBks3Resp : DecodeCommandResponseHeader failed with status: %r\n",
      Status
      ));
    goto ExitFunction;
  }

  // Validate that this is indeed a GetSealedBks3 response
  if (RspHdr.DdiOp != DDI_OP_GET_SEALED_BKS3) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsmDdi: AzihsmDecodeGetSealedBks3Resp : Expected DDI_OP_GET_SEALED_BKS3, got %d\n",
      RspHdr.DdiOp
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Check if the operation was successful
  if (RspHdr.DdiStatus != DDI_STATUS_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsmDdi: AzihsmDecodeGetSealedBks3Resp :  GetSealedBks3 request failed with DDI status: %d\n",
      RspHdr.DdiStatus
      ));
    Status = ConvertDdiStatusToEfiStatus (RspHdr.DdiStatus);
    goto ExitFunction;
  }

  // Decode the data field ID
  Status = AziHsmMborDecodeFieldId (Decoder, &FieldId);
  if (EFI_ERROR (Status)) {
    goto ExitFunction;
  }

  if (FieldId != REQ_CMD_DATA_FIELD_ID) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsmDdi: AzihsmDecodeGetSealedBks3Resp : Expected data field ID %d, got %d\n",
      REQ_CMD_DATA_FIELD_ID,
      FieldId
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Decode the GetSealedBks3 response data structure
  Status = AziHsmMborDecodeMap (Decoder, &DataFieldCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsmDdi: AzihsmDecodeGetSealedBks3Resp : AziHsmMborDecodeMap failed with status: %r\n",
      Status
      ));
    goto ExitFunction;
  }

  // For GetSealedBks3, the response data should contain the sealed BKS3 data
  if (DataFieldCount != AZIHSM_DDI_GET_SEALED_BKS3_RESP_FIELD_COUNT) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsmDdi: AzihsmDecodeGetSealedBks3Resp : Expected 1 field in GetSealedBks3 response data, got %d\n",
      DataFieldCount
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Read field id for actual data
  Status = AziHsmMborDecodeFieldId (Decoder, &FieldId);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsmDdi: AzihsmDecodeGetSealedBks3Resp : AziHsmMborDecodeFieldId failed with status: %r\n",
      Status
      ));
    goto ExitFunction;
  }

  // check if field id is response id or not
  if (FieldId != API_GET_SEALED_BKS3_CMD_RESP_DATA_FIELD_ID) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsmDdi: AzihsmDecodeGetSealedBks3Resp : Expected BKS3 response data field ID %d, got %d\n",
      API_GET_SEALED_BKS3_CMD_RESP_DATA_FIELD_ID,
      FieldId
      ));
    Status = EFI_PROTOCOL_ERROR;
    goto ExitFunction;
  }

  // Decode the sealed BKS3 data
  Status = AziHsmMborDecodePaddedBytes (Decoder, Response->SealedBks3.Data, &Response->SealedBks3.Length);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsmDdi: AzihsmDecodeGetSealedBks3Resp : AziHsmMborDecodeBytes failed with status: %r\n",
      Status
      ));
    goto ExitFunction;
  }

  // Success - response is valid and contains the expected data
  DEBUG ((DEBUG_INFO, "AziHsmDdi: AzihsmDecodeGetSealedBks3Resp : GetSealedBks3 response successfully decoded\n"));

ExitFunction:
  *DecodedSize = Decoder->Position - DecoderStartMark;
  return Status;
}
