/** @file

  Implementation EFI file protocol over vmbus.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "VmbfsEfi.h"

const EFI_FILE_PROTOCOL gVmbFsEfiFileProtocol =
{
    (UINT64)                    0x00010000,
    (EFI_FILE_OPEN)             VmbfsOpen,
    (EFI_FILE_CLOSE)            VmbfsClose,
    (EFI_FILE_DELETE)           VmbfsDelete,
    (EFI_FILE_READ)             VmbfsRead,
    (EFI_FILE_WRITE)            VmbfsWrite,
    (EFI_FILE_GET_POSITION)     VmbfsGetPosition,
    (EFI_FILE_SET_POSITION)     VmbfsSetPosition,
    (EFI_FILE_GET_INFO)         VmbfsGetInfo,
    (EFI_FILE_SET_INFO)         VmbfsSetInfo,
    (EFI_FILE_FLUSH)            VmbfsFlush
};

const EFI_FILE_INFO gVmbFsEfiFileInfoPrototype =
{
    0, // Size, To be filled by VmbfsOpen
    0, // FileSize, To be filled by VmbfsOpen
    0, // PhysicalSize, To be filled by VmbfsOpen
    {0},
    {0},
    EFI_FILE_VALID_ATTR, // Attributes, more attributes to be filled by VmbfsOpen
    {0} // FileName, To be filled by VmbfsOpen
};


VOID
VmbfsReceivePacketCallback (
    IN  VOID *ReceiveContext,
    IN  VOID *PacketContext,
    IN  VOID *Buffer,
    IN  UINT32 BufferLength,
    IN  UINT16 TransferPageSetId,
    IN  UINT32 RangeCount,
    IN  EFI_TRANSFER_RANGE *Ranges
    )

/*++

Routine Description:

    Callback for receiving a packet in the VMBus pipe. Copies the in-place
    buffer to the buffer in the FILESYSTEM_INFORMATION context and signals
    the receiver.

Arguments:

    ReceiveContext - The receive context, which is the FILESYSTEM_INFORMATION
        structure.

    PacketContext - The packet context of the EMCL.

    Buffer - The inplace receive buffer.

    BufferLength - Length of the packet.

    TransferPageSetId - Not used.

    RangeCount - Not used.

    Ranges - Not used.

Return Value:

    None.

--*/

{
    PFILESYSTEM_INFORMATION fileSystemInformation;

    fileSystemInformation = (PFILESYSTEM_INFORMATION)ReceiveContext;

    if (BufferLength > VMBFS_MAXIMUM_MESSAGE_SIZE)
    {
        VMBFS_BAD_HOST;
        BufferLength = MIN(BufferLength, VMBFS_MAXIMUM_MESSAGE_SIZE);
    }

    CopyMem(fileSystemInformation->PacketBuffer, Buffer, BufferLength);
    fileSystemInformation->PacketSize = BufferLength;

    gBS->SignalEvent(fileSystemInformation->ReceivePacketEvent);
}


EFI_STATUS
VmbfsSendReceivePacket (
    IN  PFILESYSTEM_INFORMATION FileSystemInformation,
    IN  VOID *Buffer,
    IN  UINTN BufferLength,
    IN  UINT32 GpaRangeHandle,
    IN  VOID *ExternalBuffer,
    IN  UINTN ExternalBufferLength,
    IN  BOOLEAN IsWritable
    )

/*++

Routine Description:

    Synchronously send a packet and wait for its response.

Arguments:

    FileSystemInformation - The FileSystemInformation context, containing
        the buffer information.

    Buffer - Pointer to the buffer to send.

    BufferLength - Length of the packet to send.

    GpaRangeHandle - If there is an external buffer, use this handle
        value to establish a GPA range while the transaction is in
        progress.

    ExternalBuffer - A pointer to the external data for this transaction.

    ExternalBufferLength - The length of the external buffer in bytes.

    IsWritable - If TRUE, the external buffer may be written by the host.

Return Value:

    EFI_SUCCESS on success.

    Error code of EmclProtocol->SendPacket or WaitForEvent otherwise.

--*/

