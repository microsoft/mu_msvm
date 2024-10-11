/** @file
  Header definitions for the VPCI VSC UEFI driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

#include <Uefi.h>
#include <PiDxe.h>
#include <Library/CrashLib.h>

#include <Protocol/Emcl.h>
#include <Protocol/ComponentName.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/DevicePath.h>
#include <Protocol/PciIo.h>
#include <Protocol/Vmbus.h>

#include "VpciProtocol.h"
#include "PciBars.h"

typedef struct _VPCIVSC_CONTEXT VPCIVSC_CONTEXT;

typedef struct _VPCI_BAR_INFORMATION
{
    UINT64 MappedAddress; // The address where this bar was allocated and mapped
    UINT64 Size; // The size of this bar
    BOOLEAN Is64Bit; // The type of bar, 32 bit or 64 bit
    UINT8 BarIndex; // The index into the RawBars array for where this bar starts
} VPCI_BAR_INFORMATION, *PVPCI_BAR_INFORMATION;

// Context structure for each VPCI device
typedef struct _VPCI_DEVICE_CONTEXT
{
    UINTN Signature;

    // PCI Io Protocol for this device
    EFI_PCI_IO_PROTOCOL PciIo;

    EFI_HANDLE Handle;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;

    // The raw bar information returned from the VSP
    PCI_BAR_FORMAT RawBars[PCI_MAX_BAR];
    VPCI_BAR_INFORMATION MappedBars[PCI_MAX_BAR];

    VPCIVSC_CONTEXT *VpcivscContext;
    PCI_SLOT_NUMBER Slot;

} VPCI_DEVICE_CONTEXT, *PVPCI_DEVICE_CONTEXT;

#define VPCI_DEVICE_CONTEXT_SIGNATURE SIGNATURE_32('v', 'p', 'c', 'd')

#define VPCI_DEVICE_CONTEXT_FROM_PCI_IO(a) \
    CR(a, \
       VPCI_DEVICE_CONTEXT, \
       PciIo, \
       VPCI_DEVICE_CONTEXT_SIGNATURE \
       )

// Context structure for each VPCI channel offer
typedef struct _VPCIVSC_CONTEXT
{
    UINTN Signature;
    EFI_HANDLE Handle;

    EFI_EMCL_V2_PROTOCOL *Emcl;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;

    EFI_EVENT WaitForBusRelationsMessage;
    VPCI_DEVICE_DESCRIPTION *Devices;
    UINT32 DeviceCount;

    // We only really care about NVMe devices.
    VPCI_DEVICE_CONTEXT *NvmeDevices;
    UINT32 NvmeDeviceCount;
} VPCIVSC_CONTEXT, *PVPCIVSC_CONTEXT;

#define VPCIVSC_DRIVER_VERSION 0x1
#define VPCIVSC_CONTEXT_SIGNATURE SIGNATURE_32('v','p','c','i')

#define VPCIVSC_CONTEXT_FROM_EMCL(a) \
    CR(a, \
       VPCIVSC_CONTEXT, \
       Emcl, \
       VPCIVSC_CONTEXT_SIGNATURE \
       )

#define TPL_VPCIVSC_CALLBACK TPL_CALLBACK
#define DEBUG_VPCI_INFO DEBUG_INFO

#define VPCIVSC_WAIT_FOR_HOST_TIMEOUT  EFI_TIMER_PERIOD_SECONDS(60)

extern EFI_DRIVER_BINDING_PROTOCOL gVpcivscDriverBinding;
extern EFI_COMPONENT_NAME_PROTOCOL gVpcivscComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL gVpcivscComponentName2;

EFI_STATUS
EFIAPI
VpcivscDriverBindingSupported (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    );

EFI_STATUS
EFIAPI
VpcivscDriverBindingStop (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  UINTN NumberOfChildren,
    IN  EFI_HANDLE *ChildHandleBuffer
    );

EFI_STATUS
EFIAPI
VpcivscDriverBindingStart (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    );

EFI_STATUS
EFIAPI
VpcivscComponentNameGetDriverName(
    IN  EFI_COMPONENT_NAME_PROTOCOL *This,
    IN  CHAR8 *Language,
    OUT CHAR16 **DriverName
    );

EFI_STATUS
EFIAPI
VpcivscComponentNameGetControllerName(
    IN  EFI_COMPONENT_NAME_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_HANDLE ChildHandle OPTIONAL,
    IN  CHAR8 *Language,
    OUT CHAR16 **ControllerName
    );

// EMCL functions, VSC protocol functions
VOID
VpciChannelReceivePacketCallback(
    IN  VOID *ReceiveContext,
    IN  VOID *PacketContext,
    IN  VOID *Buffer,
    IN  UINT32 BufferLength,
    IN  UINT16 TransferPageSetId,
    IN  UINT32 RangeCount,
    IN  EFI_TRANSFER_RANGE *Ranges
    );

// PciIo Protocol functions
EFI_STATUS
EFIAPI
VpcivscPciIoPollMem(
    IN  EFI_PCI_IO_PROTOCOL          *This,
    IN  EFI_PCI_IO_PROTOCOL_WIDTH    Width,
    IN  UINT8                        BarIndex,
    IN  UINT64                       Offset,
    IN  UINT64                       Mask,
    IN  UINT64                       Value,
    IN  UINT64                       Delay,
    OUT UINT64                       *Result
    );

EFI_STATUS
EFIAPI
VpcivscPciIoPollIo(
    IN  EFI_PCI_IO_PROTOCOL        *This,
    IN  EFI_PCI_IO_PROTOCOL_WIDTH  Width,
    IN  UINT8                      BarIndex,
    IN  UINT64                     Offset,
    IN  UINT64                     Mask,
    IN  UINT64                     Value,
    IN  UINT64                     Delay,
    OUT UINT64                     *Result
    );

EFI_STATUS
EFIAPI
VpcivscPciIoMemRead(
    IN      EFI_PCI_IO_PROTOCOL        *This,
    IN      EFI_PCI_IO_PROTOCOL_WIDTH  Width,
    IN      UINT8                      BarIndex,
    IN      UINT64                     Offset,
    IN      UINTN                      Count,
    IN OUT  VOID                       *Buffer
    );

EFI_STATUS
EFIAPI
VpcivscPciIoMemWrite(
    IN      EFI_PCI_IO_PROTOCOL        *This,
    IN      EFI_PCI_IO_PROTOCOL_WIDTH  Width,
    IN      UINT8                      BarIndex,
    IN      UINT64                     Offset,
    IN      UINTN                      Count,
    IN OUT  VOID                       *Buffer
    );

EFI_STATUS
EFIAPI
VpcivscPciIoIoRead(
    IN      EFI_PCI_IO_PROTOCOL        *This,
    IN      EFI_PCI_IO_PROTOCOL_WIDTH  Width,
    IN      UINT8                      BarIndex,
    IN      UINT64                     Offset,
    IN      UINTN                      Count,
    IN OUT  VOID                       *Buffer
    );

EFI_STATUS
EFIAPI
VpcivscPciIoIoWrite(
    IN      EFI_PCI_IO_PROTOCOL        *This,
    IN      EFI_PCI_IO_PROTOCOL_WIDTH  Width,
    IN      UINT8                      BarIndex,
    IN      UINT64                     Offset,
    IN      UINTN                      Count,
    IN OUT  VOID                       *Buffer
    );

EFI_STATUS
EFIAPI
VpcivscPciIoConfigRead(
    IN      EFI_PCI_IO_PROTOCOL        *This,
    IN      EFI_PCI_IO_PROTOCOL_WIDTH  Width,
    IN      UINT32                     Offset,
    IN      UINTN                      Count,
    IN OUT  VOID                       *Buffer
    );

EFI_STATUS
EFIAPI
VpcivscPciIoConfigWrite(
    IN      EFI_PCI_IO_PROTOCOL        *This,
    IN      EFI_PCI_IO_PROTOCOL_WIDTH  Width,
    IN      UINT32                     Offset,
    IN      UINTN                      Count,
    IN OUT  VOID                       *Buffer
    );

EFI_STATUS
EFIAPI
VpcivscPciIoCopyMem(
    IN  EFI_PCI_IO_PROTOCOL         *This,
    IN  EFI_PCI_IO_PROTOCOL_WIDTH   Width,
    IN  UINT8                       DestBarIndex,
    IN  UINT64                      DestOffset,
    IN  UINT8                       SrcBarIndex,
    IN  UINT64                      SrcOffset,
    IN  UINTN                       Count
    );

EFI_STATUS
EFIAPI
VpcivscPciIoMap(
    IN      EFI_PCI_IO_PROTOCOL            *This,
    IN      EFI_PCI_IO_PROTOCOL_OPERATION  Operation,
    IN      VOID                           *HostAddress,
    IN OUT  UINTN                          *NumberOfBytes,
    OUT     EFI_PHYSICAL_ADDRESS           *DeviceAddress,
    OUT     VOID                           **Mapping
    );

EFI_STATUS
EFIAPI
VpcivscPciIoUnmap(
    IN  EFI_PCI_IO_PROTOCOL     *This,
    IN  VOID                    *Mapping
    );

EFI_STATUS
EFIAPI
VpcivscPciIoAllocateBuffer(
    IN  EFI_PCI_IO_PROTOCOL     *This,
    IN  EFI_ALLOCATE_TYPE       Type,
    IN  EFI_MEMORY_TYPE         MemoryType,
    IN  UINTN                   Pages,
    OUT VOID                    **HostAddress,
    IN  UINT64                  Attributes
    );

EFI_STATUS
EFIAPI
VpcivscPciIoFreeBuffer(
    IN  EFI_PCI_IO_PROTOCOL     *This,
    IN  UINTN                   Pages,
    IN  VOID                    *HostAddress
    );

EFI_STATUS
EFIAPI
VpcivscPciIoFlush(
    IN  EFI_PCI_IO_PROTOCOL     *This
    );

EFI_STATUS
EFIAPI
VpcivscPciIoGetLocation(
    IN  EFI_PCI_IO_PROTOCOL     *This,
    OUT UINTN                   *Segment,
    OUT UINTN                   *Bus,
    OUT UINTN                   *Device,
    OUT UINTN                   *Function
    );

EFI_STATUS
EFIAPI
VpcivscPciIoAttributes(
    IN  EFI_PCI_IO_PROTOCOL                     *This,
    IN  EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION Operation,
    IN  UINT64                                  Attributes,
    OUT UINT64                                  *Result OPTIONAL
    );

EFI_STATUS
EFIAPI
VpcivscPciIoGetBarAttributes(
    IN  EFI_PCI_IO_PROTOCOL         * This,
    IN  UINT8                       BarIndex,
    OUT UINT64                      *Supports, OPTIONAL
    OUT VOID                        **Resources OPTIONAL
    );

EFI_STATUS
EFIAPI
VpcivscPciIoSetBarAttributes(
    IN      EFI_PCI_IO_PROTOCOL     *This,
    IN      UINT64                  Attributes,
    IN      UINT8                   BarIndex,
    IN OUT  UINT64                  *Offset,
    IN OUT  UINT64                  *Length
    );