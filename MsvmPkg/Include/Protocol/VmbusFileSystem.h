/** @file
    Implements the VMBus file system protocol.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once
#pragma warning(push)

//
// Make sure padding works correctly for the flexible array members.
//

#pragma warning(error:4820)

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


typedef struct _VMBFS_MESSAGE_VERSION_REQUEST
{
    VMBFS_MESSAGE_HEADER Header;
    UINT32 RequestedVersion;

} VMBFS_MESSAGE_VERSION_REQUEST, *PVMBFS_MESSAGE_VERSION_REQUEST;


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


typedef struct _VMBFS_MESSAGE_GET_FILE_INFO
{
    VMBFS_MESSAGE_HEADER Header;
    CHAR16 FilePath[];

} VMBFS_MESSAGE_GET_FILE_INFO, *PVMBFS_MESSAGE_GET_FILE_INFO;


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


typedef struct _VMBFS_MESSAGE_READ_FILE
{
    VMBFS_MESSAGE_HEADER Header;
    UINT32 ByteCount;
    UINT64 Offset;
    CHAR16 FilePath[];

} VMBFS_MESSAGE_READ_FILE, *PVMBFS_MESSAGE_READ_FILE;


typedef struct _VMBFS_MESSAGE_READ_FILE_RESPONSE
{
    VMBFS_MESSAGE_HEADER Header;
    UINT32 Status;

    UINT8 Payload[];

} VMBFS_MESSAGE_READ_FILE_RESPONSE, *PVMBFS_MESSAGE_READ_FILE_RESPONSE;

typedef struct _VMBFS_MESSAGE_READ_FILE_RDMA
{
    VMBFS_MESSAGE_HEADER Header;
    UINT32 Handle;
    UINT32 ByteCount;
    UINT64 FileOffset;
    UINT64 TokenOffset;
    CHAR16 FilePath[];

} VMBFS_MESSAGE_READ_FILE_RDMA, *PVMBFS_MESSAGE_READ_FILE_RDMA;

typedef struct _VMBFS_MESSAGE_READ_FILE_RDMA_RESPONSE
{
    VMBFS_MESSAGE_HEADER Header;
    UINT32 Status;
    UINT32 ByteCount;

} VMBFS_MESSAGE_READ_FILE_RDMA_RESPONSE, *PVMBFS_MESSAGE_READ_FILE_RDMA_RESPONSE;

#pragma pack(pop)
#pragma warning(pop)