{
    EFI_EXTERNAL_BUFFER externalBuffers[1];
    BOOLEAN createdGpaRange;
    EFI_STATUS status;
    UINTN eventIndex;

    createdGpaRange = FALSE;

    AcquireSpinLock(&FileSystemInformation->VmbusIoLock);

    if (ExternalBufferLength > 0)
    {
        externalBuffers[0].Buffer = ExternalBuffer;
        externalBuffers[0].BufferSize = (UINT32)ExternalBufferLength;
        status = FileSystemInformation->EmclProtocol->CreateGpaRange(
                            FileSystemInformation->EmclProtocol,
                            GpaRangeHandle,
                            externalBuffers,
                            1,
                            IsWritable);

        if (EFI_ERROR(status))
        {
            goto Cleanup;
        }

        createdGpaRange = TRUE;
    }

    status = FileSystemInformation->EmclProtocol->SendPacket(
                            FileSystemInformation->EmclProtocol,
                            Buffer,
                            (UINT32)BufferLength,
                            NULL,
                            0,
                            NULL,
                            NULL);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = gBS->WaitForEvent(1, &FileSystemInformation->ReceivePacketEvent, &eventIndex);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = EFI_SUCCESS;

Cleanup:
    if (createdGpaRange)
    {
        FileSystemInformation->EmclProtocol->DestroyGpaRange(
                FileSystemInformation->EmclProtocol,
                GpaRangeHandle);
    }

    ReleaseSpinLock(&FileSystemInformation->VmbusIoLock);
    return status;
}


EFI_STATUS
EFIAPI
VmbfsOpen (
    IN  EFI_FILE_PROTOCOL *This,
    OUT EFI_FILE_PROTOCOL **NewHandle,
    IN  CHAR16 *FileName,
    IN  UINT64 OpenMode,
    IN  UINT64 Attributes
    )

/*++

Routine Description:

    Opens a new file relative to the source file's location.

    To open a file, we have to ensure that the file exists.  This routine sends
    a message to the host querying the file's properties.

Arguments:

    This - A pointer to the EFI_FILE_PROTOCOL instance that is the file handle to
        the source location. This would typically be an open handle to a directory.

    NewHandle - A pointer to the location to return the opened handle for the new file.

    FileNameThe - Null-terminated string of the name of the file to be opened.
        The file name may contain the following path modifiers: "\", ".", and "..".

    OpenMode - The mode to open the file. The only valid combinations that the file
        may be opened with are: Read, Read/Write, or Create/Read/Write.

    Attributes - Only valid for EFI_FILE_MODE_CREATE, in which case these are the
        attribute bits for the newly created file.


Return Value:

    EFI_SUCCESS - The file was opened.

    EFI_NOT_FOUND - The specified file could not be found on the device.

    EFI_NO_MEDIA - The device has no medium.

    EFI_MEDIA_CHANGED - The device has a different medium in it or the medium is no
        longer supported.

    EFI_DEVICE_ERROR - The device reported an error.

--*/

