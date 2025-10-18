/** @file
  Azure Integrated HSM BKS3 Key Derivation Implementation using TPM 2.0

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "AziHsmBKS3.h"
#include <Library/TpmMeasurementLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseCryptLib.h>
#include <Library/PcdLib.h>

//
// Forward declarations for internal helper functions
//

STATIC
EFI_STATUS
InternalTpm2CreatePrimary (
  IN  TPMI_RH_HIERARCHY       PrimaryHandle,
  IN  TPM2B_SENSITIVE_CREATE  *InSensitive,
  IN  TPM2B_PUBLIC            *InPublic,
  OUT TPM_HANDLE              *ObjectHandle
  );

STATIC
EFI_STATUS
InternalTpm2HMAC (
  IN  TPM_HANDLE        Handle,
  IN  TPM2B_MAX_BUFFER  *Buffer,
  IN  TPMI_ALG_HASH     HashAlg,
  OUT TPM2B_DIGEST      *Result
  );

STATIC
EFI_STATUS
InternalTpmHkdfExpand (
  IN  TPM_HANDLE        PrimaryHandle,
  IN  TPM2B_MAX_BUFFER  *KdfInput,
  IN  UINT8             *Info,
  IN  UINTN             InfoSize,
  OUT UINT8             *DerivedKey,
  IN  UINTN             DerivedKeySize
  );

STATIC
EFI_STATUS
ManualHkdfSha256Expand (
  IN  UINT8  *PRK,
  IN  UINTN  PRKSize,
  IN  UINT8  *Info,
  IN  UINTN  InfoSize,
  OUT UINT8  *DerivedKey,
  IN  UINTN  DerivedKeySize
  );

//
// ============================================================================
// INTERNAL TPM COMMAND IMPLEMENTATIONS USING Tpm2SubmitCommand
// ============================================================================
//

/**
  Internal structure for TPM2_ HMAC and CREATEPRIMARY commands.
**/
#pragma pack(1)

typedef struct {
  TPM2_COMMAND_HEADER    Header;            // tag / size / commandCode
  UINT32                 Handle;            // object handle
  UINT32                 AuthAreaSize;      // size of following auth session area
  UINT32                 SessionHandle;     // TPM_RS_PW or provided session
  UINT16                 NonceSize;         // 0
  UINT8                  SessionAttributes; // 0
  UINT16                 SessionHmacSize;   // 0
  UINT16                 BufferSize;        // size of data to HMAC
  UINT8                  CmdBuffer[AZIHSM_TPM_CMD_BUFSIZE];
} TPM2_HMAC_CMD;

typedef struct {
  TPM2_COMMAND_HEADER    Header;                            // tag/size/command
  UINT32                 PrimaryHandle;                     // hierarchy handle
  UINT32                 AuthAreaSize;                      // size of following auth session block
  UINT32                 SessionHandle;                     // TPM_RS_PW
  UINT16                 NonceSize;                         // 0
  UINT8                  SessionAttributes;                 // 0
  UINT16                 HmacSize;                          // 0
  UINT8                  CmdBuffer[AZIHSM_TPM_CMD_BUFSIZE]; //  inSensitive, inPublic, outsideInfo, creationPCR
} TPM2_CREATE_CMD;

// TPM2_Load command - header portion only (variable data follows)
typedef struct {
  TPM2_COMMAND_HEADER    Header;            // tag/size/command
  UINT32                 ParentHandle;      // parent handle
  UINT32                 AuthAreaSize;      // size of following auth session block
  UINT32                 SessionHandle;     // TPM_RS_PW
  UINT16                 NonceSize;         // 0
  UINT8                  SessionAttributes; // 0
  UINT16                 HmacSize;          // 0
} TPM2_LOAD_CMD_HEADER;

// Create aTPM2_Unseal command structure
typedef struct {
  TPM2_COMMAND_HEADER    Header;            // tag/size/command
  UINT32                 ObjectHandle;      // object handle
  UINT32                 AuthAreaSize;      // size of following auth session block
  UINT32                 SessionHandle;     // TPM_RS_PW
  UINT16                 NonceSize;         // 0
  UINT8                  SessionAttributes; // 0
  UINT16                 HmacSize;          // 0
} TPM2_UNSEAL_CMD;

#pragma pack()

/*
  Copy the public area data from the TPM2B_PUBLIC structure to the provided buffer.
  @param[in] InPublic             Pointer to the TPM2B_PUBLIC structure.
  @param[in,out] Buffer           Pointer to the buffer to copy data into.
  @param[in,out] BufferCapacity   Pointer to the size of the buffer remaining.

  @retval EFI_SUCCESS           Successfully copied the public area data.
  @retval EFI_INVALID_PARAMETER  Invalid parameter.
  @retval EFI_BUFFER_TOO_SMALL   Buffer is too small to hold the data.
*/
STATIC
EFI_STATUS
CopyPublicDataToBuffer (
  IN  CONST TPM2B_PUBLIC  *InPublic,
  IN OUT UINT8            **Buffer,
  IN OUT UINT32           *BufferCapacity
  )
{
  UINT32  DataFieldSize = 0;
  UINT16  SchemeAlg     = 0;
  UINT32  PublicContentSize = 0;
  UINT32  BytesToWrite = 0;
  UINT8  *BufPtr = NULL;
  UINTN  BytesWritten = 0;

  if ((InPublic == NULL) || (Buffer == NULL) || (BufferCapacity == NULL)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: CopyPublicDataToBuffer invalid parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  if (InPublic->publicArea.type == TPM_ALG_RSA) {
    SchemeAlg     = InPublic->publicArea.parameters.rsaDetail.scheme.scheme; // often TPM_ALG_NULL
    DataFieldSize = sizeof (InPublic->publicArea.parameters.rsaDetail.symmetric.algorithm) +
                    sizeof (InPublic->publicArea.parameters.rsaDetail.symmetric.keyBits.aes) +
                    sizeof (InPublic->publicArea.parameters.rsaDetail.symmetric.mode.aes) +
                    sizeof (InPublic->publicArea.parameters.rsaDetail.scheme.scheme) +
                    sizeof (InPublic->publicArea.parameters.rsaDetail.keyBits) +
                    sizeof (InPublic->publicArea.parameters.rsaDetail.exponent) +
                    sizeof (InPublic->publicArea.unique.rsa.size); // unique size field (digest empty)
  } else if (InPublic->publicArea.type == TPM_ALG_KEYEDHASH) {
    SchemeAlg = InPublic->publicArea.parameters.keyedHashDetail.scheme.scheme; // often TPM_ALG_NULL
    // Always include scheme algorithm (2). Only include scheme detail (hashAlg) if scheme != TPM_ALG_NULL.
    DataFieldSize = sizeof (InPublic->publicArea.parameters.keyedHashDetail.scheme.scheme);
    if (SchemeAlg != TPM_ALG_NULL) {
      DataFieldSize += sizeof (InPublic->publicArea.parameters.keyedHashDetail.scheme.details.hmac.hashAlg);
    }

    DataFieldSize += sizeof (InPublic->publicArea.unique.keyedHash.size) + InPublic->publicArea.unique.keyedHash.size;
  }

  // Base fields: type + nameAlg + objectAttributes + authPolicy.size + authPolicy.bytes + DataFieldSize
  PublicContentSize = (UINT32)(sizeof (InPublic->publicArea.type) +
                                       sizeof (InPublic->publicArea.nameAlg) +
                                       sizeof (InPublic->publicArea.objectAttributes) +
                                       sizeof (InPublic->publicArea.authPolicy.size) +
                                       InPublic->publicArea.authPolicy.size +
                                       DataFieldSize);

  BytesToWrite = sizeof (UINT16) + PublicContentSize;

  if (BytesToWrite > *BufferCapacity) {
    DEBUG ((DEBUG_ERROR, "AziHsm: CopyPublicDataToBuffer  input buffer too small\n"));
    return EFI_BUFFER_TOO_SMALL;
  }

  if (BytesToWrite > MAX_UINT16) {
    DEBUG ((DEBUG_ERROR, "AziHsm: CopyPublicDataToBuffer input buffer size exceeds maximum limit\n"));
    return EFI_BUFFER_TOO_SMALL;
  }

  BufPtr = *Buffer;

  WriteUnaligned16 ((UINT16 *)BufPtr, SwapBytes16 (*(UINT16 *)&PublicContentSize));
  BufPtr += sizeof (UINT16);

  // type
  WriteUnaligned16 ((UINT16 *)BufPtr, SwapBytes16 (InPublic->publicArea.type));
  BufPtr += sizeof (UINT16);

  // nameAlg
  WriteUnaligned16 ((UINT16 *)BufPtr, SwapBytes16 (InPublic->publicArea.nameAlg));
  BufPtr += sizeof (UINT16);

  // objectAttributes
  WriteUnaligned32 ((UINT32 *)BufPtr, SwapBytes32 (*(UINT32 *)&InPublic->publicArea.objectAttributes));
  BufPtr += sizeof (UINT32);

  // authPolicy (size field already accounted; currently size=0)
  WriteUnaligned16 ((UINT16 *)BufPtr, SwapBytes16 (InPublic->publicArea.authPolicy.size));
  BufPtr += sizeof (UINT16);
  if (InPublic->publicArea.authPolicy.size > 0) {
    CopyMem (BufPtr, InPublic->publicArea.authPolicy.buffer, InPublic->publicArea.authPolicy.size);
    BufPtr += InPublic->publicArea.authPolicy.size;
  }

  switch (InPublic->publicArea.type) {
    case TPM_ALG_RSA:
      WriteUnaligned16 ((UINT16 *)BufPtr, SwapBytes16 (InPublic->publicArea.parameters.rsaDetail.symmetric.algorithm));
      BufPtr += sizeof (UINT16);
      WriteUnaligned16 (
        (UINT16 *)BufPtr,
        SwapBytes16 (InPublic->publicArea.parameters.rsaDetail.symmetric.keyBits.aes)
        );
      BufPtr += sizeof (UINT16);
      WriteUnaligned16 ((UINT16 *)BufPtr, SwapBytes16 (InPublic->publicArea.parameters.rsaDetail.symmetric.mode.aes));
      BufPtr += sizeof (UINT16);
      WriteUnaligned16 ((UINT16 *)BufPtr, SwapBytes16 (InPublic->publicArea.parameters.rsaDetail.scheme.scheme));
      BufPtr += sizeof (UINT16);
      WriteUnaligned16 ((UINT16 *)BufPtr, SwapBytes16 ((UINT16)InPublic->publicArea.parameters.rsaDetail.keyBits));
      BufPtr += sizeof (UINT16);
      WriteUnaligned32 ((UINT32 *)BufPtr, SwapBytes32 (InPublic->publicArea.parameters.rsaDetail.exponent));
      BufPtr += sizeof (UINT32);
      WriteUnaligned16 ((UINT16 *)BufPtr, SwapBytes16 (0)); /* unique size */
      BufPtr += sizeof (UINT16);
      break;

    case TPM_ALG_KEYEDHASH:
      WriteUnaligned16 ((UINT16 *)BufPtr, SwapBytes16 (SchemeAlg));
      BufPtr += sizeof (UINT16);
      if (SchemeAlg != TPM_ALG_NULL) {
        WriteUnaligned16 (
          (UINT16 *)BufPtr,
          SwapBytes16 (InPublic->publicArea.parameters.keyedHashDetail.scheme.details.hmac.hashAlg)
          );
        BufPtr += sizeof (UINT16);
      }

      WriteUnaligned16 ((UINT16 *)BufPtr, SwapBytes16 (InPublic->publicArea.unique.keyedHash.size));
      BufPtr += sizeof (UINT16);
      if (InPublic->publicArea.unique.keyedHash.size > 0) {
        CopyMem (BufPtr, InPublic->publicArea.unique.keyedHash.buffer, InPublic->publicArea.unique.keyedHash.size);
        BufPtr += InPublic->publicArea.unique.keyedHash.size;
      }

      break;

    default:
      break;
  }

  BytesWritten = (BufPtr - *Buffer);

  // Update buffer pointer and remaining buffer size
  if ((UINT32)BytesWritten == BytesToWrite) {
    *BufferCapacity -= (UINT32)(BytesWritten);
    *Buffer          = BufPtr;
    return EFI_SUCCESS;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsm: CopyPublicDataToBuffer BytesToWrite=%u, BytesWritten=%u\n",
      BytesToWrite,
      BytesWritten
      ));
    return EFI_BUFFER_TOO_SMALL;
  }
}

