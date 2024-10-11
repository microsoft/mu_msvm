/** @file
  PciIo protocol implementation for the UEFI VPCI VSC, used by child drivers.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include "VpcivscDxe.h"

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>

#include <IndustryStandard/Pci.h>

const UINT16 DEFAULT_PCI_VENDOR_ID              = 0x1414; // Microsoft
const UINT16 DEFAULT_PCI_DEVICE_ID              = 0xb111;

/// \brief      PciIo Protocol Poll mem. Unimplemented.
///
/// \param      This      The this
/// \param[in]  Width     The width
/// \param[in]  BarIndex  The bar index
/// \param[in]  Offset    The offset
/// \param[in]  Mask      The mask
/// \param[in]  Value     The value
/// \param[in]  Delay     The delay
/// \param      Result    The result
///
/// \return     EFI_STATUS
///
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
    )
{
    ASSERT(FALSE);
    return EFI_DEVICE_ERROR;
}

/// \brief      PciIoProtocol Poll Io. Unimplemented.
///
/// \param      This      The this
/// \param[in]  Width     The width
/// \param[in]  BarIndex  The bar index
/// \param[in]  Offset    The offset
/// \param[in]  Mask      The mask
/// \param[in]  Value     The value
/// \param[in]  Delay     The delay
/// \param      Result    The result
///
/// \return     EFI_STATUS.
///
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
    )
{
    ASSERT(FALSE);
    return EFI_DEVICE_ERROR;
}

/// \brief      Return how big a PciIo protocol width is, in bytes.
///
/// \param[in]  Width  The width to decode
///
/// \return     The size in bytes.
///
UINT64
DecodePciIoProtocolWidth(
    EFI_PCI_IO_PROTOCOL_WIDTH Width
    )
{
    UINT64 size = 0;

    // The NVMe driver does not use any of the Fill/Fifo types. Unknown what
    // they mean, probably a specific pattern for accessing memory/io space but
    // probably doesn't matter in a VM.
    switch (Width)
    {
        case EfiPciIoWidthFillUint8:
        case EfiPciIoWidthFifoUint8:
            ASSERT(FALSE);
            //fallthru
        case EfiPciIoWidthUint8:
            size = 1;
            break;

        case EfiPciIoWidthFifoUint16:
        case EfiPciIoWidthFillUint16:
            ASSERT(FALSE);
            //fallthru
        case EfiPciIoWidthUint16:
            size = 2;
            break;

        case EfiPciIoWidthFifoUint32:
        case EfiPciIoWidthFillUint32:
            ASSERT(FALSE);
            //fallthru
        case EfiPciIoWidthUint32:
            size = 4;
            break;

        case EfiPciIoWidthFifoUint64:
        case EfiPciIoWidthFillUint64:
            ASSERT(FALSE);
            //fallthru
        case EfiPciIoWidthUint64:
            size = 8;
            break;

        default:
            ASSERT(FALSE);
            break;
    }

    return size;
}

// \brief      Validate that a given bar is mapped and valid to access
//
// \param[in]  Context   The device context
// \param[in]  Width     The access width
// \param[in]  BarIndex  The bar index, must not be out of bounds.
// \param[in]  Offset    The offset in the bar
// \param[in]  Count     The count of times to access the bar with the given
//                       width
//
// \return     TRUE if a valid access, FALSE otherwise
//
BOOLEAN
VpcivscValidateBarAccess(
    PVPCI_DEVICE_CONTEXT Context,
    EFI_PCI_IO_PROTOCOL_WIDTH Width,
    UINT8 BarIndex,
    UINT64 Offset,
    UINTN Count
    )
{
    ASSERT(BarIndex < PCI_MAX_BAR);

    // Check how big of a region we're accessing. Width * Count gives us that.
    UINT64 totalSize = Count * DecodePciIoProtocolWidth(Width) + Offset;

    // If we overflowed, can't possibly be valid.
    if (totalSize < Offset)
    {
        return FALSE;
    }

    // Check the size of access is less than the total size.
    return (totalSize <= Context->MappedBars[BarIndex].Size);
}

  /// \brief      Enable a PCI driver to read from PCI controller registers in PCI
  ///             memory. Done by accessing the mapped MMIO bar.
  ///
  /// NOTE: For VPCI, we only support MMIO space.
  ///
  /// \param      This                   A pointer to the EFI_PCI_IO_PROTOCOL
  ///                                    instance.
  /// \param      Width                  Signifies the width of the memory or
  ///                                    I/O operations.
  /// \param      BarIndex               The BAR index of the standard PCI
  ///                                    Configuration header to use as the base
  ///                                    address for the memory or I/O operation
  ///                                    to perform.
  /// \param      Offset                 The offset within the selected BAR to
  ///                                    start the memory or I/O operation.
  /// \param      Count                  The number of memory or I/O operations
  ///                                    to perform.
  /// \param      Buffer                 For read operations, the destination
  ///                                    buffer to store the results. For write
  ///                                    operations, the source buffer to write
  ///                                    data from.
  /// \retval     EFI_SUCCESS            The data was read from or written to
  ///                                    the PCI controller.
  /// \retval     EFI_UNSUPPORTED        BarIndex not valid for this PCI
  ///                                    controller.
  /// \retval     EFI_UNSUPPORTED        The address range specified by Offset,
  ///                                    Width, and Count is not valid for the
  ///                                    PCI BAR specified by BarIndex.
  /// \retval     EFI_OUT_OF_RESOURCES   The request could not be completed due
  ///                                    to a lack of resources.
  /// \retval     EFI_INVALID_PARAMETER  One or more parameters are invalid.
  ///
  /// \return     { description_of_the_return_value }
  ///
EFI_STATUS
EFIAPI
VpcivscPciIoMemRead(
    IN      EFI_PCI_IO_PROTOCOL        *This,
    IN      EFI_PCI_IO_PROTOCOL_WIDTH  Width,
    IN      UINT8                      BarIndex,
    IN      UINT64                     Offset,
    IN      UINTN                      Count,
    IN OUT  VOID                       *Buffer
    )
{
    PVPCI_DEVICE_CONTEXT context = NULL;

    // DEBUG((DEBUG_VPCI_INFO, "VpcivscPciIoMemRead called with width 0x%x, bar index 0x%x, offset %x, count %x\n",
    //     Width,
    //     BarIndex,
    //     Offset,
    //     Count));

    if (BarIndex >= PCI_MAX_BAR)
    {
        return EFI_INVALID_PARAMETER;
    }

    context = VPCI_DEVICE_CONTEXT_FROM_PCI_IO(This);

    // Read from mapped config space. Bar index tells us which bar to read from, first check if the read is valid.
    if (!VpcivscValidateBarAccess(context, Width, BarIndex, Offset, Count))
    {
        return EFI_UNSUPPORTED;
    }

    // Do the mmio read, starting at the specified address
    UINTN startAddress = context->MappedBars[BarIndex].MappedAddress + Offset;
    switch (Width)
    {
        case EfiPciIoWidthFillUint8:
        case EfiPciIoWidthFifoUint8:
            ASSERT(FALSE);
            //fallthru
        case EfiPciIoWidthUint8:
            MmioReadBuffer8(startAddress, Count, Buffer);
            break;

        case EfiPciIoWidthFifoUint16:
        case EfiPciIoWidthFillUint16:
            ASSERT(FALSE);
            //fallthru
        case EfiPciIoWidthUint16:
            MmioReadBuffer16(startAddress, Count * 2, Buffer);
            break;

        case EfiPciIoWidthFifoUint32:
        case EfiPciIoWidthFillUint32:
            ASSERT(FALSE);
            //fallthru
        case EfiPciIoWidthUint32:
            MmioReadBuffer32(startAddress, Count * 4, Buffer);
            break;

        case EfiPciIoWidthFifoUint64:
        case EfiPciIoWidthFillUint64:
            ASSERT(FALSE);
            //fallthru
        case EfiPciIoWidthUint64:
            MmioReadBuffer64(startAddress, Count * 4, Buffer);
            break;

        default:
            ASSERT(FALSE);
            return EFI_DEVICE_ERROR;
    }

    return EFI_SUCCESS;
}

  /// \brief      Enable a PCI driver to write to PCI controller registers in PCI
  ///             memory. Done by accessing the mapped MMIO bar.
  ///
  /// NOTE: For VPCI, we only support MMIO space.
  ///
  /// \param      This                   A pointer to the EFI_PCI_IO_PROTOCOL
  ///                                    instance.
  /// \param      Width                  Signifies the width of the memory or
  ///                                    I/O operations.
  /// \param      BarIndex               The BAR index of the standard PCI
  ///                                    Configuration header to use as the base
  ///                                    address for the memory or I/O operation
  ///                                    to perform.
  /// \param      Offset                 The offset within the selected BAR to
  ///                                    start the memory or I/O operation.
  /// \param      Count                  The number of memory or I/O operations
  ///                                    to perform.
  /// \param      Buffer                 For read operations, the destination
  ///                                    buffer to store the results. For write
  ///                                    operations, the source buffer to write
  ///                                    data from.
  /// \retval     EFI_SUCCESS            The data was read from or written to
  ///                                    the PCI controller.
  /// \retval     EFI_UNSUPPORTED        BarIndex not valid for this PCI
  ///                                    controller.
  /// \retval     EFI_UNSUPPORTED        The address range specified by Offset,
  ///                                    Width, and Count is not valid for the
  ///                                    PCI BAR specified by BarIndex.
  /// \retval     EFI_OUT_OF_RESOURCES   The request could not be completed due
  ///                                    to a lack of resources.
  /// \retval     EFI_INVALID_PARAMETER  One or more parameters are invalid.
  ///
EFI_STATUS
EFIAPI
VpcivscPciIoMemWrite(
    IN      EFI_PCI_IO_PROTOCOL        *This,
    IN      EFI_PCI_IO_PROTOCOL_WIDTH  Width,
    IN      UINT8                      BarIndex,
    IN      UINT64                     Offset,
    IN      UINTN                      Count,
    IN OUT  VOID                       *Buffer
    )
{
    PVPCI_DEVICE_CONTEXT context = NULL;

    // DEBUG((DEBUG_VPCI_INFO, "VpcivscPciIoMemWrite called with width 0x%x, bar index 0x%x, offset %x, count %x\n",
    //     Width,
    //     BarIndex,
    //     Offset,
    //     Count));

    if (BarIndex >= PCI_MAX_BAR)
    {
        return EFI_INVALID_PARAMETER;
    }

    context = VPCI_DEVICE_CONTEXT_FROM_PCI_IO(This);

    // Read from mapped config space. Bar index tells us which bar to read from, first check if the read is valid.
    if (!VpcivscValidateBarAccess(context, Width, BarIndex, Offset, Count))
    {
        return EFI_UNSUPPORTED;
    }

    // Do the read, width tells us what access size to use for mmio, count how many times.
    UINTN startAddress = context->MappedBars[BarIndex].MappedAddress + Offset;
    switch (Width)
    {
        case EfiPciIoWidthFillUint8:
        case EfiPciIoWidthFifoUint8:
            ASSERT(FALSE);
            //fallthru
        case EfiPciIoWidthUint8:
            MmioWriteBuffer8(startAddress, Count, Buffer);
            break;

        case EfiPciIoWidthFifoUint16:
        case EfiPciIoWidthFillUint16:
            ASSERT(FALSE);
            //fallthru
        case EfiPciIoWidthUint16:
            MmioWriteBuffer16(startAddress, Count * 2, Buffer);
            break;

        case EfiPciIoWidthFifoUint32:
        case EfiPciIoWidthFillUint32:
            ASSERT(FALSE);
            //fallthru
        case EfiPciIoWidthUint32:
            MmioWriteBuffer32(startAddress, Count * 4, Buffer);
            break;

        case EfiPciIoWidthFifoUint64:
        case EfiPciIoWidthFillUint64:
            ASSERT(FALSE);
            //fallthru
        case EfiPciIoWidthUint64:
            MmioWriteBuffer64(startAddress, Count * 4, Buffer);
            break;

        default:
            ASSERT(FALSE);
            return EFI_DEVICE_ERROR;
    }

    return EFI_SUCCESS;
}


/// \brief      Read from an IO space PCI config register. Not supported for
///             VPCI devices.
///
/// \return     EFI_DEVICE_ERROR
///
EFI_STATUS
EFIAPI
VpcivscPciIoIoRead(
    IN      EFI_PCI_IO_PROTOCOL        *This,
    IN      EFI_PCI_IO_PROTOCOL_WIDTH  Width,
    IN      UINT8                      BarIndex,
    IN      UINT64                     Offset,
    IN      UINTN                      Count,
    IN OUT  VOID                       *Buffer
    )
{
    ASSERT(FALSE);
    return EFI_DEVICE_ERROR;
}

/// \brief      Write to an IO space PCI config register. Not supported for
///             VPCI devices.
///
/// \return     EFI_DEVICE_ERROR
///
EFI_STATUS
EFIAPI
VpcivscPciIoIoWrite(
    IN      EFI_PCI_IO_PROTOCOL        *This,
    IN      EFI_PCI_IO_PROTOCOL_WIDTH  Width,
    IN      UINT8                      BarIndex,
    IN      UINT64                     Offset,
    IN      UINTN                      Count,
    IN OUT  VOID                       *Buffer
    )
{
    ASSERT(FALSE);
    return EFI_DEVICE_ERROR;
}

/// \brief      Read from PCI config space. For VPCI, the only usage of this is
///             by the NVMe driver to check device IDs. Since the only device we
///             support is NVMe, we hardcode the NVMe device identifiers.
///
/// \param      This    A pointer to the EFI_PCI_IO_PROTOCOL instance.
/// \param[in]  Width   Signifies the width of the memory or I/O operations.
/// \param[in]  Offset  The offset in config space to start the read.
/// \param[in]  Count   The count of memory operations to perform.
/// \param      Buffer  The buffer to return the read value.
///
/// \return     EFI_STATUS
///
EFI_STATUS
EFIAPI
VpcivscPciIoConfigRead(
    IN      EFI_PCI_IO_PROTOCOL        *This,
    IN      EFI_PCI_IO_PROTOCOL_WIDTH  Width,
    IN      UINT32                     Offset,
    IN      UINTN                      Count,
    IN OUT  VOID                       *Buffer
    )
{
    PVPCI_DEVICE_CONTEXT context = NULL;

    DEBUG((DEBUG_VPCI_INFO, "VpcivscPciIoConfigRead called with offset 0x%x and count 0x%x\n", Offset, Count));

    if (Buffer == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    context = VPCI_DEVICE_CONTEXT_FROM_PCI_IO(This);

    // TODO:     The device would need to have a VPCI_DEVICE_DESCRIPTION for if
    //           we ever want to support more than just NVMe. But we don't,
    //           so just return the spec values for an NVMe device.

    switch (Offset)
    {
        case PCI_CLASSCODE_OFFSET:
            if (!((Count == 3) && (Width == EfiPciIoWidthUint8)))
            {
                ASSERT(FALSE);
                return EFI_DEVICE_ERROR;
            }

            UINT8* classCode = (UINT8*) Buffer;
            classCode[0] = 0x2; //ProgIf
            classCode[1] = 0x8; //SubClass
            classCode[2] = 0x1; //BaseClass
            break;
        case PCI_VENDOR_ID_OFFSET:
            // PCI_VENDOR_ID_OFFSET and PCI_DEVICE_ID_OFFSET can be read together with a count of 2 at offset PCI_VENDOR_ID_OFFSET
            if (!(((Count == 1) || (Count == 2)) && (Width == EfiPciIoWidthUint16)))
            {
                ASSERT(FALSE);
                return EFI_DEVICE_ERROR;
            }

            UINT16* id = (UINT16*) Buffer;
            id[0] = DEFAULT_PCI_VENDOR_ID;
            if (Count == 2)
            {
                // Read the PCI_DEVICE_ID_OFFSET too
                id[1] = DEFAULT_PCI_DEVICE_ID;
            }
            break;
        case PCI_DEVICE_ID_OFFSET:
            if (!((Count == 1) && (Width == EfiPciIoWidthUint16)))
            {
                ASSERT(FALSE);
                return EFI_DEVICE_ERROR;
            }
            UINT16* deviceId = (UINT16*) Buffer;
            *deviceId = DEFAULT_PCI_DEVICE_ID;
            break;
        default:
            ASSERT(FALSE);
            return EFI_DEVICE_ERROR;

    }

    return EFI_SUCCESS;
}

/// \brief      Write to PCI config space. Unimplemented.
///
/// \return     EFI_DEVICE_ERROR
///
EFI_STATUS
EFIAPI
VpcivscPciIoConfigWrite(
    IN      EFI_PCI_IO_PROTOCOL        *This,
    IN      EFI_PCI_IO_PROTOCOL_WIDTH  Width,
    IN      UINT32                     Offset,
    IN      UINTN                      Count,
    IN OUT  VOID                       *Buffer
    )
{
    ASSERT(FALSE);
    return EFI_DEVICE_ERROR;
}


/// \brief      Copy memory in PCI config space. Unimplemented.
///
/// \return     EFI_DEVICE_ERROR
///
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
    )
{
    ASSERT(FALSE);
    return EFI_DEVICE_ERROR;
}

/// \brief      Provides the PCI controller-specific addresses needed to access
///             system memory. For VPCI, this is a noop.
///
/// \param      This                   A pointer to the EFI_PCI_IO_PROTOCOL
///                                    instance.
/// \param      Operation              Indicates if the bus master is going to
///                                    read or write to system memory.
/// \param      HostAddress            The system memory address to map to the
///                                    PCI controller.
/// \param      NumberOfBytes          On input the number of bytes to map. On
///                                    output the number of bytes that were
///                                    mapped.
/// \param      DeviceAddress          The resulting map address for the bus
///                                    master PCI controller to use to access
///                                    the hosts HostAddress.
/// \param      Mapping                A resulting value to pass to Unmap().
///
/// \return     EFI_SUCCESS
///
EFI_STATUS
EFIAPI
VpcivscPciIoMap(
    IN      EFI_PCI_IO_PROTOCOL            *This,
    IN      EFI_PCI_IO_PROTOCOL_OPERATION  Operation,
    IN      VOID                           *HostAddress,
    IN OUT  UINTN                          *NumberOfBytes,
    OUT     EFI_PHYSICAL_ADDRESS           *DeviceAddress,
    OUT     VOID                           **Mapping
    )
{
    // For VPCI, the VSC has DMA access to all pages. So nothing to do here,
    // just return the address of the buffer.

    *DeviceAddress = (UINTN) HostAddress;
    *Mapping = NULL;

    return EFI_SUCCESS;
}


/// \brief      Unmap the buffer mapped by the corresponding call to PciIoMap.
///             For VPCI, also a noop.
///
/// \param      This     A pointer to the EFI_PCI_IO_PROTOCOL instance.
/// \param      Mapping  The mapping to undo.
///
/// \return     EFI_SUCCESS
///
EFI_STATUS
EFIAPI
VpcivscPciIoUnmap(
    IN  EFI_PCI_IO_PROTOCOL  *This,
    IN  VOID                 *Mapping
    )
{
    // DEBUG((DEBUG_VPCI_INFO, "VpcivscPciIoUnmap called with mapping %llx\n", Mapping));

    ASSERT(Mapping == NULL);

    return EFI_SUCCESS;
}

/**
  Allocates pages that are suitable for an EfiPciIoOperationBusMasterCommonBuffer
  or EfiPciOperationBusMasterCommonBuffer64 mapping.

  @param  This                  A pointer to the EFI_PCI_IO_PROTOCOL instance.
  @param  Type                  This parameter is not used and must be ignored.
  @param  MemoryType            The type of memory to allocate, EfiBootServicesData or
                                EfiRuntimeServicesData.
  @param  Pages                 The number of pages to allocate.
  @param  HostAddress           A pointer to store the base system memory address of the
                                allocated range.
  @param  Attributes            The requested bit mask of attributes for the allocated range.

  @retval EFI_SUCCESS           The requested memory pages were allocated.
  @retval EFI_UNSUPPORTED       Attributes is unsupported. The only legal attribute bits are
                                MEMORY_WRITE_COMBINE, MEMORY_CACHED and DUAL_ADDRESS_CYCLE.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The memory pages could not be allocated.

**/
EFI_STATUS
EFIAPI
VpcivscPciIoAllocateBuffer(
    IN  EFI_PCI_IO_PROTOCOL   *This,
    IN  EFI_ALLOCATE_TYPE     Type,
    IN  EFI_MEMORY_TYPE       MemoryType,
    IN  UINTN                 Pages,
    OUT VOID                  **HostAddress,
    IN  UINT64                Attributes
    )
{
    VOID* buffer = NULL;

    DEBUG((DEBUG_VPCI_INFO, "VpcivscPciIoAllocateBuffer called with pages %x\n", Pages));

    // For VPCI, the VSC has DMA access to all pages. So nothing special here,
    // just allocate memory like normal.

    // NVMe dxe driver doesn't use attributes.
    if (Attributes != 0)
    {
        ASSERT(FALSE);
        return EFI_UNSUPPORTED;
    }

    if (MemoryType != EfiBootServicesData)
    {
        ASSERT(FALSE);
        return EFI_UNSUPPORTED;
    }

    buffer = AllocatePages(Pages);

    if (buffer == NULL)
    {
        return EFI_OUT_OF_RESOURCES;
    }

    // The host address is just the buffer address. No special mapping.
    *HostAddress = buffer;

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
VpcivscPciIoFreeBuffer(
    IN  EFI_PCI_IO_PROTOCOL   *This,
    IN  UINTN                 Pages,
    IN  VOID                  *HostAddress
    )
{
    DEBUG((DEBUG_VPCI_INFO, "VpcivscPciIoFreeBuffer called with addr %llx pages %x\n", HostAddress, Pages));

    // Free the buffer allocated with AllocatePages
    // FreePages(HostAddress, Pages);
    // NOTE: To workaround a bug with ND2 on the host registering write
    // notifications for pages resulting in the VM hanging, do not put
    // pages used for NVMe queues back onto the free list. Instead,
    // leak these pages as they will be reclaimed later at ExitBootServices.

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
VpcivscPciIoFlush(
    IN  EFI_PCI_IO_PROTOCOL  *This
    )
{
    ASSERT(FALSE);
    return EFI_DEVICE_ERROR;
}

EFI_STATUS
EFIAPI
VpcivscPciIoGetLocation(
    IN  EFI_PCI_IO_PROTOCOL  *This,
    OUT UINTN                *Segment,
    OUT UINTN                *Bus,
    OUT UINTN                *Device,
    OUT UINTN                *Function
    )
{
    ASSERT(FALSE);
    return EFI_DEVICE_ERROR;
}

/**
  Performs an operation on the attributes that this PCI controller supports. The operations include
  getting the set of supported attributes, retrieving the current attributes, setting the current
  attributes, enabling attributes, and disabling attributes.

  @param  This                  A pointer to the EFI_PCI_IO_PROTOCOL instance.
  @param  Operation             The operation to perform on the attributes for this PCI controller.
  @param  Attributes            The mask of attributes that are used for Set, Enable, and Disable
                                operations.
  @param  Result                A pointer to the result mask of attributes that are returned for the Get
                                and Supported operations.

  @retval EFI_SUCCESS           The operation on the PCI controller's attributes was completed.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_UNSUPPORTED       one or more of the bits set in
                                Attributes are not supported by this PCI controller or one of
                                its parent bridges when Operation is Set, Enable or Disable.

**/
EFI_STATUS
EFIAPI
VpcivscPciIoAttributes(
    IN  EFI_PCI_IO_PROTOCOL                       * This,
    IN  EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION  Operation,
    IN  UINT64                                   Attributes,
    OUT UINT64                                   *Result OPTIONAL
    )
{
    DEBUG((DEBUG_VPCI_INFO, "VpcivscPciIoAttributes called\n"));

    // These are meaningless for VPCI. Our VSC pretends to be a Windows VSC, so
    // the PdoD0Entry packet handles the correct bus enable on the host side.

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
VpcivscPciIoGetBarAttributes(
    IN  EFI_PCI_IO_PROTOCOL             * This,
    IN  UINT8                          BarIndex,
    OUT UINT64                         *Supports, OPTIONAL
    OUT VOID                           **Resources OPTIONAL
    )
{
    ASSERT(FALSE);
    return EFI_DEVICE_ERROR;
}

EFI_STATUS
EFIAPI
VpcivscPciIoSetBarAttributes(
    IN      EFI_PCI_IO_PROTOCOL          *This,
    IN      UINT64                       Attributes,
    IN      UINT8                        BarIndex,
    IN OUT  UINT64                       *Offset,
    IN OUT  UINT64                       *Length
    )
{
    ASSERT(FALSE);
    return EFI_DEVICE_ERROR;
}