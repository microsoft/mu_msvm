/** @file

  Implementation EFI simple file system protocol over vmbus.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "VmbfsEfi.h"

const EFI_SIMPLE_FILE_SYSTEM_PROTOCOL gVmbFsSimpleFileSystemProtocol =
{
    (UINT64)                                         0x00010000,
    (EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME)    VmbfsOpenVolume
};

const EFI_FILE_SYSTEM_INFO gVmbFsEfiFileSystemInfoPrototype =
{
    sizeof(EFI_FILE_SYSTEM_INFO),
    TRUE,
    0,
    0,
    0,
    {0}
};


VOID
VmbfsCloseVolume (
    IN  PVMBFS_SIMPLE_FILE_SYSTEM_PROTOCOL VmbfsSimpleFileSystemProtocol,
    IN  BOOLEAN ChannelOpened
    )

/*++

Routine Description:

    Destroys the VMBus file system.

Arguments:

    VmbfsSimpleFileSystemProtocol - The VMBus file system to destroy.

    ChannelOpened - Whether the VMBus channel has been opened.

Return Value:

    None.

--*/

{
    PFILESYSTEM_INFORMATION fileSystemInformation =
        &VmbfsSimpleFileSystemProtocol->FileSystemInformation;

    if (VmbfsSimpleFileSystemProtocol != NULL)
    {
        if (ChannelOpened)
        {
            fileSystemInformation->EmclProtocol->StopChannel(
                fileSystemInformation->EmclProtocol);
        }

        if (fileSystemInformation->ReceivePacketEvent != NULL)
        {
            gBS->CloseEvent(fileSystemInformation->ReceivePacketEvent);
        }

        if (fileSystemInformation->PacketBuffer != NULL)
        {
            gBS->FreePool(fileSystemInformation->PacketBuffer);
        }
    }
}


EFI_STATUS
EFIAPI
VmbfsOpenVolume (
    IN  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
    OUT EFI_FILE_PROTOCOL **Root
    )

/*++

Routine Description:

    The OpenVolume() function opens a volume, and returns a file handle
    to the volume's root directory. This handle is used to perform all
    other file I/O operations. The volume remains open until all the
    file handles to it are closed.

Arguments:

    This - A pointer to the volume to open the root directory of.

    Root - A pointer to the location to return the opened file handle
        for the root directory.

Return Value:

    EFI_SUCCESS - The file volume was opened.

    EFI_UNSUPPORTED - The volume does not support the requested file system type.

    EFI_OUT_OF_RESOURCES - if there was an error allocating memory for any internal
        data structures.

--*/

{
    UINT32 bytesRead;
    PVMBFS_FILE allocatedFileProtocol = NULL;
    PFILE_INFORMATION fileInformation;
    volatile PFILESYSTEM_INFORMATION fileSystemInformation;
    EFI_FILE_SYSTEM_INFO *efiFileSystemInfo;
    EFI_STATUS status;
    VMBFS_MESSAGE_VERSION_REQUEST VersionRequestMessage;
    PVMBFS_MESSAGE_VERSION_RESPONSE VersionResponseMessage;
    BOOLEAN ChannelOpened = FALSE;

    ZeroMem(&VersionRequestMessage, sizeof(VMBFS_MESSAGE_VERSION_REQUEST));

    fileSystemInformation = GetThisFileSystemInformation(This);
    efiFileSystemInfo = GetThisEfiFileSystemInfo(This);

    if (fileSystemInformation->ReferenceCount > 0)
    {
        return EFI_SUCCESS;
    }

    //
    // Allocate and initialize datastructures.
    //
    status = gBS->CreateEvent(0,
                              0,
                              NULL,
                              NULL,
                              &fileSystemInformation->ReceivePacketEvent);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    InitializeSpinLock(&fileSystemInformation->VmbusIoLock);

    status = gBS->AllocatePool(EfiBootServicesData,
                               VMBFS_MAXIMUM_MESSAGE_SIZE,
                               &fileSystemInformation->PacketBuffer);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = gBS->AllocatePool(EfiBootServicesData, sizeof(*allocatedFileProtocol), &allocatedFileProtocol);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    ZeroMem(allocatedFileProtocol, sizeof(*allocatedFileProtocol));

    fileInformation = &allocatedFileProtocol->FileInformation;

    allocatedFileProtocol->EfiFileInfo = gVmbFsEfiFileInfoPrototype;
    allocatedFileProtocol->EfiFileInfo.FileName[0] = L'\0';

    fileInformation->IsDirectory = TRUE;

    //
    // Start the VMBus channel.
    //
    status = fileSystemInformation->EmclProtocol->SetReceiveCallback(
                 fileSystemInformation->EmclProtocol,
                 VmbfsReceivePacketCallback,
                 fileSystemInformation,
                 TPL_CALLBACK
                 );

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = fileSystemInformation->EmclProtocol->StartChannel(
                 fileSystemInformation->EmclProtocol,
                 EFI_SIZE_TO_PAGES(VMBFS_MAXIMUM_MESSAGE_SIZE) + 1,
                 EFI_SIZE_TO_PAGES(VMBFS_MAXIMUM_MESSAGE_SIZE) + 1
                 );

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    ChannelOpened = TRUE;

    //
    // Negotiate protocol with the host.
    //
    VersionRequestMessage.Header.Type = VmbfsMessageTypeVersionRequest;
    VersionRequestMessage.RequestedVersion = VMBFS_VERSION_WIN10;

    status = VmbfsSendReceivePacket(fileSystemInformation,
                                    &VersionRequestMessage,
                                    sizeof(VersionRequestMessage),
                                    0,
                                    NULL,
                                    0,
                                    FALSE);

    VersionResponseMessage = (PVMBFS_MESSAGE_VERSION_RESPONSE)fileSystemInformation->PacketBuffer;
    bytesRead = fileSystemInformation->PacketSize;

    if (bytesRead != sizeof(*VersionResponseMessage) ||
        VersionResponseMessage->Header.Type != VmbfsMessageTypeVersionResponse)
    {

        VMBFS_BAD_HOST;
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    if (VersionResponseMessage->Status != VmbfsVersionSupported)
    {
        status = EFI_DEVICE_ERROR;
        goto Cleanup;
    }

    allocatedFileProtocol->EfiFileProtocol = gVmbFsEfiFileProtocol;

    fileInformation->FileSystem = (PVMBFS_SIMPLE_FILE_SYSTEM_PROTOCOL)This;
    fileSystemInformation->ReferenceCount++;

    *efiFileSystemInfo = gVmbFsEfiFileSystemInfoPrototype;

    *Root = &allocatedFileProtocol->EfiFileProtocol;
    status = EFI_SUCCESS;

Cleanup:
    if (EFI_ERROR(status))
    {
        VmbfsCloseVolume((PVMBFS_SIMPLE_FILE_SYSTEM_PROTOCOL)This, ChannelOpened);

        if (allocatedFileProtocol != NULL)
        {
            gBS->FreePool(allocatedFileProtocol);
        }
    }

    return status;
}


