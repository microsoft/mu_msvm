/** @file
    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "AziHsmMbor.h"
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

const UINT8  MBOR_U8_MARKER          = 0x18 | 0x00;
const UINT8  MBOR_U16_MARKER         = 0x18 | 0x01;
const UINT8  MBOR_U32_MARKER         = 0x18 | 0x02;
const UINT8  MBOR_U64_MARKER         = 0x18 | 0x03;
const UINT8  MBOR_BOOLEAN_MARKER     = 0x14;
const UINT8  MBOR_MAP_MARKER         = 0xA0;
const UINT8  MBOR_MAP_MARKER_MASK    = 0xF0;
const UINT8  MBOR_MAP_FIELD_MASK     = 0x0F;
const UINT8  MBOR_BYTES_MARKER       = 0x80;
const UINT8  MBOR_BYTES_PADDING_MASK = 0x03;


// Define MBOR Boolean values
#define MBOR_BOOLEAN_FALSE 0x14
#define MBOR_BOOLEAN_TRUE  0x15

// Macro to check if advancing position exceeds capacity
#define AZIHSM_MBOR_IS_POSITION_EXCEEDS_CAPACITY(e, len)  ((e->Position) + ((UINT32)len) > (e->Capacity))

/**
  Swaps bytes if the current platform is little endian.

  @param[in] Value  The value to potentially swap bytes for.

  @return The value with bytes swapped if on little endian platform, unchanged otherwise.
**/
STATIC
inline
UINT16
ConvertToBigEndian16 (
  IN UINT16  Value
  )
{
 #if defined (MDE_CPU_IA32) || defined (MDE_CPU_X64) || defined (MDE_CPU_ARM) || defined (MDE_CPU_AARCH64) || \
  defined (MDE_CPU_RISCV64) || defined (MDE_CPU_LOONGARCH64)
  // These architectures are little-endian, so we need to swap bytes
  return SwapBytes16 (Value);
 #elif defined (MDE_CPU_EBC)
  // EBC is endian-neutral, assume no swap needed for big-endian output
  return Value;
 #else
  #error "Unknown architecture - please define endianness for this platform"
 #endif
}

/**
  Swaps bytes if the current platform is little endian.

  @param[in] Value  The value to potentially swap bytes for.

  @return The value with bytes swapped if on little endian platform, unchanged otherwise.
**/
STATIC
inline
UINT32
ConvertToBigEndian32 (
  IN UINT32  Value
  )
{
 #if defined (MDE_CPU_IA32) || defined (MDE_CPU_X64) || defined (MDE_CPU_ARM) || defined (MDE_CPU_AARCH64) || \
  defined (MDE_CPU_RISCV64) || defined (MDE_CPU_LOONGARCH64)
  // These architectures are little-endian, so we need to swap bytes
  return SwapBytes32 (Value);
 #elif defined (MDE_CPU_EBC)
  // EBC is endian-neutral, assume no swap needed for big-endian output
  return Value;
 #else
  #error "Unknown architecture - please define endianness for this platform"
 #endif
}

/**
  Swaps bytes if the current platform is little endian.

  @param[in] Value  The value to potentially swap bytes for.

  @return The value with bytes swapped if on little endian platform, unchanged otherwise.
**/
STATIC
inline
UINT64
ConvertToBigEndian64 (
  IN UINT64  Value
  )
{
 #if defined (MDE_CPU_IA32) || defined (MDE_CPU_X64) || defined (MDE_CPU_ARM) || defined (MDE_CPU_AARCH64) || \
  defined (MDE_CPU_RISCV64) || defined (MDE_CPU_LOONGARCH64)
  // These architectures are little-endian, so we need to swap bytes
  return SwapBytes64 (Value);
 #elif defined (MDE_CPU_EBC)
  // EBC is endian-neutral, assume no swap needed for big-endian output
  return Value;
 #else
  #error "Unknown architecture - please define endianness for this platform"
 #endif
}

/**
  Swaps bytes if the current platform is little endian.

  @param[in] Value  The big-endian value to convert to host endianness.

  @return The value converted to host endianness.
**/
STATIC
inline
UINT16
ConvertToLittleEndian16 (
  IN UINT16  Value
  )
{
 #if defined (MDE_CPU_IA32) || defined (MDE_CPU_X64) || defined (MDE_CPU_ARM) || defined (MDE_CPU_AARCH64) || \
  defined (MDE_CPU_RISCV64) || defined (MDE_CPU_LOONGARCH64)
  // These architectures are little-endian, so we need to swap bytes from big-endian
  return SwapBytes16 (Value);
 #elif defined (MDE_CPU_EBC)
  // EBC is endian-neutral, assume big-endian input needs no conversion
  return Value;
 #else
  #error "Unknown architecture - please define endianness for this platform"
 #endif
}

