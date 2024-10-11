/** @file

    Necessary definitions from Storport.h

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <MsvmBase.h>

#define SCSI_MAXIMUM_LUNS_PER_TARGET 255

//
// Command Descriptor Block constants.
//
#define CDB6GENERIC_LENGTH                   6
#define CDB10GENERIC_LENGTH                  10
#define CDB12GENERIC_LENGTH                  12

#define SRB_STATUS_PENDING 0
#define SRB_STATUS_SUCCESS 1

typedef struct _SENSE_DATA
{
    UINT8 ErrorCode:7;
    UINT8 Valid:1;
    UINT8 SegmentNumber;
    UINT8 SenseKey:4;
    UINT8 Reserved:1;
    UINT8 IncorrectLength:1;
    UINT8 EndOfMedia:1;
    UINT8 FileMark:1;
    UINT8 Information[4];
    UINT8 AdditionalSenseLength;
    UINT8 CommandSpecificInformation[4];
    UINT8 AdditionalSenseCode;
    UINT8 AdditionalSenseCodeQualifier;
    UINT8 FieldReplaceableUnitCode;
    UINT8 SenseKeySpecific[3];
} SENSE_DATA, *PSENSE_DATA;

//
// Fixed Sense Data Format
//
typedef struct _SENSE_DATA FIXED_SENSE_DATA, *PFIXED_SENSE_DATA;

//
// Descriptor Sense Data Format
//
typedef struct _DESCRIPTOR_SENSE_DATA
{
    UINT8 ErrorCode:7;
    UINT8 Reserved1:1;
    UINT8 SenseKey:4;
    UINT8 Reserved2:4;
    UINT8 AdditionalSenseCode;
    UINT8 AdditionalSenseCodeQualifier;
    UINT8 Reserved3[3];
    UINT8 AdditionalSenseLength;
    UINT8 DescriptorBuffer[];
} DESCRIPTOR_SENSE_DATA, *PDESCRIPTOR_SENSE_DATA;

//
// Sense Data Error Codes
//
#define SCSI_SENSE_ERRORCODE_FIXED_CURRENT        0x70
#define SCSI_SENSE_ERRORCODE_FIXED_DEFERRED       0x71
#define SCSI_SENSE_ERRORCODE_DESCRIPTOR_CURRENT   0x72
#define SCSI_SENSE_ERRORCODE_DESCRIPTOR_DEFERRED  0x73

typedef struct _LUN_LIST
{
    UINT8 LunListLength[4]; // sizeof LunSize * 8
    UINT8 Reserved[4];
    UINT8 Lun[1][8];        // 4 level of addressing.  2 bytes each.
} LUN_LIST, *PLUN_LIST;

//
// Maximum request sense buffer size
//
#define MAX_SENSE_BUFFER_SIZE 255


//
// Obtain Error Code from the sense info buffer.
// Note: Error Code is same as "Response Code" defined in SPC Specification.
//
#define ScsiGetSenseErrorCode(SenseInfoBuffer) (((UINT8*)(SenseInfoBuffer))[0] & 0x7f)


//
// Determine if sense data is in Fixed format
//
#define IsFixedSenseDataFormat(SenseInfoBuffer) \
            ((ScsiGetSenseErrorCode(SenseInfoBuffer)) == SCSI_SENSE_ERRORCODE_FIXED_CURRENT || \
             (ScsiGetSenseErrorCode(SenseInfoBuffer)) == SCSI_SENSE_ERRORCODE_FIXED_DEFERRED)

//
// Determine if sense data is in Descriptor format
//
#define IsDescriptorSenseDataFormat(SenseInfoBuffer) \
            ((ScsiGetSenseErrorCode(SenseInfoBuffer)) == SCSI_SENSE_ERRORCODE_DESCRIPTOR_CURRENT || \
             (ScsiGetSenseErrorCode(SenseInfoBuffer)) == SCSI_SENSE_ERRORCODE_DESCRIPTOR_DEFERRED)

//
// Determine if sense data is Current error type
//
#define IsSenseDataCurrentError(SenseInfoBuffer) \
            ((ScsiGetSenseErrorCode(SenseInfoBuffer)) == SCSI_SENSE_ERRORCODE_FIXED_CURRENT || \
             (ScsiGetSenseErrorCode(SenseInfoBuffer)) == SCSI_SENSE_ERRORCODE_DESCRIPTOR_CURRENT)


__forceinline
 BOOLEAN
ScsiGetTotalSenseByteCountIndicated (
   IN  VOID  *SenseInfoBuffer,
   IN  UINT8 SenseInfoBufferLength,
   OUT UINT8 *TotalByteCountIndicated
   )
/*++

Description:

    This function returns size of available sense data. This is based on
    AdditionalSenseLength field in the sense data payload as indicated
    by the device.

    This function handles both Fixed and Desciptor format.

Arguments:

    SenseInfoBuffer - A pointer to sense info buffer

    SenseInfoBufferLength - Size of the buffer SenseInfoBuffer points to.

    TotalByteCountIndicated - On output, it contains total byte counts of available sense data

Returns:

    TRUE if the function is able to determine size of available sense data.

    Otherwise, FALSE.

    Note: The routine returns FALSE when available sense data amount is
          greater than MAX_SENSE_BUFFER_SIZE.

--*/
{
    BOOLEAN succeed = FALSE;
    UINT8 byteCount = 0;
    PFIXED_SENSE_DATA senseInfoBuffer = NULL;

    if (SenseInfoBuffer == NULL ||
        SenseInfoBufferLength == 0 ||
        TotalByteCountIndicated == NULL)
    {
        return FALSE;
    }

    //
    // Offset to AdditionalSenseLength field is same between
    // Fixed and Descriptor format.
    //
    senseInfoBuffer = (PFIXED_SENSE_DATA)SenseInfoBuffer;

    if (CONTAINS_FIELD(senseInfoBuffer,
                           SenseInfoBufferLength,
                           AdditionalSenseLength))
    {
        if (senseInfoBuffer->AdditionalSenseLength <=
            (MAX_SENSE_BUFFER_SIZE - SIZEOF_THROUGH_FIELD(FIXED_SENSE_DATA, AdditionalSenseLength)))
        {
            byteCount = senseInfoBuffer->AdditionalSenseLength
                        + SIZEOF_THROUGH_FIELD(FIXED_SENSE_DATA, AdditionalSenseLength);

            *TotalByteCountIndicated = byteCount;

            succeed = TRUE;
        }
    }

    return succeed;
}