/**
 * Copy Sensitive data from the TPM2B_SENSITIVE_CREATE to the provided buffer.
 * @param[in]  *InSensitive  Pointer to the sensitive data structure.
 * @param[in,out] Buffer     Pointer to the buffer to copy data into.
 * @param[in,out] BufferCapacity  Pointer to the buffer capacity.
 */
STATIC
EFI_STATUS
CopySensitiveData (
  IN  CONST TPM2B_SENSITIVE_CREATE  *InSensitive,
  IN OUT UINT8                      **Buffer,
  IN OUT UINT32                     *BufferCapacity
  )
{
  UINT16  UserAuthLen = 0;
  UINT16  DataLen     = 0;
  UINT32  SensitiveBodySize = 0;
  UINT32  TotalNeeded = 0;
  UINT8  *BufPtr = NULL;
  UINTN  BytesWritten = 0;

  if ((InSensitive == NULL) || (Buffer == NULL) || (BufferCapacity == NULL)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: CopySensitiveData: Invalid parameter to function.\n"));
    return EFI_INVALID_PARAMETER;
  }

  // Validate declared sizes against their buffers.
  if ((InSensitive->sensitive.userAuth.size > sizeof (InSensitive->sensitive.userAuth.buffer)) ||
      (InSensitive->sensitive.data.size > sizeof (InSensitive->sensitive.data.buffer)))
  {
    DEBUG ((DEBUG_ERROR, "AziHsm: CopySensitiveData: Sensitive data struct size and buffer malformed\n"));
    return EFI_BAD_BUFFER_SIZE;
  }

  UserAuthLen = InSensitive->sensitive.userAuth.size;
  DataLen     = InSensitive->sensitive.data.size;

  // Compute the TPMS_SENSITIVE_CREATE payload size (excluding outer size field).
  // Layout inside the TPM2B body: userAuth (2+N) + data (2+M)
  SensitiveBodySize = sizeof (UINT16) + UserAuthLen + sizeof (UINT16) + DataLen;

  if (SensitiveBodySize > MAX_UINT16) {
    DEBUG ((DEBUG_ERROR, "AziHsm: CopySensitiveData: Sensitive data buffer sizes are incorrect\n"));
    return EFI_BAD_BUFFER_SIZE;
  }

  // Total bytes we will write including outer size field
  TotalNeeded = sizeof (UINT16) + SensitiveBodySize;

  if (*BufferCapacity < TotalNeeded) {
    DEBUG ((DEBUG_ERROR, "AziHsm: CopySensitiveData: Buffer too small\n"));
    return EFI_BUFFER_TOO_SMALL;
  }

  BufPtr = *Buffer;

  // Outer size (size of TPMS_SENSITIVE_CREATE body only)
  WriteUnaligned16 ((UINT16 *)BufPtr, SwapBytes16 ((UINT16)SensitiveBodySize));
  BufPtr += sizeof (UINT16);

  // userAuth.size
  WriteUnaligned16 ((UINT16 *)BufPtr, SwapBytes16 (UserAuthLen));
  BufPtr += sizeof (UINT16);

  if (UserAuthLen > 0) {
    CopyMem (BufPtr, InSensitive->sensitive.userAuth.buffer, UserAuthLen);
    BufPtr += UserAuthLen;
  }

  // data.size
  WriteUnaligned16 ((UINT16 *)BufPtr, SwapBytes16 (DataLen));
  BufPtr += sizeof (UINT16);

  if (DataLen > 0) {
    CopyMem (BufPtr, InSensitive->sensitive.data.buffer, DataLen);
    BufPtr += DataLen;
  }

  // Update buffer pointer and remaining buffer size
  BytesWritten = (BufPtr - *Buffer);

  if (BytesWritten == TotalNeeded) {
    *BufferCapacity -= (UINT32)BytesWritten;
    *Buffer          = BufPtr;
    return EFI_SUCCESS;
  } else {
    DEBUG ((DEBUG_ERROR, "AziHsm: CopySensitiveData - buffer too small\n"));
    return EFI_BUFFER_TOO_SMALL;
  }
}

/**
  Internal helper to execute TPM2_CreatePrimary command.

  Manually constructs TPM2_CreatePrimary command following TPM 2.0 specification
  format. Based on tpm2-tools reference implementation approach.

  @param[in]  PrimaryHandle    Hierarchy for the primary object.
  @param[in]  InSensitive      Sensitive data for the object.
  @param[in]  InPublic         Public template for the object.
  @param[out] ObjectHandle     Handle of the created primary object.

  @retval EFI_SUCCESS          Operation completed successfully.
  @retval EFI_DEVICE_ERROR     The command was unsuccessful.
**/
STATIC
EFI_STATUS
InternalTpm2CreatePrimary (
  IN  TPMI_RH_HIERARCHY       PrimaryHandle,
  IN  TPM2B_SENSITIVE_CREATE  *InSensitive,
  IN  TPM2B_PUBLIC            *InPublic,
  OUT TPM_HANDLE              *ObjectHandle
  )
{
  EFI_STATUS            Status;
  UINT8                 RecvBuffer[AZIHSM_TPM_RSP_BUFSIZE];
  UINT32                RecvBufferSize;
  TPM2_RESPONSE_HEADER  *ResponseHeader;
  UINT8                 *BufPtr;
  UINT32                BufCapacity;
  TPM2_CREATE_CMD       SendBuffer;
  UINT32                TotalSize = 0;
  UINT32                ResponseCode = 0;
  UINT8                 *RspCursor;

  if ((InSensitive == NULL) || (InPublic == NULL) || (ObjectHandle == NULL)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: InternalTpm2CreatePrimary - invalid parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "AziHsm: InternalTpm2CreatePrimary (struct) - building command\n"));

  ZeroMem (&SendBuffer, sizeof (SendBuffer));

  // Fixed header fields (size later)
  SendBuffer.Header.tag         = SwapBytes16 (TPM_ST_SESSIONS);
  SendBuffer.Header.commandCode = SwapBytes32 (TPM_CC_CreatePrimary);
  SendBuffer.PrimaryHandle      = SwapBytes32 (PrimaryHandle);

  // Single password session (empty auth)
  SendBuffer.SessionHandle = SwapBytes32 (TPM_RS_PW);
  SendBuffer.AuthAreaSize  = SwapBytes32 (
                               sizeof (SendBuffer.SessionHandle) +
                               sizeof (SendBuffer.NonceSize) +
                               sizeof (SendBuffer.SessionAttributes) +
                               sizeof (SendBuffer.HmacSize)
                               );

  // Serialize variable parameters into CmdBuffer
  BufPtr      = SendBuffer.CmdBuffer;
  BufCapacity = sizeof (SendBuffer.CmdBuffer);

  // ---- inSensitive ----
  Status = CopySensitiveData (InSensitive, &BufPtr, &BufCapacity);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: CopySensitiveData failed\n"));
    goto Cleanup;
  }

  // ---- inPublic ----
  Status = CopyPublicDataToBuffer (InPublic, &BufPtr, &BufCapacity);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: CopyPublicDataToBuffer failed : Buffer sizing error\n"));
    goto Cleanup;
  }

  // ---- outsideInfo (TPM2B_DATA empty) ----
  if (BufCapacity < sizeof (UINT16)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: InternalTpm2CreatePrimary - outsideInfo buffer too small\n"));
    Status = EFI_BUFFER_TOO_SMALL;
    goto Cleanup;
  }

  WriteUnaligned16 ((UINT16 *)BufPtr, SwapBytes16 (0));
  BufPtr      += sizeof (UINT16);
  BufCapacity -= sizeof (UINT16);

  // ---- creationPCR (TPML_PCR_SELECTION empty) ----
  if (BufCapacity < sizeof (UINT32)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: InternalTpm2CreatePrimary - creationPCR buffer too small\n"));
    Status = EFI_BUFFER_TOO_SMALL;
    goto Cleanup;
  }

  WriteUnaligned32 ((UINT32 *)BufPtr, SwapBytes32 (0));
  BufPtr      += sizeof (UINT32);
  BufCapacity -= sizeof (UINT32);

  // Final size
  TotalSize = (UINT32)(OFFSET_OF (TPM2_CREATE_CMD, CmdBuffer) + (BufPtr - SendBuffer.CmdBuffer));

  SendBuffer.Header.paramSize = SwapBytes32 (TotalSize);

  // Transmit
  RecvBufferSize = sizeof (RecvBuffer);
  ZeroMem (RecvBuffer, RecvBufferSize);
  Status = Tpm2SubmitCommand (TotalSize, (UINT8 *)&SendBuffer, &RecvBufferSize, RecvBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Tpm2SubmitCommand failed. Status: %r\n", Status));
    goto Cleanup;
  }

  if (RecvBufferSize < sizeof (TPM2_RESPONSE_HEADER)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: CreatePrimary response too small\n"));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  ResponseHeader = (TPM2_RESPONSE_HEADER *)RecvBuffer;
  ResponseCode = SwapBytes32 (ResponseHeader->responseCode);
  if (ResponseCode != TPM_RC_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsm: CreatePrimary command failed with TPM error code: 0x%08X\n",
      ResponseCode
      ));
    if (ResponseCode == TPM_RC_HIERARCHY) {
      DEBUG ((DEBUG_ERROR, "AziHsm: TPM_RC_HIERARCHY - Hierarchy is not enabled or not correct for use\n"));
    }
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  // Parse response: header | handle | parameterSize | params | auth
  RspCursor = RecvBuffer + sizeof (TPM2_RESPONSE_HEADER);

  *ObjectHandle = SwapBytes32 (ReadUnaligned32 ((UINT32 *)RspCursor));
  Status        = EFI_SUCCESS;

