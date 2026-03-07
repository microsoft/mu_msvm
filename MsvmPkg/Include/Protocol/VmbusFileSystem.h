/** @file
    Implements the VMBus file system protocol.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#pragma once
#include "StaticAssert1.h"

// Make sure padding works correctly for the flexible array members.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(error:4820)
#endif

#define VMBFS_MAXIMUM_MESSAGE_SIZE 12288
#define VMBFS_MAXIMUM_PAYLOAD_SIZE(_Header_) (VMBFS_MAXIMUM_MESSAGE_SIZE - sizeof(_Header_))

#define VMBFS_MAKE_VERSION(Major, Minor) ((UINT32)((Major) << 16) | (Minor))

#define VMBFS_VERSION_WIN10         VMBFS_MAKE_VERSION(1, 0)


typedef enum _VMBFS_MESSAGE_TYPE
{
    VmbfsMessageTypeInvalid = 0,
    VmbfsMessageTypeVersionRequest,
    VmbfsMessageTypeVersionResponse,
    VmbfsMessageTypeGetFileInfo,
    VmbfsMessageTypeGetFileInfoResponse,
    VmbfsMessageTypeReadFile,
    VmbfsMessageTypeReadFileResponse,
    VmbfsMessageTypeReadFileRdma,
    VmbfsMessageTypeReadFileRdmaResponse,

    VmbfsMessageTypeMax

} VMBFS_MESSAGE_TYPE, *PVMBFS_MESSAGE_TYPE;


#define VMBFS_GET_FILE_INFO_FLAG_DIRECTORY    0x1
#define VMBFS_GET_FILE_INFO_FLAG_RDMA_CAPABLE 0x2

#define VMBFS_GET_FILE_INFO_FLAGS (VMBFS_GET_FILE_INFO_FLAG_DIRECTORY | VMBFS_GET_FILE_INFO_FLAG_RDMA_CAPABLE)

#pragma pack(push)
#pragma pack(1)

typedef struct _VMBFS_MESSAGE_HEADER
{
    VMBFS_MESSAGE_TYPE Type;
    UINT32 Reserved;
} VMBFS_MESSAGE_HEADER, *PVMBFS_MESSAGE_HEADER;

STATIC_ASSERT_1 (sizeof (VMBFS_MESSAGE_HEADER) == 8);

// Factor out the header, etc.
#define STATIC_ASSERT_VMBFS_MESSAGE_SIZE(type, size) STATIC_ASSERT_1 (sizeof (type) == 8 + size);

typedef struct _VMBFS_MESSAGE_VERSION_REQUEST
{
    VMBFS_MESSAGE_HEADER Header;
    UINT32 RequestedVersion;

} VMBFS_MESSAGE_VERSION_REQUEST, *PVMBFS_MESSAGE_VERSION_REQUEST;

STATIC_ASSERT_VMBFS_MESSAGE_SIZE (VMBFS_MESSAGE_VERSION_REQUEST, 4)

typedef enum _VMBFS_STATUS_VERSION_RESPONSE
{
    VmbfsVersionSupported = 0,
    VmbfsVersionUnsupported = 1

} VMBFS_STATUS_VERSION_RESPONSE, *PVMBFS_STATUS_VERSION_RESPONSE;


typedef struct _VMBFS_MESSAGE_VERSION_RESPONSE
{
    VMBFS_MESSAGE_HEADER Header;
    UINT32 Status;

} VMBFS_MESSAGE_VERSION_RESPONSE, *PVMBFS_MESSAGE_VERSION_RESPONSE;

STATIC_ASSERT_VMBFS_MESSAGE_SIZE (VMBFS_MESSAGE_VERSION_RESPONSE, 4)

typedef struct _VMBFS_MESSAGE_GET_FILE_INFO
{
    VMBFS_MESSAGE_HEADER Header;
    CHAR16 FilePath[];

} VMBFS_MESSAGE_GET_FILE_INFO, *PVMBFS_MESSAGE_GET_FILE_INFO;

STATIC_ASSERT_VMBFS_MESSAGE_SIZE (VMBFS_MESSAGE_GET_FILE_INFO, 0)

typedef enum _VMBFS_STATUS_FILE_RESPONSE
{
    VmbfsFileSuccess = 0,
    VmbfsFileNotFound = 1,
    VmbfsFileEndOfFile = 2,
    VmbfsFileError = 3

} VMBFS_STATUS_FILE_RESPONSE, *PVMBFS_STATUS_FILE_RESPONSE;


typedef struct _VMBFS_MESSAGE_GET_FILE_INFO_RESPONSE
{
    VMBFS_MESSAGE_HEADER Header;
    UINT32 Status;

    UINT32 Flags;
    UINT64 FileSize;

} VMBFS_MESSAGE_GET_FILE_INFO_RESPONSE, *PVMBFS_MESSAGE_GET_FILE_INFO_RESPONSE;

STATIC_ASSERT_VMBFS_MESSAGE_SIZE (VMBFS_MESSAGE_GET_FILE_INFO_RESPONSE, 16)

typedef struct _VMBFS_MESSAGE_READ_FILE
{
    VMBFS_MESSAGE_HEADER Header;
    UINT32 ByteCount;
    UINT64 Offset;
    CHAR16 FilePath[];

} VMBFS_MESSAGE_READ_FILE, *PVMBFS_MESSAGE_READ_FILE;

STATIC_ASSERT_VMBFS_MESSAGE_SIZE (VMBFS_MESSAGE_READ_FILE, 12)

typedef struct _VMBFS_MESSAGE_READ_FILE_RESPONSE
{
    VMBFS_MESSAGE_HEADER Header;
    UINT32 Status;

    UINT8 Payload[];
} VMBFS_MESSAGE_READ_FILE_RESPONSE, *PVMBFS_MESSAGE_READ_FILE_RESPONSE;

STATIC_ASSERT_VMBFS_MESSAGE_SIZE (VMBFS_MESSAGE_READ_FILE_RESPONSE, 4)

typedef struct _VMBFS_MESSAGE_READ_FILE_RDMA
{
    VMBFS_MESSAGE_HEADER Header;
    UINT32 Handle;
    UINT32 ByteCount;
    UINT64 FileOffset;
    UINT64 TokenOffset;
    CHAR16 FilePath[];
} VMBFS_MESSAGE_READ_FILE_RDMA, *PVMBFS_MESSAGE_READ_FILE_RDMA;

STATIC_ASSERT_VMBFS_MESSAGE_SIZE (VMBFS_MESSAGE_READ_FILE_RDMA, 24)

typedef struct _VMBFS_MESSAGE_READ_FILE_RDMA_RESPONSE
{
    VMBFS_MESSAGE_HEADER Header;
    UINT32 Status;
    UINT32 ByteCount;
} VMBFS_MESSAGE_READ_FILE_RDMA_RESPONSE, *PVMBFS_MESSAGE_READ_FILE_RDMA_RESPONSE;

STATIC_ASSERT_VMBFS_MESSAGE_SIZE (VMBFS_MESSAGE_READ_FILE_RDMA_RESPONSE, 8)

#pragma pack(pop)

#ifdef _MSC_VER
#pragma warning(pop)
#endif