__forceinline
BOOLEAN
ScsiGetFixedSenseKeyAndCodes (
   IN  VOID     *SenseInfoBuffer,
   IN  UINT8    SenseInfoBufferLength,
   OUT UINT8    *SenseKey OPTIONAL,
   OUT UINT8    *AdditionalSenseCode OPTIONAL,
   OUT UINT8    *AdditionalSenseCodeQualifier OPTIONAL
   )
/*++

Description:

    This function retrieves the following information from sense data
    in Fixed format:

        1. Sense key
        2. Additional Sense Code
        3. Additional Sense Code Qualifier

    If Additional Sense Code or Additional Sense Code Qualifer is not available,
    it is set to 0 when the function returns.

Arguments:

    SenseInfoBuffer - A pointer to sense info buffer

    SenseInfoBufferLength - Size of the buffer SenseInfoBuffer points to.

    SenseKey - On output, buffer contains the sense key.
        If null is specified, the function will not retrieve the sense key

    AdditionalSenseCode - On output, buffer contains the additional sense code.
        If null is specified, the function will not retrieve the additional sense code.

    AdditionalSenseCodeQualifier - On output, buffer contains the additional sense code qualifier.
        If null is specified, the function will not retrieve the additional sense code qualifier.

Returns:

    TRUE if the function is able to retrieve the requested information.

    Otherwise, FALSE

--*/
{
    PFIXED_SENSE_DATA fixedSenseData = (PFIXED_SENSE_DATA)SenseInfoBuffer;
    BOOLEAN succeed = FALSE;
    UINT32 dataLength = 0;

    if (SenseInfoBuffer == NULL || SenseInfoBufferLength == 0)
    {
        return FALSE;
    }

    if (CONTAINS_FIELD(fixedSenseData, SenseInfoBufferLength, AdditionalSenseLength))
    {
        dataLength = fixedSenseData->AdditionalSenseLength + SIZEOF_THROUGH_FIELD(FIXED_SENSE_DATA, AdditionalSenseLength);

        if (dataLength > SenseInfoBufferLength)
        {
            dataLength = SenseInfoBufferLength;
        }

        if (SenseKey != NULL)
        {
           *SenseKey = fixedSenseData->SenseKey;
        }

        if (AdditionalSenseCode != NULL)
        {
           *AdditionalSenseCode = CONTAINS_FIELD(fixedSenseData, dataLength, AdditionalSenseCode) ?
                                  fixedSenseData->AdditionalSenseCode : 0;
        }

        if (AdditionalSenseCodeQualifier != NULL)
        {
           *AdditionalSenseCodeQualifier = CONTAINS_FIELD(fixedSenseData, dataLength, AdditionalSenseCodeQualifier) ?
                                           fixedSenseData->AdditionalSenseCodeQualifier : 0;
        }

        succeed = TRUE;
    }

    return succeed;
}


