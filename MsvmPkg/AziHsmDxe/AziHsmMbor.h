/** @file
    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _AZIHSM_MBOR_H_
#define _AZIHSM_MBOR_H_

#include <Uefi.h>

typedef struct _AZIHSM_MBOR_ENCODER {
  UINT8     *Buffer;
  UINT16    Capacity;
  UINT16    Position;
} AZIHSM_MBOR_ENCODER;

typedef struct _AZIHSM_MBOR_DECODER {
  UINT8     *Buffer;
  UINT16    Capacity;
  UINT16    Position;
} AZIHSM_MBOR_DECODER;

/**
  Initializes the MBOR encoder structure with the provided buffer and capacity.

  @param[in, out] Encoder   Pointer to the encoder structure to initialize.
  @param[in]      Buffer    Pointer to the buffer to use for encoding.
  @param[in]      Capacity  Size of the buffer in bytes.

  @retval EFI_SUCCESS           Encoder initialized successfully.
  @retval EFI_INVALID_PARAMETER Encoder, Buffer, or Capacity is invalid.
  @retval EFI_ALREADY_STARTED   Encoder is already initialized.

  Usage: Call before encoding any MBOR data. Must reset before reusing.
**/
EFI_STATUS
AziHsmMborEncoderInit (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN UINT8                    *Buffer,
  IN UINT16                   Capacity
  );

/**
  Resets the MBOR encoder structure, clearing its buffer and state.

  @param[in, out] Encoder   Pointer to the encoder structure to reset.

  @retval EFI_SUCCESS           Encoder reset successfully.
  @retval EFI_INVALID_PARAMETER Encoder is NULL.

  Usage: Call to clear encoder before reinitializing or freeing resources.
**/
EFI_STATUS
AziHsmMborEncoderReset (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder
  );

/**
  Advances the encoder position by the specified length, skipping bytes.

  @param[in, out] Encoder   Pointer to the encoder structure.
  @param[in]      Length    Number of bytes to skip.

  @retval EFI_SUCCESS           Position advanced successfully.
  @retval EFI_INVALID_PARAMETER Encoder is NULL.
  @retval EFI_BUFFER_TOO_SMALL  Skipping would exceed buffer capacity.

  Usage: Use to reserve space or skip unused bytes in the buffer.
**/
EFI_STATUS
AziHsmMborEncoderSkip (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN UINT16                   Length
  );

/**
  Encodes an 8-bit unsigned integer value into the buffer.

  @param[in, out] Encoder   Pointer to the encoder structure.
  @param[in]      Value     8-bit value to encode.

  @retval EFI_SUCCESS           Value encoded successfully.
  @retval EFI_INVALID_PARAMETER Encoder is NULL.
  @retval EFI_BUFFER_TOO_SMALL  Not enough space in buffer.

  Usage: Use to encode MBOR U8 type.
**/
EFI_STATUS
AziHsmMborEncodeU8 (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN UINT8                    Value
  );

/**
  Encodes a 16-bit unsigned integer value into the buffer (big-endian).

  @param[in, out] Encoder   Pointer to the encoder structure.
  @param[in]      Value     16-bit value to encode.

  @retval EFI_SUCCESS           Value encoded successfully.
  @retval EFI_INVALID_PARAMETER Encoder is NULL.
  @retval EFI_BUFFER_TOO_SMALL  Not enough space in buffer.

  Usage: Use to encode MBOR U16 type.
**/
EFI_STATUS
AziHsmMborEncodeU16 (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN UINT16                   Value
  );

/**
  Encodes a 32-bit unsigned integer value into the buffer (big-endian).

  @param[in, out] Encoder   Pointer to the encoder structure.
  @param[in]      Value     32-bit value to encode.

  @retval EFI_SUCCESS           Value encoded successfully.
  @retval EFI_INVALID_PARAMETER Encoder is NULL.
  @retval EFI_BUFFER_TOO_SMALL  Not enough space in buffer.

  Usage: Use to encode MBOR U32 type.
**/
EFI_STATUS
AziHsmMborEncodeU32 (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN UINT32                   Value
  );