Cleanup:
  ZeroMem (&SendBuffer, sizeof (SendBuffer));
  ZeroMem (RecvBuffer, sizeof (RecvBuffer));
  return Status;
}

/**
  Internal helper to execute TPM2_HMAC command using manual marshalling.
  Based on the successful CreatePrimary fix approach.

  @param[in]  Handle        Handle of the key to use for HMAC.
  @param[in]  Buffer        Data to be HMACed.
  @param[in]  HashAlg       Hash algorithm to use.
  @param[out] OutHMAC       HMAC result.

  @retval EFI_SUCCESS       Operation completed successfully.
  @retval EFI_DEVICE_ERROR  The command was unsuccessful.
**/
STATIC
EFI_STATUS
InternalTpm2HMAC (
  IN  TPMI_DH_OBJECT    Handle,
  IN  TPM2B_MAX_BUFFER  *Buffer,
  IN  TPMI_ALG_HASH     HashAlg,
  OUT TPM2B_DIGEST      *OutHMAC
  )
{
  EFI_STATUS            Status;
  UINT8                 RecvBuffer[AZIHSM_TPM_RSP_BUFSIZE];
  UINT32                RecvBufferSize;
  TPM2_HMAC_CMD         SendBuffer;
  UINT32                TotalSize = 0;
  TPM2_RESPONSE_HEADER  *Rsp      = NULL;
  UINT16                RspTag    = 0;
  UINT32                Rc        = 0;
  UINT8                 *RspPtr   = NULL;

  if ((Buffer == NULL) || (OutHMAC == NULL) || (Buffer->size == 0) || (Buffer->size > MAX_DIGEST_BUFFER)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: InternalTpm2HMAC invalid parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&SendBuffer, sizeof (SendBuffer));

  // Header
  SendBuffer.Header.tag         = SwapBytes16 (TPM_ST_SESSIONS);
  SendBuffer.Header.commandCode = SwapBytes32 (TPM_CC_HMAC);
  // size filled later

  // Object handle
  SendBuffer.Handle = SwapBytes32 (Handle);

  // Auth area: single password (or provided) session with empty nonce/HMAC
  SendBuffer.SessionHandle = SwapBytes32 (TPM_RS_PW);

  SendBuffer.AuthAreaSize = SwapBytes32 (
                              sizeof (SendBuffer.SessionHandle) +
                              sizeof (SendBuffer.NonceSize) +
                              sizeof (SendBuffer.SessionAttributes) +
                              sizeof (SendBuffer.SessionHmacSize)
                              );

  // Buffer size + data
  SendBuffer.BufferSize = SwapBytes16 (Buffer->size);
  CopyMem (SendBuffer.CmdBuffer, Buffer->buffer, Buffer->size);

  // HashAlg goes immediately after data
  if ((Buffer->size + sizeof (UINT16)) > sizeof (SendBuffer.CmdBuffer)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: InternalTpm2HMAC Command Buffer too small\n"));
    return EFI_BUFFER_TOO_SMALL;
  }

  *(UINT16 *)(SendBuffer.CmdBuffer + Buffer->size) = SwapBytes16 (HashAlg);

  // Compute total command size
  TotalSize = (UINT32)(OFFSET_OF (TPM2_HMAC_CMD, CmdBuffer) + Buffer->size + sizeof (UINT16));

  SendBuffer.Header.paramSize = SwapBytes32 (TotalSize);

  // Transmit
  RecvBufferSize = sizeof (RecvBuffer);
  ZeroMem (RecvBuffer, RecvBufferSize);
  Status = Tpm2SubmitCommand (TotalSize, (UINT8 *)&SendBuffer, &RecvBufferSize, RecvBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Tpm2SubmitCommand (HMAC) failed\n"));
    goto Cleanup;
  }

  if (RecvBufferSize < sizeof (TPM2_RESPONSE_HEADER)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: HMAC response too small\n"));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  Rsp   = (TPM2_RESPONSE_HEADER *)RecvBuffer;
  RspTag = SwapBytes16 (Rsp->tag);
  Rc     = SwapBytes32 (Rsp->responseCode);

  if (Rc == TPM_RC_SUCCESS) {
    RspPtr = RecvBuffer + sizeof (TPM2_RESPONSE_HEADER);
    if (RspTag == TPM_ST_SESSIONS) {
      if ((UINTN)(RspPtr + sizeof (UINT32) - RecvBuffer) > RecvBufferSize) {
        DEBUG ((DEBUG_ERROR, "AziHsm: HMAC response from TPM too small\n"));
        Status = EFI_DEVICE_ERROR;
        goto Cleanup;
      }

      // Skip parameterSize
      RspPtr += sizeof (UINT32);
    }

    if ((UINTN)(RspPtr + sizeof (UINT16) - RecvBuffer) > RecvBufferSize) {
      DEBUG ((DEBUG_ERROR, "AziHsm: HMAC response from TPM too small\n"));
      Status = EFI_DEVICE_ERROR;
      goto Cleanup;
    }

    OutHMAC->size = SwapBytes16 (ReadUnaligned16 ((UINT16 *)RspPtr));
    RspPtr       += sizeof (UINT16);

    if ((OutHMAC->size > sizeof (OutHMAC->buffer)) || ((UINTN)(RspPtr + OutHMAC->size - RecvBuffer) > RecvBufferSize)) {
      DEBUG ((DEBUG_ERROR, "AziHsm: HMAC result too large or truncated\n"));
      Status = EFI_DEVICE_ERROR;
      goto Cleanup;
    }

    CopyMem (OutHMAC->buffer, RspPtr, OutHMAC->size);

    DEBUG ((DEBUG_INFO, "AziHsm: HMAC success, size=%u\n", OutHMAC->size));
    Status = EFI_SUCCESS;
  } else {
    DEBUG ((DEBUG_ERROR, "AziHsm: HMAC failed with error code %u\n", Rc));
    Status = EFI_DEVICE_ERROR;
  }

Cleanup:
  ZeroMem (&SendBuffer, sizeof (SendBuffer));
  ZeroMem (RecvBuffer, sizeof (RecvBuffer));
  return Status;
}

/**
  Derive key material using HKDF-Expand with TPM-based PRK.

  This function implements RFC 5869 HKDF-Expand using a TPM primary key handle
  to generate the pseudo-random key (PRK) via HMAC, then performs HKDF-Expand
  to derive the final key material.

  @param[in]  PrimaryHandle    TPM handle of the primary key for PRK generation.
  @param[in]  KdfInput         Input material for PRK generation (salt/context).
  @param[in]  Info             Application-specific context information.
  @param[in]  InfoSize         Size of the context information in bytes.
  @param[out] DerivedKey       Buffer to receive the derived key material.
  @param[in]  DerivedKeySize   Size of the derived key material to generate.

  @retval EFI_SUCCESS          Key derivation completed successfully.
  @retval EFI_INVALID_PARAMETER Invalid input parameters.
  @retval EFI_DEVICE_ERROR     TPM operation failed or Crypto library operation failed.
**/
STATIC
EFI_STATUS
InternalTpmHkdfExpand (
  IN  TPM_HANDLE        PrimaryHandle,
  IN  TPM2B_MAX_BUFFER  *KdfInput,
  IN  UINT8             *Info,
  IN  UINTN             InfoSize,
  OUT UINT8             *DerivedKey,
  IN  UINTN             DerivedKeySize
  )
{
  EFI_STATUS    Status;
  TPM2B_DIGEST  HmacResult;
  UINT8  PRK[SHA256_DIGEST_SIZE]; // For SHA-256


  /**
   * DerivedKeySize > 255 * 32: HKDF limit L ≤ 255 * HashLen (HashLen=32 for SHA-256).
   * Because block index is a single octet (T(1)..T(n)), you can’t produce more than 255 blocks.
   */
  if ((KdfInput == NULL) || (DerivedKey == NULL) ||
      (DerivedKeySize == 0) || (DerivedKeySize > MAX_HKDF_BLOCKS * SHA256_DIGEST_SIZE) ||
      (Info == NULL) || (InfoSize > SHA256_DIGEST_SIZE))
  {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "AziHsm: HKDF-Expand: Step 1 - PRK generation via TPM HMAC\n"));

  //
  // Step 1: PRK via TPM HMAC using the input material as HMAC data
  //
  ZeroMem (&HmacResult, sizeof (HmacResult));
  ZeroMem (PRK, sizeof (PRK));

  Status = InternalTpm2HMAC (
             PrimaryHandle,
             KdfInput,
             TPM_ALG_SHA256,
             &HmacResult
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: HKDF-Expand: TPM HMAC for PRK generation failed\n"));
    return Status;
  }

  // Copy TPM HMAC result to PRK buffer (should be 32 bytes for SHA-256)
  if (HmacResult.size != sizeof (PRK)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: HKDF-Expand: PRK size mismatch\n"));
    ZeroMem (&HmacResult, sizeof (HmacResult));
    return EFI_DEVICE_ERROR;
  }

  CopyMem (PRK, HmacResult.buffer, HmacResult.size);

  //
  // Step 2: HKDF-Expand in software using CryptoPkg
  //
  Status = ManualHkdfSha256Expand (
             PRK,                           // PRK from TPM
             sizeof (PRK),                  // PRK size (32 bytes)
             Info,                          // Context info
             InfoSize,                      // Info size
             DerivedKey,                    // Output buffer
             DerivedKeySize                 // Output size
             );

  //
  // Clear sensitive PRK material
  //
  ZeroMem (&HmacResult, sizeof (HmacResult));
  ZeroMem (PRK, sizeof (PRK));

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: HKDF-Expand: Manual HKDF operation failed\n"));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "AziHsm: HKDF-Expand: Key derivation completed successfully\n"));
  return EFI_SUCCESS;
}