__forceinline
BOOLEAN
ScsiGetDescriptorSenseKeyAndCodes (
   IN  VOID     *SenseInfoBuffer,
   IN  UINT8    SenseInfoBufferLength,
   OUT UINT8    *SenseKey OPTIONAL,
   OUT UINT8    *AdditionalSenseCode OPTIONAL,
   OUT UINT8    *AdditionalSenseCodeQualifier OPTIONAL
   )
/*++

Description:

    This function retrieves the following information from sense data
    in Descriptor format:

        1. Sense key
        2. Additional Sense Code
        3. Additional Sense Code Qualifier

Arguments:

    SenseInfoBuffer - A pointer to sense info buffer

    SenseInfoBufferLength - Size of the buffer SenseInfoBuffer points to.

    SenseKey - On output, buffer contains the sense key.
        Note: If null is specified, the function will not retrieve the sense key

    AdditionalSenseCode - On output, buffer contains the additional sense code.
        Note: If null is specified, the function will not retrieve the additional sense code.

    AdditionalSenseCodeQualifier - On output, buffer contains the additional sense code qualifier.
        Note: If null is specified, the function will not retrieve the additional sense code qualifier.

Returns:

    TRUE if the function is able to retrieve the requested information.

    Otherwise, FALSE

--*/
{
    PDESCRIPTOR_SENSE_DATA descriptorSenseData = (PDESCRIPTOR_SENSE_DATA)SenseInfoBuffer;
    BOOLEAN succeed = FALSE;

    if (SenseInfoBuffer == NULL || SenseInfoBufferLength == 0)
    {
        return FALSE;
    }

    if (CONTAINS_FIELD(descriptorSenseData, SenseInfoBufferLength, AdditionalSenseLength))
    {
        if (SenseKey)
        {
            *SenseKey = descriptorSenseData->SenseKey;
        }

        if (AdditionalSenseCode != NULL)
        {
            *AdditionalSenseCode = descriptorSenseData->AdditionalSenseCode;
        }

        if (AdditionalSenseCodeQualifier != NULL)
        {
            *AdditionalSenseCodeQualifier = descriptorSenseData->AdditionalSenseCodeQualifier;
        }

        succeed = TRUE;
    }

    return succeed;
}

//
// SCSI_SENSE_OPTIONS
//
typedef UINT32 SCSI_SENSE_OPTIONS;

//
// No options are specified
//
#define SCSI_SENSE_OPTIONS_NONE                                      ((SCSI_SENSE_OPTIONS)0x00000000)

//
// If no known format is indicated in the sense buffer, interpret
// the sense buffer as Fixed format.
//
#define SCSI_SENSE_OPTIONS_FIXED_FORMAT_IF_UNKNOWN_FORMAT_INDICATED  ((SCSI_SENSE_OPTIONS)0x00000001)


__forceinline
BOOLEAN
ScsiGetSenseKeyAndCodes (
   IN  VOID  *SenseInfoBuffer,
   IN  UINT8 SenseInfoBufferLength,
   IN  SCSI_SENSE_OPTIONS Options,
   OUT UINT8 *SenseKey OPTIONAL,
   OUT UINT8 *AdditionalSenseCode OPTIONAL,
   OUT UINT8 *AdditionalSenseCodeQualifier OPTIONAL
   )
