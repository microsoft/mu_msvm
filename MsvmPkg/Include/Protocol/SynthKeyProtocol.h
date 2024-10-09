/** @file
  Definitions of the keyboard message structures used by
  the synthetic keyboard device and its VSC.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

extern GUID gSyntheticKeyboardClassGuid;

#define HK_MAKE_VERSION(Major, Minor) ((UINT32)(Major) << 16 | (UINT32)(Minor))
#define HK_VERSION_WIN8 HK_MAKE_VERSION(1, 0)

typedef enum _HK_MESSAGE_TYPE
{
    HkMessageProtocolRequest  = 1,
    HkMessageProtocolResponse = 2,
    HkMessageEvent            = 3,
    HkMessageSetLedIndicators = 4,
} HK_MESSAGE_TYPE;

typedef struct _HK_MESSAGE_HEADER
{
    HK_MESSAGE_TYPE MessageType;
} HK_MESSAGE_HEADER, *PHK_MESSAGE_HEADER;

typedef struct _HK_MESSAGE_PROTOCOL_REQUEST
{
    HK_MESSAGE_HEADER Header;
    UINT32 Version;
} HK_MESSAGE_PROTOCOL_REQUEST, *PHK_MESSAGE_PROTOCOL_REQUEST;

typedef struct _HK_MESSAGE_LED_INDICATORS_STATE
{
    HK_MESSAGE_HEADER Header;
    UINT16 LedFlags;
} HK_MESSAGE_LED_INDICATORS_STATE, *PHK_MESSAGE_LED_INDICATORS_STATE;

typedef struct _HK_MESSAGE_PROTOCOL_RESPONSE
{
    HK_MESSAGE_HEADER Header;
    UINT32 Accepted:1;
    UINT32 Reserved:31;
} HK_MESSAGE_PROTOCOL_RESPONSE, *PHK_MESSAGE_PROTOCOL_RESPONSE;

typedef struct _HK_MESSAGE_KEYSTROKE
{
    HK_MESSAGE_HEADER Header;
    UINT16 MakeCode;
    UINT32 IsUnicode:1;
    UINT32 IsBreak:1;
    UINT32 IsE0:1;
    UINT32 IsE1:1;
    UINT32 Reserved:28;
} HK_MESSAGE_KEYSTROKE, *PHK_MESSAGE_KEYSTROKE;

#define HK_MAXIMUM_MESSAGE_SIZE 256

