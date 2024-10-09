/** @file

  EFI simple file system protocol over vmbus.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Protocol/Emcl.h>
#include <Protocol/Vmbus.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/VmbusFileSystem.h>
#include <Guid/FileSystemInfo.h>
#include <Guid/FileInfo.h>

#include <Library/BaseMemoryLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/SynchronizationLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/EmclLib.h>


#define VMBFS_BAD_HOST ASSERT(FALSE)

#define GetPacketBuffer(fileInformation, Type) ((Type*)((fileInformation)->FileSystem->FileSystemInformation.PacketBuffer))
#define GetPacketSize(fileInformation) (((fileInformation)->FileSystem->FileSystemInformation.PacketSize))
#define GetFileSystemInformation(fileInformation) (&((fileInformation)->FileSystem->FileSystemInformation))
#define GetThisFileSystemInformation(SimpleFSProtocol) (&(((PVMBFS_SIMPLE_FILE_SYSTEM_PROTOCOL)(SimpleFSProtocol))->FileSystemInformation))
#define GetThisEfiFileSystemInfo(SimpleFSProtocol) (&(((PVMBFS_SIMPLE_FILE_SYSTEM_PROTOCOL)(SimpleFSProtocol))->EfiFileSystemInfo))

#define GetThisFileInformation(EfiFileProtocol) (&(((PVMBFS_FILE)(EfiFileProtocol))->FileInformation))
#define GetThisEfiFileInfo(EfiFileProtocol) (&(((PVMBFS_FILE)(EfiFileProtocol))->EfiFileInfo))

//
// Choose a maximum size that is known to fit in a VMBus pipe message.
//
#define VMBFS_MAXIMUM_RDMA_SIZE (7 * 1024 * 1024)

typedef struct _FILESYSTEM_INFORMATION {
    EFI_DEVICE_PATH_PROTOCOL *DevicePathProtocol;
    EFI_EMCL_PROTOCOL *EmclProtocol;
    INTN ReferenceCount;
    EFI_EVENT ReceivePacketEvent;
    UINT8 *PacketBuffer;
    UINT32 PacketSize;
    SPIN_LOCK VmbusIoLock;
} FILESYSTEM_INFORMATION, *PFILESYSTEM_INFORMATION;

typedef struct _VMBFS_SIMPLE_FILE_SYSTEM_PROTOCOL {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EfiSimpleFileSystemProtocol;
    FILESYSTEM_INFORMATION FileSystemInformation;
    EFI_FILE_SYSTEM_INFO EfiFileSystemInfo;
} VMBFS_SIMPLE_FILE_SYSTEM_PROTOCOL, *PVMBFS_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct _FILE_INFORMATION {
    BOOLEAN IsDirectory;
    BOOLEAN RdmaCapable;
    PVMBFS_SIMPLE_FILE_SYSTEM_PROTOCOL FileSystem;
    UINT64 FileOffset;
    UINTN FilePathLength;
} FILE_INFORMATION, *PFILE_INFORMATION;

typedef struct _VMBFS_FILE {
    EFI_FILE_PROTOCOL EfiFileProtocol;
    FILE_INFORMATION FileInformation;
    EFI_FILE_INFO EfiFileInfo;
} VMBFS_FILE, *PVMBFS_FILE;


extern const EFI_FILE_PROTOCOL gVmbFsEfiFileProtocol;
extern const EFI_SIMPLE_FILE_SYSTEM_PROTOCOL gVmbFsSimpleFileSystemProtocol;
extern const EFI_FILE_SYSTEM_INFO gVmbFsEfiFileSystemInfoPrototype;
extern const EFI_FILE_INFO gVmbFsEfiFileInfoPrototype;
UINTN gEventIndexDiscarded;

//
// Driver protocol implementation.
//
EFI_STATUS
EFIAPI
VmbfsStart (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    );

//
// File System protocol implementation.
//
EFI_STATUS
EFIAPI
VmbfsOpenVolume (
    IN  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
    OUT EFI_FILE_PROTOCOL **Root
    );

VOID
VmbfsCloseVolume (
    IN  PVMBFS_SIMPLE_FILE_SYSTEM_PROTOCOL VmbfsSimpleFileSystemProtocol,
    IN  BOOLEAN ChannelOpened
    );

//
// File protocol implementation.
//
VOID
VmbfsReceivePacketCallback (
    IN  VOID *ReceiveContext,
    IN  VOID *PacketContext,
    IN  VOID *Buffer,
    IN  UINT32 BufferLength,
    IN  UINT16 TransferPageSetId,
    IN  UINT32 RangeCount,
    IN  EFI_TRANSFER_RANGE *Ranges
    );

EFI_STATUS
VmbfsSendReceivePacket (
    IN  PFILESYSTEM_INFORMATION FileSystemInformation,
    IN  VOID *Buffer,
    IN  UINTN BufferLength,
    IN  UINT32 GpaRangeHandle,
    IN  VOID *ExternalBuffer,
    IN  UINTN ExternalBufferLength,
    IN  BOOLEAN IsWritable
    );

EFI_STATUS
EFIAPI
VmbfsOpen (
    IN  EFI_FILE_PROTOCOL *This,
    OUT EFI_FILE_PROTOCOL **NewHandle,
    IN  CHAR16 *FileName,
    IN  UINT64 OpenMode,
    IN  UINT64 Attributes
    );

EFI_STATUS
EFIAPI
VmbfsClose (
    IN  EFI_FILE_PROTOCOL *This
    );

EFI_STATUS
EFIAPI
VmbfsRead (
    IN      EFI_FILE_PROTOCOL *This,
    IN OUT  UINTN *BufferSize,
    OUT     VOID *Buffer
    );

EFI_STATUS
EFIAPI
VmbfsWrite (
    IN      EFI_FILE_PROTOCOL *This,
    IN OUT  UINTN *BufferSize,
    IN      VOID *Buffer
    );

EFI_STATUS
EFIAPI
VmbfsGetPosition (
    IN  EFI_FILE_PROTOCOL *This,
    OUT UINT64 *Position
    );

EFI_STATUS
EFIAPI
VmbfsSetPosition (
    IN  EFI_FILE_PROTOCOL *This,
    IN  UINT64 Position
    );

EFI_STATUS
EFIAPI
VmbfsGetInfo (
    IN      EFI_FILE_PROTOCOL *This,
    IN      EFI_GUID *InformationType,
    IN OUT  UINTN *BufferSize,
    OUT     VOID *Buffer
    );

EFI_STATUS
EFIAPI
VmbfsSetInfo (
    IN      EFI_FILE_PROTOCOL *This,
    IN      EFI_GUID *InformationType,
    IN OUT  UINTN *BufferSize,
    IN      VOID *Buffer
    );

EFI_STATUS
EFIAPI
VmbfsFlush (
  IN  EFI_FILE_PROTOCOL *This
  );

EFI_STATUS
EFIAPI
VmbfsDelete (
  IN  EFI_FILE_PROTOCOL *This
  );