{
    VMBFS_FILE* allocatedFileProtocol = NULL;
    PFILE_INFORMATION parentFileInformation;
    CHAR16 *parentFilePath;
    UINT32 bytesRead;
    PFILE_INFORMATION fileInformation = NULL;
    EFI_FILE_INFO* efiFileInfo = NULL;
    CHAR16* filePath = NULL;
    const CHAR16* pathFormat;
    UINTN filePathLength = 0;
    UINTN parentFilePathLength = 0;
    UINTN filePathLengthInBytes;
    PVMBFS_MESSAGE_GET_FILE_INFO getFileInfoMessage = NULL;
    PVMBFS_MESSAGE_GET_FILE_INFO_RESPONSE getFileInfoResponseMessage = NULL;
    UINTN getFileInfoMessageSize;
    EFI_STATUS status = EFI_SUCCESS;

    parentFileInformation = GetThisFileInformation(This);
    parentFilePath = &GetThisEfiFileInfo(This)->FileName[0];

    //
    // VMBFS does not support write.
    //
    if (parentFileInformation == NULL || OpenMode & EFI_FILE_MODE_WRITE || OpenMode & EFI_FILE_MODE_CREATE)
    {
        status = EFI_INVALID_PARAMETER;
        goto Cleanup;
    }

    filePathLength = StrLen(parentFilePath);
    parentFilePathLength = filePathLength;
    if (filePathLength > 1)
    {

        //
        // Include space for the separating backslash iff the parent is not
        // the root.
        //
        filePathLength += 1;
    }

    filePathLength += (StrLen(FileName) + 1);
    filePathLengthInBytes = filePathLength * sizeof(CHAR16);


    status = gBS->AllocatePool(EfiBootServicesData,
                               sizeof(*allocatedFileProtocol) + filePathLengthInBytes,
                               &allocatedFileProtocol);
    if (EFI_ERROR(status))
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    ZeroMem(allocatedFileProtocol, sizeof(*allocatedFileProtocol));

    fileInformation = &allocatedFileProtocol->FileInformation;

    efiFileInfo = &allocatedFileProtocol->EfiFileInfo;

    filePath = &efiFileInfo->FileName[0];

    *efiFileInfo = gVmbFsEfiFileInfoPrototype;

    //
    // File path in VMBus message does not require a terminating NULL.
    //
    filePathLengthInBytes -= sizeof(CHAR16);

    getFileInfoMessageSize = sizeof(*getFileInfoMessage) + filePathLengthInBytes;

    if (filePathLengthInBytes > VMBFS_MAXIMUM_PAYLOAD_SIZE(*getFileInfoMessage))
    {
        status = EFI_BAD_BUFFER_SIZE;
        goto Cleanup;
    }

    if (parentFilePathLength == 0 || parentFilePath[parentFilePathLength - 1] == L'\\')
    {
        pathFormat = L"%ls%ls";
    }
    else
    {
        pathFormat = L"%ls\\%ls";
    }

    UnicodeSPrint(filePath,
                  filePathLengthInBytes + sizeof(CHAR16),
                  pathFormat,
                  parentFilePath,
                  FileName);

    efiFileInfo->Size = sizeof(EFI_FILE_INFO) + filePathLengthInBytes + sizeof(CHAR16);

    //
    // Build a GetFileInfo message.
    //
    getFileInfoMessage = GetPacketBuffer(parentFileInformation, VMBFS_MESSAGE_GET_FILE_INFO);
    ZeroMem(getFileInfoMessage, sizeof(*getFileInfoMessage));
    getFileInfoMessage->Header.Type = VmbfsMessageTypeGetFileInfo;

    CopyMem(getFileInfoMessage->FilePath,
            filePath,
            filePathLengthInBytes);


    status = VmbfsSendReceivePacket(GetFileSystemInformation(parentFileInformation),
                                    getFileInfoMessage,
                                    getFileInfoMessageSize,
                                    0,
                                    NULL,
                                    0,
                                    FALSE);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    getFileInfoResponseMessage = GetPacketBuffer(parentFileInformation, VMBFS_MESSAGE_GET_FILE_INFO_RESPONSE);
    bytesRead = GetPacketSize(parentFileInformation);

    if (bytesRead != sizeof(*getFileInfoResponseMessage) ||
        getFileInfoResponseMessage->Header.Type != VmbfsMessageTypeGetFileInfoResponse)
    {

        VMBFS_BAD_HOST;
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    if (getFileInfoResponseMessage->Status == VmbfsFileNotFound)
    {
        status = EFI_NOT_FOUND;
        goto Cleanup;
    }

    if (getFileInfoResponseMessage->Status != VmbfsFileSuccess)
    {
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    efiFileInfo->FileSize = getFileInfoResponseMessage->FileSize;
    efiFileInfo->PhysicalSize = getFileInfoResponseMessage->FileSize;
    efiFileInfo->Attribute |= EFI_FILE_READ_ONLY;

    if ((getFileInfoResponseMessage->Flags & VMBFS_GET_FILE_INFO_FLAG_DIRECTORY) != 0)
    {
        fileInformation->IsDirectory = TRUE;
        efiFileInfo->Attribute |= EFI_FILE_DIRECTORY;
    }

    fileInformation->RdmaCapable = ((getFileInfoResponseMessage->Flags & VMBFS_GET_FILE_INFO_FLAG_RDMA_CAPABLE) != 0);

    fileInformation->FileSystem = parentFileInformation->FileSystem;

    fileInformation->FilePathLength = filePathLengthInBytes / sizeof(CHAR16);

    CopyMem(&allocatedFileProtocol->EfiFileProtocol, This, sizeof(allocatedFileProtocol->EfiFileProtocol));

    fileInformation->FileSystem->FileSystemInformation.ReferenceCount++;

    *NewHandle = &allocatedFileProtocol->EfiFileProtocol;
    status = EFI_SUCCESS;

Cleanup:
    if (EFI_ERROR(status))
    {
        if (allocatedFileProtocol != NULL)
        {
            gBS->FreePool(allocatedFileProtocol);
        }
    }

    return status;
}


EFI_STATUS
EFIAPI
VmbfsClose (
    IN  EFI_FILE_PROTOCOL *This
    )

/*++

Routine Description:

    Closes a file.  No open state is maintained on the host, so just free the
    descriptors.

Arguments:

    This - The EFI file protocol to close.

Return Value:

    EFI_SUCCESS.

--*/

{
    PFILE_INFORMATION fileInformation;

    fileInformation = GetThisFileInformation(This);

    fileInformation->FileSystem->FileSystemInformation.ReferenceCount--;

    if (fileInformation->FileSystem->FileSystemInformation.ReferenceCount == 0)
    {
        VmbfsCloseVolume(fileInformation->FileSystem, TRUE);
    }

    gBS->FreePool(This);

    return EFI_SUCCESS;
}


EFI_STATUS
VmbfsErrorToEfiError(
    IN  UINT32 Error
    )
/*++

Routine Description:

    Converts a Vmbfs error to an EFI error.

Arguments:

    Error - A Vmbfs error returned by the host.

Return Value:

    The corresponding EFI error code.

--*/
{
    switch (Error)
    {
    case VmbfsFileSuccess:
        return EFI_SUCCESS;

    case VmbfsFileNotFound:
        return EFI_NOT_FOUND;

    case VmbfsFileEndOfFile:
        return EFI_END_OF_FILE;

    default:
        return EFI_DEVICE_ERROR;
    }
}


EFI_STATUS
VmbfsReadPayload(
    IN  VMBFS_FILE *File,
    IN  UINT64 FileOffset,
    OUT VOID *Buffer,
    IN  UINTN BufferSize,
    OUT UINTN *BytesRead
    )
/*++

Routine Description:

    Issues a single file read request to the host and copies the results
    into the provided buffer.

Arguments:

    File - A pointer to the file.

    FileOffset - The offset within the file at which to read.

    Buffer - A pointer to the buffer to read into.

    BufferSize - The size of the buffer in bytes.

    BytesRead - On return, the number of bytes of the file that were
        successfully read into the buffer.

Return Value:

    EFI_STATUS.

--*/
{
    UINTN bytesReceived;
    UINTN bytesRequested;
    PVMBFS_MESSAGE_READ_FILE readFileMessage;
    PVMBFS_MESSAGE_READ_FILE_RESPONSE readFileResponseMessage;
    EFI_STATUS status;

    //
    // Ensure the path will fit in the request message.
    //
    if (File->FileInformation.FilePathLength * sizeof(CHAR16) > VMBFS_MAXIMUM_PAYLOAD_SIZE(*readFileMessage))
    {
        status = EFI_BUFFER_TOO_SMALL;
        goto Cleanup;
    }

    bytesRequested = MIN(BufferSize, VMBFS_MAXIMUM_PAYLOAD_SIZE(*readFileResponseMessage));
    readFileMessage = GetPacketBuffer(&File->FileInformation, VMBFS_MESSAGE_READ_FILE);
    ZeroMem(readFileMessage, sizeof(*readFileMessage));
    readFileMessage->Header.Type = VmbfsMessageTypeReadFile;
    readFileMessage->Offset = FileOffset;
    readFileMessage->ByteCount = (UINT32)bytesRequested;
    CopyMem(readFileMessage->FilePath,
            File->EfiFileInfo.FileName,
            File->FileInformation.FilePathLength * sizeof(CHAR16));

    status = VmbfsSendReceivePacket(GetFileSystemInformation(&File->FileInformation),
                                    readFileMessage,
                                    sizeof(*readFileMessage) + File->FileInformation.FilePathLength * sizeof(CHAR16),
                                    0,
                                    NULL,
                                    0,
                                    FALSE);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    readFileResponseMessage = GetPacketBuffer(&File->FileInformation, VMBFS_MESSAGE_READ_FILE_RESPONSE);
    bytesReceived = GetPacketSize(&File->FileInformation);

    if (bytesReceived < sizeof(*readFileResponseMessage) ||
        readFileResponseMessage->Header.Type != VmbfsMessageTypeReadFileResponse)
    {
        VMBFS_BAD_HOST;
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    status = VmbfsErrorToEfiError(readFileResponseMessage->Status);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    bytesReceived -= sizeof(VMBFS_MESSAGE_READ_FILE_RESPONSE);
    if (bytesReceived > bytesRequested)
    {
        VMBFS_BAD_HOST;
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    CopyMem((UINT8*)Buffer,
            readFileResponseMessage->Payload,
            bytesReceived);

    *BytesRead = bytesReceived;
    status = EFI_SUCCESS;

Cleanup:
    return status;
}


EFI_STATUS
VmbfsReadRdma(
    IN  VMBFS_FILE *File,
    IN  UINT64 FileOffset,
    OUT VOID *Buffer,
    IN  UINTN BufferSize,
    OUT UINTN *BytesRead
    )
/*++

Routine Description:

    Issues a single file read request to the host, using vRDMA to
    allow the host to directly write the result to guest memory
    without copying through the ring.

Arguments:

    File - A pointer to the file.

    FileOffset - The offset within the file at which to read.

    Buffer - A pointer to the buffer to read into.

    BufferSize - The size of the buffer in bytes.

    BytesRead - On return, the number of bytes of the file that were
        successfully read into the buffer.

Return Value:

    EFI_STATUS.

--*/
{
    UINTN bytesReceived;
    UINTN bytesRequested;
    PVMBFS_MESSAGE_READ_FILE_RDMA readFileMessage;
    PVMBFS_MESSAGE_READ_FILE_RDMA_RESPONSE readFileResponseMessage;
    EFI_STATUS status;

    //
    // Ensure the path will fit in the request message.
    //
    if (File->FileInformation.FilePathLength * sizeof(CHAR16) > VMBFS_MAXIMUM_PAYLOAD_SIZE(*readFileMessage))
    {
        status = EFI_BUFFER_TOO_SMALL;
        goto Cleanup;
    }

    bytesRequested = MIN(BufferSize, VMBFS_MAXIMUM_RDMA_SIZE);
    readFileMessage = GetPacketBuffer(&File->FileInformation, VMBFS_MESSAGE_READ_FILE_RDMA);
    ZeroMem(readFileMessage, sizeof(*readFileMessage));
    readFileMessage->Header.Type = VmbfsMessageTypeReadFileRdma;
    readFileMessage->Handle = 1;
    readFileMessage->FileOffset = FileOffset;
    readFileMessage->ByteCount = (UINT32)bytesRequested;
    CopyMem(readFileMessage->FilePath,
            File->EfiFileInfo.FileName,
            File->FileInformation.FilePathLength * sizeof(CHAR16));

    status = VmbfsSendReceivePacket(GetFileSystemInformation(&File->FileInformation),
                                    readFileMessage,
                                    sizeof(*readFileMessage) + File->FileInformation.FilePathLength * sizeof(CHAR16),
                                    readFileMessage->Handle,
                                    Buffer,
                                    bytesRequested,
                                    TRUE);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    readFileResponseMessage = GetPacketBuffer(&File->FileInformation, VMBFS_MESSAGE_READ_FILE_RDMA_RESPONSE);
    bytesReceived = GetPacketSize(&File->FileInformation);

    if (bytesReceived < sizeof(*readFileResponseMessage) ||
        readFileResponseMessage->Header.Type != VmbfsMessageTypeReadFileRdmaResponse)
    {
        VMBFS_BAD_HOST;
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    status = VmbfsErrorToEfiError(readFileResponseMessage->Status);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    if (readFileResponseMessage->ByteCount > bytesRequested)
    {
        VMBFS_BAD_HOST;
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    *BytesRead = readFileResponseMessage->ByteCount;
    status = EFI_SUCCESS;

Cleanup:
    return status;
}


EFI_STATUS
EFIAPI
VmbfsRead (
    IN      EFI_FILE_PROTOCOL *This,
    IN OUT  UINTN *BufferSize,
    OUT     VOID *Buffer
    )

/*++

Routine Description:

    Reads Size bytes starting at the offset indicated in the file information
    structure : FileEntry->FileData.Infomation.FileOffset.

Arguments:

    This - A pointer to the EFI_FILE_PROTOCOL instance that is the file handle
        to read data from.

    BufferSize - On input, the size of the Buffer. On output, the amount of
        data returned in Buffer. In both cases, the size is measured in bytes.

    Buffer - Transfer buffer for the read operation.

Return Value:

    EFI_SUCCESS when successful.

    EFI_END_OF_FILE if a read offset is invalid.

--*/

{
    UINTN bytesRead;
    UINTN bytesReadThisTime;
    VMBFS_FILE *file;
    EFI_STATUS status;

    bytesRead = 0;
    file = (VMBFS_FILE *)This;

    if (file->FileInformation.IsDirectory)
    {
        status = EFI_INVALID_PARAMETER;
        goto Cleanup;
    }

    while (bytesRead < *BufferSize &&
           (file->FileInformation.FileOffset + bytesRead) < file->EfiFileInfo.FileSize)
    {
        if (file->FileInformation.RdmaCapable)
        {
            status = VmbfsReadRdma(file,
                                   file->FileInformation.FileOffset + bytesRead,
                                   (UINT8*)Buffer + bytesRead,
                                   *BufferSize - bytesRead,
                                   &bytesReadThisTime);
        }
        else
        {
            status = VmbfsReadPayload(file,
                                      file->FileInformation.FileOffset + bytesRead,
                                      (UINT8*)Buffer + bytesRead,
                                      *BufferSize - bytesRead,
                                      &bytesReadThisTime);
        }

        if (EFI_ERROR(status))
        {
            goto Cleanup;
        }

        bytesRead += bytesReadThisTime;
    }

    file->FileInformation.FileOffset += bytesRead;
    *BufferSize = bytesRead;
    status = EFI_SUCCESS;

Cleanup:
    return status;
}


EFI_STATUS
EFIAPI
VmbfsWrite (
    IN      EFI_FILE_PROTOCOL *This,
    IN OUT  UINTN *BufferSize,
    IN      VOID *Buffer
    )

/*++

Routine Description:

    VMBus file system does not support writes.

Arguments:

    This - A pointer to the EFI_FILE_PROTOCOL instance that is the file
        handle to write data to.

    BufferSize - On input, the size of the Buffer. On output, the amount
        of data actually written. In both cases, the size is measured in bytes.

    Buffer - The buffer of data to write.

Return Value:

    EFI_NOT_SUPPORTED.

--*/

{
    return EFI_UNSUPPORTED;
}


EFI_STATUS
EFIAPI
VmbfsGetPosition (
    IN  EFI_FILE_PROTOCOL *This,
    OUT UINT64 *Position
    )

/*++

Routine Description:

    Copies FileEntry->FileData.Information to the caller's buffer.

Arguments:

    This - A pointer to the EFI_FILE_PROTOCOL instance that is the he file handle
        to get the requested position on.

    Position - Pointer to buffer to copy file position.

Return Value:

    EFI_SUCCESS - The position was returned.

    EFI_UNSUPPORTED - The request is not valid on open directories.

--*/

{
    PFILE_INFORMATION fileInformation;

    fileInformation = GetThisFileInformation(This);

    if (fileInformation->IsDirectory)
    {
        return EFI_UNSUPPORTED;
    }

    *Position = fileInformation->FileOffset;
    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
VmbfsSetPosition (
    IN  EFI_FILE_PROTOCOL *This,
    IN  UINT64 Position
    )

/*++

Routine Description:

    The SetPosition() function sets the current file position for the handle to
    the position supplied. With the exception of seeking to position 0xFFFFFFFFFFFFFFFF,
    only absolute positioning is supported, and seeking past the end of the file is
    allowed (a subsequent write would grow the file).

    Seeking to position 0xFFFFFFFFFFFFFFFF causes the current position to be set to
    the end of the file.

Arguments:

    This - A pointer to the EFI_FILE_PROTOCOL instance that is the he file handle
        to set the requested position on.

    Position - The byte position from the start of the file to set.

Return Value:

    EFI_SUCCESS - The position was set.

    EFI_UNSUPPORTED - The seek request for nonzero is not valid on open
    directories.

--*/

{
    PFILE_INFORMATION fileInformation;
    EFI_FILE_INFO* efiFileInfo;

    fileInformation = GetThisFileInformation(This);
    efiFileInfo = GetThisEfiFileInfo(This);

    if (fileInformation->IsDirectory)
    {
        return (Position == 0 ? EFI_SUCCESS : EFI_UNSUPPORTED);
    }

    if (Position < efiFileInfo->FileSize)
    {
        fileInformation->FileOffset = Position;
        return EFI_SUCCESS;
    }
    else if (Position == 0xFFFFFFFFFFFFFFFF)
    {
        fileInformation->FileOffset = efiFileInfo->FileSize - 1;
        return EFI_SUCCESS;
    }

    return EFI_INVALID_PARAMETER;
}


EFI_STATUS
EFIAPI
VmbfsGetInfo (
    IN      EFI_FILE_PROTOCOL *This,
    IN      EFI_GUID *InformationType,
    IN OUT  UINTN *BufferSize,
    OUT     VOID *Buffer
    )

/*++

Routine Description:

    The GetInfo() function returns information of type InformationType for the
    requested file. If the file does not support the requested information type,
    then EFI_UNSUPPORTED is returned. If the buffer is not large enough to fit
    the requested structure, EFI_BUFFER_TOO_SMALL is returned and the BufferSize
    is set to the size of buffer that is required to make the request.

Arguments:

    This - A pointer to the EFI_FILE_PROTOCOL instance that is the file handle the
        requested information is for.

    InformationType - The type identifier for the information being requested.

    BufferSize - On input, the size of Buffer. On output, the amount of data returned
        in Buffer. In both cases, the size is measured in bytes.

    Buffer - A pointer to the data buffer to return. The buffer's type is indicated
        by InformationType.

Return Value:

    EFI_SUCCESS - The information has been successfully retrieved and stored in
        the provided buffer.

    EFI_UNSUPPORTED - The InformationType is not known.

--*/

{
    UINT64 bufferSize;
    VOID* srcBuffer;
    EFI_STATUS status = EFI_SUCCESS;

    if (CompareGuid(InformationType, &gEfiFileInfoGuid))
    {
        EFI_FILE_INFO* efiFileInfo;

        efiFileInfo = GetThisEfiFileInfo(This);
        bufferSize = efiFileInfo->Size;
        srcBuffer = (VOID*)efiFileInfo;
    }
    else if (CompareGuid(InformationType, &gEfiFileSystemInfoGuid))
    {
        EFI_FILE_SYSTEM_INFO* efiFileSystemInfo;

        efiFileSystemInfo = &GetThisFileInformation(This)->FileSystem->EfiFileSystemInfo;
        bufferSize = efiFileSystemInfo->Size;
        srcBuffer = (VOID*)efiFileSystemInfo;
    }
    else
    {
        return EFI_UNSUPPORTED;
    }

    if (*BufferSize < bufferSize)
    {
        status = EFI_BUFFER_TOO_SMALL;
    }
    else
    {
        CopyMem(Buffer, srcBuffer, (UINTN)bufferSize);
    }

    *BufferSize = (UINTN)bufferSize;

    return status;
}


EFI_STATUS
EFIAPI
VmbfsSetInfo (
    IN      EFI_FILE_PROTOCOL *This,
    IN      EFI_GUID *InformationType,
    IN OUT  UINTN *BufferSize,
    IN      VOID *Buffer
    )

/*++

Routine Description:

    The SetInfo() function sets information of type InformationType on the
    requested file. Because a read-only file can be opened only in read-only
    mode, an InformationType of EFI_FILE_INFO_ID can be used with a read-only
    file because this method is the only one that can be used to convert a
    read-only file to a read-write file. In this circumstance, only the Attribute
    field of the EFI_FILE_INFO structure may be modified. One or more calls to
    SetInfo() to change the Attribute field are permitted before it is closed.
    The file attributes will be valid the next time the file is opened with Open().

    An InformationType of EFI_FILE_SYSTEM_INFO_ID or EFI_FILE_SYSTEM_VOLUME_LABEL_ID
    may not be used on read-only media.

Arguments:

    This - A pointer to the EFI_FILE_PROTOCOL instance that is the file handle the
        requested information is for.

    InformationType - The type identifier for the information being requested.

    BufferSize - On input, the size of Buffer. On output, the amount of data returned
        in Buffer. In both cases, the size is measured in bytes.

    Buffer - A pointer to the data buffer to write. The buffer's type is indicated
        by InformationType.

Return Value:

    EFI_UNSUPPORTED - The InformationType is not known.

--*/

{
    return EFI_UNSUPPORTED;
}


EFI_STATUS
EFIAPI
VmbfsFlush (
    IN  EFI_FILE_PROTOCOL *This
    )

/*++

Routine Description:

    The Flush() function flushes all modified data associated with a file to a device.

Arguments:

    This - A pointer to the EFI_FILE_PROTOCOL instance that is the file handle to flush.

Return Value:

    EFI_UNSUPPORTED - Vmbfs currently does not support write.

--*/

{
    return EFI_UNSUPPORTED;
}


EFI_STATUS
EFIAPI
VmbfsDelete (
    IN  EFI_FILE_PROTOCOL *This
    )

/*++

Routine Description:

    The Delete() function closes and deletes a file. In all cases the file
    handle is closed. If the file cannot be deleted, the warning code
    EFI_WARN_DELETE_FAILURE is returned, but the handle is still closed.

Arguments:

    This - A pointer to the EFI_FILE_PROTOCOL instance that is the file handle to delete.

Return Value:

    EFI_UNSUPPORTED - Vmbfs currently does not support delete operation.

--*/

{
    return EFI_UNSUPPORTED;
}