/**
  Manual implementation of HKDF-Expand per RFC 5869.

  This function implements the HKDF-Expand operation using available HMAC
  functions from BaseCryptLib when HkdfSha256Expand is not available.

  @param[in]  PRK              Pseudo-random key (output from HKDF-Extract).
  @param[in]  PRKSize          Size of the PRK in bytes.
  @param[in]  Info             Application-specific context information.
  @param[in]  InfoSize         Size of the context information in bytes.
  @param[out] DerivedKey       Buffer to receive the derived key material.
  @param[in]  DerivedKeySize   Size of the derived key material to generate.

  @retval EFI_SUCCESS          Key derivation completed successfully.
  @retval EFI_INVALID_PARAMETER Invalid input parameters.
  @retval EFI_DEVICE_ERROR      HMAC operation failed.
**/
STATIC
EFI_STATUS
ManualHkdfSha256Expand (
  IN  UINT8  *PRK,
  IN  UINTN  PRKSize,
  IN  UINT8  *Info,
  IN  UINTN  InfoSize,
  OUT UINT8  *DerivedKey,
  IN  UINTN  DerivedKeySize
  )
{
  BOOLEAN  CryptoResult;
  UINTN    NumBlocks;
  UINT8    T_prev[SHA256_DIGEST_SIZE];
  UINT8    T_current[SHA256_DIGEST_SIZE];
  UINT8    HmacInputBuffer[SHA256_DIGEST_SIZE + AZIHSM_HKDF_MAX_INFO_LEN + 1];
  UINTN    HmacInputSize;
  UINTN    OutputOffset = 0;
  UINT8    Counter;
  UINTN    BytesToCopy  = 0;

  if (  (PRK == NULL) || (DerivedKey == NULL) || (PRKSize != SHA256_DIGEST_SIZE) ||
        (DerivedKeySize == 0) || (DerivedKeySize > MAX_HKDF_BLOCKS * SHA256_DIGEST_SIZE)
     || (Info == NULL) || (InfoSize > AZIHSM_HKDF_MAX_INFO_LEN))
  {
    return EFI_INVALID_PARAMETER;
  }

  NumBlocks = (DerivedKeySize + SHA256_DIGEST_SIZE - 1) / SHA256_DIGEST_SIZE;

  if (NumBlocks > MAX_HKDF_BLOCKS) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (T_prev, sizeof (T_prev));
  ZeroMem (T_current, sizeof (T_current));
  ZeroMem (HmacInputBuffer, sizeof (HmacInputBuffer));

  for (Counter = 1; Counter <= NumBlocks; Counter++) {
    // Build HMAC input: T(i-1) | info | Counter
    HmacInputSize = 0;

    // T(i-1): empty for first iteration, previous T for subsequent
    if (Counter > 1) {
      CopyMem (HmacInputBuffer + HmacInputSize, T_prev, SHA256_DIGEST_SIZE);
      HmacInputSize += SHA256_DIGEST_SIZE;
    }

    // Info
    if (InfoSize > 0) {
      CopyMem (HmacInputBuffer + HmacInputSize, Info, InfoSize);
      HmacInputSize += InfoSize;
    }

    // Counter (single byte)
    HmacInputBuffer[HmacInputSize] = Counter;
    HmacInputSize                 += 1;

    // Compute T(i) = HMAC-SHA256(PRK, T(i-1) | info | Counter)
    CryptoResult = HmacSha256All (HmacInputBuffer, HmacInputSize, PRK, PRKSize, T_current);

    if (!CryptoResult) {
      DEBUG ((DEBUG_ERROR, "AziHsm: ManualHkdfExpand: HMAC computation failed\n"));
      ZeroMem (T_prev, sizeof (T_prev));
      ZeroMem (T_current, sizeof (T_current));
      ZeroMem (HmacInputBuffer, sizeof (HmacInputBuffer));
      return EFI_DEVICE_ERROR;
    }

    // Copy appropriate amount to output
    BytesToCopy = MIN (SHA256_DIGEST_SIZE, DerivedKeySize - OutputOffset);
    CopyMem (DerivedKey + OutputOffset, T_current, BytesToCopy);
    OutputOffset += BytesToCopy;

    // T(i) becomes T(i-1) for next iteration
    CopyMem (T_prev, T_current, SHA256_DIGEST_SIZE);

    if (OutputOffset >= DerivedKeySize) {
      break;
    }
  }

  // Clear sensitive material
  ZeroMem (T_prev, sizeof (T_prev));
  ZeroMem (T_current, sizeof (T_current));
  ZeroMem (HmacInputBuffer, sizeof (HmacInputBuffer));

  return EFI_SUCCESS;
}