/**
  Swaps bytes if the current platform is little endian.

  @param[in] Value  The big-endian value to convert to host endianness.

  @return The value converted to host endianness.
**/
STATIC
inline
UINT32
ConvertToLittleEndian32 (
  IN UINT32  Value
  )
{
 #if defined (MDE_CPU_IA32) || defined (MDE_CPU_X64) || defined (MDE_CPU_ARM) || defined (MDE_CPU_AARCH64) || \
  defined (MDE_CPU_RISCV64) || defined (MDE_CPU_LOONGARCH64)
  // These architectures are little-endian, so we need to swap bytes from big-endian
  return SwapBytes32 (Value);
 #elif defined (MDE_CPU_EBC)
  // EBC is endian-neutral, assume big-endian input needs no conversion
  return Value;
 #else
  #error "Unknown architecture - please define endianness for this platform"
 #endif
}

/**
  Swaps bytes if the current platform is little endian.

  @param[in] Value  The big-endian value to convert to host endianness.

  @return The value converted to host endianness.
**/
STATIC
inline
UINT64
ConvertToLittleEndian64 (
  IN UINT64  Value
  )
{
 #if defined (MDE_CPU_IA32) || defined (MDE_CPU_X64) || defined (MDE_CPU_ARM) || defined (MDE_CPU_AARCH64) || \
  defined (MDE_CPU_RISCV64) || defined (MDE_CPU_LOONGARCH64)
  // These architectures are little-endian, so we need to swap bytes from big-endian
  return SwapBytes64 (Value);
 #elif defined (MDE_CPU_EBC)
  // EBC is endian-neutral, assume big-endian input needs no conversion
  return Value;
 #else
  #error "Unknown architecture - please define endianness for this platform"
 #endif
}

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
  )
{
  if ((Encoder == NULL) || (Buffer == NULL) || (Capacity == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  // should return error if Encoder is already used
  if (Encoder != NULL) {
    // Check buffer only for now, Capacity and Position can be added if needed.
    if ((Encoder->Buffer != NULL)) {
      // Encoder is not Reset properly, return error, user might be making a mistake
      return EFI_ALREADY_STARTED;
    }
  }

  Encoder->Buffer   = Buffer;
  Encoder->Capacity = Capacity;
  Encoder->Position = 0;

  return EFI_SUCCESS;
}

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
  )
{
  if (Encoder == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Encoder->Buffer   = NULL;
  Encoder->Capacity = 0;
  Encoder->Position = 0;

  return EFI_SUCCESS;
}

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
  )
{
  if (Encoder == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Doesn't fail if Length is U16_MAX and Position > 0 , we should check if ((U32)Encoder->Position + Length)
  if (AZIHSM_MBOR_IS_POSITION_EXCEEDS_CAPACITY (Encoder, Length)) {
    return EFI_BUFFER_TOO_SMALL;
  }

  Encoder->Position += Length;

  return EFI_SUCCESS;
}

/**
  Encodes raw bytes into the encoder buffer at the current position.

  @param[in, out] Encoder   Pointer to the encoder structure.
  @param[in]      Buffer    Pointer to the data to encode.
  @param[in]      Length    Number of bytes to encode.

  @retval EFI_SUCCESS           Data encoded successfully.
  @retval EFI_INVALID_PARAMETER Encoder or Buffer is NULL.
  @retval EFI_BUFFER_TOO_SMALL  Not enough space in buffer.

  Usage: Use to write arbitrary data to the MBOR buffer.
**/
STATIC
EFI_STATUS
AziHsmMborEncoderEncode (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN UINT8                    *Buffer,
  IN UINT16                   Length
  )
{
  // should check if Length == 0 too.
  if ((Encoder == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  // same as before comment, I think we should create a macro to check capacity
  if (AZIHSM_MBOR_IS_POSITION_EXCEEDS_CAPACITY (Encoder, Length)) {
    return EFI_BUFFER_TOO_SMALL;
  }

  CopyMem (Encoder->Buffer + Encoder->Position, Buffer, Length);

  Encoder->Position += Length;

  return EFI_SUCCESS;
}

// we should make this function as static, no need to access this function from outside

/**
  Encodes a marker byte into the encoder buffer.

  @param[in, out] Encoder   Pointer to the encoder structure.
  @param[in]      Marker    Marker byte to encode.

  @retval EFI_SUCCESS           Marker encoded successfully.
  @retval EFI_INVALID_PARAMETER Encoder or its buffer is NULL.
  @retval EFI_BUFFER_TOO_SMALL  Not enough space in buffer.

  Usage: Internal use for encoding MBOR type markers.
**/
EFI_STATUS
AziHsmMborEncodeMarker (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN UINT8                    Marker
  )
{
  // Function to encode, marker, note this function will just copy the marker do not add anything else.

  // should check if Length == 0 too.
  if (Encoder == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Encoder->Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // same as before comment, I think we should create a macro to check capacity
  if (AZIHSM_MBOR_IS_POSITION_EXCEEDS_CAPACITY (Encoder, sizeof (Marker))) {
    return EFI_BUFFER_TOO_SMALL;
  }

  Encoder->Buffer[Encoder->Position] = Marker;

  Encoder->Position += sizeof (Marker);

  return EFI_SUCCESS;
}

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
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT8       Marker = MBOR_U8_MARKER;

  if (Encoder == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Doeosn't seems to be correct, calling recursive function.
  Status = AziHsmMborEncodeMarker (Encoder, Marker);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = AziHsmMborEncoderEncode (Encoder, &Value, sizeof (Value));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

Exit:

  return Status;
}

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
  )
{
  EFI_STATUS  Status       = EFI_SUCCESS;
  UINT8       Marker       = MBOR_U16_MARKER;
  UINT16      EncodedValue = ConvertToBigEndian16 (Value);

  if (Encoder == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  /// Add marker
  Status = AziHsmMborEncodeMarker (Encoder, Marker);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = AziHsmMborEncoderEncode (Encoder, (UINT8 *)&EncodedValue, sizeof (EncodedValue));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

Exit:

  return Status;
}

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
  )
{
  EFI_STATUS  Status       = EFI_SUCCESS;
  UINT8       Marker       = MBOR_U32_MARKER;
  UINT32      EncodedValue = ConvertToBigEndian32 (Value);

  if (Encoder == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Encode Marker
  Status = AziHsmMborEncodeMarker (Encoder, Marker);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = AziHsmMborEncoderEncode (Encoder, (UINT8 *)&EncodedValue, sizeof (EncodedValue));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

Exit:

  return Status;
}

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
  )
{
  EFI_STATUS  Status       = EFI_SUCCESS;
  UINT8       Marker       = MBOR_U64_MARKER;
  UINT64      EncodedValue = ConvertToBigEndian64 (Value);

  if (Encoder == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AziHsmMborEncodeMarker (Encoder, Marker);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = AziHsmMborEncoderEncode (Encoder, (UINT8 *)&EncodedValue, sizeof (EncodedValue));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

Exit:

  return Status;
}

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
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  // This will not work if user passes > 1 value for Boolean, which might be valid . We should enforce TRUE = 1, FALSE = 0.
  UINT8  Marker = MBOR_BOOLEAN_MARKER;

  if (Encoder == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Marker = (Value == FALSE) ? MBOR_BOOLEAN_FALSE : MBOR_BOOLEAN_TRUE;

  // Boolean is 1 byte marker itself
  Status = AziHsmMborEncodeMarker (Encoder, Marker);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

Exit:

  return Status;
}

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
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT8       Marker = MBOR_MAP_MARKER | (FieldCount & MBOR_MAP_FIELD_MASK); // Map marker with field count

  // return Error if FieldCount can't be fit in 4 bits
  if (FieldCount > MBOR_MAP_FIELD_MASK) {
    return EFI_INVALID_PARAMETER;
  }

  if (Encoder == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AziHsmMborEncodeMarker (Encoder, Marker);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

Exit:

  return Status;
}

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
  )
{
  EFI_STATUS  Status        = EFI_SUCCESS;
  UINT8       Marker        = MBOR_BYTES_MARKER;
  UINT16      EncodedLength = ConvertToBigEndian16 (Length);

  if ((Encoder == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AziHsmMborEncodeMarker (Encoder, Marker);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  // Encode length without any marker
  Status = AziHsmMborEncoderEncode (Encoder, (UINT8 *)&EncodedLength, sizeof (EncodedLength));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = AziHsmMborEncoderEncode (Encoder, Buffer, Length);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

Exit:

  return Status;
}

/**
  Encodes a padded byte array into the buffer with MBOR marker and length.

  @param[in, out] Encoder       Pointer to the encoder structure.
  @param[in]      Buffer        Pointer to the byte array to encode.
  @param[in]      Length        Number of bytes to encode.
  @param[in]      PaddingLength Number of padding bytes (max 3).

  @retval EFI_SUCCESS           Bytes encoded successfully.
  @retval EFI_INVALID_PARAMETER Encoder or Buffer is NULL, or PaddingLength > 3.
  @retval EFI_BUFFER_TOO_SMALL  Not enough space in buffer.

  Usage: Use to encode MBOR padded bytes type. Padding bytes are zero.
**/
EFI_STATUS
AziHsmMborEncodePaddedBytes (
  IN OUT AZIHSM_MBOR_ENCODER  *Encoder,
  IN UINT8                    *Buffer,
  IN UINT16                   Length,
  IN UINT8                    PaddingLength
  )
{
  EFI_STATUS  Status        = EFI_SUCCESS;
  UINT8       Marker        = MBOR_BYTES_MARKER | (PaddingLength & MBOR_BYTES_PADDING_MASK);
  UINT8       PadByte       = 0;
  UINT16      EncodedLength = ConvertToBigEndian16 (Length);

  if ((Encoder == NULL) || (Buffer == NULL) || (PaddingLength > MBOR_BYTES_PADDING_MASK)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AziHsmMborEncodeMarker (Encoder, Marker);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = AziHsmMborEncoderEncode (Encoder, (UINT8 *)&EncodedLength, sizeof (EncodedLength));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  for (UINT8 i = 0; i < PaddingLength; i++) {
    // can be optimized by copying directly or by creating a 4 byte array
    Status = AziHsmMborEncoderEncode (Encoder, (UINT8 *)&PadByte, sizeof (PadByte));
    if (EFI_ERROR (Status)) {
      goto Exit;
    }
  }

  ASSERT (Encoder->Position % 4 == 0);

  Status = AziHsmMborEncoderEncode (Encoder, Buffer, Length);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

Exit:

  return Status;
}

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
  )
{
  if ((Decoder == NULL) || (Buffer == NULL) || (Capacity == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  // Decoder is already initialized, should return error if Decoder is already in use
  if (Decoder != NULL) {
    // Check buffer only for now, Capacity and Position can be added if needed.
    if ((Decoder->Buffer != NULL)) {
      // Encoder is not Reset properly, return error, user might be making a mistake
      return EFI_ALREADY_STARTED;
    }
  }

  Decoder->Buffer   = Buffer;
  Decoder->Capacity = Capacity;
  Decoder->Position = 0;

  return EFI_SUCCESS;
}

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
  )
{
  if (Decoder == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Decoder->Buffer   = NULL;
  Decoder->Capacity = 0;
  Decoder->Position = 0;

  return EFI_SUCCESS;
}

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
  )
{
  if (Decoder == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // check if Buffer is properly initialized
  if (Decoder->Buffer == NULL) {
    return EFI_NOT_READY;
  }

  if (AZIHSM_MBOR_IS_POSITION_EXCEEDS_CAPACITY (Decoder, Length)) {
    return EFI_BUFFER_TOO_SMALL;
  }

  Decoder->Position += Length;

  return EFI_SUCCESS;
}

/**
  Decodes raw bytes from the decoder buffer at the current position.

  @param[in, out] Decoder   Pointer to the decoder structure.
  @param[out]     Buffer    Pointer to the buffer to store decoded data.
  @param[in]      Length    Number of bytes to decode.

  @retval EFI_SUCCESS           Data decoded successfully.
  @retval EFI_INVALID_PARAMETER Decoder or Buffer is NULL.
  @retval EFI_BUFFER_TOO_SMALL  Not enough data in buffer.

  Usage: Use to read arbitrary data from the MBOR buffer.
**/
STATIC
EFI_STATUS
AziHsmMborDecoderDecode (
  IN OUT AZIHSM_MBOR_DECODER  *Decoder,
  OUT UINT8                   *Buffer,
  IN UINT16                   Length
  )
{
  if ((Decoder == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  // check if Buffer is properly initialized
  if (Decoder->Buffer == NULL) {
    return EFI_NOT_READY;
  }

  if (AZIHSM_MBOR_IS_POSITION_EXCEEDS_CAPACITY (Decoder, Length)) {
    return EFI_BUFFER_TOO_SMALL;
  }

  CopyMem (Buffer, Decoder->Buffer + Decoder->Position, Length);
  Decoder->Position += Length;

  return EFI_SUCCESS;
}

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
  )
{
  EFI_STATUS  Status       = EFI_SUCCESS;
  UINT8       Marker       = 0;
  UINT8       DecodedValue = 0;

  if ((Decoder == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AziHsmMborDecoderDecode (Decoder, &Marker, sizeof (Marker));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  if (Marker != MBOR_U8_MARKER) {
    Status = EFI_COMPROMISED_DATA;
    goto Exit;
  }

  Status = AziHsmMborDecoderDecode (Decoder, (UINT8 *)&DecodedValue, sizeof (DecodedValue));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  *Value = DecodedValue;

Exit:

  return Status;
}

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
  )
{
  EFI_STATUS  Status       = EFI_SUCCESS;
  UINT8       Marker       = 0;
  UINT16      DecodedValue = 0;

  if ((Decoder == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AziHsmMborDecoderDecode (Decoder, &Marker, sizeof (Marker));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  if (Marker != MBOR_U16_MARKER) {
    Status = EFI_COMPROMISED_DATA;
    goto Exit;
  }

  Status = AziHsmMborDecoderDecode (Decoder, (UINT8 *)&DecodedValue, sizeof (DecodedValue));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  *Value = ConvertToLittleEndian16 (DecodedValue);

Exit:

  return Status;
}

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
  )
{
  EFI_STATUS  Status       = EFI_SUCCESS;
  UINT8       Marker       = 0;
  UINT32      DecodedValue = 0;

  if ((Decoder == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AziHsmMborDecoderDecode (Decoder, &Marker, sizeof (Marker));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  if (Marker != MBOR_U32_MARKER) {
    Status = EFI_COMPROMISED_DATA;
    goto Exit;
  }

  Status = AziHsmMborDecoderDecode (Decoder, (UINT8 *)&DecodedValue, sizeof (DecodedValue));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  *Value = ConvertToLittleEndian32 (DecodedValue);

Exit:

  return Status;
}

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
  )
{
  EFI_STATUS  Status       = EFI_SUCCESS;
  UINT8       Marker       = 0;
  UINT64      DecodedValue = 0;

  if ((Decoder == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AziHsmMborDecoderDecode (Decoder, &Marker, sizeof (Marker));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  if (Marker != MBOR_U64_MARKER) {
    Status = EFI_COMPROMISED_DATA;
    goto Exit;
  }

  Status = AziHsmMborDecoderDecode (Decoder, (UINT8 *)&DecodedValue, sizeof (DecodedValue));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  *Value = ConvertToLittleEndian64 (DecodedValue);

Exit:

  return Status;
}

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
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT8       Marker = 0;

  if ((Decoder == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AziHsmMborDecoderDecode (Decoder, &Marker, sizeof (Marker));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  *Value = (Marker == MBOR_BOOLEAN_TRUE) ? TRUE : FALSE;

Exit:

  return Status;
}

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
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT8       Marker = 0;

  if ((Decoder == NULL) || (FieldCount == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AziHsmMborDecoderDecode (Decoder, &Marker, sizeof (Marker));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  // Should be checking with the MBOR_MAP_MARKER_MASK (upper nibble ), otherwise this code will
  // work for both 0xA3 and 0xE3 , which is wrong
  if ((Marker & MBOR_MAP_MARKER_MASK) != MBOR_MAP_MARKER) {
    return EFI_COMPROMISED_DATA;
  }

  *FieldCount = Marker & MBOR_MAP_FIELD_MASK;

Exit:

  return Status;
}

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
  )
{
  EFI_STATUS  Status          = EFI_SUCCESS;
  UINT8       Marker          = 0;
  UINT16      LengthBigEndian = 0;

  if ((Decoder == NULL) || (Buffer == NULL) || (Length == NULL)) {
    DEBUG ((DEBUG_ERROR, "AziHsmMborDecodeBytes: Invalid parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  Status = AziHsmMborDecoderDecode (Decoder, &Marker, sizeof (Marker));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsmMborDecodeBytes: Failed to decode marker 0x%02X: %r\n", Marker, Status));
    DEBUG ((DEBUG_ERROR, "Position: %u, Capacity: %u\n", Decoder->Position, Decoder->Capacity));
    goto Exit;
  }

  if (Marker != MBOR_BYTES_MARKER) {
    Status = EFI_COMPROMISED_DATA;
    goto Exit;
  }

  Status = AziHsmMborDecoderDecode (Decoder, (UINT8 *)&LengthBigEndian, sizeof (LengthBigEndian));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  *Length = ConvertToLittleEndian16 (LengthBigEndian);

  if (AZIHSM_MBOR_IS_POSITION_EXCEEDS_CAPACITY (Decoder, *Length)) {
    Status = EFI_BUFFER_TOO_SMALL;
    goto Exit;
  }

  Status = AziHsmMborDecoderDecode (Decoder, Buffer, *Length);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  // Note: Decoder->Position is already advanced by AziHsmMborDecoderDecode, no need to advance again

Exit:

  return Status;
}

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
  )
{
  EFI_STATUS  Status              = EFI_SUCCESS;
  UINT8       Marker              = 0;
  UINT16      LengthBigEndian     = 0;
  UINT8       PaddedByte          = 0;
  UINT8       NumberOfPaddedBytes = 0;

  if ((Decoder == NULL) || (Buffer == NULL) || (Length == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AziHsmMborDecoderDecode (Decoder, &Marker, sizeof (Marker));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsmMborDecodeBytes: Failed to decode marker 0x%02X: %r\n", Marker, Status));
    DEBUG ((DEBUG_ERROR, "Position: %u, Capacity: %u\n", Decoder->Position, Decoder->Capacity));
    goto Exit;
  }

  if ((Marker & ((UINT8) ~MBOR_BYTES_PADDING_MASK)) != MBOR_BYTES_MARKER) {
    DEBUG ((DEBUG_ERROR, "AziHsmMborDecodeBytes: Failed to decode marker 0x%02X: %r\n", Marker, Status));
    DEBUG ((DEBUG_ERROR, "Position: %u, Capacity: %u\n", Decoder->Position, Decoder->Capacity));
    Status = EFI_COMPROMISED_DATA;
    goto Exit;
  }

  // Extract padded bytes
  NumberOfPaddedBytes = (Marker & MBOR_BYTES_PADDING_MASK);
  // extract length
  Status = AziHsmMborDecoderDecode (Decoder, (UINT8 *)&LengthBigEndian, sizeof (LengthBigEndian));
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  *Length = ConvertToLittleEndian16 (LengthBigEndian);
  if (AZIHSM_MBOR_IS_POSITION_EXCEEDS_CAPACITY (Decoder, (*Length + NumberOfPaddedBytes))) {
    Status = EFI_BUFFER_TOO_SMALL;
    goto Exit;
  }

  // start reading padded bytes
  while (NumberOfPaddedBytes > 0) {
    Status = AziHsmMborDecoderDecode (Decoder, &PaddedByte, sizeof (PaddedByte));
    if (EFI_ERROR (Status)) {
      goto Exit;
    }

    if (PaddedByte != 0) {
      DEBUG ((DEBUG_ERROR, "AziHsmMborDecodeBytes: not expected padded byte 0x%02X: %r\n", PaddedByte, Status));
      DEBUG ((DEBUG_ERROR, "Position: %u, Capacity: %u\n", Decoder->Position, Decoder->Capacity));
      // Invalid padding
      Status = EFI_COMPROMISED_DATA;
      goto Exit;
    }

    NumberOfPaddedBytes--;
  }

  Status = AziHsmMborDecoderDecode (Decoder, Buffer, *Length);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  // Note: Decoder->Position is already advanced by AziHsmMborDecoderDecode, no need to advance again

Exit:

  return Status;
}

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
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  if ((Decoder == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (AZIHSM_MBOR_IS_POSITION_EXCEEDS_CAPACITY (Decoder, sizeof (UINT8))) {
    return EFI_BUFFER_TOO_SMALL;
  }

  // Peek the value without changing the position
  *Value = Decoder->Buffer[Decoder->Position];

  return Status;
}
