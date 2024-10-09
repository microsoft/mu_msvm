/** @file
  This file contains the definitions for the Hyper-V synthetic video protocol.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

//
// Latest version of the SynthVid protocol.
//
#define SYNTHVID_VERSION_MAJOR 3
#define SYNTHVID_VERSION_MINOR 5

#define SYNTHVID_VERSION_CURRENT ((SYNTHVID_VERSION_MINOR << 16) | (SYNTHVID_VERSION_MAJOR))
#define TRUE_WITH_VERSION_EXCHANGE (TRUE + 1)

#pragma once
#pragma pack(push,1)


//
// SynthVid Message Types
//
typedef enum
{
    SynthvidError                  = 0,
    SynthvidVersionRequest         = 1,
    SynthvidVersionResponse        = 2,
    SynthvidVramLocation           = 3,
    SynthvidVramLocationAck        = 4,
    SynthvidSituationUpdate        = 5,
    SynthvidSituationUpdateAck     = 6,
    SynthvidPointerPosition        = 7,
    SynthvidPointerShape           = 8,
    SynthvidFeatureChange          = 9,
    SynthvidDirt                   = 10,
    SynthvidBiosInfoRequest        = 11,
    SynthvidBiosInfoResponse       = 12,

    SynthvidMax                    = 13
} SYNTHVID_MESSAGE_TYPE;

//
// Basic message structures.
//
typedef struct
{
    SYNTHVID_MESSAGE_TYPE   Type;    // Type of the enclosed message
    UINT32                  Size;    // Size of the enclosed message (size of the data payload)
} SYNTHVID_MESSAGE_HEADER, *PSYNTHVID_MESSAGE_HEADER;

typedef struct
{
    SYNTHVID_MESSAGE_HEADER Header;
    BYTE                    Data[1]; // Enclosed message
} SYNTHVID_MESSAGE, *PSYNTHVID_MESSAGE;


#pragma warning(push)
#pragma warning(disable : 4201)
typedef union
{
    struct
    {
        UINT16 MajorVersion;
        UINT16 MinorVersion;
    };

    UINT32 AsDWORD;
} SYNTHVID_VERSION, *PSYNTHVID_VERSION;
#pragma warning(pop)

//
// The following messages are listed in order of occurance during startup
// and handshaking.
//

// VSC to VSP /////////////////////////////////////////////////////
typedef struct
{
    SYNTHVID_MESSAGE_HEADER Header;
    SYNTHVID_VERSION        Version;
} SYNTHVID_VERSION_REQUEST_MESSAGE, *PSYNTHVID_VERSION_REQUEST_MESSAGE;

// VSP to VSC /////////////////////////////////////////////////////
typedef struct
{
    SYNTHVID_MESSAGE_HEADER Header;
    SYNTHVID_VERSION        Version;
    BOOLEAN IsAccepted;
    UINT8                   MaxVideoOutputs; // 1 in Veridian 1.0
} SYNTHVID_VERSION_RESPONSE_MESSAGE, *PSYNTHVID_VERSION_RESPONSE_MESSAGE;

// VSC to VSP /////////////////////////////////////////////////////
typedef struct
{
    SYNTHVID_MESSAGE_HEADER Header;
    UINT64                  UserContext;
    BOOLEAN                 IsVramGpaAddressSpecified;
    UINT64                  VramGpaAddress;
} SYNTHVID_VRAM_LOCATION_MESSAGE, *PSYNTHVID_VRAM_LOCATION_MESSAGE;


// VSP to VSC ////////////////////////////////////////////////////
// This is called "acknowledge", but in addition, it indicates to the VSC
// that the new physical address location is backed with a memory block
// that the guest can safely write to, knowing that the writes will actually
// be reflected in the VRAM memory block.
typedef struct
{
    SYNTHVID_MESSAGE_HEADER Header;
    UINT64                  UserContext;
} SYNTHVID_VRAM_LOCATION_ACK_MESSAGE, *PSYNTHVID_VRAM_LOCATION_ACK_MESSAGE;




//
// These messages are used to communicate "situation updates" or changes
// in the layout of the primary surface.
//
typedef struct
{
    BOOLEAN Active;
    UINT32  PrimarySurfaceVramOffset;
    UINT8   DepthBits;
    UINT32  WidthPixels;
    UINT32  HeightPixels;
    UINT32  PitchBytes;
} VIDEO_OUTPUT_SITUATION, *PVIDEO_OUTPUT_SITUATION;

// VSC to VSP ////////////////////////////////////////////////////
typedef struct
{
    SYNTHVID_MESSAGE_HEADER Header;
    UINT64                  UserContext;
    UINT8                   VideoOutputCount; // 1 in Veridian 1.0
    VIDEO_OUTPUT_SITUATION  VideoOutput[1];
} SYNTHVID_SITUATION_UPDATE_MESSAGE, *PSYNTHVID_SITUATION_UPDATE_MESSAGE;

// VSP to VSC ////////////////////////////////////////////////////
typedef struct
{
    SYNTHVID_MESSAGE_HEADER Header;
    UINT64                  UserContext;
} SYNTHVID_SITUATION_UPDATE_ACK_MESSAGE, *PSYNTHVID_SITUATION_UPDATE_ACK_MESSAGE;

#pragma pack(pop)