/**
  Encodes a 64-bit unsigned integer value into the buffer (big-endian).

  @param[in, out] Encoder   Pointer to the encoder structure.
  @param[in]      Value     64-bit value to encode.

  @retval EFI_SUCCESS           Value encoded successfully.
  @retval EFI_INVALID_PARAMETER Encoder is NULL.
  @retval EFI_BUFFER_TOO_SMALL  Not enough space in buffer.

  Usage: Use to encode MBOR U64 type.
**/
EFI_STATUS
AziHsmMborEncodeU64 (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN UINT64                   Value
  );

/**
  Encodes a boolean value into the buffer as MBOR marker.

  @param[in, out] Encoder   Pointer to the encoder structure.
  @param[in]      Value     Boolean value to encode.

  @retval EFI_SUCCESS           Value encoded successfully.
  @retval EFI_INVALID_PARAMETER Encoder is NULL.
  @retval EFI_BUFFER_TOO_SMALL  Not enough space in buffer.

  Usage: Use to encode MBOR boolean type. TRUE=1, FALSE=0.
**/
EFI_STATUS
AziHsmMborEncodeBoolean (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN BOOLEAN                  Value
  );

/**
  Encodes a map marker with the specified field count.

  @param[in, out] Encoder    Pointer to the encoder structure.
  @param[in]      FieldCount Number of fields in the map (max 15).

  @retval EFI_SUCCESS           Marker encoded successfully.
  @retval EFI_INVALID_PARAMETER Encoder is NULL or FieldCount > 15.
  @retval EFI_BUFFER_TOO_SMALL  Not enough space in buffer.

  Usage: Use to encode MBOR map type.
**/
EFI_STATUS
AziHsmMborEncodeMap (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN UINT8                    FieldCount
  );

/**
  Encodes a byte array into the buffer with MBOR bytes marker and length.

  @param[in, out] Encoder   Pointer to the encoder structure.
  @param[in]      Buffer    Pointer to the byte array to encode.
  @param[in]      Length    Number of bytes to encode.

  @retval EFI_SUCCESS           Bytes encoded successfully.
  @retval EFI_INVALID_PARAMETER Encoder or Buffer is NULL.
  @retval EFI_BUFFER_TOO_SMALL  Not enough space in buffer.

  Usage: Use to encode MBOR bytes type.
**/
EFI_STATUS
AziHsmMborEncodeBytes (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN UINT8                    *Buffer,
  IN UINT16                   Length
  );

/**
  Encodes a padded byte array into the buffer with MBOR marker and length.

  @param[in, out] Encoder       Pointer to the encoder structure.
  @param[in]      Buffer        Pointer to the byte array to encode.
  @param[in]      Length        Number of bytes to encode.
  @param[in]      Padding       Number of padding bytes (max 3).

  @retval EFI_SUCCESS           Bytes encoded successfully.
  @retval EFI_INVALID_PARAMETER Encoder or Buffer is NULL, or Padding > 3.
  @retval EFI_BUFFER_TOO_SMALL  Not enough space in buffer.

  Usage: Use to encode MBOR padded bytes type. Padding bytes are zero.
**/
EFI_STATUS
AziHsmMborEncodePaddedBytes (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN UINT8                    *Buffer,
  IN UINT16                   Length,
  IN UINT8                    Padding
  );

/**
  Initializes the MBOR decoder structure with the provided buffer and capacity.

  @param[in, out] Decoder   Pointer to the decoder structure to initialize.
  @param[in]      Buffer    Pointer to the buffer to use for decoding.
  @param[in]      Capacity  Size of the buffer in bytes.

  @retval EFI_SUCCESS           Decoder initialized successfully.
  @retval EFI_INVALID_PARAMETER Decoder, Buffer, or Capacity is invalid.
  @retval EFI_ALREADY_STARTED   Decoder is already initialized.

  Usage: Call before decoding any MBOR data. Must reset before reusing.
**/
EFI_STATUS
AziHsmMborDecoderInit (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  IN UINT8                    *Buffer,
  IN UINT16                   Capacity
  );