/*++

Description:

    This function retrieves the following information from sense data

        1. Sense key
        2. Additional Sense Code
        3. Additional Sense Code Qualifier

    This function handles both Fixed and Descriptor format.

Arguments:

    SenseInfoBuffer - A pointer to sense info buffer

    SenseInfoBufferLength - Size of the buffer SenseInfoBuffer points to.

    Options - Options used by this routine. It is a bit-field value. See defintions
        of list of #define SCSI_SENSE_OPTIONS above in this file.

    SenseKey - On output, buffer contains the sense key.
        Note: If null is specified, the function will not retrieve the sense key

    AdditionalSenseCode - On output, buffer contains the additional sense code.
        Note: If null is specified, the function will not retrieve the additional sense code.

    AdditionalSenseCodeQualifier - On output, buffer contains the additional sense code qualifier.
        Note: If null is specified, the function will not retrieve the additional sense code qualifier.

Returns:

    TRUE if the function is able to retrieve the requested information.

    Otherwise, FALSE

--*/
{
    BOOLEAN succeed = FALSE;

    if (SenseInfoBuffer == NULL || SenseInfoBufferLength == 0)
    {
        return FALSE;
    }

    if (IsDescriptorSenseDataFormat(SenseInfoBuffer))
    {
        succeed = ScsiGetDescriptorSenseKeyAndCodes(SenseInfoBuffer,
                                                    SenseInfoBufferLength,
                                                    SenseKey,
                                                    AdditionalSenseCode,
                                                    AdditionalSenseCodeQualifier);
    }
    else
    {
        if ((Options & SCSI_SENSE_OPTIONS_FIXED_FORMAT_IF_UNKNOWN_FORMAT_INDICATED) ||
            IsFixedSenseDataFormat(SenseInfoBuffer))
        {
            succeed = ScsiGetFixedSenseKeyAndCodes(SenseInfoBuffer,
                                                   SenseInfoBufferLength,
                                                   SenseKey,
                                                   AdditionalSenseCode,
                                                   AdditionalSenseCodeQualifier);
        }
    }

    return succeed;
}


__forceinline
BOOLEAN
ScsiConvertToFixedSenseFormat(
    IN  VOID* SenseInfoBuffer,
    IN  UINT8 SenseInfoBufferLength,
    OUT VOID* OutBuffer,
    IN  UINT8 OutBufferLength
    )
/*++

Description:

    This routine converts descriptor format sense data to fixed format sense data.

    Due to differences between two formats, the conversion is only based on Sense Key,
    Additional Sense Code, and Additional Sense Code Qualififer.

Arguments:

    SenseInfoBuffer - A pointer to sense data buffer

    SenseInfoBufferLength - Size of the buffer SenseInfoBuffer points to.

    OutBuffer - On output, OutBuffer contains the fixed sense data as result of conversion.

    OutBufferLength - Size of the buffer that OutBuffer points to.

Returns:

    TRUE if conversion to Fixed format is successful.

    Otherwise, FALSE.

--*/
{
    BOOLEAN succeed = FALSE;
    BOOLEAN validSense  = FALSE;
    UINT8 senseKey = 0;
    UINT8 additionalSenseCode = 0;
    UINT8 additionalSenseCodeQualifier = 0;
    PFIXED_SENSE_DATA outBuffer = (PFIXED_SENSE_DATA)OutBuffer;

    if (SenseInfoBuffer == NULL ||
        SenseInfoBufferLength == 0 ||
        OutBuffer == NULL ||
        OutBufferLength < sizeof(FIXED_SENSE_DATA))
    {
        return FALSE;
    }

    if (IsDescriptorSenseDataFormat(SenseInfoBuffer))
    {
        ZeroMem(OutBuffer, OutBufferLength);

        validSense = ScsiGetSenseKeyAndCodes(SenseInfoBuffer,
                                             SenseInfoBufferLength,
                                             SCSI_SENSE_OPTIONS_NONE,
                                             &senseKey,
                                             &additionalSenseCode,
                                             &additionalSenseCodeQualifier);
        if (validSense)
        {
            if (IsSenseDataCurrentError(SenseInfoBuffer))
            {
                outBuffer->ErrorCode = SCSI_SENSE_ERRORCODE_FIXED_CURRENT;
            }
            else
            {
                outBuffer->ErrorCode = SCSI_SENSE_ERRORCODE_FIXED_DEFERRED;
            }

            outBuffer->AdditionalSenseLength = sizeof(FIXED_SENSE_DATA) - SIZEOF_THROUGH_FIELD(FIXED_SENSE_DATA, AdditionalSenseLength);
            outBuffer->SenseKey = senseKey;
            outBuffer->AdditionalSenseCode = additionalSenseCode;
            outBuffer->AdditionalSenseCodeQualifier = additionalSenseCodeQualifier;

            succeed = TRUE;
        }
    }

    return succeed;
}


