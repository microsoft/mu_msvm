/** @file
  Implementation of the VPCI VSC DXE UEFI driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include "VpcivscDxe.h"

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/EmclLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MmioAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Vmbus/NtStatus.h>

#include <IsolationTypes.h>

#define AZIHSM_VENDOR_ID 0x1414
#define AZIHSM_DEVICE_ID 0xC003

EFI_DRIVER_BINDING_PROTOCOL gVpcivscDriverBinding =
{
    VpcivscDriverBindingSupported,
    VpcivscDriverBindingStart,
    VpcivscDriverBindingStop,
    VPCIVSC_DRIVER_VERSION,
    NULL,
    NULL
};

VPCI_DEVICE_CONTEXT gVpciDeviceContextTemplate =
{
    VPCI_DEVICE_CONTEXT_SIGNATURE,
    { // PciIo Protocol
        VpcivscPciIoPollMem,
        VpcivscPciIoPollIo,
        {
            VpcivscPciIoMemRead,
            VpcivscPciIoMemWrite
        },
        {
            VpcivscPciIoIoRead,
            VpcivscPciIoIoWrite
        },
        {
            VpcivscPciIoConfigRead,
            VpcivscPciIoConfigWrite
        },
        VpcivscPciIoCopyMem,
        VpcivscPciIoMap,
        VpcivscPciIoUnmap,
        VpcivscPciIoAllocateBuffer,
        VpcivscPciIoFreeBuffer,
        VpcivscPciIoFlush,
        VpcivscPciIoGetLocation,
        VpcivscPciIoAttributes,
        VpcivscPciIoGetBarAttributes,
        VpcivscPciIoSetBarAttributes,
        0, // RomSize
        NULL // RomImage
    },
    0, // Handle
    NULL, // DevicePath
    {0}, // RawBars
    {0}, // MappedBars
    NULL, // VPCIVSC_CONTEXT *VpcivscContext
    0, // Slot
};

VPCIVSC_CONTEXT gVpcivscContextTemplate =
{
    VPCIVSC_CONTEXT_SIGNATURE, // Signature
    0, // Handle
    NULL, // Emcl
    NULL, // DevicePath
    NULL, // EFI_EVENT WaitForBusRelationsMessage
    NULL, // VPCI_DEVICE_DESCRIPTION *Devices
    0, // DeviceCount
    NULL, // VPCI_DEVICE_CONTEXT *NvmeDevices
    0, // NvmeDeviceCount
    NULL, // VPCI_DEVICE_CONTEXT *AziHsmDevices
    0, // AziHsmDeviceCount
};

// Packet completion structure used during packet completion callback
typedef struct _VPCIVSC_COMPLETION_CONTEXT
{
    EFI_EVENT WaitForCompletion;
    VOID* CompletionPacket;
    UINT32 CompletionPacketLength;
    UINT32 BytesCopied;
} VPCIVSC_COMPLETION_CONTEXT, *PVPCIVSC_COMPLETION_CONTEXT;

UINTN mSharedGpaBoundary;
UINT64 mCanonicalizationMask;

//
// VmBus incoming ring buffer page count
// VmBus outgoing ring buffer page count
//
// These are both sized off our largest message (VPCI_DEVICE_TRANSLATE_2)
// with the maximum number of resources (currently 500)
// the size of this structure works out to 28+(500*70)= 35,028 bytes,
// or 8.5 4K pages, round up to nearest power of two for some breathing
// room.
//
#define RING_BUFFER_INCOMING_PAGE_COUNT    16
#define RING_BUFFER_OUTGOING_PAGE_COUNT    16

#define UINT32_MAX 0xffffffff

/// \brief      Debug print a VPCI device.
///
/// \param[in]  Device  The device to print
///
VOID
DebugPrintVpciDevice(
    CONST PVPCI_DEVICE_DESCRIPTION Device
    )
{
    DEBUG((DEBUG_VPCI_INFO,
        "ID:\n \t VendorId %x \n\t DeviceId %x \n\t RevisionId %x \n\t ProgIf %x \n\t SubClass %x \n\t BaseClass %x \n\t SubVendorID %x \n\t SubSystemID %x \n",
        Device->IDs.VendorID,
        Device->IDs.DeviceID,
        Device->IDs.RevisionID,
        Device->IDs.ProgIf,
        Device->IDs.SubClass,
        Device->IDs.BaseClass,
        Device->IDs.SubVendorID,
        Device->IDs.SubSystemID));

    DEBUG((DEBUG_VPCI_INFO,
        "Slot %x SerialNumber %x\n",
        Device->Slot,
        Device->SerialNumber));
}

/// \brief      Checks if a given device is an NVME device or not.
///
/// \param[in]  Device  The device to check
///
/// \return     True if NVME device, False otherwise.
///
BOOLEAN
IsNvmeDevice(
    CONST PVPCI_DEVICE_DESCRIPTION Device
    )
{
    if (Device->IDs.BaseClass == 0x1 &&
        Device->IDs.SubClass == 0x8 &&
        Device->IDs.ProgIf == 0x2)
    {
        return TRUE;
    }

    return FALSE;
}

/// \brief      Checks if a given device is an Azure Integrated HSM device or not.
///
/// \param[in]  Device  The device to check
///
/// \return     True if Azure Integrated HSM device, False otherwise.
///
BOOLEAN
IsAziHsmDevice(
    CONST PVPCI_DEVICE_DESCRIPTION Device
    )
{
    if (Device->IDs.VendorID == AZIHSM_VENDOR_ID &&  // Microsoft Vendor ID
        Device->IDs.DeviceID == AZIHSM_DEVICE_ID)     // Azure Integrated HSM Device ID
    {
        return TRUE;
    }

    return FALSE;
}


/// \brief      The callback function called by EMCL when a packet is received
///             on the channel.
///
/// NOTE: The only packet we look for is a VpciMsgBusRelations packet. This VSC does
/// not handle any other power/hotremove transitions, as it's assumed for simplicity
/// that any devices present will be present at boot time, and not removed.
///
/// \param      ReceiveContext     The receive context set during the callback
///                                registration
/// \param      PacketContext      The packet context
/// \param[in]  Buffer             The buffer representing the data of the actual packet
/// \param[in]  BufferLength       The buffer length
/// \param[in]  TransferPageSetId  The transfer page set identifier
/// \param[in]  RangeCount         The range count
/// \param[in]  Ranges             The EFI transfer ranges
///
VOID
VpciChannelReceivePacketCallback(
    IN  VOID *ReceiveContext,
    IN  VOID *PacketContext,
    IN  VOID *Buffer,
    IN  UINT32 BufferLength,
    IN  UINT16 TransferPageSetId,
    IN  UINT32 RangeCount,
    IN  EFI_TRANSFER_RANGE *Ranges
    )
{
    PVPCIVSC_CONTEXT context = ReceiveContext;
    PVPCI_PACKET_HEADER header = Buffer;
    UINT32 sizeRequired = 0;

    if (BufferLength < sizeof(*header))
    {
        DEBUG((DEBUG_ERROR, "Recv VPCI channel packet less than header size!\n"));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    // See if it's a VpciMsgBusRelations packet, those are the only ones we care about.

    DEBUG((DEBUG_VPCI_INFO, "Recv VPCI channel packet with type 0x%x, len 0x%x\n", header->MessageType, BufferLength));

    if (header->MessageType == VpciMsgBusRelations)
    {
        // Since this is data coming from the host, validate before proceeding
        if (BufferLength < (UINT32) OFFSET_OF(VPCI_QUERY_BUS_RELATIONS, Devices))
        {
            DEBUG((DEBUG_ERROR, "Recv VPCI channel packet very short\n"));
            FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
        }
        PVPCI_QUERY_BUS_RELATIONS busRelationsPacket = Buffer;

        if (busRelationsPacket->DeviceCount > VPCI_MAX_DEVICES_PER_BUS)
        {
            DEBUG((DEBUG_ERROR, "Recv VPCI bus relations packet with too many devices (%d)\n", busRelationsPacket->DeviceCount));
            FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
        }

        if (busRelationsPacket->DeviceCount == 0)
        {
            DEBUG((DEBUG_ERROR, "vpci child device list empty!\n"));
            FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
        }

        if (busRelationsPacket->Devices == NULL)
        {
            DEBUG((DEBUG_ERROR, "vpci child device list empty!\n"));
            FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
        }

        DEBUG((DEBUG_VPCI_INFO, "Recv VpciMsgBusRelations packet, number of child devices 0x%x\n", busRelationsPacket->Devices));

        sizeRequired = context->DeviceCount * sizeof(VPCI_DEVICE_DESCRIPTION) + OFFSET_OF(VPCI_QUERY_BUS_RELATIONS, Devices);

        if (BufferLength < sizeRequired)
        {
            DEBUG((DEBUG_ERROR, "Recv VPCI bus relations packet with not enough size for all devices.  Size: %x\n", BufferLength));
            FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
        }

        // Signal that we have received a valid VpciMsgBusRelations packet.
        gBS->SignalEvent(context->WaitForBusRelationsMessage);

        // Allocate a buffer to hold the child devices.
        context->DeviceCount = busRelationsPacket->DeviceCount;
        context->Devices = AllocateCopyPool(context->DeviceCount * sizeof(VPCI_DEVICE_DESCRIPTION),
            busRelationsPacket->Devices);

        DEBUG((DEBUG_VPCI_INFO, "Printing all child devices..\n"));
        for (UINTN i = 0; i < context->DeviceCount; i++)
        {
            DebugPrintVpciDevice(&context->Devices[i]);
        }
    }

    // Complete the packet.
    DEBUG((DEBUG_VPCI_INFO, "Completing VPCI recv packet.\n"));
    context->Emcl->CompletePacket((EFI_EMCL_PROTOCOL*) context->Emcl,
        PacketContext,
        Buffer,
        BufferLength);
}

// \brief      Completion routine called by EMCL when sending a packet.
//             Optionally copies the packet response into a buffer passed into
//             the completion context.
//
// \param      Context       The packed completion context
// \param[in]  Buffer        The buffer containing the packet response
// \param[in]  BufferLength  The packet length
//
// \return     EFI_SUCCESS
//
EFI_STATUS
VpciChannelSendCompletionCallback(
    IN  VOID *Context OPTIONAL,
    IN  VOID *Buffer,
    IN  UINT32 BufferLength
    )
{
    // Context is the response buffer info, check if big enough, copy if so
    PVPCIVSC_COMPLETION_CONTEXT completionContext = Context;

    DEBUG((DEBUG_VPCI_INFO, "Got vpci completion packet of size 0x%x\n", BufferLength));

    if ((completionContext->CompletionPacketLength != 0) &&
            (BufferLength < completionContext->CompletionPacketLength))
    {
        DEBUG((DEBUG_ERROR, "Recv VPCI packet with unexpected size:\n"));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    if (completionContext->CompletionPacket != NULL)
    {
        UINT32 copyAmount = MIN(BufferLength, completionContext->CompletionPacketLength);
        completionContext->BytesCopied = copyAmount;

        CopyMem(completionContext->CompletionPacket, Buffer, copyAmount);
    }

    gBS->SignalEvent(completionContext->WaitForCompletion);

    return EFI_SUCCESS;
}

/// \brief      Sends a synchronous packet to the VSP.
///
/// \param[in]  Context                        The VSC context
/// \param      Packet                         The packet to send
/// \param[in]  PacketLength                   The packet length
/// \param      CompletionPacket               The optional completion packet to
///                                            return
/// \param[in]  CompletionPacketSize           The completion packet size
/// \param      CompletionPacketBytesReceived  The completion packet bytes
///                                            actually received from the VSP
///
/// \return     EFI_SUCCESS on successful send, error otherwise.
///
EFI_STATUS
VpciChannelSendPacketSync(
    IN  PVPCIVSC_CONTEXT Context,
    IN  VOID* Packet,
    IN  UINT32 PacketLength,
    OUT VOID* CompletionPacket OPTIONAL,
    IN  UINT32 CompletionPacketSize OPTIONAL,
    OUT UINT32* CompletionPacketBytesReceived OPTIONAL
    )
{
    EFI_STATUS status = EFI_DEVICE_ERROR;
    UINTN signaledEventIndex = 0;
    VPCIVSC_COMPLETION_CONTEXT completionContext = { NULL, NULL, 0, 0 };
    completionContext.CompletionPacket = CompletionPacket;
    completionContext.CompletionPacketLength = CompletionPacketSize;
    EFI_EVENT timerEvent;
    EFI_EVENT waitList[2];

    status = gBS->CreateEvent(0, 0, NULL, NULL, &completionContext.WaitForCompletion);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = gBS->CreateEvent(EVT_TIMER,
                              0,
                              NULL,
                              NULL,
                              &timerEvent);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = Context->Emcl->SendPacket((EFI_EMCL_PROTOCOL*)Context->Emcl,
        Packet,
        PacketLength,
        NULL,
        0,
        VpciChannelSendCompletionCallback,
        &completionContext);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    gBS->SetTimer (
            timerEvent,
            TimerRelative,
            VPCIVSC_WAIT_FOR_HOST_TIMEOUT
            );
    waitList[0] = completionContext.WaitForCompletion;
    waitList[1] = timerEvent;
    status = gBS->WaitForEvent(2, waitList, &signaledEventIndex);
    if (EFI_ERROR(status))
    {
        DEBUG((DEBUG_ERROR, "vpci WaitForEvent failed!\n"));
        goto Cleanup;
    }

    // If the timer expired, fail fast.
    if (signaledEventIndex == 1)
    {
        DEBUG((DEBUG_ERROR, "Host did not send a completion packet!\n"));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    DEBUG((DEBUG_VPCI_INFO, "vpci vsc packet sent got 0x%x byte completion back copied\n", completionContext.BytesCopied));
    *CompletionPacketBytesReceived = completionContext.BytesCopied;

Cleanup:
    if (completionContext.WaitForCompletion != NULL)
    {
        gBS->CloseEvent(completionContext.WaitForCompletion);
    }

    return status;
}

/// \brief      Open the channel to the VSP, setup EMCL callbacks.
///
/// \param[in]  Context  The VSC context
///
/// \return     EFI_SUCCESS or error on failure.
///
EFI_STATUS
VpciChannelOpen(
    IN  PVPCIVSC_CONTEXT Context
    )
{
    EFI_STATUS status = EFI_DEVICE_ERROR;

    status = Context->Emcl->SetReceiveCallback((EFI_EMCL_PROTOCOL*)Context->Emcl,
        VpciChannelReceivePacketCallback,
        Context,
        TPL_VPCIVSC_CALLBACK
        );

    ASSERT_EFI_ERROR(status);

    if (EFI_ERROR(status))
    {
        return status;
    }

    status = Context->Emcl->StartChannel((EFI_EMCL_PROTOCOL*)Context->Emcl,
        RING_BUFFER_INCOMING_PAGE_COUNT,
        RING_BUFFER_OUTGOING_PAGE_COUNT);

    ASSERT_EFI_ERROR(status);

    return status;
}

/// \brief      Close the channel. The VSP should reset the VSP state machine in
///             response.
///
/// \param[in]  Context  The VSC context
///
VOID
VpciChannelClose(
    IN  PVPCIVSC_CONTEXT Context
    )
{
    Context->Emcl->StopChannel((EFI_EMCL_PROTOCOL*)Context->Emcl);
}

/// \brief      Negotiate the protocol with the VSP. See corresponding windows
///             VSC function FdoProtocolCommunication in fdo.c
///
/// \param[in]  Context  The VSC context
///
/// \return     EFI_STATUS
///
EFI_STATUS
VpciChannelNegotiateProtocol(
    IN  PVPCIVSC_CONTEXT Context
    )
{
    EFI_STATUS status = EFI_DEVICE_ERROR;
    VPCI_QUERY_PROTOCOL_VERSION versionPacket;
    VPCI_PROTOCOL_VERSION_REPLY replyPacket;
    UINT32 replyPacketSize = sizeof(replyPacket);
    UINT32 replyPacketBytesRecv = 0;

    ZeroMem(&versionPacket, sizeof(versionPacket));
    ZeroMem(&replyPacket, sizeof(replyPacket));

    // Negotiate only latest protocol - don't support older ones
    versionPacket.Header.MessageType = VpciMsgQueryProtocolVersion;
    versionPacket.ProtocolVersion = VPCI_PROTOCOL_VERSION_CURRENT;

    status = VpciChannelSendPacketSync(Context,
        &versionPacket,
        sizeof(versionPacket),
        &replyPacket,
        replyPacketSize,
        &replyPacketBytesRecv);

    if (EFI_ERROR(status))
    {
        return status;
    }

    if (replyPacketBytesRecv != replyPacketSize)
    {
        // Reply packet size didn't match, give up.
        return EFI_DEVICE_ERROR;
    }

    // Check the reply status
    NTSTATUS ntStatus = replyPacket.Header.Status;

    if (NT_SUCCESS(ntStatus))
    {
        // Version accepted by VSP
        // N.B. The reply doesn't contain the version we negotiated; rather, it contains
        // the highest version the VSP supports, which can be higher than the one we negotiated.
        DEBUG((DEBUG_VPCI_INFO, "vpci VSP accepted requested version\n"));
        DEBUG((DEBUG_VPCI_INFO, "vpci VSP latest version is %x\n", replyPacket.ProtocolVersion));
    }
    else
    {
        // Either a version mismatch or internal VSP error. Regardless, we can't continue.
        if (ntStatus == STATUS_REVISION_MISMATCH)
        {
            DEBUG((DEBUG_ERROR, "vcpi VSP version negotiation returned version mismatch\n"));
        }
        else
        {
            DEBUG((DEBUG_ERROR, "vpci VSP version negotiation returned status %x\n", ntStatus));
        }

        status = EFI_DEVICE_ERROR;
    }

    return status;
}

#define VPCI_CONFIG_SPACE_PAGES 2

/// \brief      Allocate mmio for config space, and tell the VSP where it is,
///             does what VpciD0Entry does in fdo.c. In response, the VSP will
///             send a VpciMsgBusRelations which contains the list of child
///             devices on this bus.
///
/// \param[in]  Context  The vsc context
///
/// \return     EFI_STATUS
///
EFI_STATUS
VpciChannelFdoD0Entry(
    IN  PVPCIVSC_CONTEXT Context
    )
{
    EFI_STATUS status = EFI_DEVICE_ERROR;
    VPCI_FDO_D0_ENTRY fdoD0EntryPacket = {0};
    VPCI_FDO_D0_ENTRY_REPLY packetResponse = {0};
    UINT32 packetBytesRecv = 0;

    // Config space is two pages in the current protocol version.
    UINT64 mmioBaseAddress = (UINT64)AllocateMmioPages(VPCI_CONFIG_SPACE_PAGES);

    DEBUG((DEBUG_VPCI_INFO, "got mmio pages starting at 0x%llx\n", mmioBaseAddress));

    if (mmioBaseAddress == 0)
    {
        DEBUG((DEBUG_ERROR, "mmio alloc failed"));
        return EFI_OUT_OF_RESOURCES;
    }

    // Send the packet with where we allocated config space at
    fdoD0EntryPacket.Header.MessageType = VpciMsgFdoD0Entry;
    fdoD0EntryPacket.MmioStart = mmioBaseAddress;
    status = VpciChannelSendPacketSync(Context,
        &fdoD0EntryPacket,
        sizeof(fdoD0EntryPacket),
        &packetResponse,
        sizeof(packetResponse),
        &packetBytesRecv);

    if (EFI_ERROR(status))
    {
        return status;
    }

    if (packetBytesRecv != sizeof(packetResponse))
    {
        DEBUG((DEBUG_ERROR, "VSP response invalid packet size."));
        return EFI_DEVICE_ERROR;
    }

    // Check reply, should be NT_SUCCESS.
    if (!NT_SUCCESS(packetResponse.NtStatus))
    {
        // VSP failed for some reason.
        DEBUG((DEBUG_ERROR, "vpci vsp returned some failure %x\n", packetResponse.NtStatus));
        status = EFI_DEVICE_ERROR;
    }
    
    return status;
}

/// \brief      Send a PdoQueryResourceRequirements to the VSP, asking for the
///             bars needed for this device. Returns the packet response via the
///             passed in device context.
///
/// \param[in]  Context  The device context to ask bars for
///
/// \return     EFI_STATUS
///
EFI_STATUS
VpciChannelPdoQueryResourceRequirements(
    IN OUT  PVPCI_DEVICE_CONTEXT Context
    )
{
    EFI_STATUS status = EFI_DEVICE_ERROR;
    VPCI_QUERY_RESOURCE_REQUIREMENTS queryResourcesPacket = {0};
    VPCI_RESOURCE_REQUIREMENTS_REPLY packetResponse = {0};
    UINT32 packetBytesRecv = 0;

    // Send the packed asking the VSP what the bars are
    queryResourcesPacket.Header.MessageType = VpciMsgCurrentResourceRequirements;
    queryResourcesPacket.Slot = Context->Slot;
    status = VpciChannelSendPacketSync(Context->VpcivscContext,
        &queryResourcesPacket,
        sizeof(queryResourcesPacket),
        &packetResponse,
        sizeof(packetResponse),
        &packetBytesRecv);

    if (EFI_ERROR(status))
    {
        return status;
    }

    if (packetBytesRecv != sizeof(packetResponse))
    {
        DEBUG((DEBUG_ERROR, "VSP response invalid packet size."));
        return EFI_DEVICE_ERROR;
    }

    // Check the status of the reply
    if (!NT_SUCCESS(packetResponse.Header.Status))
    {
        DEBUG((DEBUG_ERROR, "vpci vsp returned failure for VpciChannelPdoQueryResourceRequirements %x\n", packetResponse.Header.Status));
        return EFI_DEVICE_ERROR;
    }

    // Stash the response in the device context
    CopyMem(Context->RawBars, packetResponse.Bars, sizeof(packetResponse.Bars));

    return status;
}

/// \brief      Parse the raw bars returned by the VSP for this device and
///             allocate them.
///
///             TODO: Function doesn't handle 32 bit bars. See comment in VpcivscDriverBindingStart.
///
/// \param[in]  Context  The device context
///
/// \return     EFI_STATUS
///
EFI_STATUS
VpciParseAndAllocateBars(
    IN OUT  PVPCI_DEVICE_CONTEXT Context
    )
{
    UINTN index = 0;

    while (index < PCI_MAX_BAR)
    {
        // If the whole bar is 0, skip it (aka unused).
        if (Context->RawBars[index].AsUINT32 == 0)
        {
            index++;
            continue;
        }

        // Read the BAR, it must be memory type.
        if (Context->RawBars[index].Memory.MemorySpaceIndicator != 0)
        {
            DEBUG((DEBUG_ERROR, "Bar %x is an IO space bar, unsupported\n", index));
            return EFI_DEVICE_ERROR;
        }

        // Check if it's a 64-bit bar
        if (Context->RawBars[index].Memory.MemoryType != PCI_BAR_MEMORY_TYPE_64BIT)
        {
            ASSERT(FALSE);
            DEBUG((DEBUG_ERROR, "Bar %x is a 32 bit bar, unsupported\n", index));
            return EFI_DEVICE_ERROR;
        }

        // The last bar can't be a 64 bit bar.
        if (index == (PCI_MAX_BAR - 1))
        {
            DEBUG((DEBUG_ERROR, "VCPI VSP reported last bar as 64bit, invalid!\n"));
            return EFI_DEVICE_ERROR;
        }

        // 64 bit bars take two bars, the second 32 bit value being the upper bits of the size.
        // Mask off the lower 4 bits as they're ignored for size calculation.
        //
        // Total BAR Size is calculated by taking all bits and inverting + 1
        UINT64 barSize = ~(((UINT64)(Context->RawBars[index + 1].AsUINT32) << 32) | (Context->RawBars[index].Memory.Address << 4)) + 1;

        DEBUG((DEBUG_VPCI_INFO, "Allocating bar %x with size 0x%llx\n", index, barSize));

        // Align up bar size to nearest page because we only allocate mmio in terms of pages.
        UINT64 barSizeInPages = ALIGN_VALUE(barSize, EFI_PAGE_SIZE) / EFI_PAGE_SIZE;

        // Allocate the bar from the high mmio gap.
        UINT64 barAddress = (UINT64) AllocateMmioPages(barSizeInPages);

        if (barAddress == 0)
        {
            DEBUG((DEBUG_ERROR, "No mmio space available to allocate bar!\n"));
            return EFI_OUT_OF_RESOURCES;
        }

        // Update the information in the device context.
        Context->MappedBars[index].MappedAddress = barAddress;
        Context->MappedBars[index].Size = barSize;
        Context->MappedBars[index].Is64Bit = TRUE;

        index += 2;
    }

    return EFI_SUCCESS;
}


/// \brief      Encode a BAR mapped at MappedAddress and Size to a
///             CM_PARTIAL_RESOURCE_DESCRIPTOR.
///
/// \param[in]  Descriptor     The descriptor.
/// \param[in]  MappedAddress  The Address where the BAR was mapped.
/// \param[in]  Size           The size of the mapped BAR.
///
VOID
EncodeBar(
    IN OUT  PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor,
    IN      UINT64 MappedAddress,
    IN      UINT64 Size
    )
{
    Descriptor->Type = CmResourceTypeMemory;
    Descriptor->u.Generic.Start = MappedAddress;

    // See which bucket the size falls into
    // FIXME: alignment? seems like for shifted ones we need to force alignment
    //        on bigger boundaries. Not sure if we'd even see BARs that big for NVMe.
    if (Size < UINT32_MAX)
    {
        // Fits in normal
        Descriptor->u.Generic.Length = (UINT32) Size;
    }
    else if (Size < CM_RESOURCE_MEMORY_LARGE_40_MAXLEN)
    {
        // Shift by 8 to fit, set flag
        Descriptor->Flags |= CM_RESOURCE_MEMORY_LARGE_40;
        Descriptor->u.Generic.Start = (UINT32)(Size >> 8);
    }
    else if (Size < CM_RESOURCE_MEMORY_LARGE_48_MAXLEN)
    {
        // Shift by 16
        Descriptor->Flags |= CM_RESOURCE_MEMORY_LARGE_48;
        Descriptor->u.Generic.Start = (UINT32)(Size >> 16);
    }
    else
    {
        // Shift by 32
        Descriptor->Flags |= CM_RESOURCE_MEMORY_LARGE_64;
        Descriptor->u.Generic.Start = (UINT32)(Size >> 32);
    }
}

/// \brief      Tell the VSP where the VSC has mapped this device's BARs. See
///             PdoPrepareHardware in the Windows VSC.
///
/// \param[in]  Context  The device context
///
/// \return     EFI_STATUS
///
EFI_STATUS
VpciChannelPdoSendAssignedResourcesMessage(
    IN  PVPCI_DEVICE_CONTEXT Context
    )
{
    EFI_STATUS status = EFI_DEVICE_ERROR;
    VPCI_DEVICE_TRANSLATE_2 assignedResourcesPacket;
    VPCI_DEVICE_TRANSLATE_2_REPLY assignedResourcesPacketPartialResponse;
    UINT32 packetBytesRecv = 0;
    ZeroMem(&assignedResourcesPacket, sizeof(assignedResourcesPacket));
    ZeroMem(&assignedResourcesPacketPartialResponse, sizeof(assignedResourcesPacketPartialResponse));

    assignedResourcesPacket.Header.MessageType = VpciMsgAssignedResources2;
    assignedResourcesPacket.Slot = Context->Slot;

    // Translate the BARs we mapped to CM_PARTIAL_RESOURCE_DESCRIPTOR
    //
    // NOTE: Each descriptor starts as CmResourceTypeNull due to being zeroed above.
    //
    // NOTE: 64 bit bars have the 2nd bar as CmResourceTypeNull.
    //
    // NOTE: UEFI doesn't support MSIs, so due to the struct being zero initialized
    //       we respond that we assigned 0 interrupts.
    for (UINTN i = 0; i < PCI_MAX_BAR; i++)
    {
        // Check and see if this bar is even mapped. If not, skip it.
        if (Context->MappedBars[i].Size == 0)
        {
            continue;
        }

        UINT8 rawBarIndex = (UINT8) i;
        
        ASSERT(rawBarIndex < PCI_MAX_BAR);

        PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor = &assignedResourcesPacket.MmioResources[rawBarIndex];

        // The VSP doesn't seem to care about anything except the type, base, and the
        // encoded length. So don't bother setting anything else.
        EncodeBar(descriptor,
            Context->MappedBars[i].MappedAddress,
            Context->MappedBars[i].Size);

        // For confidental VMs, MMIO is translated to a shared section of memory
        // above the shared GPA boundary. This requires a translation which must be
        // reflected in config space, but not sent to the VSP.
        if (IsIsolated())
        {
            Context->MappedBars[i].MappedAddress += mSharedGpaBoundary;

            // Canonicalize the address.
            Context->MappedBars[i].MappedAddress |= mCanonicalizationMask;
        }

        // Since this is a 64 bit bar, set the next descriptor as null type.
        rawBarIndex++;
        ASSERT(rawBarIndex < PCI_MAX_BAR);
        descriptor = &assignedResourcesPacket.MmioResources[rawBarIndex];
        descriptor->Type = CmResourceTypeNull;
    }

    // Send the packet and confirm the VSP accepted it
    status = VpciChannelSendPacketSync(Context->VpcivscContext,
        &assignedResourcesPacket,
        sizeof(assignedResourcesPacket),
        &assignedResourcesPacketPartialResponse,
        sizeof(assignedResourcesPacketPartialResponse),
        &packetBytesRecv);

    if (EFI_ERROR(status))
    {
        return status;
    }

    if (packetBytesRecv != sizeof(assignedResourcesPacketPartialResponse))
    {
        DEBUG((DEBUG_ERROR, "VSP response invalid packet size."));
        return EFI_DEVICE_ERROR;
    }

    if (!NT_SUCCESS(assignedResourcesPacketPartialResponse.Header.Status))
    {
        DEBUG((DEBUG_ERROR, "vpci vsp returned failure for PdoSendAssignedResourcesMessage %x\n",
            assignedResourcesPacketPartialResponse.Header.Status));
        return EFI_DEVICE_ERROR;
    }

    ASSERT(assignedResourcesPacketPartialResponse.Slot.u.AsULONG == Context->Slot.u.AsULONG);

    return status;
}

/// \brief      Tell the VSP that this device is ready to start, via a
///             PdoD0Entry packet.
///
/// \param[in]  Context  The device context
///
/// \return     EFI_STATUS
///
EFI_STATUS
VpciChannelPdoD0Entry(
    IN  PVPCI_DEVICE_CONTEXT Context
    )
{
    EFI_STATUS status = EFI_DEVICE_ERROR;
    VPCI_DEVICE_POWER_CHANGE powerChangePacket;
    VPCI_FDO_D0_ENTRY_REPLY responsePacket;
    UINT32 packetBytesRecv = 0;
    ZeroMem(&powerChangePacket, sizeof(powerChangePacket));
    ZeroMem(&responsePacket, sizeof(responsePacket));

    powerChangePacket.Header.MessageType = VpciMsgDevicePowerStateChange;
    powerChangePacket.Slot = Context->Slot;
    powerChangePacket.TargetState = PowerDeviceD0;

    status = VpciChannelSendPacketSync(Context->VpcivscContext,
        &powerChangePacket,
        sizeof(powerChangePacket),
        &responsePacket,
        sizeof(responsePacket),
        &packetBytesRecv);

    if (EFI_ERROR(status))
    {
        return status;
    }

    if (packetBytesRecv != sizeof(responsePacket))
    {
        DEBUG((DEBUG_ERROR, "VSP response invalid packet size."));
        return EFI_DEVICE_ERROR;
    }

    if (!NT_SUCCESS(responsePacket.NtStatus))
    {
        DEBUG((DEBUG_ERROR, "vpci vsp returned failure for PdoD0Entry %x\n", responsePacket.NtStatus));
        return EFI_DEVICE_ERROR;
    }

    return status;
}

/// \brief      Initialize a device context using a VPCI_DEVICE_DESCRIPTION.
///
/// \param[in]  VscContext         The vsc context
/// \param[in]  DeviceDescription  The VPCI device description to use
/// \param[in]  DeviceContext      The device context to initialize
///
VOID
InitializeVpciDeviceContext(
    IN      PVPCIVSC_CONTEXT VscContext,
    IN      PVPCI_DEVICE_DESCRIPTION DeviceDescription,
    IN OUT  PVPCI_DEVICE_CONTEXT DeviceContext
    )
{
    // Copy the template that contains information for all devices
    CopyMem(DeviceContext, &gVpciDeviceContextTemplate, sizeof(gVpciDeviceContextTemplate));

    // Initialize the slot information, setup the context pointer
    DeviceContext->Slot.u.AsULONG = DeviceDescription->Slot;
    DeviceContext->VpcivscContext = VscContext;
    DeviceContext->DeviceDescription = DeviceDescription;
}

/// \brief      Create an EFI handle with device path for the given device
///             context so DxeCore can load other drivers on child devices.
///
/// \param[in]  Context  The device context
///
/// \return     EFI_STATUS
///
EFI_STATUS
VpciCreateChildDevice(
    IN  PVPCI_DEVICE_CONTEXT Context
    )
{
    EFI_STATUS status = EFI_DEVICE_ERROR;
    PCI_DEVICE_PATH PciNode;
    EFI_EMCL_V2_PROTOCOL dummyProtocol;

    ZeroMem(&PciNode, sizeof(PciNode));

    PciNode.Header.Type = HARDWARE_DEVICE_PATH;
    PciNode.Header.SubType = HW_PCI_DP;
    SetDevicePathNodeLength(&PciNode.Header, sizeof(PciNode));

    PciNode.Device = (UINT8)Context->Slot.u.bits.DeviceNumber;
    PciNode.Function = (UINT8)Context->Slot.u.bits.FunctionNumber;

    // Build the device path using the parent vsc device path, and append the PCI node.
    Context->DevicePath = AppendDevicePathNode(Context->VpcivscContext->DevicePath,
        &PciNode.Header);

    if (Context->DevicePath == NULL)
    {
        return EFI_OUT_OF_RESOURCES;
    }

    status = gBS->InstallMultipleProtocolInterfaces(&Context->Handle,
        &gEfiDevicePathProtocolGuid,
        Context->DevicePath,
        &gEfiPciIoProtocolGuid,
        &Context->PciIo,
        NULL);

    if (EFI_ERROR(status))
    {
        return status;
    }

    // Open child device by child controller for tracking to support DisconnectController
    status = gBS->OpenProtocol(Context->VpcivscContext->Handle,
        &gEfiEmclV2ProtocolGuid,
        (VOID **) &dummyProtocol,
        gImageHandle,
        Context->Handle,
        EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);

    if (EFI_ERROR(status))
    {
        return status;
    }

    return status;
}

/// \brief      Free the memory associated with the device context, like a
///             destructor.
///
/// \param[in]  Context  The device context
///
VOID
VpcivscDestroyDevice(
    IN  PVPCI_DEVICE_CONTEXT Context
    )
{
    FreePool(Context->DevicePath);
}

/// \brief      Free the memory associated with the vsc context, like a
///             destructor.
///
/// \param[in]  Context  The vsc context
///
VOID
VpscivscDestroyContext(
    IN  PVPCIVSC_CONTEXT Context
    )
{
    if (Context->NvmeDevices != NULL)
    {
        FreePool(Context->NvmeDevices);
    }

    if (Context->AziHsmDevices != NULL)
    {
        FreePool(Context->AziHsmDevices);
    }

    if (Context->Devices != NULL)
    {
        FreePool(Context->Devices);
    }

    FreePool(Context);
}

/// \brief      The driver entry point called by the image loader.
///
/// \param[in]  ImageHandle  The image handle of this image
/// \param      SystemTable  The EFI system table
///
/// \return     EFI_STATUS
///
EFI_STATUS
EFIAPI
VpcivscDriverEntryPoint (
    IN  EFI_HANDLE ImageHandle,
    IN  EFI_SYSTEM_TABLE *SystemTable
    )
{
    EFI_STATUS status = EFI_DEVICE_ERROR;

    // If vPCI boot isn't enabled, don't bother starting the driver.
    if (!PcdGetBool(PcdVpciBootEnabled))
    {
        DEBUG((DEBUG_VPCI_INFO, "PcdVpciBootEnabled is false, VPCI VSC not being registered\n"));
        return EFI_UNSUPPORTED;
    }

    // Install the DriverBinding and component name protocols onto the driver image handle
    status = EfiLibInstallDriverBindingComponentName2(ImageHandle,
                                                      SystemTable,
                                                      &gVpcivscDriverBinding,
                                                      ImageHandle,
                                                      &gVpcivscComponentName,
                                                      &gVpcivscComponentName2);
    ASSERT_EFI_ERROR(status);

    mSharedGpaBoundary = PcdGet64(PcdIsolationSharedGpaBoundary);
    mCanonicalizationMask = PcdGet64(PcdIsolationSharedGpaCanonicalizationBitmask);

    return status;
}


/// \brief      Returns if this driver is supported on a given handle.
///
/// \param      This                 The EFI driver binding protocol
/// \param[in]  ControllerHandle     The controller handle
/// \param      RemainingDevicePath  The remaining device path
///
/// \return     EFI_STATUS
///
EFI_STATUS
EFIAPI
VpcivscDriverBindingSupported (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    )
{
    EFI_STATUS status = EFI_DEVICE_ERROR;
    EFI_VMBUS_PROTOCOL *vmbus = NULL;
    EFI_GUID* instanceFilter = (EFI_GUID*) ((UINTN) PcdGet64(PcdVpciInstanceFilterGuidPtr));

    // Get the vmbus protocol
    status = gBS->OpenProtocol(
        ControllerHandle,
        &gEfiVmbusProtocolGuid,
        &vmbus,
        This->DriverBindingHandle,
        ControllerHandle,
        EFI_OPEN_PROTOCOL_TEST_PROTOCOL);

    if (EFI_ERROR(status))
    {
        return status;
    }

    // Test and see if the channel offer is a VPCI one, and if it matches the
    // specific instance guid if set.
    return EmclChannelTypeAndInstanceSupported(
        ControllerHandle,
        &gSyntheticVpciClassGuid,
        This->DriverBindingHandle,
        instanceFilter);
}

/// \brief      The driver start routine, called by DxeCore when driver binding
///             supported returns success. Sets up the channel, negotiates the
///             protocol, and proceeds through the VSC state machine up to the
///             point of PdoD0Entry for NVMe devices, before exposing child
///             handles so other drivers can load.
///
///             This VSC closely follows the Windows VSC, and uses the same
///             conventions and packets the Windows VSC does, made simpler by not
///             needing to support hot add/remove nor power transitions.
///
/// \param      This                 The EFI driver binding protocol
/// \param[in]  ControllerHandle     The controller handle
/// \param      RemainingDevicePath  The remaining device path
///
/// \return     EFI_STATUS
///
EFI_STATUS
EFIAPI
VpcivscDriverBindingStart (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    )
{
    EFI_STATUS status = EFI_DEVICE_ERROR;
    PVPCIVSC_CONTEXT instance = NULL;
    BOOLEAN driverStarted = FALSE;
    BOOLEAN emclInstalled = FALSE;
    BOOLEAN channelStarted = FALSE;
    UINTN index = 0;
    EFI_EVENT   timerEvent;
    EFI_EVENT   waitList[2];

    status = EmclInstallProtocol(ControllerHandle);

    if (status == EFI_ALREADY_STARTED)
    {
        DEBUG((DEBUG_ERROR, "vpci emcl already installed\n"));
        driverStarted = TRUE;
        goto Cleanup;
    }
    else if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    emclInstalled = TRUE;

    instance = AllocateCopyPool(sizeof(*instance), &gVpcivscContextTemplate);

    if (instance == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    status = gBS->OpenProtocol(ControllerHandle,
        &gEfiEmclV2ProtocolGuid,
        (VOID **) &instance->Emcl,
        This->DriverBindingHandle,
        ControllerHandle,
        EFI_OPEN_PROTOCOL_BY_DRIVER);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = gBS->OpenProtocol(ControllerHandle,
        &gEfiDevicePathProtocolGuid,
        (VOID **) &instance->DevicePath,
        This->DriverBindingHandle,
        ControllerHandle,
        EFI_OPEN_PROTOCOL_BY_DRIVER);

    if (EFI_ERROR(status))
    {
        ASSERT(FALSE);
        goto Cleanup;
    }

    // Setup the Event used to unblock driver start after a VpciMsgBusRelations is received.
    // We do not trust the host to send this message so set a timeout as well.
    status = gBS->CreateEvent(0,
                              0,
                              NULL,
                              (VOID*) instance,
                              &instance->WaitForBusRelationsMessage);

    if (EFI_ERROR(status))
    {
        ASSERT_EFI_ERROR(status);
        goto Cleanup;
    }

    status = gBS->CreateEvent(EVT_TIMER,
                              0,
                              NULL,
                              NULL,
                              &timerEvent);

    if (EFI_ERROR(status))
    {
        ASSERT_EFI_ERROR(status);
        goto Cleanup;
    }

    instance->Handle = ControllerHandle;

    // Open channel, setup callback
    status = VpciChannelOpen(instance);

    if (EFI_ERROR(status))
    {
        ASSERT_EFI_ERROR(status);
        goto Cleanup;
    }

    channelStarted = TRUE;

    // Exchange version
    status = VpciChannelNegotiateProtocol(instance);

    if (EFI_ERROR(status))
    {
        DEBUG((DEBUG_ERROR, "vpci negotiate protocol failed!\n"));
        ASSERT_EFI_ERROR(status);
        goto Cleanup;
    }

    // Map config space - Send VpciMsgFdoD0Entry.
    // In response, the VSP sends a VirtualBusSendBusRelationsPacket packet which
    // will contain the list of child devices.
    status = VpciChannelFdoD0Entry(instance);

    if (EFI_ERROR(status))
    {
        DEBUG((DEBUG_ERROR, "vpci FdoD0Entry failed!\n"));
        ASSERT_EFI_ERROR(status);
        goto Cleanup;
    }

    // Wait synchronously via EFI_EVENT for a valid VpciMsgBusRelations packet before proceeding.
    gBS->SetTimer (
            timerEvent,
            TimerRelative,
            VPCIVSC_WAIT_FOR_HOST_TIMEOUT
            );
    waitList[0] = instance->WaitForBusRelationsMessage;
    waitList[1] = timerEvent;
    status = gBS->WaitForEvent(2, waitList, &index);
    if (EFI_ERROR(status))
    {
        DEBUG((DEBUG_ERROR, "vpci WaitForEvent failed!\n"));
        goto Cleanup;
    }

    // If the timer expired, fail fast.
    if (index == 1)
    {
        DEBUG((DEBUG_ERROR, "Host did not send a bus relations packet!\n"));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    status = gBS->CloseEvent(instance->WaitForBusRelationsMessage);
    instance->WaitForBusRelationsMessage = NULL;
    status = gBS->CloseEvent(timerEvent);


    DEBUG((DEBUG_VPCI_INFO, "got %x child devices\n", instance->DeviceCount));

    // Figure out how many NVMe devices we have and allocate appropriately
    for (UINT32 i = 0; i < instance->DeviceCount; i++)
    {
        if (IsNvmeDevice(&instance->Devices[i]))
        {
            instance->NvmeDeviceCount++;
        } 
        else if (IsAziHsmDevice(&instance->Devices[i])) 
        {
            instance->AziHsmDeviceCount++;
        }
    }

    DEBUG((DEBUG_VPCI_INFO, "channel has 0x%x nvme devices and 0x%x AziHsmDevices\n",
        instance->NvmeDeviceCount,
        instance->AziHsmDeviceCount));

    // If no NVMe/AziHsm devices, we just leave the channel open so subsequent calls to
    // DriverStart on this driver will stop early.
    if ( (instance->NvmeDeviceCount == 0) && 
         (instance->AziHsmDeviceCount == 0))
    {
        DEBUG((DEBUG_ERROR, "no NVME/AziHsm devices, driver leaving channel open and returning\n"));
        status = EFI_SUCCESS;
        driverStarted = TRUE;
        goto Cleanup;
    }

    if (instance->NvmeDeviceCount) 
    {
        // Allocate the array of NVMe device contexts. Each device should copy from
        // the template.
        instance->NvmeDevices = AllocateZeroPool(sizeof(VPCI_DEVICE_CONTEXT) * instance->NvmeDeviceCount);

        if (instance->NvmeDevices == NULL)
        {
            status = EFI_OUT_OF_RESOURCES;
            goto Cleanup;
        }
    }

    // Allocate the array of Azure Integrated HSM device contexts if needed
    if (instance->AziHsmDeviceCount)
    {
        instance->AziHsmDevices = AllocateZeroPool(sizeof(VPCI_DEVICE_CONTEXT) * instance->AziHsmDeviceCount);

        if (instance->AziHsmDevices == NULL)
        {
            status = EFI_OUT_OF_RESOURCES;
            goto Cleanup;
        }
    }

    // Look thru the child devices, and for each NVME device
    UINT32 nvmeDeviceIndex = 0, aziHsmDeviceIndex = 0;
    for (UINT32 i = 0; i < instance->DeviceCount; i++)
    {
        if ( (!IsNvmeDevice(&instance->Devices[i])) && 
             (!IsAziHsmDevice(&instance->Devices[i])))
        {
            // Don't care about this device, skip.
            continue;
        }

        if (instance->NvmeDeviceCount)
            ASSERT(nvmeDeviceIndex <= instance->NvmeDeviceCount);

        if (instance->AziHsmDeviceCount) 
            ASSERT(aziHsmDeviceIndex <= instance->AziHsmDeviceCount);

        // Avoid Accessing Invalid Memory     
        if (nvmeDeviceIndex >= instance->NvmeDeviceCount && IsNvmeDevice(&instance->Devices[i])) 
        {
            DEBUG((DEBUG_ERROR, "NvmeDeviceIndex out of bounds!\n"));
            status = EFI_DEVICE_ERROR;
            goto Cleanup;
        }

        // Avoid Accessing Invalid Memory     
        if (aziHsmDeviceIndex >= instance->AziHsmDeviceCount && IsAziHsmDevice(&instance->Devices[i])) 
        {
            DEBUG((DEBUG_ERROR, "AziHsmDeviceIndex out of bounds!\n"));
            status = EFI_DEVICE_ERROR;
            goto Cleanup;
        }

        PVPCI_DEVICE_CONTEXT deviceContext = 
            IsNvmeDevice(&instance->Devices[i]) ?
            &instance->NvmeDevices[nvmeDeviceIndex++] :
            &instance->AziHsmDevices[aziHsmDeviceIndex++];

        InitializeVpciDeviceContext(instance,
            &instance->Devices[i],
            deviceContext);

        // Ask the VSP what resources we need for the device, aka WDF PdoQueryResourceRequirements
        status = VpciChannelPdoQueryResourceRequirements(deviceContext);

        if (EFI_ERROR(status))
        {
            DEBUG((DEBUG_ERROR, "vpci pdo query resource requirements failed!\n"));
            ASSERT(FALSE);
            goto Cleanup;
        }


        // Allocate the BARs and stash where we allocated them for when the device access them later
        //
        // TODO:     No devices need 32 bit bars but we really should support it.
        //           This means we need to allocate from the low mmio gap which is less straightforward than the high gap
        //           since we have some devices on some platforms in that gap. The low mmio allocator
        //           should probably start from the other end and exclude some number of reserved pages.
        status = VpciParseAndAllocateBars(deviceContext);

        if (EFI_ERROR(status))
        {
            DEBUG((DEBUG_ERROR, "vpci failed to parse and map bars!\n"));
            ASSERT(FALSE);
            goto Cleanup;
        }

        // Notify the VSP that we assigned resources, withwhere they were assigned.
        status = VpciChannelPdoSendAssignedResourcesMessage(deviceContext);

        if (EFI_ERROR(status))
        {
            DEBUG((DEBUG_ERROR, "vpci pdo send assigned resource message failed!\n"));
            ASSERT(FALSE);
            goto Cleanup;
        }

        // Next state in the VSC state machine is PdoD0Entry, send it.
        // On success, the device is ready to use.
        status = VpciChannelPdoD0Entry(deviceContext);

        if (EFI_ERROR(status))
        {
            DEBUG((DEBUG_ERROR, "vpci pdo d0 entry failed!\n"));
            ASSERT(FALSE);
            goto Cleanup;
        }

        status = VpciCreateChildDevice(deviceContext);

        if (EFI_ERROR(status))
        {
            DEBUG((DEBUG_ERROR, "vpci create child device failed!\n"));
            ASSERT(FALSE);
            goto Cleanup;
        }
    }

    ASSERT(nvmeDeviceIndex == instance->NvmeDeviceCount);
    ASSERT(aziHsmDeviceIndex == instance->AziHsmDeviceCount);

    driverStarted = TRUE;

    DEBUG((DEBUG_ERROR, "AziHsmDeviceCnt:%d NvmeDevCnt=%d\n",
        instance->AziHsmDeviceCount, 
        instance->NvmeDeviceCount));

Cleanup:
    if (!driverStarted)
    {
        if (instance != NULL)
        {
            if (channelStarted)
            {
                // TODO:     Technically we should also go through the state machine to teardown devices, but
                //           the VSP needs to support the ExitBootServices flow where the only notification it gets is a
                //           channel close notification. So this is fine.
                VpciChannelClose(instance);
            }

            if (instance->WaitForBusRelationsMessage != NULL)
            {
                gBS->CloseEvent(instance->WaitForBusRelationsMessage);
                instance->WaitForBusRelationsMessage = NULL;
            }

            if (instance->NvmeDevices != NULL)
            {
                for (UINTN i = 0; i < instance->NvmeDeviceCount; i++)
                {
                    VpcivscDestroyDevice(&instance->NvmeDevices[i]);
                }
            }

            if (instance->AziHsmDevices != NULL)
            {
                for (UINTN i = 0; i < instance->AziHsmDeviceCount; i++)
                {
                    VpcivscDestroyDevice(&instance->AziHsmDevices[i]);
                }
            }

            VpscivscDestroyContext(instance);
        }

        gBS->CloseProtocol(ControllerHandle,
            &gEfiEmclV2ProtocolGuid,
            This->DriverBindingHandle,
            ControllerHandle);

        gBS->CloseProtocol(ControllerHandle,
            &gEfiDevicePathProtocolGuid,
            This->DriverBindingHandle,
            ControllerHandle);

        if (emclInstalled)
        {
            EmclUninstallProtocol(ControllerHandle);
        }
    }

    return status;
}

/// \brief      Driver stop, called during disconnect controller.
///
/// \param      This              The EFI driver binding protocol
/// \param[in]  ControllerHandle  The controller handle
/// \param[in]  NumberOfChildren  The number of children handles
/// \param[in]  ChildHandleBuffer An array of child handles to be stopped
///
/// \return     EFI_STATUS
///
EFI_STATUS
EFIAPI
VpcivscDriverBindingStop (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  UINTN NumberOfChildren,
    IN  EFI_HANDLE *ChildHandleBuffer
    )
{
    EFI_STATUS status = EFI_DEVICE_ERROR;
    PVPCIVSC_CONTEXT vscContext = NULL;
    PVPCI_DEVICE_CONTEXT deviceContext = NULL;
    EFI_PCI_IO_PROTOCOL  *PciIo = NULL;
    EFI_EMCL_V2_PROTOCOL *Emcl = NULL;

    status = gBS->OpenProtocol(ControllerHandle,
        &gEfiEmclV2ProtocolGuid,
        (VOID**) &Emcl,
        This->DriverBindingHandle,
        ControllerHandle,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    ASSERT_EFI_ERROR(status);

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    vscContext = VPCIVSC_CONTEXT_FROM_EMCL(Emcl);

    if (NumberOfChildren > 0)
    {
        // If child devices, tear down all children devices
        //      For each child device, send d0 exit packet
        //      Then send ReleaseResources
        ASSERT(NumberOfChildren == (vscContext->NvmeDeviceCount + vscContext->AziHsmDeviceCount));

        for (UINTN i = 0; i < NumberOfChildren; i++)
        {
            status = gBS->OpenProtocol(ChildHandleBuffer[i],
                &gEfiPciIoProtocolGuid,
                (VOID**) PciIo,
                This->DriverBindingHandle,
                ControllerHandle,
                EFI_OPEN_PROTOCOL_GET_PROTOCOL);

            ASSERT_EFI_ERROR(status);

            if (EFI_ERROR(status))
            {
                goto Exit;
            }

            deviceContext = VPCI_DEVICE_CONTEXT_FROM_PCI_IO(PciIo);

            VpcivscDestroyDevice(deviceContext);

            // TODO: do we need to uninstall the pci io protocol? or does the
            // handle go away once we return?
        }
    }
    else
    {
        // All children are removed, close the channel, remove protocols.
        VpciChannelClose(vscContext);

        VpscivscDestroyContext(vscContext);

        gBS->CloseProtocol(ControllerHandle,
            &gEfiEmclV2ProtocolGuid,
            This->DriverBindingHandle,
            ControllerHandle);

        gBS->CloseProtocol(ControllerHandle,
            &gEfiDevicePathProtocolGuid,
            This->DriverBindingHandle,
            ControllerHandle);

        EmclUninstallProtocol(ControllerHandle);

        return EFI_SUCCESS;
    }

Exit:
    return status;
}


//
// Driver name table
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE gVpcivscDriverNameTable[] =
{
    { "eng;en", (CHAR16 *)L"Hyper-V VPCI VSC Driver"},
    { NULL, NULL }
};


//
// Controller name table
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE gVpcivscControllerNameTable[] =
{
    { "eng;en", (CHAR16 *)L"Hyper-V VPCI VSC Controller"},
    { NULL, NULL }
};


//
// EFI Component Name Protocol
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME_PROTOCOL gVpcivscComponentName =
{
    VpcivscComponentNameGetDriverName,
    VpcivscComponentNameGetControllerName,
    "eng"
};

//
// EFI Component Name 2 Protocol
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME2_PROTOCOL gVpcivscComponentName2 =
{
    (EFI_COMPONENT_NAME2_GET_DRIVER_NAME) VpcivscComponentNameGetDriverName,
    (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME) VpcivscComponentNameGetControllerName,
    "en"
};


/// \brief      Retrieves a Unicode string that is the user readable name of the
///             EFI Driver.
///
///             This function retrieves the user readable name of a
///             driver in the form of a Unicode string. If the driver specified
///             by This has a user readable name in the language specified by
///             Language, then a pointer to the driver name is returned in
///             DriverName, and EFI_SUCCESS is returned. If the driver specified
///             by This does not support the language specified by Language,
///             then EFI_UNSUPPORTED is returned.
///
/// \param      This        A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
///                         EFI_COMPONENT_NAME_PROTOCOL instance.
/// \param      Language    A pointer to a Null-terminated ASCII string array
///                         indicating the language.
/// \param      DriverName  A pointer to the string to return.
///
/// \return     EFI_STATUS
///
EFI_STATUS
EFIAPI
VpcivscComponentNameGetDriverName (
    IN  EFI_COMPONENT_NAME_PROTOCOL *This,
    IN  CHAR8 *Language,
    OUT CHAR16 **DriverName
    )
{
    return LookupUnicodeString2(
        Language,
        This->SupportedLanguages,
        gVpcivscControllerNameTable,
        DriverName,
        (BOOLEAN)(This == &gVpcivscComponentName));
}


EFI_STATUS
EFIAPI
VpcivscComponentNameGetControllerName(
    IN  EFI_COMPONENT_NAME_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_HANDLE ChildHandle OPTIONAL,
    IN  CHAR8 *Language,
    OUT CHAR16 **ControllerName
    )
/*++

Routine Description:

    Retrieves a Unicode string that is the user readable name of the controller
    that is being managed by a Driver.


    This function retrieves the user readable name of the controller specified by
    ControllerHandle and ChildHandle in the form of a Unicode string. If the
    driver specified by This has a user readable name in the language specified by
    Language, then a pointer to the controller name is returned in ControllerName,
    and EFI_SUCCESS is returned.  If the driver specified by This is not currently
    managing the controller specified by ControllerHandle and ChildHandle,
    then EFI_UNSUPPORTED is returned.  If the driver specified by This does not
    support the language specified by Language, then EFI_UNSUPPORTED is returned.

Arguments:

    This - A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or EFI_COMPONENT_NAME_PROTOCOL instance.

    ControllerHandle - The handle of a controller that the driver specified by This
           is managing.  This handle specifies the controller whose name is to be returned.

    ChildHandle - The handle of the child controller to retrieve the name of. This is an
        optional parameter that may be NULL.  It will be NULL for device drivers.  It will
        also be NULL for a bus drivers that wish to retrieve the name of the bus controller.
        It will not be NULL for a bus  driver that wishes to retrieve the name of a
        child controller.

    Language - A pointer to a Null-terminated ASCII string array indicating the language.
        This is the language of the driver name that the caller is requesting, and it
        must match one of the languages specified in SupportedLanguages. The number of
        languages supported by a driver is up to the driver writer. Language is specified in
        RFC 4646 or ISO 639-2 language code format.

    ControllerName - A pointer to the Unicode string to return. This Unicode string is the
        name of the controller specified by ControllerHandle and ChildHandle in the language
        specified by Language from the point of view of the driver specified by This.

Return Value:

    EFI_STATUS.


--*/
{
    EFI_STATUS status;

    //
    // Make sure this driver is currently managing a ControllerHandle
    //
    status = EfiTestManagedDevice(
        ControllerHandle,
        gVpcivscDriverBinding.DriverBindingHandle,
        &gEfiEmclV2ProtocolGuid
        );
    if (EFI_ERROR(status))
    {
        return status;
    }

    //
    // ChildHandle must be NULL for a Device Driver
    //
    if (ChildHandle != NULL)
    {
        return EFI_UNSUPPORTED;
    }

    return LookupUnicodeString2(
        Language,
        This->SupportedLanguages,
        gVpcivscControllerNameTable,
        ControllerName,
        (BOOLEAN)(This == &gVpcivscComponentName));
}