/**
  Resets the MBOR decoder structure, clearing its buffer and state.

  @param[in, out] Decoder   Pointer to the decoder structure to reset.

  @retval EFI_SUCCESS           Decoder reset successfully.
  @retval EFI_INVALID_PARAMETER Decoder is NULL.

  Usage: Call to clear decoder before reinitializing or freeing resources.
**/
EFI_STATUS
AziHsmMborDecoderReset (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder
  );

/**
  Advances the decoder position by the specified length, skipping bytes.

  @param[in, out] Decoder   Pointer to the decoder structure.
  @param[in]      Length    Number of bytes to skip.

  @retval EFI_SUCCESS           Position advanced successfully.
  @retval EFI_INVALID_PARAMETER Decoder is NULL.
  @retval EFI_BUFFER_TOO_SMALL  Skipping would exceed buffer capacity.

  Usage: Use to skip unused bytes in the buffer.
**/
EFI_STATUS
AziHsmMborDecoderSkip (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  IN UINT16                   Length
  );

/**
  Decodes an 8-bit unsigned integer value from the buffer.

  @param[in, out] Decoder   Pointer to the decoder structure.
  @param[out]     Value     Pointer to store the decoded 8-bit value.

  @retval EFI_SUCCESS           Value decoded successfully.
  @retval EFI_INVALID_PARAMETER Decoder or Value is NULL.
  @retval EFI_COMPROMISED_DATA  Marker does not match expected type.
  @retval EFI_BUFFER_TOO_SMALL  Not enough data in buffer.

  Usage: Use to decode MBOR U8 type.
**/
EFI_STATUS
AziHsmMborDecodeU8 (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  OUT UINT8                   *Value
  );

/**
  Decodes a 16-bit unsigned integer value from the buffer (big-endian).

  @param[in, out] Decoder   Pointer to the decoder structure.
  @param[out]     Value     Pointer to store the decoded 16-bit value.

  @retval EFI_SUCCESS           Value decoded successfully.
  @retval EFI_INVALID_PARAMETER Decoder or Value is NULL.
  @retval EFI_COMPROMISED_DATA  Marker does not match expected type.
  @retval EFI_BUFFER_TOO_SMALL  Not enough data in buffer.

  Usage: Use to decode MBOR U16 type.
**/
EFI_STATUS
AziHsmMborDecodeU16 (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  OUT UINT16                  *Value
  );

/**
  Decodes a 32-bit unsigned integer value from the buffer (big-endian).

  @param[in, out] Decoder   Pointer to the decoder structure.
  @param[out]     Value     Pointer to store the decoded 32-bit value.

  @retval EFI_SUCCESS           Value decoded successfully.
  @retval EFI_INVALID_PARAMETER Decoder or Value is NULL.
  @retval EFI_COMPROMISED_DATA  Marker does not match expected type.
  @retval EFI_BUFFER_TOO_SMALL  Not enough data in buffer.

  Usage: Use to decode MBOR U32 type.
**/
EFI_STATUS
AziHsmMborDecodeU32 (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  OUT UINT32                  *Value
  );

/**
  Decodes a 64-bit unsigned integer value from the buffer (big-endian).

  @param[in, out] Decoder   Pointer to the decoder structure.
  @param[out]     Value     Pointer to store the decoded 64-bit value.

  @retval EFI_SUCCESS           Value decoded successfully.
  @retval EFI_INVALID_PARAMETER Decoder or Value is NULL.
  @retval EFI_COMPROMISED_DATA  Marker does not match expected type.
  @retval EFI_BUFFER_TOO_SMALL  Not enough data in buffer.

  Usage: Use to decode MBOR U64 type.
**/
EFI_STATUS
AziHsmMborDecodeU64 (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  OUT UINT64                  *Value
  );