/**
  Create platform hierarchy KeyedHash primary key.

  This function creates a KeyedHash primary key under the platform hierarchy
  (TPM_RH_PLATFORM) with the following characteristics:
  - No policy associated (simple RS_PW authorization)
  - KeyedHash type with HMAC scheme using SHA256

  @param[out] PrimaryHandle  Pointer to receive the handle of the created primary key.
  @param[in]  PrimaryKeyUserData  Pointer to the user data for the primary key.
  @param[in]  PrimaryKeyUserDataLength  Length of the user data for the primary key.

  @retval EFI_SUCCESS        The primary key was created successfully.
  @retval Others             An error occurred during key creation.
**/
EFI_STATUS
EFIAPI
AziHsmCreatePlatformPrimaryKeyedHash (
  OUT TPM_HANDLE  *PrimaryHandle,
  IN BYTE         *PrimaryKeyUserData,
  IN UINT16       PrimaryKeyUserDataLength
  )
{
  EFI_STATUS              Status;
  TPM2B_SENSITIVE_CREATE  InSensitive;
  TPM2B_PUBLIC            InPublic;
  TPM_HANDLE              Handle = 0;

  // Validate parameters: Max Buffer size for (InSensitive.sensitive.data.buffer) is MAX_SYM_DATA
  if (  (PrimaryHandle == NULL) || (PrimaryKeyUserData == NULL)
     || (PrimaryKeyUserDataLength > MAX_SYM_DATA))
  {
    DEBUG ((DEBUG_ERROR, "AziHsm: CreatePlatformPrimaryKeyedHash invalid parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  *PrimaryHandle = 0;

  DEBUG ((DEBUG_INFO, "AziHsm: Creating platform hierarchy KeyedHash primary (no policy)\n"));

  ZeroMem (&InSensitive, sizeof (InSensitive));
  InSensitive.size                    = sizeof (TPMS_SENSITIVE_CREATE);
  InSensitive.sensitive.userAuth.size = 0; // Empty platformAuth assumed
  CopyMem (InSensitive.sensitive.data.buffer, PrimaryKeyUserData, PrimaryKeyUserDataLength);
  InSensitive.sensitive.data.size = PrimaryKeyUserDataLength;

  ZeroMem (&InPublic, sizeof (InPublic));
  InPublic.size                                     = sizeof (TPMT_PUBLIC);
  InPublic.publicArea.type                          = TPM_ALG_KEYEDHASH;
  InPublic.publicArea.nameAlg                       = TPM_ALG_SHA256;
  InPublic.publicArea.objectAttributes.fixedTPM     = 1;
  InPublic.publicArea.objectAttributes.fixedParent  = 1;
  InPublic.publicArea.objectAttributes.userWithAuth = 1;         // RS_PW allowed
  InPublic.publicArea.objectAttributes.sign         = 1;
  InPublic.publicArea.objectAttributes.noDA         = 1;         // No dictionary attack protection
  // Not restricted, no decrypt, no policy -> simple HMAC key
  InPublic.publicArea.authPolicy.size                                        = 0;
  InPublic.publicArea.parameters.keyedHashDetail.scheme.scheme               = TPM_ALG_HMAC;
  InPublic.publicArea.parameters.keyedHashDetail.scheme.details.hmac.hashAlg = TPM_ALG_SHA256;
  InPublic.publicArea.unique.keyedHash.size                                  = 0;

  Status = InternalTpm2CreatePrimary (
             TPM_RH_PLATFORM,   // Use platform hierarchy
             &InSensitive,
             &InPublic,
             &Handle
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Platform CreatePrimary failed. Status: %r\n", Status));
    goto Cleanup;
  }

  *PrimaryHandle = Handle;
  Status         = EFI_SUCCESS;
  DEBUG ((DEBUG_INFO, "AziHsm: Platform primary KeyedHash created\n"));

  Cleanup:
  ZeroMem (&InSensitive, sizeof (InSensitive));
  ZeroMem (&InPublic, sizeof (InPublic));
  return Status;
}

/**

  This function implements the complete secret derivation process:
  1. Create Primary KeyedHash based on platform hierarchy
  2. HMAC KDF Derivation

  @param[in, out] DerivedKey Pointer to the structure to hold the derived key material.

  @retval EFI_SUCCESS  The key derivation workflow completed successfully.
  @retval Others       An error occurred during the workflow.
**/
EFI_STATUS
EFIAPI
AziHsmDeriveSecretFromTpm (
  IN OUT AZIHSM_DERIVED_KEY  *DerivedKey
  )
{
  EFI_STATUS        Status;
  TPM_HANDLE        PrimaryHandle;
  TPM2B_MAX_BUFFER  KdfInput;
  TPM2B_MAX_BUFFER  AppInfo;
  CONST CHAR8       *WellKnownString           = AZIHSM_HASH_USER_INPUT;
  CHAR8             PrimaryKeyUserData[AZIHSM_PRIMARY_KEY_USER_DATA_MAX_LEN] = AZIHSM_PRIMARY_KEY_USER_DATA;
  UINT16            PrimaryKeyUserDataLength   = (UINT16)AsciiStrLen (PrimaryKeyUserData);

  if (DerivedKey == NULL) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Invalid parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  PrimaryHandle = 0;

  //
  // Primary Key User Data to be input to primary key creation
  //
  DEBUG ((DEBUG_INFO, "AziHsm: Creating Platform hierarchy primary\n"));
  Status = AziHsmCreatePlatformPrimaryKeyedHash (&PrimaryHandle, PrimaryKeyUserData, PrimaryKeyUserDataLength);
  AZIHSM_CHECK_RC (Status, "Primary (platform) creation failed\n");

  //
  // Step 2: HMAC KDF Derivation
  //

  ZeroMem (KdfInput.buffer, sizeof (KdfInput.buffer));
  ZeroMem (AppInfo.buffer, sizeof (AppInfo.buffer));
  ZeroMem (DerivedKey->KeyData, sizeof (DerivedKey->KeyData));
  DerivedKey->KeySize = 0;

  //
  // Prepare HMAC input: Well-known string
  //
  KdfInput.size = (UINT16)AsciiStrLen (WellKnownString);
  if (KdfInput.size > sizeof (KdfInput.buffer)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: KDF input string too long\n"));
    Status = EFI_INVALID_PARAMETER;
    goto Cleanup;
  }

  CopyMem (KdfInput.buffer, WellKnownString, KdfInput.size);

  AppInfo.size = (UINT16)AsciiStrLen (AZIHSM_APPLICATION_INFO);
  if (AppInfo.size > sizeof (AppInfo.buffer)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Application info string too long\n"));
    Status = EFI_INVALID_PARAMETER;
    goto Cleanup;
  }

  CopyMem (AppInfo.buffer, AZIHSM_APPLICATION_INFO, AppInfo.size);

  Status = InternalTpmHkdfExpand (
             PrimaryHandle,
             &KdfInput,
             (UINT8 *)AppInfo.buffer,
             AppInfo.size,
             DerivedKey->KeyData,
             AZIHSM_DERIVED_KEY_SIZE
             );

  AZIHSM_CHECK_RC (Status, "Step 2: HKDF-Expand KDF failed");

  DerivedKey->KeySize = AZIHSM_DERIVED_KEY_SIZE;

Cleanup:
  //
  // Clean up TPM handles
  //

  ZeroMem (PrimaryKeyUserData, sizeof (PrimaryKeyUserData));
  ZeroMem (&KdfInput.buffer, sizeof (KdfInput.buffer));
  ZeroMem (&AppInfo.buffer, sizeof (AppInfo.buffer));

  if (PrimaryHandle != 0) {
    DEBUG ((DEBUG_INFO, "AziHsm: Flushing TPM primary handle\n"));
    Tpm2FlushContext (PrimaryHandle);
    PrimaryHandle = 0;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Key derivation workflow failed\n"));
  }

  return Status;
}

/**
   Given the Manticore PCI Identifier(serial number) and the Unsealed blob, use manual KDF to derive the BKS3 key
  @param[in]  UnsealedBlob         Pointer to the unsealed blob containing necessary data.
  @param[in]  PciIdentifier        Pointer to the PCI Identifier (serial number).
  @param[in]  PciIdentifierLength  Length of the PCI Identifier in bytes.
  @param[out] DerivedKey           Pointer to the structure to hold the derived key material.

  @retval EFI_SUCCESS              The key derivation completed successfully.
  @retval Others                   An error occurred during the key derivation.
 */

EFI_STATUS
AziHsmDeriveSecretFromBlob (
  IN AZIHSM_BUFFER  *UnsealedBlob,
  IN UINT8          *PciIdentifier,
  IN UINTN          PciIdentifierLength,
  OUT AZIHSM_DERIVED_KEY  *DerivedKey
  )
{
  EFI_STATUS        Status;
  UINT8             HmacResult[SHA256_DIGEST_SIZE];
  UINT8             PciIdBuffer[AZIHSM_PCI_IDENTIFIER_MAX_LEN];
  UINT8             InfoBuffer[AZIHSM_HKDF_MAX_INFO_LEN];
  UINTN             InfoSize;
  TPM2B_MAX_BUFFER  HmacInput;
  BOOLEAN           HmacSuccess;
  CONST CHAR8       *AppInfoString = AZIHSM_APPLICATION_INFO;

  if ((UnsealedBlob == NULL) || (UnsealedBlob->Size == 0) ||
      (DerivedKey == NULL) || (PciIdentifier == NULL) ||
      (PciIdentifierLength == 0) || (PciIdentifierLength > AZIHSM_PCI_IDENTIFIER_MAX_LEN))
  {
    DEBUG ((DEBUG_ERROR, "AziHsm: Invalid parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "AziHsm: Starting BKS3 key derivation from unsealed blob..\n"));

  ZeroMem (HmacResult, sizeof (HmacResult));
  ZeroMem (PciIdBuffer, sizeof (PciIdBuffer));
  ZeroMem (InfoBuffer, sizeof (InfoBuffer));
  ZeroMem (&HmacInput, sizeof (HmacInput));

  //
  // Convert PCI Identifier to byte array (big-endian)
  //
  WriteUnaligned32 ((UINT32 *)PciIdBuffer, SwapBytes32 (*(UINT32 *)PciIdentifier));

  //
  // Step 1: PRK via HMAC using the unsealed blob as key and PCI Identifier as data
  //
  HmacInput.size = (UINT16)PciIdentifierLength;
  CopyMem (HmacInput.buffer, PciIdBuffer, PciIdentifierLength);

  DEBUG ((DEBUG_INFO, "AziHsm: Step 1 - PRK generation via HMAC\n"));

  // HmacSha256All returns BOOLEAN, not EFI_STATUS
  HmacSuccess = HmacSha256All (
             HmacInput.buffer,
             HmacInput.size,
             UnsealedBlob->Data,
             UnsealedBlob->Size,
             HmacResult
             );
  if (!HmacSuccess) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Step 1: HMAC for PRK generation failed\n"));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  // Step 2: HKDF-Expand in software using CryptoPkg
  InfoSize = (UINTN)AsciiStrLen (AppInfoString);
  if (InfoSize > sizeof (InfoBuffer)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Application info string too long\n"));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  CopyMem (InfoBuffer, AppInfoString, InfoSize);
  DEBUG ((DEBUG_INFO, "AziHsm: Step 2 - HKDF-Expand\n"));

  Status = ManualHkdfSha256Expand (
             HmacResult,                   // PRK from HMAC
             SHA256_DIGEST_SIZE,           // PRK size (32 bytes)
             InfoBuffer,                   // Context info
             InfoSize,                     // Info size
             DerivedKey->KeyData,          // Output buffer
             AZIHSM_DERIVED_KEY_SIZE       // Output size
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Step 2: HKDF-Expand failed\n"));
    goto Exit;
  }

  DerivedKey->KeySize = AZIHSM_DERIVED_KEY_SIZE;

  DEBUG ((DEBUG_INFO, "AziHsm: Step 2 - HKDF-Expand completed successfully\n"));
  Status = EFI_SUCCESS;

Exit:
  ZeroMem (HmacResult, sizeof (HmacResult));
  ZeroMem (PciIdBuffer, sizeof (PciIdBuffer));
  ZeroMem (InfoBuffer, sizeof (InfoBuffer));
  ZeroMem (&HmacInput, sizeof (HmacInput));
  return Status;
}

/**
  Measure Azure Integrated HSM device unique GUID to TPM.

  @param[in]  Context      Azure Integrated HSM TCG context

  @retval EFI_SUCCESS      Measurement completed successfully
  @retval Other            Error occurred during measurement
**/
EFI_STATUS
AziHsmMeasureGuidEvent (
  IN AZIHSM_TCG_CONTEXT  *Context
  )
{
  EFI_STATUS  Status;

  if (Context == NULL) {
    DEBUG ((DEBUG_ERROR, "AziHsm: No valid context found, skipping measurement\n"));
    return EFI_INVALID_PARAMETER;
  }

  // Measure the GUID to TPM PCR 6
  Status = TpmMeasureAndLogData (
             AZIHSM_TCG_PCR_INDEX,
             AZIHSM_TCG_EVENT_TYPE,
             Context,
             sizeof (*Context),
             Context,
             sizeof (*Context)
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to measure Azure Integrated HSM GUID: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
 Create a NULL hierarchy primary key.

 @param[out] PrimaryHandle  The handle of the created primary key.

 @retval EFI_SUCCESS        The primary key was created successfully.
 @retval EFI_INVALID_PARAMETER  The parameter is invalid.
 @retval EFI_OUT_OF_RESOURCES  Failed to allocate resources.
*/
EFI_STATUS
AziHsmCreateNullAesPrimary (
  OUT UINT32  *PrimaryHandle
  )
{
  TPM_HANDLE              Handle = 0;
  TPM2B_SENSITIVE_CREATE  InSensitive;
  TPM2B_PUBLIC            InPublic;
  EFI_STATUS              Status;

  if (PrimaryHandle == NULL) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Invalid parameter - PrimaryHandle is NULL\n"));
    return EFI_INVALID_PARAMETER;
  }


  ZeroMem (&InPublic, sizeof (InPublic));
  ZeroMem (&InSensitive, sizeof (InSensitive));
  InSensitive.sensitive.userAuth.size = 0;
  InSensitive.sensitive.data.size     = 0;

  // Build an RSA storage primary (restricted+decrypt) suitable as parent for sealed object under NULL hierarchy.
  InPublic.publicArea.type                                       = TPM_ALG_RSA;
  InPublic.publicArea.nameAlg                                    = TPM_ALG_SHA256;
  InPublic.publicArea.objectAttributes.fixedTPM                  = 1;
  InPublic.publicArea.objectAttributes.fixedParent               = 1;
  InPublic.publicArea.objectAttributes.sensitiveDataOrigin       = 1;
  InPublic.publicArea.objectAttributes.userWithAuth              = 1;
  InPublic.publicArea.objectAttributes.noDA                      = 1;
  InPublic.publicArea.objectAttributes.restricted                = 1;
  InPublic.publicArea.objectAttributes.decrypt                   = 1;
  InPublic.publicArea.objectAttributes.sign                      = 0; // storage only
  InPublic.publicArea.parameters.rsaDetail.symmetric.algorithm   = TPM_ALG_AES;
  InPublic.publicArea.parameters.rsaDetail.symmetric.keyBits.aes = AES_KEYBITS;
  InPublic.publicArea.parameters.rsaDetail.symmetric.mode.aes    = TPM_ALG_CFB;
  InPublic.publicArea.parameters.rsaDetail.scheme.scheme         = TPM_ALG_NULL; // no signing scheme
  InPublic.publicArea.parameters.rsaDetail.keyBits               = KEYBITS_SIZE;
  InPublic.publicArea.parameters.rsaDetail.exponent              = 0; // default 65537
  InPublic.publicArea.unique.rsa.size                            = 0; // let TPM fill

  Status = InternalTpm2CreatePrimary (
                        TPM_RH_NULL,
                        &InSensitive,
                        &InPublic,
                        &Handle
                        );

  ZeroMem (&InSensitive, sizeof (InSensitive));

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [MIG] InternalTpm2CreatePrimary failed %r\n", Status));
    // Note: when underlying library exposes raw RC, we could decode; currently Status only.
    return Status;
  }

  *PrimaryHandle = Handle;
  ZeroMem (&Handle, sizeof (Handle));
  return EFI_SUCCESS;
}

/**
 * Seal a buffer under a TPM primary key.
 *
 * @param[in] ParentHandle  The handle of the parent key.
 * @param[in] PlainBuffer    The buffer to seal.
 * @param[in] PlainSize      The size of the buffer to seal.
 * @param[out] SealedBlob  The sealed blob output.
 */
EFI_STATUS
AziHsmTpmSealBuffer (
  IN  UINT32         ParentHandle,
  IN  CONST VOID     *PlainBuffer,
  IN  UINTN          PlainSize,
  OUT AZIHSM_BUFFER  *SealedBlob
  )
{
  UINTN  Capacity = 0;
  TPM2_CREATE_CMD  SendBuffer;
  UINT8            RecvBuffer[AZIHSM_TPM_RSP_BUFSIZE];
  UINT32           RecvBufferSize;
  TPM2B_SENSITIVE_CREATE  InSensitive;
  UINTN  SensitivePayloadLen = 0;
  TPM2B_PUBLIC  InPublic;
  UINT8       *BufPtr     = NULL;
  UINT32      BufCapacity = 0;
  EFI_STATUS  Status;
  UINT32  TotalSize = 0;
  // Response parsing
  TPM2_RESPONSE_HEADER  *ResponseHeader;
  UINT32                Responsecode   = 0;
  UINT8  *RspCursor;
  UINT32  ParamSize;
  UINT16  OutPrivBody = 0;
  UINT8  *PrivStart = NULL;
  UINT16  OutPubBody = 0;
  UINT8  *PubLenPos = NULL;
  UINTN  Needed = 0;
  UINT8  *Dst = NULL;


  if ((ParentHandle == 0) || (PlainBuffer == NULL) || (PlainSize == 0) ||
      (SealedBlob == NULL) || (SealedBlob->Data == NULL))
  {
    DEBUG ((DEBUG_ERROR, "AziHsm: AziHsmTpmSealBuffer() Invalid parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  Capacity = sizeof (SealedBlob->Data);

  SealedBlob->Size = 0;

  // Guard: TPM2B_SENSITIVE_CREATE data max to keep command small
  if (PlainSize > MAX_DIGEST_BUFFER) {
    return EFI_BAD_BUFFER_SIZE;
  }


  ZeroMem (&SendBuffer, sizeof (SendBuffer));

  SendBuffer.Header.tag         = SwapBytes16 (TPM_ST_SESSIONS);
  SendBuffer.Header.commandCode = SwapBytes32 (TPM_CC_Create);
  SendBuffer.PrimaryHandle      = SwapBytes32 (ParentHandle);

  // Single password session (empty auth)
  SendBuffer.SessionHandle = SwapBytes32 (TPM_RS_PW);
  SendBuffer.AuthAreaSize  = SwapBytes32 (
                               sizeof (SendBuffer.SessionHandle) +
                               sizeof (SendBuffer.NonceSize) +
                               sizeof (SendBuffer.SessionAttributes) +
                               sizeof (SendBuffer.HmacSize)
                               );

  ZeroMem (&InSensitive, sizeof (InSensitive));

  InSensitive.sensitive.userAuth.size = 0; // Empty platformAuth assumed
  CopyMem (InSensitive.sensitive.data.buffer, PlainBuffer, PlainSize);
  InSensitive.sensitive.data.size = (UINT16)PlainSize;
  SensitivePayloadLen =  sizeof (InSensitive.sensitive.userAuth.size) +
                               InSensitive.sensitive.userAuth.size
                               +sizeof (InSensitive.sensitive.data.size) +
                               InSensitive.sensitive.data.size;

  if (SensitivePayloadLen > MAX_SYM_DATA ) {
    DEBUG ((DEBUG_ERROR, "AziHsm: AziHsmTpmSealBuffer() Sensitive data too large\n"));
    return EFI_BAD_BUFFER_SIZE;
  }

  InSensitive.size = (UINT16)SensitivePayloadLen;


  ZeroMem (&InPublic, sizeof (InPublic));
  InPublic.size                                     = sizeof (TPMT_PUBLIC);
  InPublic.publicArea.type                          = TPM_ALG_KEYEDHASH;
  InPublic.publicArea.nameAlg                       = TPM_ALG_SHA256;
  InPublic.publicArea.objectAttributes.fixedTPM     = 1;
  InPublic.publicArea.objectAttributes.fixedParent  = 1;
  InPublic.publicArea.objectAttributes.userWithAuth = 1;         // RS_PW allowed
  InPublic.publicArea.objectAttributes.noDA         = 1;         // No dictionary attack protection

  InPublic.publicArea.authPolicy.size                          = 0;
  InPublic.publicArea.parameters.keyedHashDetail.scheme.scheme = TPM_ALG_NULL;
  InPublic.publicArea.unique.keyedHash.size                    = 0;

  // Serialize variable parameters into CmdBuffer
  BufPtr     = SendBuffer.CmdBuffer;
  BufCapacity = sizeof (SendBuffer.CmdBuffer);

  // ---- inSensitive ----
  Status = CopySensitiveData (&InSensitive, &BufPtr, &BufCapacity);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: CopySensitiveData failed\n"));
    return Status;
  }

  // ---- inPublic ----
  Status = CopyPublicDataToBuffer (&InPublic, &BufPtr, &BufCapacity);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: CopyPublicDataToBuffer failed : Buffer sizing error\n"));
    return Status;
  }

  // ---- outsideInfo (TPM2B_DATA empty) ----
  if (BufCapacity < sizeof (UINT16)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: AziHsmTpmSealBuffer - outsideInfo buffer too small\n"));
    return EFI_BUFFER_TOO_SMALL;
  }

  WriteUnaligned16 ((UINT16 *)BufPtr, SwapBytes16 (0));
  BufPtr      += sizeof (UINT16);
  BufCapacity -= sizeof (UINT16);

  // ---- creationPCR (TPML_PCR_SELECTION empty) ----
  if (BufCapacity < sizeof (UINT32)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: AziHsmTpmSealBuffer - creationPCR buffer too small\n"));
    return EFI_BUFFER_TOO_SMALL;
  }

  WriteUnaligned32 ((UINT32 *)BufPtr, SwapBytes32 (0));
  BufPtr      += sizeof (UINT32);
  BufCapacity -= sizeof (UINT32);

  // Final size
  TotalSize = (UINT32)(OFFSET_OF (TPM2_CREATE_CMD, CmdBuffer) + (BufPtr - SendBuffer.CmdBuffer));

  SendBuffer.Header.paramSize = SwapBytes32 (TotalSize);
  DEBUG ((
    DEBUG_WARN,
    "AziHsm: [MIG] Seal command size: %d bytes, data size: %d bytes\n",
    TotalSize,
    (UINT32)PlainSize
    ));
  DEBUG ((
    DEBUG_WARN,
    "AziHsm: [MIG] Parent handle: 0x%X, command tag: 0x%X\n",
    ParentHandle,
    SwapBytes16 (SendBuffer.Header.tag)
    ));

  ZeroMem (RecvBuffer, sizeof (RecvBuffer));
  RecvBufferSize = sizeof (RecvBuffer);
  Status         = Tpm2SubmitCommand (TotalSize, (UINT8 *)&SendBuffer, &RecvBufferSize, RecvBuffer);

  if (EFI_ERROR (Status) || (RecvBufferSize < sizeof (TPM2_RESPONSE_HEADER))) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [MIG] Seal submit failed st=%r resp=%u\n", Status, RecvBufferSize));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  ResponseHeader = (TPM2_RESPONSE_HEADER *)RecvBuffer;
  Responsecode    = SwapBytes32 (ResponseHeader->responseCode);

  if (Responsecode != TPM_RC_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [MIG] Seal failed rc=0x%X\n", Responsecode));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  if (RecvBufferSize < sizeof (TPM2_RESPONSE_HEADER) + sizeof (UINT32)) {
    DEBUG ((DEBUG_ERROR, "Cmd does not contain sufficient bytes\n"));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  // Parse response: header | handle | parameterSize | params | auth
  RspCursor = RecvBuffer + sizeof (TPM2_RESPONSE_HEADER);

  ParamSize = SwapBytes32 (*(UINT32 *)RspCursor);

  RspCursor += sizeof (UINT32);
  // Validate that declared ParamSize fits in remaining response buffer
  if (ParamSize > (RecvBufferSize - sizeof (TPM2_RESPONSE_HEADER) - sizeof (ParamSize))) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsm: Seal response paramSize overflow (%u > %u)\n",
      ParamSize,
      (UINT32)(RecvBufferSize - sizeof (TPM2_RESPONSE_HEADER) - sizeof (ParamSize))
      ));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  // outPrivate
  if (RspCursor + sizeof (UINT16) > RecvBuffer + RecvBufferSize) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Seal response outPrivate overflow\n"));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  OutPrivBody = SwapBytes16 (*(UINT16 *)RspCursor);

  RspCursor += sizeof (UINT16);

  if (RspCursor + OutPrivBody > RecvBuffer + RecvBufferSize) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Seal response outPrivate overflow\n"));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  // Copy outPrivate into temp linear form (including its leading size field) for packing
  PrivStart = RspCursor - sizeof (UINT16); // points to the start of the 2-byte length we just consumed+advanced

  RspCursor += OutPrivBody; // advance cursor over private body

  // outPublic
  if (RspCursor + sizeof (UINT16) > RecvBuffer + RecvBufferSize) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Seal response outPublic overflow\n"));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  OutPubBody = SwapBytes16 (*(UINT16 *)RspCursor);

  RspCursor += sizeof (UINT16);

  if (RspCursor + OutPubBody > RecvBuffer + RecvBufferSize) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Seal response outPublic overflow\n"));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  PubLenPos = RspCursor - sizeof (UINT16);

  RspCursor += OutPubBody;

  UINT16  PrivTotal = (UINT16)(OutPrivBody + sizeof (UINT16));
  UINT16  PubTotal  = (UINT16)(OutPubBody + sizeof (UINT16));

  // Required packed size: 2 + PrivTotal + 2 + PubTotal
  Needed = sizeof (UINT16) + PrivTotal + sizeof (UINT16) + PubTotal;

  if (Needed > Capacity) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsm: [MIG] Seal packed buffer too small need=%u cap=%u\n",
      (UINT32)Needed,
      (UINT32)Capacity
      ));
    Status = EFI_BUFFER_TOO_SMALL;
    goto Cleanup;
  }

  Dst = SealedBlob->Data;

  // Copy Size fields + bodies
  CopyMem (Dst, &PrivTotal, sizeof (UINT16));
  Dst += sizeof (UINT16);
  CopyMem (Dst, PrivStart, PrivTotal);
  Dst += PrivTotal;

  // Copy public area size + body
  CopyMem (Dst, &PubTotal, sizeof (UINT16));
  Dst += sizeof (UINT16);
  CopyMem (Dst, PubLenPos, PubTotal);
  Dst += PubTotal;

  SealedBlob->Size = (UINT16)(Dst - SealedBlob->Data);
  Status           = EFI_SUCCESS;

Cleanup:

  ZeroMem (&SendBuffer, sizeof (SendBuffer));
  ZeroMem (RecvBuffer, sizeof (RecvBuffer));
  return Status;
}

/*
  Cleanup the TPM resources.
  @param[in] PrimaryHandle  The handle of the primary key.
*/
VOID
AziHsmMigTpmCleanup (
  IN UINT32  PrimaryHandle
  )
{
  if (PrimaryHandle != 0) {
    Tpm2FlushContext (PrimaryHandle);
  }
}

/*
  Seals the ephemeral key blob using the TPM NULL hierarchy. This will ensure that the
  blob is tied to the current boot session (TPM NULL hierarchy seed is reset on a reboot).
  Any primary keys created under Null hierarchy cannot be recreated after reboot. It ensures
  that we dont persist secrets across reboots.

  @param[in]  DataBuffer  Pointer to AZIHSM_BUFFER containing wrapped ephemeral key.
  @param[out] SealedBlob   Pointer to AZIHSM_BUFFER to hold the sealed blob.

  @retval EFI_SUCCESS              The sealing operation was successful.
  @retval EFI_INVALID_PARAMETER     One or more parameters are invalid.
  @retval EFI_DEVICE_ERROR         An error occurred while accessing the TPM.
  */
EFI_STATUS
AziHsmSealToNullHierarchy (
  IN  AZIHSM_BUFFER  *DataBuffer,
  OUT AZIHSM_BUFFER  *SealedBlob
  )
{
  EFI_STATUS  Status;
  UINT32      Primary = 0;

  if ((DataBuffer == NULL) || (SealedBlob == NULL)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: SealEphemeralNullHierarchy invalid parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  if ((DataBuffer->Size == 0) || (DataBuffer->Size > AZIHSM_BUFFER_MAX_SIZE)) {
    DEBUG ((DEBUG_ERROR, "AziHsm:  SealEphemeralNullHierarchy invalid BKSEphemeralWrapped size parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "AziHsm:  Creating NULL primary for sealing\n"));
  Status = AziHsmCreateNullAesPrimary (&Primary);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm:  Failed to create NULL primary for sealing: %r\n", Status));
    goto Exit;
  }

  DEBUG ((DEBUG_INFO, "AziHsm:  Created NULL primary handle 0x%X\n", Primary));

  // Directly seal into caller-provided packed buffer using new packed API
  Status = AziHsmTpmSealBuffer (
             Primary,
             DataBuffer->Data,
             DataBuffer->Size,
             SealedBlob
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: [MIG] SealEphemeralNullHierarchy failed %r\n", Status));
    goto Exit;
  }

  /* Validate the sealed blob */
  if ((SealedBlob->Size == 0) || (SealedBlob->Size > sizeof (SealedBlob->Data))) {
    DEBUG ((
      DEBUG_ERROR,
      "AziHsm: [MIG] SealEphemeralNullHierarchy produced malformed blob size size=%u\n",
      SealedBlob->Size
      ));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  Status = EFI_SUCCESS;
Exit:
  AziHsmMigTpmCleanup (Primary);
  DEBUG ((
    DEBUG_INFO,
    "AziHsm: [MIG] SealEphemeralNullHierarchy st=%r total=%u\n",
    Status,
    (SealedBlob != NULL) ? SealedBlob->Size : 0
    ));
  return Status;
}

/**
  Load a sealed Buffer into a TPM and return the object handle

  @param[in] Primary         The handle of the primary key.
  @param[in] SealedBlob      The sealed blob to load.
  @param[out] ObjectHandle   The handle of the loaded sealed object.

  @retval EFI_SUCCESS              The load operation was successful.
  @retval EFI_INVALID_PARAMETER     One or more parameters are invalid.
  @retval EFI_DEVICE_ERROR         An error occurred while accessing the TPM.
 */

EFI_STATUS
AziHsmTpmLoadSealedBuffer(
  IN  UINT32         Primary,
  IN  AZIHSM_BUFFER  *SealedBlob,
  OUT UINT32        *ObjectHandle
  ) {

  EFI_STATUS  Status;
  UINT8   *Cur = NULL, *End = NULL, *PrivBlob = NULL, *PubBlob = NULL;
  UINT16   PrivTotal = 0, PubTotal = 0;
  UINT16   PrivBodySize = 0;
  UINT16   PubBodySize = 0;
  UINT8                   SendBuffer[AZIHSM_TPM_CMD_BUFSIZE];
  UINT8                   RecvBuffer[AZIHSM_TPM_RSP_BUFSIZE];
  UINT32                  RecvBufferSize;
  TPM2_LOAD_CMD_HEADER    *LoadCmd;
  UINT8                   *CmdPtr;
  UINT32                  RequiredSize = 0;
  UINT32                  TotalSize = 0;
  TPM2_RESPONSE_HEADER  *ResponseHeader;
  UINT32                Responsecode;


  if (Primary == 0 || SealedBlob == NULL || ObjectHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Load the sealed buffer into the TPM
  Cur = SealedBlob->Data;
  End = SealedBlob->Data + SealedBlob->Size;

  if (Cur + sizeof(UINT16) > End) {
    Status = EFI_COMPROMISED_DATA;
    DEBUG ((DEBUG_ERROR, "AziHsm: Sealed blob too small to contain private size\n"));
    goto Exit;
  }

  PrivTotal = *(UINT16 *)Cur;
  Cur      += sizeof(UINT16);
  if (Cur + PrivTotal > End) {
    Status = EFI_COMPROMISED_DATA;
    DEBUG ((DEBUG_ERROR, "AziHsm: Sealed blob too small to contain private blob\n"));
    goto Exit;
  }

  PrivBlob = Cur;
  Cur     += PrivTotal;
  if (Cur + sizeof(UINT16) > End) {
    Status = EFI_COMPROMISED_DATA;
    DEBUG ((DEBUG_ERROR, "AziHsm: Sealed blob too small to contain public size\n"));
    goto Exit;
  }

  PubTotal = *(UINT16 *)Cur;
  Cur     += sizeof(UINT16);
  if (Cur + PubTotal > End) {
    Status = EFI_COMPROMISED_DATA;
    DEBUG ((DEBUG_ERROR, "AziHsm: Sealed blob too small to contain public blob\n"));
    goto Exit;
  }
  PubBlob = Cur;

  if ((PrivTotal <= sizeof(UINT16)) || (PubTotal <= sizeof(UINT16))) {
    Status = EFI_COMPROMISED_DATA;
    DEBUG ((DEBUG_ERROR, "AziHsm: Sealed blob has invalid TPM2B sizes\n"));
    goto Exit;
  }

  // PrivBlob and PubBlob point to TPM2B structures: [2-byte size][body]
  // Extract body sizes (the first 2 bytes of each blob)
  PrivBodySize = SwapBytes16(*(UINT16*)PrivBlob);
  PubBodySize = SwapBytes16(*(UINT16*)PubBlob);
  
  
  // Validate that body sizes match what we expect (PrivTotal/PubTotal should equal bodySize + 2)
  if ((PrivBodySize + sizeof(UINT16) != PrivTotal) || (PubBodySize + sizeof(UINT16) != PubTotal)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: TPM2B size mismatch - privBody=%u privTotal=%u pubBody=%u pubTotal=%u\n", 
            (UINT32)PrivBodySize, (UINT32)PrivTotal, (UINT32)PubBodySize, (UINT32)PubTotal));
    Status = EFI_COMPROMISED_DATA;
    goto Exit;
  }

  // Build TPM2_Load command: use structure for header, then append variable data
  
  ZeroMem (SendBuffer, sizeof (SendBuffer));
  ZeroMem (RecvBuffer, sizeof (RecvBuffer));

  // Build command header using structure
  LoadCmd = (TPM2_LOAD_CMD_HEADER *)SendBuffer;
  LoadCmd->Header.tag         = SwapBytes16 (TPM_ST_SESSIONS);
  LoadCmd->Header.commandCode = SwapBytes32 (TPM_CC_Load);
  LoadCmd->ParentHandle       = SwapBytes32 (Primary);
  
  // Single password session (empty auth)
  LoadCmd->SessionHandle = SwapBytes32 (TPM_RS_PW);
  LoadCmd->AuthAreaSize  = SwapBytes32 (
                             sizeof (LoadCmd->SessionHandle) +
                             sizeof (LoadCmd->NonceSize) +
                             sizeof (LoadCmd->SessionAttributes) +
                             sizeof (LoadCmd->HmacSize)
                           );
  LoadCmd->NonceSize         = 0;
  LoadCmd->SessionAttributes = 0;
  LoadCmd->HmacSize          = 0;
  
  // Append variable data after header
  CmdPtr = SendBuffer + sizeof(TPM2_LOAD_CMD_HEADER);

    // Calculate total size needed for the command
  RequiredSize = sizeof(TPM2_LOAD_CMD_HEADER) + sizeof(UINT16) + PrivBodySize + sizeof(UINT16) + PubBodySize;
  if (RequiredSize > sizeof(SendBuffer)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Load command buffer too small, required=%u, available=%u\n", RequiredSize, (UINT32)sizeof(SendBuffer)));
    Status = EFI_BUFFER_TOO_SMALL;
    goto Exit;
  }
  
  // InPrivate: size + body
  WriteUnaligned16((UINT16 *)CmdPtr, SwapBytes16(PrivBodySize));
  CmdPtr += sizeof(UINT16);
  CopyMem(CmdPtr, PrivBlob + sizeof(UINT16), PrivBodySize);
  CmdPtr += PrivBodySize;
  
  // InPublic: size + body
  WriteUnaligned16((UINT16 *)CmdPtr, SwapBytes16(PubBodySize));
  CmdPtr += sizeof(UINT16);
  CopyMem(CmdPtr, PubBlob + sizeof(UINT16), PubBodySize);
  CmdPtr += PubBodySize;

  // Set total command size
  TotalSize = (UINT32)(CmdPtr - SendBuffer);
  LoadCmd->Header.paramSize = SwapBytes32 (TotalSize);
  DEBUG ((DEBUG_WARN, "AziHsm: Load command size: %d bytes\n", TotalSize));

  RecvBufferSize = sizeof (RecvBuffer);
  Status         = Tpm2SubmitCommand (TotalSize, SendBuffer, &RecvBufferSize, RecvBuffer);
  if (EFI_ERROR (Status) || (RecvBufferSize < sizeof (TPM2_RESPONSE_HEADER))) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Load submit failed st=%r resp=%u\n", Status, RecvBufferSize));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  ResponseHeader = (TPM2_RESPONSE_HEADER *)RecvBuffer;
  Responsecode   = SwapBytes32 (ResponseHeader->responseCode);
  if (Responsecode != TPM_RC_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "AziHsm: TPM Load failed rc=0x%X\n", Responsecode));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  if (RecvBufferSize < (sizeof (TPM2_RESPONSE_HEADER) + sizeof (UINT32))) {
   DEBUG ((DEBUG_ERROR, "AziHsm: Load response too small\n"));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  *ObjectHandle = SwapBytes32 (*(UINT32 *)(RecvBuffer + sizeof (TPM2_RESPONSE_HEADER)));
  DEBUG ((DEBUG_INFO, "AziHsm: Load success, handle=0x%X\n", *ObjectHandle));


  Status = EFI_SUCCESS;
  Exit:
  ZeroMem (SendBuffer, sizeof (SendBuffer));
  ZeroMem (RecvBuffer, sizeof (RecvBuffer));
  return Status;
}


/** 
 *
 *  Unseal a sealed buffer given a loaded object handle 
 *
 */

 EFI_STATUS
 AziHsmTpmUnsealBuffer(
  IN  UINT32         LoadedObjectHandle,
  OUT AZIHSM_BUFFER  *UnsealedBlob
  ) {

  EFI_STATUS  Status;
  TPM2_RESPONSE_HEADER  *ResponseHeader;
  UINT32                Responsecode;
  UINT8                 *RspCursor, *ParamEnd;
  UINT16                OutDataSize;
  TPM2_UNSEAL_CMD  SendBuffer;
  UINT8            RecvBuffer[AZIHSM_TPM_RSP_BUFSIZE];
  UINT32           RecvBufferSize;
  UINT32           TotalSize = 0;
  UINT32           ParamSize = 0;

  if (LoadedObjectHandle == 0 || UnsealedBlob == NULL) {
    DEBUG ((DEBUG_ERROR, "AziHsm: UnsealBuffer invalid parameter\n"));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  // Unseal the sealed object

  ZeroMem (&SendBuffer, sizeof (SendBuffer));
  ZeroMem (RecvBuffer, sizeof (RecvBuffer));
  SendBuffer.Header.tag         = SwapBytes16 (TPM_ST_SESSIONS);
  SendBuffer.Header.commandCode = SwapBytes32 (TPM_CC_Unseal);
  SendBuffer.ObjectHandle      = SwapBytes32 (LoadedObjectHandle);
  // Single password session (empty auth)
  SendBuffer.SessionHandle = SwapBytes32 (TPM_RS_PW);
  SendBuffer.AuthAreaSize  = SwapBytes32 (
                                sizeof (SendBuffer.SessionHandle) +
                                sizeof (SendBuffer.NonceSize) +
                                sizeof (SendBuffer.SessionAttributes) +
                                sizeof (SendBuffer.HmacSize)
                                );
  TotalSize = sizeof (TPM2_UNSEAL_CMD);
  SendBuffer.Header.paramSize = SwapBytes32 (TotalSize);
  RecvBufferSize = sizeof (RecvBuffer);
  Status         = Tpm2SubmitCommand (TotalSize, (UINT8 *)&SendBuffer, &RecvBufferSize, RecvBuffer);
  if (EFI_ERROR (Status) || (RecvBufferSize < sizeof (TPM2_RESPONSE_HEADER))) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Unseal submit failed st=%r resp=%u\n", Status, RecvBufferSize));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }


  ResponseHeader = (TPM2_RESPONSE_HEADER *)RecvBuffer;
  Responsecode   = SwapBytes32 (ResponseHeader->responseCode);
  if (Responsecode != TPM_RC_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "AziHsm: TPM Unseal failed rc=0x%X\n", Responsecode));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  if (RecvBufferSize < (sizeof (TPM2_RESPONSE_HEADER) + sizeof (UINT32) + sizeof (UINT16))) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Unseal response too small\n"));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  RspCursor = RecvBuffer + sizeof (TPM2_RESPONSE_HEADER);
  ParamSize = SwapBytes32 (*(UINT32 *)RspCursor);
  RspCursor += sizeof (UINT32);
  ParamEnd  = RspCursor + ParamSize;
  // Validate that declared ParamSize fits in remaining response buffer
  if (ParamEnd > (RecvBuffer + RecvBufferSize)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Unseal response parameter size mismatch\n"));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  if (RspCursor + sizeof (UINT16) > ParamEnd) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Unseal response outData overflow\n"));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  // Get the size of unsealed data
  OutDataSize = SwapBytes16 (*(UINT16 *)RspCursor);
  RspCursor  += sizeof (UINT16);

  if (RspCursor + OutDataSize > ParamEnd) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Unseal response outData overflow\n"));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  UnsealedBlob->Size = OutDataSize;

  if (OutDataSize > sizeof (UnsealedBlob->Data)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Unseal outData buffer too small need=%u cap=%u\n", OutDataSize, (UINT32)sizeof (UnsealedBlob->Data)));
    Status = EFI_BUFFER_TOO_SMALL;
    goto Exit;
  }

  // Copy the unsealed data to caller buffer
  CopyMem (UnsealedBlob->Data, RspCursor, OutDataSize);

  Status = EFI_SUCCESS;

  Exit:
  ZeroMem (&SendBuffer, sizeof (SendBuffer));
  ZeroMem (RecvBuffer, sizeof (RecvBuffer));
  return Status;
}

/**
  Unseal a TPM Null Hierarchy sealed blob. This function unseals a sealed blob only tied to
  the current boot session since TPM Null hierarchy seed gets reset on a reboot.
 *
 * @param[in]  SealedBlob   Pointer to AZIHSM_BUFFER containing the sealed blob.
 * @param[out] UnsealedBlob Pointer to AZIHSM_BUFFER to hold the unsealed blob.
 *
 * @retval EFI_SUCCESS              The unsealing operation was successful.
 * @retval EFI_INVALID_PARAMETER     One or more parameters are invalid.
 * @retval EFI_DEVICE_ERROR         An error occurred while accessing the TPM.
 */
EFI_STATUS
AziHsmUnsealNullHierarchy(
  IN AZIHSM_BUFFER *SealedBlob, 
  OUT AZIHSM_BUFFER *UnsealedBlob
) {
  EFI_STATUS          Status;
  UINT32              Primary      = 0;
  UINT32              ObjectHandle = 0;

  // Validate input args 
  if (SealedBlob == NULL || UnsealedBlob == NULL) {
    DEBUG ((DEBUG_ERROR, "AziHsm: AziHsmUnsealNullHierarchy invalid parameter\n"));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  Status = AziHsmCreateNullAesPrimary (&Primary);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Create primary for unseal failed %r\n", Status));
    goto Exit;
  }

  // Unseal the buffer using the TPM Load and unseal
  // Load the sealed object
  Status = AziHsmTpmLoadSealedBuffer(Primary, SealedBlob, &ObjectHandle);
  if (EFI_ERROR(Status)) {
    DEBUG((DEBUG_ERROR, "AziHsm: LoadSealedBuffer failed %r\n", Status));
    goto Exit;
  } 

  Status = AziHsmTpmUnsealBuffer (ObjectHandle, UnsealedBlob);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: UnsealNullHierarchy failed %r\n", Status));
    goto Exit;
  }

  Status = EFI_SUCCESS;

  Exit:
      AziHsmMigTpmCleanup (ObjectHandle);
      AziHsmMigTpmCleanup (Primary);
      return Status;
}