/**
  Decodes a boolean value from the buffer as MBOR marker.

  @param[in, out] Decoder   Pointer to the decoder structure.
  @param[out]     Value     Pointer to store the decoded boolean value.

  @retval EFI_SUCCESS           Value decoded successfully.
  @retval EFI_INVALID_PARAMETER Decoder or Value is NULL.
  @retval EFI_COMPROMISED_DATA  Marker does not match expected type.
  @retval EFI_BUFFER_TOO_SMALL  Not enough data in buffer.

  Usage: Use to decode MBOR boolean type.
**/
EFI_STATUS
AziHsmMborDecodeBoolean (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  OUT BOOLEAN                 *Value
  );

/**
  Decodes a map marker and extracts the field count.

  @param[in, out] Decoder    Pointer to the decoder structure.
  @param[out]     FieldCount Pointer to store the number of fields in the map.

  @retval EFI_SUCCESS           Marker decoded successfully.
  @retval EFI_INVALID_PARAMETER Decoder or FieldCount is NULL.
  @retval EFI_COMPROMISED_DATA  Marker does not match expected type.
  @retval EFI_BUFFER_TOO_SMALL  Not enough data in buffer.

  Usage: Use to decode MBOR map type.
**/
EFI_STATUS
AziHsmMborDecodeMap (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  OUT UINT8                   *FieldCount
  );

/**
  Decodes a byte array from the buffer with MBOR bytes marker and length.

  @param[in, out] Decoder   Pointer to the decoder structure.
  @param[in, out] Buffer    Pointer to the buffer to store decoded bytes.
  @param[in, out] Length    Pointer to store the number of bytes decoded.

  @retval EFI_SUCCESS           Bytes decoded successfully.
  @retval EFI_INVALID_PARAMETER Decoder, Buffer, or Length is NULL.
  @retval EFI_COMPROMISED_DATA  Marker does not match expected type.
  @retval EFI_BUFFER_TOO_SMALL  Not enough data in buffer.

  Usage: Use to decode MBOR bytes type.
**/
EFI_STATUS
AziHsmMborDecodeBytes (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  IN OUT UINT8                *Buffer,
  IN OUT UINT16               *Length
  );

/**
  Decodes a padded byte array from the buffer with MBOR marker and length.

  @param[in, out] Decoder   Pointer to the decoder structure.
  @param[in, out] Buffer    Pointer to the buffer to store decoded bytes.
  @param[in, out] Length    Pointer to store the number of bytes decoded.

  @retval EFI_SUCCESS           Bytes decoded successfully.
  @retval EFI_INVALID_PARAMETER Decoder, Buffer, or Length is NULL.
  @retval EFI_COMPROMISED_DATA  Marker or padding is invalid.
  @retval EFI_BUFFER_TOO_SMALL  Not enough data in buffer.

  Usage: Use to decode MBOR padded bytes type. Padding bytes must be zero.
**/
EFI_STATUS
AziHsmMborDecodePaddedBytes (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  IN OUT UINT8                *Buffer,
  IN OUT UINT16               *Length
  );

/**
  Peeks the next byte in the decoder buffer without advancing position.

  @param[in, out] Decoder   Pointer to the decoder structure.
  @param[out]     Value     Pointer to store the peeked byte value.

  @retval EFI_SUCCESS           Byte peeked successfully.
  @retval EFI_INVALID_PARAMETER Decoder or Value is NULL.
  @retval EFI_BUFFER_TOO_SMALL  Not enough data in buffer.

  Usage: Use to inspect the next byte before decoding.
**/
EFI_STATUS
AziHsmMborPeekByte (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  OUT UINT8                   *Value
  );

#endif // End of _AZIHSM_MBOR_H_
