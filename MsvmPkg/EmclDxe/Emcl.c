/** @file
  Implements the EFI EMCL protocol.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <IsolationTypes.h>

#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CrashLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>

#include <Protocol/Emcl.h>
#include <Protocol/Vmbus.h>
#include <Protocol/EfiHv.h>

#include <Vmbus/VmbusPacketInterface.h>

#include <Synchronization.h>

#define EMCL_DRIVER_VERSION 0x10

#define ADDRESS_AND_SIZE_TO_SPAN_PAGES(_Addr_,_Size_) \
    (ALIGN_VALUE((((UINTN)(_Addr_)) & EFI_PAGE_MASK) + (_Size_), EFI_PAGE_SIZE) >> EFI_PAGE_SHIFT)

#define VARIABLE_STRUCT_SIZE(_Type_,_Field_,_Size_) \
    ((OFFSET_OF(_Type_,_Field_)) + sizeof(*(((_Type_ *)0)->_Field_)) * (_Size_))

#define UINT64_MAX       0xffffffffffffffff

typedef struct _EMCL_BOUNCE_BLOCK
{
    LIST_ENTRY                  BlockListEntry;

    struct _EMCL_BOUNCE_PAGE *FreePageListHead;

    UINT32                      InUsePageCount;
    BOOLEAN                     IsHostVisible;

    VOID*                       BlockBase;
    UINT32                      BlockPageCount;
    EFI_HV_PROTECTION_HANDLE    ProtectionHandle;

    // Allocate associated _EMCL_BOUNCE_PAGE as a large block
    struct _EMCL_BOUNCE_PAGE    *BouncePageStructureBase;
} EMCL_BOUNCE_BLOCK, *PEMCL_BOUNCE_BLOCK;


//
// EMCL_BOUNCE_PAGE - represents one guest physical page of a block.
// Units of pages are allocated to a vmbus packet as required and
// returned to the 'block pool' when not in use.
//
typedef struct _EMCL_BOUNCE_PAGE
{
    struct _EMCL_BOUNCE_PAGE*   NextBouncePage;
    struct _EMCL_BOUNCE_BLOCK*  BounceBlock;
    VOID*                       PageVA;
    UINT64                      HostVisiblePA;
} EMCL_BOUNCE_PAGE, *PEMCL_BOUNCE_PAGE;



typedef struct _EMCL_COMPLETION_ENTRY
{
    EFI_EMCL_COMPLETION_ROUTINE CompletionRoutine;
    VOID *CompletionContext;
    LIST_ENTRY Link;

    EFI_EXTERNAL_BUFFER OriginalBuffer; // just one
    PEMCL_BOUNCE_PAGE EmclBouncePageList;
    UINT32 SendPacketFlags;
    UINT64 TransactionId;
} EMCL_COMPLETION_ENTRY;

#define EMCL_CONTEXT_SIGNATURE         SIGNATURE_32('e','m','c','l')

typedef struct _EMCL_CONTEXT
{
    UINT32 Signature;

    EFI_HANDLE Handle;
    EFI_EMCL_V2_PROTOCOL EmclProtocol;
    EFI_VMBUS_PROTOCOL *VmbusProtocol;
    BOOLEAN IsPipe;

    PACKET_LIB_CONTEXT PkLibContext;
    UINT32 IncomingPageCount;
    UINT32 OutgoingPageCount;
    VOID *RingBufferPages;
    VOID *IncomingData;
    VOID *OutgoingData;
    EFI_VMBUS_GPADL *RingBufferGpadl;

    EFI_EVENT ReceiveEvent;
    EFI_EMCL_RECEIVE_PACKET ReceiveCallback;
    VOID *ReceiveContext;
    EFI_TPL ReceiveTpl;
    BOOLEAN AllocationFailure;

    LIST_ENTRY CompletionEntries;
    LIST_ENTRY OutgoingQueue;

    BOOLEAN IsRunning;
    BOOLEAN InterruptDeferred;

    LIST_ENTRY  BounceBlockListHead;

} EMCL_CONTEXT;

#pragma warning(disable : 4201)
typedef struct _EMCL_INCOMING_PACKET
{
    union
    {
        VMPACKET_DESCRIPTOR Descriptor;
        VMTRANSFER_PAGE_PACKET_HEADER TransferHeader;
        VMDATA_GPA_DIRECT GpaHeader;
    };

} EMCL_INCOMING_PACKET;
#pragma warning(default : 4201)

typedef struct _EMCL_OUTGOING_PACKET
{
    VOID *Buffer;
    UINT32 BufferSize;

    LIST_ENTRY QueueLink;

} EMCL_OUTGOING_PACKET;

EFI_HANDLE mImageHandle;
EFI_HV_IVM_PROTOCOL *mHv;
BOOLEAN mUseBounceBuffer = FALSE;
UINT64 mCurrentTransactionId = 0;

extern EFI_GUID gEfiEmclTagProtocolGuid;

EFI_STATUS
EFIAPI
EmclDestroyGpadl(
    IN  EFI_EMCL_PROTOCOL *This,
    IN  EFI_EMCL_GPADL *Gpadl
    );

EFI_STATUS
EFIAPI
EmclComponentNameGetDriverName (
    IN  EFI_COMPONENT_NAME_PROTOCOL *This,
    IN  CHAR8 *Language,
    OUT CHAR16 **DriverName
    );

EFI_STATUS
EFIAPI
EmclComponentNameGetControllerName(
    IN  EFI_COMPONENT_NAME_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_HANDLE ChildHandle OPTIONAL,
    IN  CHAR8 *Language,
    OUT CHAR16 **ControllerName
    );

EFI_STATUS
EmclpAllocateBounceBlock(
    IN  EMCL_CONTEXT *Context,
    IN  UINT32 BlockByteCount
    );

VOID
EmclpFreeBounceBlock(
    IN  PEMCL_BOUNCE_BLOCK Block
    );

VOID
EmclpFreeAllBounceBlocks(
    IN  EMCL_CONTEXT *Context
    );

PEMCL_BOUNCE_PAGE
EmclpAcquireBouncePages(
    IN  EMCL_CONTEXT *Context,
    IN  UINT32 PageCount
    );

VOID
EmclpReleaseBouncePages(
    IN   EMCL_CONTEXT *Context,
    IN   PEMCL_BOUNCE_PAGE BounceListHead
    );

VOID
EmclpCopyBouncePagesToExternalBuffer(
    IN  EFI_EXTERNAL_BUFFER *ExternalBuffer,
    IN  PEMCL_BOUNCE_PAGE BouncePageList,
    IN  BOOLEAN CopyToBounce
    );

VOID
EmclpZeroBouncePageList(
    IN  PEMCL_BOUNCE_PAGE BouncePageList
    );

VOID
EmclDestroyPacketLibrary(
    IN  EMCL_CONTEXT *Context
    )
/*++

Routine Description:

    This routine destroys the packet library.

Arguments:

    Context - Pointer to the EMCL context.

Return Value:

    None.

--*/
{
    if (Context->RingBufferGpadl != NULL)
    {
        Context->VmbusProtocol->DestroyGpadl(Context->VmbusProtocol,
                                             Context->RingBufferGpadl);
        Context->RingBufferGpadl = NULL;
    }

    if (Context->RingBufferPages != NULL)
    {
        FreePages(Context->RingBufferPages,
                  Context->IncomingPageCount + Context->OutgoingPageCount);

        Context->IncomingData = NULL;
        Context->OutgoingData = NULL;
        Context->RingBufferPages = NULL;
        Context->IncomingPageCount = 0;
        Context->OutgoingPageCount = 0;
    }

    EmclpFreeAllBounceBlocks(Context);

}


EFI_STATUS
EmclInitializePacketLibrary(
    IN  EMCL_CONTEXT *Context,
    IN  UINT32 IncomingRingBufferPageCount,
    IN  UINT32 OutgoingRingBufferPageCount
    )
/*++

Routine Description:

    This routine initializes the packet library.

Arguments:

    Context - Pointer to the EMCL context.

    IncomingRingBufferPageCount - Size of incoming ring buffer in pages.

    OutgoingRingBufferPageCount - Size of outgoing ring buffer in pages.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    UINT32 pageCount;
    VOID* ringData;
    VOID *incomingControl;

    //
    // Include a control page for both ring buffer directions.
    //

    pageCount = IncomingRingBufferPageCount + OutgoingRingBufferPageCount + 2;
    Context->RingBufferPages = AllocatePages(pageCount);
    if (Context->RingBufferPages == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    Context->IncomingPageCount = IncomingRingBufferPageCount + 1;
    Context->OutgoingPageCount = OutgoingRingBufferPageCount + 1;
    status = Context->VmbusProtocol->PrepareGpadl(Context->VmbusProtocol,
                                                  Context->RingBufferPages,
                                                  pageCount * EFI_PAGE_SIZE,
                                                  (EFI_VMBUS_PREPARE_GPADL_FLAG_ZERO_PAGES |
                                                   EFI_VMBUS_PREPARE_GPADL_FLAG_RING_BUFFER),
                                                  HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE,
                                                  &Context->RingBufferGpadl);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    ringData = Context->VmbusProtocol->GetGpadlBuffer(Context->VmbusProtocol,
                                                      Context->RingBufferGpadl);

    incomingControl = (VOID*)((UINTN)ringData +
                      Context->OutgoingPageCount * EFI_PAGE_SIZE);

    Context->OutgoingData = (VOID*)((UINTN)ringData + EFI_PAGE_SIZE);
    Context->IncomingData = (VOID*)((UINTN)incomingControl + EFI_PAGE_SIZE);

    status = PkInitializeSingleMappedRingBuffer(&Context->PkLibContext,
                                                  incomingControl,
                                                  Context->IncomingData,
                                                  IncomingRingBufferPageCount,
                                                  ringData,
                                                  Context->OutgoingData,
                                                  OutgoingRingBufferPageCount);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = EFI_SUCCESS;

Cleanup:
    if (EFI_ERROR(status))
    {
        EmclDestroyPacketLibrary(Context);
    }

    return status;

}


UINT32
EmclGpaRangesSize(
    IN  EFI_EXTERNAL_BUFFER *ExternalBuffers,
    IN  UINT32 ExternalBufferCount
    )
/*++

Routine Description:

    This routine calculates the size of a series of GPA_RANGE structures.

Arguments:

    ExternalBuffers - Array of buffers to be sent.

    ExternalBufferCount - Number of buffers in ExternalBuffers.

Return Value:

    Size of the GPA_RANGE structures.

--*/
{
    UINT32 headerSize;
    UINT32 index;

    headerSize = 0;
    for (index = 0; index < ExternalBufferCount; ++index)
    {
        headerSize += VARIABLE_STRUCT_SIZE(GPA_RANGE,
            PfnArray,
            ADDRESS_AND_SIZE_TO_SPAN_PAGES(ExternalBuffers[index].Buffer,
                                           ExternalBuffers[index].BufferSize));
    }

    return headerSize;
}


VOID
EmclpInitializeGpaRanges(
    IN OUT  GPA_RANGE* Range,
    IN      EFI_EXTERNAL_BUFFER *ExternalBuffers,
    IN      UINT32 ExternalBufferCount
    )
/*++

Routine Description:

    Initializes a set of GPA ranges from a set of buffers.

Description:

    Range - A pointer to the first range. This buffer must be at least
        EmclGpaRangesSize() bytes in size.

    ExternalBuffers - A pointer to an array of external buffers.

    ExternalBufferCount - The number of external buffers.

Return Value:

    None.

--*/
{
    UINT32 index;
    UINT32 pfnCount;
    UINT32 pfnIndex;

    for (index = 0; index < ExternalBufferCount; ++index)
    {
        Range->ByteCount = ExternalBuffers[index].BufferSize;
        Range->ByteOffset = (UINT32)((UINTN)ExternalBuffers[index].Buffer & EFI_PAGE_MASK);
        pfnCount = ADDRESS_AND_SIZE_TO_SPAN_PAGES(
            ExternalBuffers[index].Buffer,
            ExternalBuffers[index].BufferSize);

        for (pfnIndex = 0; pfnIndex < pfnCount; ++pfnIndex)
        {
            Range->PfnArray[pfnIndex] =
                ((UINTN)ExternalBuffers[index].Buffer >> EFI_PAGE_SHIFT) + pfnIndex;
        }

        Range = (GPA_RANGE*)((UINTN)Range +
            VARIABLE_STRUCT_SIZE(GPA_RANGE, PfnArray, pfnCount));
    }
}


VOID
EmclWriteGpaDirectPacket(
    IN  VOID *InlineBuffer,
    IN  UINT32 InlineBufferLength,
    IN  EFI_EXTERNAL_BUFFER *ExternalBuffers,
    IN  UINT32 ExternalBufferCount,
    IN  UINT64 TransactionId,
    IN  BOOLEAN RequestCompletion,
    IN  VOID *OutputBuffer
    )
/*++

Routine Description:

    This routine constructs a GPA Direct packet into a specified buffer.

Arguments:

    InlineBuffer - Optional buffer to be sent as part of the packet.

    InlineBufferLength - Length of InlineBuffer.

    ExternalBuffers - Array of buffers to be sent as part of the GPA direct
        packet.

    ExternalBufferCount - Number of buffers in ExternalBuffers.

    TransactionId - Transaction ID of packet to be sent.

    RequestCompletion - Whether to request a completion packet from the opposite
        endpoint.

    OutputBuffer - The buffer to write the packet into. It is the caller's
        responsibility to ensure this buffer is large enough.

Return Value:

    None.

--*/
{
    VMDATA_GPA_DIRECT *header;
    UINT32 headerSize;

    headerSize = OFFSET_OF(VMDATA_GPA_DIRECT, Range) +
                 EmclGpaRangesSize(ExternalBuffers, ExternalBufferCount);

    header = (VMDATA_GPA_DIRECT*)OutputBuffer;
    header->Descriptor.Type = VmbusPacketTypeDataUsingGpaDirect;
    header->Descriptor.DataOffset8 = (UINT16)(headerSize / 8);
    header->Descriptor.Length8 =
        (UINT16)(ALIGN_VALUE(headerSize + InlineBufferLength, sizeof(UINT64)) / 8);

    header->Descriptor.Flags = 0;
    if (RequestCompletion)
    {
        header->Descriptor.Flags |= VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED;
    }

    header->Descriptor.TransactionId = TransactionId;
    header->RangeCount = ExternalBufferCount;
    EmclpInitializeGpaRanges(header->Range, ExternalBuffers, ExternalBufferCount);
    CopyMem((UINT8*)OutputBuffer + headerSize, InlineBuffer, InlineBufferLength);
}

VOID
EmclWriteGpaDirectPacketBounce(
    IN  VOID *InlineBuffer,
    IN  UINT32 InlineBufferLength,
    IN  EFI_EXTERNAL_BUFFER *ExternalBuffer, // singular; for offset and length
    IN  PEMCL_BOUNCE_PAGE BouncePageList,
    IN  UINT64 TransactionId,
    IN  BOOLEAN RequestCompletion,
    IN  VOID *OutputBuffer
    )
/*++

Routine Description:

    This routine constructs a GPA Direct packet into a specified buffer.

Arguments:

    InlineBuffer - Optional buffer to be sent as part of the packet.

    InlineBufferLength - Length of InlineBuffer.

    BouncePageList - list of pages to be sent as part of the GPA direct
        packet.

    TransactionId - Transaction ID of packet to be sent.

    RequestCompletion - Whether to request a completion packet from the opposite
        endpoint.

    OutputBuffer - The buffer to write the packet into. It is the caller's
        responsibility to ensure this buffer is large enough.

Return Value:

    None.

--*/
{
    PEMCL_BOUNCE_PAGE bouncePage;
    VMDATA_GPA_DIRECT *header;
    UINT32 headerSize;
    UINT32 pfnCount = 0;
    UINT32 pfnIndex;

    pfnCount = ADDRESS_AND_SIZE_TO_SPAN_PAGES(ExternalBuffer->Buffer,
                                              ExternalBuffer->BufferSize);

    headerSize = OFFSET_OF(VMDATA_GPA_DIRECT, Range) +
                 VARIABLE_STRUCT_SIZE(GPA_RANGE, PfnArray, pfnCount);

    header = (VMDATA_GPA_DIRECT*)OutputBuffer;
    header->Descriptor.Type = VmbusPacketTypeDataUsingGpaDirect;
    header->Descriptor.DataOffset8 = (UINT16)(headerSize / 8);
    header->Descriptor.Length8 =
        (UINT16)(ALIGN_VALUE(headerSize + InlineBufferLength, sizeof(UINT64)) / 8);

    header->Descriptor.Flags = 0;
    if (RequestCompletion)
    {
        header->Descriptor.Flags |= VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED;
    }

    header->Descriptor.TransactionId = TransactionId;
    header->RangeCount = 1;

    //
    // Initialize the single GPA range with bounce pages
    //

    header->Range->ByteCount = ExternalBuffer->BufferSize;
    header->Range->ByteOffset = (UINT32)((UINTN)ExternalBuffer->Buffer & EFI_PAGE_MASK);

    bouncePage = BouncePageList;
    for (pfnIndex = 0; pfnIndex < pfnCount; ++pfnIndex)
    {
        ASSERT(bouncePage);
        header->Range->PfnArray[pfnIndex] =
           (UINTN)bouncePage->HostVisiblePA >> EFI_PAGE_SHIFT;
        bouncePage = bouncePage->NextBouncePage;
    }
    ASSERT(bouncePage == NULL);

    CopyMem((UINT8*)OutputBuffer + headerSize, InlineBuffer, InlineBufferLength);
}

VOID
EmclDestroyOutgoingPacket(
    IN  EMCL_OUTGOING_PACKET *Packet
    )
/*++

Routine Description:

    This routine destroys an outgoing packet.

Arguments:

    Packet - Pointer to the packet.

Return Value:

    None.

--*/
{
    if (Packet->Buffer != NULL)
    {
        FreePool(Packet->Buffer);
        Packet->Buffer = NULL;
    }
}


EFI_STATUS
EmclpSendPacket(
    IN  EMCL_CONTEXT *Context,
    IN  VOID *InlineBuffer,
    IN  UINT32 InlineBufferLength,
    IN  EFI_EXTERNAL_BUFFER *ExternalBuffers,
    IN  UINT32 ExternalBufferCount,
    IN  VMBUS_PACKET_TYPE PacketType,
    IN  VMPIPE_PROTOCOL_MESSAGE_TYPE PipePacketType,
    IN  UINT64 TransactionId,
    IN  EMCL_COMPLETION_ENTRY *CompletionEntry,
    IN  BOOLEAN DeferInterrupt
    )
/*++

Routine Description:

    This routine tries to send a packet, queuing it if necessary.

    This routine must be called at TPL <= TPL_EMCL.

Arguments:

    Context - Pointer to the EMCL context.

    InlineBuffer - Optional buffer to be sent as part of the packet.

    InlineBufferLength - Length of InlineBuffer.

    ExternalBuffers - Optional array of buffers to be sent as part of a GPA
        direct packet.

    ExternalBufferCount - Number of buffers in ExternalBuffers.

    PacketType - Type of packet to be sent.

    PipePacketType - Type of pipe packet to be sent, if this is a pipe.

    TransactionId - Transaction ID of packet to be sent.

    CompletionEntry - An optional completion entry. When present a completion
        is requested.

    DeferInterrupt - If TRUE, don't send an interrupt with this packet even
        if one is necessary to notify the host. Instead, defer it to the
        next packet that is sent.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    UINT32 packetSize;
    VOID *packetBuffer;
    EMCL_OUTGOING_PACKET *outgoingPacket;
    VMPACKET_DESCRIPTOR *header;
    VMPIPE_PROTOCOL_HEADER *pipeHeader;
    EFI_TPL tpl;
    BOOLEAN queuePacket;
    UINT32 pageCount;

    outgoingPacket = NULL;
    queuePacket = FALSE;

    // If external buffers are used, there must be a completion
    // entry associated with this packet transfer. External buffers are
    // used for the VmbusPacketTypeDataUsingGpaDirect case.
    ASSERT(((ExternalBufferCount == 0) &&
                (ExternalBuffers == NULL) &&
                (PacketType != VmbusPacketTypeDataUsingGpaDirect)) ||
            ((ExternalBufferCount != 0) &&
                (ExternalBuffers != NULL) &&
                (PacketType == VmbusPacketTypeDataUsingGpaDirect) &&
                (CompletionEntry != NULL)));

    packetSize = (ExternalBufferCount == 0 ?
                  sizeof(VMPACKET_DESCRIPTOR) :
                  OFFSET_OF(VMDATA_GPA_DIRECT, Range) + EmclGpaRangesSize(ExternalBuffers, ExternalBufferCount)) +
                 InlineBufferLength;

    if (Context->IsPipe)
    {
        ASSERT(ExternalBufferCount == 0);
        packetSize += sizeof(*pipeHeader);
    }

    //
    // Verify that the packet isn't too large before making any allocations.
    //

    if (packetSize > PkGetOutgoingRingSize(&Context->PkLibContext))
    {
        //
        // Packet is larger than the ring buffer.
        //

        status = EFI_INVALID_PARAMETER;
        goto Cleanup;
    }

    //
    // Buffer the packet, either to queue for sending later or for sending now.
    //

    outgoingPacket = AllocateZeroPool(sizeof(*outgoingPacket));
    if (outgoingPacket == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    outgoingPacket->Buffer = AllocateZeroPool(packetSize);
    if (outgoingPacket->Buffer == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    outgoingPacket->BufferSize = packetSize;
    packetBuffer = outgoingPacket->Buffer;

    //
    // Write the packet to the buffer.
    //

    switch (PacketType)
    {
    case VmbusPacketTypeDataInBand:
    case VmbusPacketTypeCompletion:
        header = packetBuffer;
        header->Type = PacketType;
        header->DataOffset8 = (UINT16)(sizeof(*header) / 8);
        header->Flags = 0;
        if (CompletionEntry)
        {
            header->Flags |= VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED;
        }

        header->Length8 = (UINT16)(ALIGN_VALUE(packetSize, sizeof(UINT64)) / 8);
        header->TransactionId = TransactionId;
        if (Context->IsPipe)
        {
            pipeHeader = (VMPIPE_PROTOCOL_HEADER*)(header + 1);
            pipeHeader->DataSize = InlineBufferLength;
            pipeHeader->PacketType = PipePacketType;
            CopyMem(pipeHeader + 1, InlineBuffer, InlineBufferLength);
        }
        else
        {
            CopyMem(header + 1, InlineBuffer, InlineBufferLength);
        }

        break;

    case VmbusPacketTypeDataUsingGpaDirect:

        //
        // Bounce buffering is used when the VM is isolated and the channel has
        // not indicated it must use encrypted memory for GPA direct packets.
        //

        if (mUseBounceBuffer &&
            (Context->VmbusProtocol->Flags & EFI_VMBUS_PROTOCOL_FLAGS_CONFIDENTIAL_EXTERNAL_MEMORY) == 0)
        {
            pageCount = ADDRESS_AND_SIZE_TO_SPAN_PAGES(ExternalBuffers[0].Buffer,
                                                       ExternalBuffers[0].BufferSize);

            CompletionEntry->EmclBouncePageList = EmclpAcquireBouncePages(Context, pageCount);
            while (CompletionEntry->EmclBouncePageList == NULL)
            {
                UINT32 allocSize = MAX(pageCount * EFI_PAGE_SIZE, 32 * EFI_PAGE_SIZE);

                status = EmclpAllocateBounceBlock(Context, allocSize);
                if (EFI_ERROR(status))
                {
                    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
                }
                CompletionEntry->EmclBouncePageList = EmclpAcquireBouncePages(Context, pageCount);
            }
            CompletionEntry->OriginalBuffer = ExternalBuffers[0];

            if (CompletionEntry->SendPacketFlags & EMCL_SEND_FLAG_DATA_IN_ONLY)
            {
                EmclpZeroBouncePageList(CompletionEntry->EmclBouncePageList);
            }
            else
            {
                // Copy into the bounce buffer (TRUE)
                EmclpCopyBouncePagesToExternalBuffer(&ExternalBuffers[0],
                                                     CompletionEntry->EmclBouncePageList,
                                                     TRUE);
            }

            EmclWriteGpaDirectPacketBounce(InlineBuffer,
                                           InlineBufferLength,
                                           &ExternalBuffers[0],
                                           CompletionEntry->EmclBouncePageList,
                                           TransactionId,
                                           (CompletionEntry != NULL) ? TRUE : FALSE,
                                           packetBuffer);

        }
        else // Not using a Bounce Buffer
        {
            EmclWriteGpaDirectPacket(InlineBuffer,
                                     InlineBufferLength,
                                     ExternalBuffers,
                                     ExternalBufferCount,
                                     TransactionId,
                                     (CompletionEntry != NULL) ? TRUE : FALSE,
                                     packetBuffer);

        }

        break;

    default:
        ASSERT(!"Sending VMBus packet type not supported");
    }

    tpl = gBS->RaiseTPL(TPL_EMCL);

    if (CompletionEntry)
    {
        InsertTailList(&Context->CompletionEntries, &CompletionEntry->Link);
    }

    status = PkSendPacketSingleMapped(&Context->PkLibContext,
                                        packetBuffer,
                                        packetSize);

    if (status == EFI_RING_SIGNAL_OPPOSITE_ENDPOINT || Context->InterruptDeferred)
    {
        if (!DeferInterrupt)
        {
            Context->VmbusProtocol->SendInterrupt(Context->VmbusProtocol);
        }

        Context->InterruptDeferred = DeferInterrupt;
    }

    if (status == EFI_BUFFER_TOO_SMALL)
    {
        //
        // The packet should be queued to send later.
        //
        queuePacket = TRUE;
        InsertTailList(&Context->OutgoingQueue, &outgoingPacket->QueueLink);
        if (Context->InterruptDeferred)
        {
            Context->VmbusProtocol->SendInterrupt(Context->VmbusProtocol);
            Context->InterruptDeferred = FALSE;
        }
    }
    else if (EFI_ERROR(status))
    {
        // Perform cleanup actions that should be done at high TPL here so that
        // the setup, packet send and cleanup are synchronized correctly.

        if (CompletionEntry)
        {
            RemoveEntryList(&CompletionEntry->Link);
            if (CompletionEntry->EmclBouncePageList)
            {
                EmclpReleaseBouncePages(Context, CompletionEntry->EmclBouncePageList);
            }
            FreePool(CompletionEntry);
        }

        gBS->RestoreTPL(tpl);
        goto Cleanup;
    }

    gBS->RestoreTPL(tpl);
    status = EFI_SUCCESS;

Cleanup:
    if (EFI_ERROR(status) || !queuePacket)
    {
        if (outgoingPacket != NULL)
        {
            EmclDestroyOutgoingPacket(outgoingPacket);
            FreePool(outgoingPacket);
            outgoingPacket = NULL;
        }
    }

    return status;
}


VOID
EmclDispatchPacket(
    IN  EMCL_CONTEXT *Context,
    IN  EMCL_INCOMING_PACKET *Packet
    )
/*++

Routine Description:

    This routine dipatches a packet based on its type.

Arguments:

    Context - Pointer to the EMCL context.

    Packet - Pointer to the packet.

Return Value:

    None.

--*/
{
    EFI_TPL tpl;
    EMCL_COMPLETION_ENTRY *completionEntry;
    VOID *inlineBuffer;
    UINT32 inlineBufferLength;
    PVMPIPE_PROTOCOL_HEADER pipeHeader;
    LIST_ENTRY *listEntry;
    UINT32 expectedRangeCount;

    expectedRangeCount = 0;

    if ((Packet->Descriptor.Length8 * 8 < sizeof(VMPACKET_DESCRIPTOR)) ||
        (Packet->Descriptor.DataOffset8 > Packet->Descriptor.Length8))
    {
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    inlineBuffer = (VOID*)((UINTN)(&Packet->Descriptor) +
        Packet->Descriptor.DataOffset8 * 8);

    inlineBufferLength = (Packet->Descriptor.Length8 -
        Packet->Descriptor.DataOffset8) * 8;

    switch (Packet->Descriptor.Type)
    {
    case VmbusPacketTypeCompletion:

        tpl = gBS->RaiseTPL(TPL_EMCL);

        completionEntry = NULL;
        for (listEntry = Context->CompletionEntries.ForwardLink;
            listEntry != &Context->CompletionEntries;
            listEntry = listEntry->ForwardLink)
        {
            completionEntry = BASE_CR(listEntry, EMCL_COMPLETION_ENTRY, Link);
            if (completionEntry->TransactionId == Packet->Descriptor.TransactionId)
            {
                RemoveEntryList(&completionEntry->Link);
                break;
            }
        }
        gBS->RestoreTPL(tpl);

        if ((completionEntry == NULL) || (completionEntry->TransactionId != Packet->Descriptor.TransactionId))
        {
            FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
        }

        // Bounce buffering (optional) copy back and free the bounce buffers.
        if (completionEntry->EmclBouncePageList)
        {
            if ((completionEntry->SendPacketFlags & EMCL_SEND_FLAG_DATA_OUT_ONLY) == 0)
            {
                EmclpCopyBouncePagesToExternalBuffer(&completionEntry->OriginalBuffer,
                                                     completionEntry->EmclBouncePageList,
                                                     FALSE);
            }
            EmclpReleaseBouncePages(Context, completionEntry->EmclBouncePageList);
            completionEntry->EmclBouncePageList = NULL;
        }

        completionEntry->CompletionRoutine(
            completionEntry->CompletionContext,
            inlineBuffer,
            inlineBufferLength);

        FreePool(completionEntry);
        FreePool(Packet);
        break;

    case VmbusPacketTypeDataInBand:
        if (Context->ReceiveCallback != NULL)
        {
            if (Context->IsPipe)
            {
                // Validate the packet and header values before processing the packet.
                if (inlineBufferLength < sizeof(VMPIPE_PROTOCOL_HEADER))
                {
                    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
                }

                pipeHeader = (PVMPIPE_PROTOCOL_HEADER)inlineBuffer;
                if (pipeHeader->PacketType != VmPipeMessageData)
                {
                    DEBUG((EFI_D_ERROR, "Invalid pipe packet received\n"));
                    return;
                }
                if (pipeHeader->DataSize > (inlineBufferLength - sizeof(VMPIPE_PROTOCOL_HEADER)))
                {
                    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
                }

                inlineBuffer = (VOID*)((UINTN)inlineBuffer + sizeof(*pipeHeader));
                inlineBufferLength = pipeHeader->DataSize;
            }

            Context->ReceiveCallback(
                Context->ReceiveContext,
                (VOID*)Packet,
                inlineBuffer,
                inlineBufferLength,
                0,
                0,
                NULL);
        }

        break;

    case VmbusPacketTypeDataUsingTransferPages:
        if (Context->ReceiveCallback != NULL)
        {
            // Validate the packet and header values before processing the packet.
            if (Packet->Descriptor.DataOffset8 * 8 < OFFSET_OF(VMTRANSFER_PAGE_PACKET_HEADER, Ranges))
            {
                FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
            }

            expectedRangeCount =
                ((Packet->Descriptor.DataOffset8 * 8) - OFFSET_OF(VMTRANSFER_PAGE_PACKET_HEADER, Ranges)) /
                    sizeof(VMTRANSFER_PAGE_RANGE);

            if (Packet->TransferHeader.RangeCount != expectedRangeCount)
            {
                FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
            }

            Context->ReceiveCallback(
                Context->ReceiveContext,
                (VOID*)Packet,
                inlineBuffer,
                inlineBufferLength,
                Packet->TransferHeader.TransferPageSetId,
                Packet->TransferHeader.RangeCount,
                (EFI_TRANSFER_RANGE*)Packet->TransferHeader.Ranges);
        }

        break;

    default:
        DEBUG((EFI_D_ERROR, "EMCL parsed an invalid or unsupported packet\n"));
        break;
    }
}


VOID
EmclProcessQueue(
    IN  EFI_EVENT Event,
    IN  VOID *EventContext OPTIONAL
    )
/*++

Routine Description:

    This routine processes the ring buffer when the opposite endpoint signals
    the channel.

Arguments:

    Event - The event signalled.

    EventContext - Pointer to the interrupt context, which is a pointer to the
        EMCL context.

Return Value:

    None.

--*/
{
    EMCL_CONTEXT *context;
    EFI_STATUS status;
    EFI_TPL tpl;
    UINT32 ringOffset;
    UINT32 currentOffset;
    VOID* incomingBuffer;
    UINT32 bufferLength;
    EMCL_INCOMING_PACKET *incomingPacket;
    UINT32 receivedCount;
    LIST_ENTRY *entry;
    EMCL_OUTGOING_PACKET *outgoingPacket;

    context = (EMCL_CONTEXT*)EventContext;
    ringOffset = PkGetIncomingRingOffset(&context->PkLibContext);

    do
    {
        receivedCount = 0;

        for (;;)
        {
            currentOffset = ringOffset;
            status = PkGetReceiveBuffer(&context->PkLibContext,
                                        &ringOffset,
                                        &incomingBuffer,
                                        &bufferLength);

            if (status == EFI_END_OF_FILE)
            {
                break;
            }
            else if (EFI_ERROR(status))
            {
                break;
            }

            //
            // If packet allocation fails, set a flag which will cause a retry when
            // an existing packet completes and is freed.
            //

            incomingPacket = AllocateZeroPool(
                OFFSET_OF(EMCL_INCOMING_PACKET, Descriptor) + bufferLength);

            if (incomingPacket == NULL)
            {
                context->AllocationFailure = TRUE;
                break;
            }

            context->AllocationFailure = FALSE;

            PkReadPacketSingleMapped(&context->PkLibContext,
                                     incomingPacket,
                                     bufferLength,
                                     currentOffset);

            //
            // Replace with validated buffer length.
            //

            WriteNoFence16((UINT16*)&incomingPacket->Descriptor.Length8, (UINT16)(bufferLength / 8));

            EmclDispatchPacket(context, incomingPacket);
            ++receivedCount;
        }

        if (receivedCount > 0)
        {
            status = PkCompleteRemoval(&context->PkLibContext, ringOffset);
            if (status == EFI_RING_SIGNAL_OPPOSITE_ENDPOINT)
            {
                context->VmbusProtocol->SendInterrupt(context->VmbusProtocol);
            }
            else if (EFI_ERROR(status))
            {
                break;
            }
        }
    } while (receivedCount > 0);

    //
    // Try to process the outgoing queue.
    //

    tpl = gBS->RaiseTPL(TPL_EMCL);
    while (!IsListEmpty(&context->OutgoingQueue))
    {
        ASSERT(!context->InterruptDeferred);

        entry = GetFirstNode(&context->OutgoingQueue);
        outgoingPacket = BASE_CR(entry, EMCL_OUTGOING_PACKET, QueueLink);

        status = PkSendPacketSingleMapped(&context->PkLibContext,
                                            outgoingPacket->Buffer,
                                            outgoingPacket->BufferSize);

        if (status == EFI_RING_SIGNAL_OPPOSITE_ENDPOINT)
        {
            context->VmbusProtocol->SendInterrupt(context->VmbusProtocol);
        }

        if (EFI_ERROR(status))
        {
            break;
        }

        RemoveEntryList(entry);
        gBS->RestoreTPL(tpl);
        EmclDestroyOutgoingPacket(outgoingPacket);
        FreePool(outgoingPacket);
        tpl = gBS->RaiseTPL(TPL_EMCL);
    }

    gBS->RestoreTPL(tpl);
}


EFI_STATUS
EFIAPI
EmclStartChannel(
    IN  EFI_EMCL_PROTOCOL *This,
    IN  UINT32 IncomingRingBufferPageCount,
    IN  UINT32 OutgoingRingBufferPageCount
    )
/*++

Routine Description:

    This routine starts the channel.

    This routine must be called at TPL < TPL_NOTIFY.

Arguments:

    This - Pointer to the EMCL protocol.

    IncomingRingBufferPageCount - Number of pages to use for the incoming ring
        buffer.

    OutgoingRingBufferPageCount - Number of pages to use for the outgoing ring
        buffer.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EMCL_CONTEXT *context;
    BOOLEAN isrRegistered;

    isrRegistered = FALSE;
    context = CR(This,
                 EMCL_CONTEXT,
                 EmclProtocol,
                 EMCL_CONTEXT_SIGNATURE);

    ASSERT(!context->IsRunning);

    status = EmclInitializePacketLibrary(context,
                                         IncomingRingBufferPageCount,
                                         OutgoingRingBufferPageCount);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = context->VmbusProtocol->CreateGpadl(
        context->VmbusProtocol,
        context->RingBufferGpadl);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Create the receive event at the caller-set TPL or TPL_EMCL otherwise.
    //

    ASSERT(context->ReceiveEvent == NULL);

    status = gBS->CreateEvent(
        EVT_NOTIFY_SIGNAL,
        (context->ReceiveCallback == NULL ? TPL_EMCL : context->ReceiveTpl),
        EmclProcessQueue,
        (VOID*) context,
        &context->ReceiveEvent);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    if (context->ReceiveCallback == NULL)
    {
        context->ReceiveTpl = TPL_EMCL;
    }

    status = context->VmbusProtocol->RegisterIsr(context->VmbusProtocol,
                                                 context->ReceiveEvent);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    isrRegistered = TRUE;

    status = context->VmbusProtocol->OpenChannel(context->VmbusProtocol,
                                                 context->RingBufferGpadl,
                                                 context->OutgoingPageCount);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    context->IsRunning = TRUE;
    status = EFI_SUCCESS;

Cleanup:
    if (EFI_ERROR(status))
    {
        if (context->RingBufferGpadl != NULL)
        {
            context->VmbusProtocol->DestroyGpadl(context->VmbusProtocol,
                                                 context->RingBufferGpadl);
            context->RingBufferGpadl = NULL;
        }

        if (isrRegistered)
        {
            context->VmbusProtocol->RegisterIsr(context->VmbusProtocol, NULL);
        }

        if (context->ReceiveEvent != NULL)
        {
            gBS->CloseEvent(context->ReceiveEvent);
            context->ReceiveEvent = NULL;
        }

        EmclDestroyPacketLibrary(context);
    }

    return status;
}


VOID
EFIAPI
EmclStopChannel(
    IN  EFI_EMCL_PROTOCOL *This
    )
/*++

Routine Description:

    This routine stops the channel.

    This routine must be called at TPL <= MIN(ReceiveCallbackTPL, TPL_NOTIFY - 1).

Arguments:

    This - Pointer to the EMCL protocol.

Return Value:

    None.

--*/
{
    EFI_STATUS status;
    EMCL_CONTEXT *context;
    LIST_ENTRY *entry;
    EMCL_OUTGOING_PACKET *packet;

    context = CR(This,
                 EMCL_CONTEXT,
                 EmclProtocol,
                 EMCL_CONTEXT_SIGNATURE);

    ASSERT(context->IsRunning);

    //
    // Stopping the EMCL channel while running at a TPL higher than the receive
    // event is dangerous, as the receive event could be running and accessing
    // structures that are destroyed here.
    //

    ASSERT(EfiGetCurrentTpl() <= context->ReceiveTpl);

    status = context->VmbusProtocol->CloseChannel(context->VmbusProtocol);

    ASSERT_EFI_ERROR(status);

    status = context->VmbusProtocol->RegisterIsr(context->VmbusProtocol, NULL);

    ASSERT_EFI_ERROR(status);

    status = context->VmbusProtocol->DestroyGpadl(context->VmbusProtocol,
                                                  context->RingBufferGpadl);
    ASSERT_EFI_ERROR(status);

    context->RingBufferGpadl = NULL;

    //
    // If the current TPL and the receive TPL are equal, the receive event
    // could still be queued up to be run after the TPL drops. Clear out the
    // event.
    //

    gBS->CloseEvent(context->ReceiveEvent);
    context->ReceiveEvent = NULL;

    //
    // Clear out the queued packet list. No need to raise to TPL_EMCL, since
    // the receive event should not be running and sending packets is prohibited.
    //

    while (!IsListEmpty(&context->OutgoingQueue))
    {
        entry = RemoveEntryList(GetFirstNode(&context->OutgoingQueue));
        packet = BASE_CR(entry, EMCL_OUTGOING_PACKET, QueueLink);
        EmclDestroyOutgoingPacket(packet);
        FreePool(packet);
    }

    //
    // Free any outstanding completion packets.
    // FUTURE: Complete these packets back to the VSCs as
    // aborted and have the VSCs handle this case appropriately.
    //

    while (!IsListEmpty(&context->CompletionEntries))
    {
        EMCL_COMPLETION_ENTRY  *completionEntry;

        entry = RemoveEntryList(GetFirstNode(&context->CompletionEntries));

        completionEntry = BASE_CR(entry, EMCL_COMPLETION_ENTRY, Link);

        if (completionEntry->EmclBouncePageList)
        {
            EmclpReleaseBouncePages(context, completionEntry->EmclBouncePageList);
            completionEntry->EmclBouncePageList = NULL;
        }

        FreePool(completionEntry);
    }

    EmclDestroyPacketLibrary(context);
    context->IsRunning = FALSE;
}


EFI_STATUS
EFIAPI
EmclCreateGpaRange(
    IN  EFI_EMCL_PROTOCOL *This,
    IN  UINT32 Handle,
    IN  EFI_EXTERNAL_BUFFER *ExternalBuffers,
    IN  UINT32 ExternalBufferCount,
    IN  BOOLEAN Writable
    )
/*++

Routine Description:

    Create a pipe GPA range for use with vRDMA.

Arguments:

    This - Pointer to the EMCL protocol.

    Handle - The handle value to use for this GPA range.

    ExternalBuffers - Pointer to a list of external buffers.

    ExternalBufferCount - The number of external buffers.

    Writable - If TRUE, the host should be allowed to write to
        this range.

Return Value:

    EFI_STATUS.

--*/
{
    EMCL_CONTEXT *context;
    VMPIPE_SETUP_GPA_DIRECT_BODY* setupMessage;
    UINT32 setupMessageSize;
    EFI_STATUS status;

    setupMessage = NULL;
    context = CR(This,
                EMCL_CONTEXT,
                EmclProtocol,
                EMCL_CONTEXT_SIGNATURE);

    setupMessageSize = OFFSET_OF(VMPIPE_SETUP_GPA_DIRECT_BODY, Range) +
                       EmclGpaRangesSize(ExternalBuffers, ExternalBufferCount);

    setupMessage = AllocateZeroPool(setupMessageSize);
    if (setupMessage == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    setupMessage->Handle = Handle;
    setupMessage->IsWritable = Writable;
    setupMessage->RangeCount = ExternalBufferCount;
    EmclpInitializeGpaRanges(setupMessage->Range, ExternalBuffers, ExternalBufferCount);

    status = EmclpSendPacket(context,
                             setupMessage,
                             setupMessageSize,
                             NULL,
                             0,
                             VmbusPacketTypeDataInBand,
                             VmPipeMessageSetupGpaDirect,
                             0,
                             NULL,
                             TRUE);

Cleanup:
    if (setupMessage != NULL)
    {
        FreePool(setupMessage);
    }

    return status;
}


EFI_STATUS
EFIAPI
EmclDestroyGpaRange(
    IN  EFI_EMCL_PROTOCOL *This,
    IN  UINT32 Handle
    )
/*++

Routine Description:

    Destroys a pipe GPA range created with EmclCreateGpaRange.

Arguments:

    This - Pointer to EMCL protocol.

    Handle - The handle value that was used when establishing the
        GPA range.

Return Value:

    EFI_STATUS.

--*/
{
    EMCL_CONTEXT *context;
    VMPIPE_TEARDOWN_GPA_DIRECT_BODY teardownMessage;

    context = CR(This,
                 EMCL_CONTEXT,
                 EmclProtocol,
                 EMCL_CONTEXT_SIGNATURE);

    ZeroMem(&teardownMessage, sizeof(teardownMessage));
    teardownMessage.Handle = Handle;
    return EmclpSendPacket(context,
                           &teardownMessage,
                           sizeof(teardownMessage),
                           NULL,
                           0,
                           VmbusPacketTypeDataInBand,
                           VmPipeMessageTeardownGpaDirect,
                           0,
                           NULL,
                           TRUE);
}


EFI_STATUS
EFIAPI
EmclSendPacketEx(
    IN  EFI_EMCL_PROTOCOL *This,
    IN  VOID *InlineBuffer,
    IN  UINT32 InlineBufferLength,
    IN  EFI_EXTERNAL_BUFFER *ExternalBuffers,
    IN  UINT32 ExternalBufferCount,
    IN  UINT32 SendPacketFlags,
    IN  EFI_EMCL_COMPLETION_ROUTINE CompletionRoutine OPTIONAL,
    IN  VOID *CompletionRoutineContext OPTIONAL
    )
/*++

Routine Description:

    This routine sends a simple or GPA Direct packet to the opposite endpoint,
    optionally registering a callback to be called when the packet completes.

    This routine must be called at TPL <= TPL_EMCL.

Arguments:

    This - Pointer to the EMCL protocol.

    InlineBuffer - Optional buffer to be sent as part of the packet.

    InlineBufferLength - Length of InlineBuffer.

    ExternalBuffers - Optional array of buffers to be sent as part of a GPA
        Direct packet.

    ExternalBufferCount - Number of buffers in ExternalBuffers.

    SendPacketFlags - optional flags to optimize data transfer direction.

    CompletionRoutine - Optional routine to be called when the packet completes.
        This routine will be called at the same TPL specified in
        EmclSetReceiveCallback, or TPL_CALLBACK otherwise.

    CompletionRoutineContext - Context supplied when CompletionRoutine is called.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_TPL tpl;
    EMCL_CONTEXT *context;
    EMCL_COMPLETION_ENTRY *completionEntry;
    UINT32 index;

    completionEntry = NULL;

    context = CR(This,
                 EMCL_CONTEXT,
                 EmclProtocol,
                 EMCL_CONTEXT_SIGNATURE);

    //
    // Channel must be started.
    //

    ASSERT(context->IsRunning);

    //
    // Validate the external buffers.
    //

    for (index = 0; index < ExternalBufferCount; ++index)
    {
        if (ExternalBuffers[index].Buffer == NULL ||
            ExternalBuffers[index].BufferSize == 0)
        {
            status = EFI_INVALID_PARAMETER;
            goto Cleanup;
        }
    }

    if (CompletionRoutine != NULL)
    {
        completionEntry = AllocatePool(sizeof(*completionEntry));
        if (completionEntry == NULL)
        {
            status = EFI_OUT_OF_RESOURCES;
            goto Cleanup;
        }

        completionEntry->CompletionRoutine = CompletionRoutine;
        completionEntry->CompletionContext = CompletionRoutineContext;

        completionEntry->OriginalBuffer.Buffer = NULL;
        completionEntry->OriginalBuffer.BufferSize = 0;
        completionEntry->EmclBouncePageList = NULL;
        completionEntry->SendPacketFlags = SendPacketFlags;

        //
        // Insert into list so we can keep track of these entries and free
        // them if uncompleted when DriverStop is called. Increment the
        // TransactionID to use with this completion Entry. Ensure that the
        // increment of the transaction ID does not lead to an overflow.
        //

        tpl = gBS->RaiseTPL(TPL_EMCL);

        if (mCurrentTransactionId == UINT64_MAX)
        {
            FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
        }

        mCurrentTransactionId++;
        completionEntry->TransactionId = mCurrentTransactionId;
        gBS->RestoreTPL(tpl);
    }

    status = EmclpSendPacket(context,
                             InlineBuffer,
                             InlineBufferLength,
                             ExternalBuffers,
                             ExternalBufferCount,
                             (ExternalBuffers == 0 ? VmbusPacketTypeDataInBand :
                             VmbusPacketTypeDataUsingGpaDirect),
                             VmPipeMessageData,
                             ((completionEntry != NULL) ? completionEntry->TransactionId : 0),
                             completionEntry,
                             FALSE);

Cleanup:
    if (EFI_ERROR(status))
    {
        if (completionEntry != NULL)
        {
            FreePool(completionEntry);
        }
    }

    return status;
}


EFI_STATUS
EFIAPI
EmclSendPacket(
    IN  EFI_EMCL_PROTOCOL *This,
    IN  VOID *InlineBuffer,
    IN  UINT32 InlineBufferLength,
    IN  EFI_EXTERNAL_BUFFER *ExternalBuffers,
    IN  UINT32 ExternalBufferCount,
    IN  EFI_EMCL_COMPLETION_ROUTINE CompletionRoutine OPTIONAL,
    IN  VOID *CompletionRoutineContext OPTIONAL
    )
/*++

Routine Description:

    This routine sends a simple or GPA Direct packet to the opposite endpoint,
    optionally registering a callback to be called when the packet completes.

    This routine must be called at TPL <= TPL_EMCL.

Arguments:

    This - Pointer to the EMCL protocol.

    InlineBuffer - Optional buffer to be sent as part of the packet.

    InlineBufferLength - Length of InlineBuffer.

    ExternalBuffers - Optional array of buffers to be sent as part of a GPA
        Direct packet.

    ExternalBufferCount - Number of buffers in ExternalBuffers.

    CompletionRoutine - Optional routine to be called when the packet completes.
        This routine will be called at the same TPL specified in
        EmclSetReceiveCallback, or TPL_CALLBACK otherwise.

    CompletionRoutineContext - Context supplied when CompletionRoutine is called.

Return Value:

    EFI_STATUS.

--*/
{
    return EmclSendPacketEx(This,
                            InlineBuffer,
                            InlineBufferLength,
                            ExternalBuffers,
                            ExternalBufferCount,
                            0, // SendPacketFlags,
                            CompletionRoutine,
                            CompletionRoutineContext);
}


EFI_STATUS
EFIAPI
EmclCompletePacket(
    IN  EFI_EMCL_PROTOCOL *This,
    IN  VOID *PacketContext,
    IN  VOID *Buffer,
    IN  UINT32 BufferLength
    )
/*++

Routine Description:

    This routine is called when the client is finished with a packet passed
    during a receive callback. This may cause a completion packet to be sent.

    This routine must be called at TPL <= TPL_EMCL.

Arguments:

    This - Pointer to the EMCL protocol.

    PacketContext - Packet context supplied during the receive callback.

    Buffer - Optional buffer to send as part of the completion packet.

    BufferLength - Length of Buffer.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EMCL_CONTEXT *context;
    EMCL_INCOMING_PACKET *incomingPacket;

    context = CR(This,
                 EMCL_CONTEXT,
                 EmclProtocol,
                 EMCL_CONTEXT_SIGNATURE);

    incomingPacket = (EMCL_INCOMING_PACKET*)PacketContext;

    if (incomingPacket->Descriptor.Flags & VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED)
    {
        status = EmclpSendPacket(context,
                                 Buffer,
                                 BufferLength,
                                 NULL,
                                 0,
                                 VmbusPacketTypeCompletion,
                                 0,
                                 incomingPacket->Descriptor.TransactionId,
                                 NULL,
                                 FALSE);
    }
    else
    {
        status = EFI_SUCCESS;
    }

    FreePool(incomingPacket);

    //
    // We just freed a packet, so retry allocating a new one.
    //

    if (context->AllocationFailure)
    {
        gBS->SignalEvent(context->ReceiveEvent);
    }

    return status;
}


EFI_STATUS
EFIAPI
EmclSetReceiveCallback(
    IN  EFI_EMCL_PROTOCOL *This,
    IN  EFI_EMCL_RECEIVE_PACKET ReceiveCallback OPTIONAL,
    IN  VOID *ReceiveContext OPTIONAL,
    IN  EFI_TPL Tpl
    )
/*++

Routine Description:

    This routine registers a callback that is called whenever this channel is
    signalled by the opposite endpoint. This routine must be called while the
    channel is not started.

Arguments:

    This - Pointer to the EMCL protocol.

    ReceiveCallback - Callback to be called. If NULL, clears the receive
        callback.

    ReceiveContext - Context to be passed to the callback.

    Tpl - TPL at which to call the callback.

Return Value:

    EFI_STATUS.

--*/
{
    EMCL_CONTEXT *context;

    context = CR(This,
                 EMCL_CONTEXT,
                 EmclProtocol,
                 EMCL_CONTEXT_SIGNATURE);

    //
    // Make sure StartChannel hasn't been called yet.
    //

    ASSERT(!context->IsRunning);

    ASSERT(context->ReceiveEvent == NULL);

    //
    // Clear any previous receive callbacks.
    //

    if (context->ReceiveCallback != NULL)
    {
        context->ReceiveCallback = NULL;
        context->ReceiveContext = NULL;
        context->ReceiveTpl = 0;
    }

    if (ReceiveCallback != NULL)
    {
        context->ReceiveCallback = ReceiveCallback;
        context->ReceiveContext = ReceiveContext;
        context->ReceiveTpl = Tpl;
    }

    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
EmclCreateGpadl(
    IN  EFI_EMCL_PROTOCOL *This,
    IN  VOID *Buffer,
    IN  UINT32 BufferLength,
    IN  HV_MAP_GPA_FLAGS MapFlags,
    OUT EFI_EMCL_GPADL **Gpadl
    )
/*++

Routine Description:

    This routine is a wrapper for VMBus GPADL creation.

    This routine must be called at TPL < TPL_NOTIFY.

Arguments:

    This - Pointer to the EMCL protocol.

    Buffer - Buffer to be used for the GPADL.

    BufferLength - Length of Buffer.

    MapFlags - Mapping flags to control the host visibility. Only HV_MAP_GPA_READABLE and
        HV_MAP_GPA_WRITABLE are valid in the access mask when isolation isused .

    Gpadl - Returns a pointer to the GPADL.

Return Value:

    EFI_STATUS.

--*/
{
    EMCL_CONTEXT *context;
    EFI_VMBUS_GPADL *vmbusGpadl;
    EFI_STATUS status;

    context = CR(This,
                 EMCL_CONTEXT,
                 EmclProtocol,
                 EMCL_CONTEXT_SIGNATURE);

    vmbusGpadl = NULL;

    //
    // TODO: Devices should have a way to request encrypted GPADL support for
    // a confidential channel on a hardware-isolated VM.
    //

    status = context->VmbusProtocol->PrepareGpadl(context->VmbusProtocol,
                                                  Buffer,
                                                  BufferLength,
                                                  0,
                                                  MapFlags,
                                                  &vmbusGpadl);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    status = context->VmbusProtocol->CreateGpadl(context->VmbusProtocol,
                                                 vmbusGpadl);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    *Gpadl = vmbusGpadl;
    vmbusGpadl = NULL;

    status = EFI_SUCCESS;

Cleanup:

    if (vmbusGpadl != NULL)
    {
        context->VmbusProtocol->DestroyGpadl(context->VmbusProtocol,
                                             vmbusGpadl);
    }

    return status;
}


EFI_STATUS
EFIAPI
EmclDestroyGpadl(
    IN  EFI_EMCL_PROTOCOL *This,
    IN  EFI_EMCL_GPADL *Gpadl
    )
/*++

Routine Description:

    This routine is a wrapper for VMBus GPADL destruction.

    This routine must be called at TPL < TPL_NOTIFY.

Arguments:

    This - Pointer to the EMCL protocol.

    Gpadl - Pointer to the GPADL to be destroyed.

Return Value:

    EFI_STATUS.

--*/
{
    EMCL_CONTEXT *context;
    EFI_STATUS status;

    context = CR(This,
                 EMCL_CONTEXT,
                 EmclProtocol,
                 EMCL_CONTEXT_SIGNATURE);

    if (Gpadl != NULL)
    {
        status = context->VmbusProtocol->DestroyGpadl(context->VmbusProtocol,
                                                      Gpadl);
    }
    else
    {
        status = EFI_SUCCESS;
    }

    return status;
}


UINT32
EFIAPI
EmclGetGpadlHandle(
    IN  EFI_EMCL_PROTOCOL *This,
    IN  EFI_EMCL_GPADL *Gpadl
    )
/*++

Routine Description:

    This routine retrieves the GPADL handle associated with a GPADL.

Arguments:

    This - Pointer to the EMCL protocol.

    Gpadl - Pointer to the GPADL.

Return Value:

    GPADL handle.

--*/
{
    EMCL_CONTEXT *context;

    context = CR(This,
                 EMCL_CONTEXT,
                 EmclProtocol,
                 EMCL_CONTEXT_SIGNATURE);

    return context->VmbusProtocol->GetGpadlHandle(context->VmbusProtocol,
                                                  Gpadl);
}


VOID*
EFIAPI
EmclGetGpadlBuffer(
    IN  EFI_EMCL_PROTOCOL *This,
    IN  EFI_EMCL_GPADL *Gpadl
    )
/*++

Routine Description:

    This routine retrieves the usable GPADL buffer pointer associated with
    a GPADL.

Arguments:

    This - Pointer to the EMCL protocol.

    Gpadl - Pointer to the GPADL.

Return Value:

    GPADL buffer.

--*/
{
    EMCL_CONTEXT *context;

    context = CR(This,
                 EMCL_CONTEXT,
                 EmclProtocol,
                 EMCL_CONTEXT_SIGNATURE);

    return context->VmbusProtocol->GetGpadlBuffer(context->VmbusProtocol,
                                                  Gpadl);
}


VOID
EmclInitializeContext(
    IN  EMCL_CONTEXT *Context
    )
/*++

Routine Description:

    This routine initializes the EMCL context.

Arguments:

    Context - Pointer to the EMCL context.

Return Value:

    None.

--*/
{
    ZeroMem(Context, sizeof(*Context));
    Context->Signature = EMCL_CONTEXT_SIGNATURE;
    Context->EmclProtocol.StartChannel = EmclStartChannel;
    Context->EmclProtocol.StopChannel = EmclStopChannel;
    Context->EmclProtocol.SendPacket = EmclSendPacket;
    Context->EmclProtocol.CompletePacket = EmclCompletePacket;
    Context->EmclProtocol.SetReceiveCallback = EmclSetReceiveCallback;
    Context->EmclProtocol.CreateGpadl = EmclCreateGpadl;
    Context->EmclProtocol.DestroyGpadl = EmclDestroyGpadl;
    Context->EmclProtocol.GetGpadlHandle = EmclGetGpadlHandle;
    Context->EmclProtocol.GetGpadlBuffer = EmclGetGpadlBuffer;
    Context->EmclProtocol.CreateGpaRange = EmclCreateGpaRange;
    Context->EmclProtocol.DestroyGpaRange = EmclDestroyGpaRange;
    Context->EmclProtocol.SendPacketEx = EmclSendPacketEx;
    InitializeListHead(&Context->CompletionEntries);
    InitializeListHead(&Context->OutgoingQueue);
    InitializeListHead(&Context->BounceBlockListHead);
}


EFI_STATUS
EFIAPI
EmclDriverSupported(
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    )
/*++

Routine Description:

    Supported routine for EMCL driver binding protocol.

Arguments:

    This - Pointer to the driver binding protocol.

    ControllerHandle - Device handle to check if supported.

    RemainingDevicePath - Device path of child to start.

Return Value:

    EFI_STATUS.

--*/
{
    //
    // EMCL should not be autostarted by ConnectController. It should only be
    // started directly on a VMBus channel handle, preferrably using EmclLib.
    //

    return EFI_UNSUPPORTED;
}


EFI_STATUS
EFIAPI
EmclDriverStart(
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    )
/*++

Routine Description:

    Start routine for EMCL driver binding protocol.

Arguments:

    This - Pointer to the driver binding protocol.

    ControllerHandle - Device handle on which to start.

    RemainingDevicePath - Device path of device being started.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_VMBUS_PROTOCOL *vmbusProtocol;
    EMCL_CONTEXT *context;
    BOOLEAN alreadyStarted;

    vmbusProtocol = NULL;
    context = NULL;
    alreadyStarted = FALSE;

    status = gBS->OpenProtocol(ControllerHandle,
                               &gEfiVmbusProtocolGuid,
                               (VOID**) &vmbusProtocol,
                               mImageHandle,
                               ControllerHandle,
                               EFI_OPEN_PROTOCOL_BY_DRIVER);

    if (EFI_ERROR(status))
    {
        if (status == EFI_ALREADY_STARTED)
        {
            alreadyStarted = TRUE;
        }

        goto Cleanup;
    }

    // EMCL communicates directly with Hypervisor for page visibility operations.
    status = gBS->LocateProtocol(&gEfiHvIvmProtocolGuid, NULL, (VOID **)&mHv);
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR,
            "%a (%d) LocateProtocol failed. status=0x%x\n",
            __FUNCTION__,
            __LINE__,
            status));
        return status;
    }

    //
    // Bounce buffer is required for Isolated partitions.
    // TODO - Use another PCD flag to enable for non-isolated testing.
    //

    if (!IsIsolated())
    {
        mUseBounceBuffer = FALSE;
    }
    else
    {
        mUseBounceBuffer = TRUE;
    }

    context = AllocatePool(sizeof(*context));
    if (context == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    EmclInitializeContext(context);
    context->Handle = ControllerHandle;
    context->VmbusProtocol = vmbusProtocol;
    context->IsPipe =
        ((vmbusProtocol->Flags & EFI_VMBUS_PROTOCOL_FLAGS_PIPE_MODE) != 0);

    //
    // Install the EMCL protocol and store the EMCL context on the handle using
    // the CallerId protocol.
    //

    status = gBS->InstallMultipleProtocolInterfaces(&ControllerHandle,
                                                    &gEfiEmclProtocolGuid, &context->EmclProtocol,
                                                    &gEfiEmclV2ProtocolGuid, &context->EmclProtocol,
                                                    &gEfiCallerIdGuid,
                                                    context,
                                                    NULL);

Cleanup:
    if (EFI_ERROR(status) && !alreadyStarted)
    {
        if (context != NULL)
        {
            FreePool(context);
        }

        if (vmbusProtocol != NULL)
        {
            gBS->CloseProtocol(ControllerHandle,
                               &gEfiVmbusProtocolGuid,
                               mImageHandle,
                               ControllerHandle);
        }
    }

    return status;
}


EFI_STATUS
EFIAPI
EmclDriverStop(
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  UINTN NumberOfChildren,
    IN  EFI_HANDLE *ChildHandleBuffer
    )
/*++

Routine Description:

    Stop routine for EMCL driver binding protocol.

Arguments:

    This - Pointer to the driver binding protocol.

    ControllerHandle - Pointer to the device handle which needs to be stopped.

    NumberOfChildren - If 0, stop the root controller. Otherwise, the number of
        children in ChildHandleBuffer to be stopped.

    ChildHandleBuffer - An array of child handles to stop.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EMCL_CONTEXT *context;

    //
    // Discover the EMCL context using the CallerId protocol.
    //

    status = gBS->OpenProtocol(ControllerHandle,
                               &gEfiCallerIdGuid,
                               (VOID**) &context,
                               mImageHandle,
                               ControllerHandle,
                               EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (EFI_ERROR(status))
    {
        return status;
    }

    status = gBS->UninstallMultipleProtocolInterfaces(ControllerHandle,
                                                      &gEfiEmclProtocolGuid,
                                                      &context->EmclProtocol,
                                                      &gEfiCallerIdGuid,
                                                      context,
                                                      NULL);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "Could not uninstall EMCL protocol\n"));
        return status;
    }

    //
    // Channel must be stopped by now.
    //

    ASSERT(!context->IsRunning);

    FreePool(context);
    gBS->CloseProtocol(ControllerHandle,
                       &gEfiVmbusProtocolGuid,
                       mImageHandle,
                       ControllerHandle);

    return EFI_SUCCESS;
}


//
// Driver name table
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE gEmclDriverNameTable[] =
{
    {
        "eng;en",
        L"Hyper-V EMCL Driver"
    },
    {
        NULL,
        NULL
    }
};


//
// EFI Component Name Protocol
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME_PROTOCOL gEmclComponentName =
{
    EmclComponentNameGetDriverName,
    EmclComponentNameGetControllerName,
    "eng"
};


//
// EFI Component Name 2 Protocol
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME2_PROTOCOL gEmclComponentName2 =
{
    (EFI_COMPONENT_NAME2_GET_DRIVER_NAME) EmclComponentNameGetDriverName,
    (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME) EmclComponentNameGetControllerName,
    "en"
};


EFI_DRIVER_BINDING_PROTOCOL gEmclDriverBindingProtocol =
{
    EmclDriverSupported,
    EmclDriverStart,
    EmclDriverStop,
    EMCL_DRIVER_VERSION,
    NULL,
    NULL
};


EFI_STATUS
EFIAPI
EmclComponentNameGetDriverName (
    IN  EFI_COMPONENT_NAME_PROTOCOL *This,
    IN  CHAR8 *Language,
    OUT CHAR16 **DriverName
    )
/*++

Routine Description:

    Retrieves a Unicode string that is the user readable name of the EFI Driver.

    This function retrieves the user readable name of a driver in the form of a
    Unicode string. If the driver specified by This has a user readable name in
    the language specified by Language, then a pointer to the driver name is
    returned in DriverName, and EFI_SUCCESS is returned. If the driver specified
    by This does not support the language specified by Language,
    then EFI_UNSUPPORTED is returned.

Arguments:

    This - A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or EFI_COMPONENT_NAME_PROTOCOL instance.

    Language - A pointer to a Null-terminated ASCII string array indicating the language.

    DriverName - A pointer to the string to return.

Return Value:

    EFI_STATUS.

--*/
{
    return LookupUnicodeString2(
        Language,
        This->SupportedLanguages,
        gEmclDriverNameTable,
        DriverName,
        (BOOLEAN)(This == &gEmclComponentName));
}


EFI_STATUS
EFIAPI
EmclComponentNameGetControllerName(
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
    return EFI_UNSUPPORTED;
}


EFI_STATUS
EFIAPI
EmclDriverInitialize(
    IN  EFI_HANDLE ImageHandle,
    IN  EFI_SYSTEM_TABLE *SystemTable
    )
/*++

Routine Description:

    This routine is the EMCL driver entry point.

Arguments:

    ImageHandle - Handle of the EMCL image.

    SystemTable - Pointer to the system table.

Return Value:

    EFI_STATUS.

--*/
{

    mImageHandle = ImageHandle;

    //
    // Install the protocols on the driver image handle.
    //
    // The EMCL tag protocol is used by other VMBus child device drivers
    // to find the single instance EMCL driver image handle.  Once found
    // the EMCL driver is started on a VMBus child handle.
    //
    // The Driver Binding and Component Name protocols are typical.
    //
    return gBS->InstallMultipleProtocolInterfaces(
        &ImageHandle,
        &gEfiEmclTagProtocolGuid,           // tag
        NULL,
        &gEfiDriverBindingProtocolGuid,     // driver binding
        &gEmclDriverBindingProtocol,
        &gEfiComponentNameProtocolGuid,     // component name
        &gEmclComponentName,
        &gEfiComponentName2ProtocolGuid,    // component name 2
        &gEmclComponentName2,
        NULL);
}


EFI_STATUS
EmclpAllocateBounceBlock(
    IN  EMCL_CONTEXT *Context,
    IN  UINT32 BlockByteCount
    )
/*++

Routine Description:

    Allocate a large block of memory from EFI for I/O. Mark the memory as host-
    visible. Allocate tracking structures to sub-allocate the block into
    individual pages.

Arguments:

    Context - Pointer to the EMCL Context.

    BlockByteCount - Number of bytes to allocate for I/O. Must be a multiple of EFI_PAGE_SIZE.

Return Value:

    EFI_SUCCESS
    EFI_OUT_OF_RESOURCES for memory allocation failures
    other failures from hypervisor page visibility call.

--*/
{
    EFI_STATUS status = EFI_INVALID_PARAMETER;
    UINT32 pageCount = 0;
    UINT32 i = 0;
    PEMCL_BOUNCE_BLOCK bounceBlock = NULL;
    UINT8* nextVa;
    UINT64 nextPa;

    DEBUG((EFI_D_VERBOSE,
        "%a(%d) Context=%p ByteCount=0x%x\n",
        __FUNCTION__,
        __LINE__,
        Context,
        BlockByteCount));

    if (BlockByteCount % EFI_PAGE_SIZE)
    {
        status = EFI_INVALID_PARAMETER;
        goto Cleanup;
    }

    pageCount = BlockByteCount / EFI_PAGE_SIZE;

    bounceBlock = AllocatePool(sizeof(*bounceBlock));
    if (bounceBlock == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    ZeroMem(bounceBlock, sizeof(*bounceBlock));

    // Allocate the bounce page memory

    bounceBlock->BlockBase = AllocatePages(pageCount);
    if (bounceBlock->BlockBase == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    bounceBlock->BlockPageCount = pageCount;
    ZeroMem(bounceBlock->BlockBase, pageCount * EFI_PAGE_SIZE);

    // Allocate the tracking structures as one
    bounceBlock->BouncePageStructureBase = AllocatePool(pageCount * sizeof(EMCL_BOUNCE_PAGE));
    if (bounceBlock->BouncePageStructureBase == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    bounceBlock->FreePageListHead = bounceBlock->BouncePageStructureBase;
    nextVa = bounceBlock->BlockBase;
    nextPa = (UINT64)nextVa;

    //
    // Make these pages visible to the host
    //

    if (IsIsolated())
    {
        status = mHv->MakeAddressRangeHostVisible(mHv,
                                                  HV_MAP_GPA_READABLE | HV_MAP_GPA_WRITABLE,
                                                  bounceBlock->BlockBase,
                                                  pageCount * EFI_PAGE_SIZE,
                                                  FALSE,
                                                  &bounceBlock->ProtectionHandle);

        if (EFI_ERROR(status))
        {
            goto Cleanup;
        }

        //
        // Adjust the address above the shared GPA boundary if required.
        //

        nextPa += PcdGet64(PcdIsolationSharedGpaBoundary);

        //
        // Canonicalize the VA.
        //

        nextVa = (VOID*)(PcdGet64(PcdIsolationSharedGpaCanonicalizationBitmask) | nextPa);
        bounceBlock->IsHostVisible = TRUE;
    }

    for (i = 0; i < pageCount; i++)
    {
        if (i == (pageCount - 1))
        {
            bounceBlock->BouncePageStructureBase[i].NextBouncePage = NULL;
        }
        else
        {
            bounceBlock->BouncePageStructureBase[i].NextBouncePage =
                &bounceBlock->BouncePageStructureBase[i + 1];
        }

        bounceBlock->BouncePageStructureBase[i].BounceBlock = bounceBlock;
        bounceBlock->BouncePageStructureBase[i].PageVA = nextVa;
        bounceBlock->BouncePageStructureBase[i].HostVisiblePA = nextPa;
        nextVa += EFI_PAGE_SIZE;
        nextPa += EFI_PAGE_SIZE;
    }

    InsertTailList(&Context->BounceBlockListHead, &bounceBlock->BlockListEntry);
    status = EFI_SUCCESS;

Cleanup:
    DEBUG((EFI_D_INFO,
        "%a (%d) Context=%p bounceBlock=%p status=0x%x\n",
        __FUNCTION__,
        __LINE__,
        Context,
        bounceBlock,
        status));

    if (EFI_ERROR(status))
    {
        if (bounceBlock)
        {
            EmclpFreeBounceBlock(bounceBlock);
            bounceBlock = NULL;
        }

    }
    return status;
}

VOID
EmclpFreeBounceBlock(
    IN  PEMCL_BOUNCE_BLOCK Block
    )
/*++

Routine Description:

    Free the block of memory allocated for I/O.
    Marks the memory as not host-visible.

Arguments:

    Block - Bounce block that needs to be freed.

Return Value:

    none

--*/
{
    if (Block->IsHostVisible)
    {
        mHv->MakeAddressRangeNotHostVisible(mHv, Block->ProtectionHandle);
    }

    if (Block->BouncePageStructureBase)
    {
        FreePool(Block->BouncePageStructureBase);
        Block->BouncePageStructureBase = NULL;
    }

    if (Block->BlockBase)
    {
        FreePages(Block->BlockBase, Block->BlockPageCount);
        Block->BlockBase = NULL;
        Block->BlockPageCount = 0;
    }

    FreePool(Block);
}

VOID
EmclpFreeAllBounceBlocks(
    IN  EMCL_CONTEXT *Context
    )

/*++

Routine Description:

    Free all of the large blocks of memory allocated for I/O.
    Marks the memory as not host-visible. Frees the associated tracking structures.

Arguments:

    Context - Pointer to the EMCL Context.

Return Value:

    none

--*/
{
    PEMCL_BOUNCE_BLOCK block;
    LIST_ENTRY* entry;

    while (!IsListEmpty(&Context->BounceBlockListHead))
    {
        entry = GetFirstNode(&Context->BounceBlockListHead);
        RemoveEntryList(entry);

        block = BASE_CR(entry, EMCL_BOUNCE_BLOCK, BlockListEntry);

        DEBUG((EFI_D_WARN,
            "%a (%d) Context=%p block=%p IsHostVis=%d InUsePageCount=%d BlockBase=%p PageCount=0x%x\n",
            __FUNCTION__,
            __LINE__,
            Context,
            block,
            block->IsHostVisible,
            block->InUsePageCount,
            block->BlockBase,
            block->BlockPageCount));

        EmclpFreeBounceBlock(block);
        block = NULL;
    }
}


PEMCL_BOUNCE_PAGE
EmclpAcquireBouncePages(
    IN  EMCL_CONTEXT *Context,
    IN  UINT32 PageCount
    )
/*++

Routine Description:

    Remove 'PageCount' pre-allocated EMCL_BOUNCE_PAGE structures from the
    Context and return them in a linked-list. These PAGE structures will be
    used in an I/O.

Arguments:

    Context - Pointer to the EMCL Context.

    PageCount - Number of EMCL_BOUNCE_PAGE structures to acquire from the
                Context structure and return to the caller.

Return Value:

    Linked list of EMCL_BOUNCE_PAGE structures or NULL on failure.

--*/
{
    PEMCL_BOUNCE_PAGE listHead = NULL;
    UINT32 pagesToGo = PageCount;

    DEBUG((EFI_D_VERBOSE,
        "%a(%d) Context=%p PageCount=%d\n",
        __FUNCTION__,
        __LINE__,
        Context,
        PageCount));

    if (!IsListEmpty(&Context->BounceBlockListHead))
    {
        LIST_ENTRY* blockListEntry;

        for (blockListEntry = Context->BounceBlockListHead.ForwardLink;
             blockListEntry != &Context->BounceBlockListHead;
             blockListEntry = blockListEntry->ForwardLink)
        {
            PEMCL_BOUNCE_BLOCK bounceBlock;
            bounceBlock = BASE_CR(blockListEntry, EMCL_BOUNCE_BLOCK, BlockListEntry);

            while (bounceBlock->FreePageListHead && pagesToGo)
            {
                PEMCL_BOUNCE_PAGE bouncePage;

                bouncePage = bounceBlock->FreePageListHead;
                bounceBlock->FreePageListHead = bouncePage->NextBouncePage;

                bouncePage->NextBouncePage = listHead;
                listHead = bouncePage;

                bounceBlock->InUsePageCount++;

                pagesToGo--;
            }

            if (pagesToGo == 0)
            {
                break;
            }
        }
    }

    if (pagesToGo)
    {
        // failed
        EmclpReleaseBouncePages(Context, listHead);
        listHead = NULL;

        DEBUG((EFI_D_WARN,
            "%a(%d) Context=%p PageCount=%d Returning=NULL\n",
            __FUNCTION__,
            __LINE__,
            Context,
            PageCount));
    }
    else
    {
        DEBUG((EFI_D_VERBOSE,
            "%a(%d) Context=%p PageCount=%d Returning=%p\n",
            __FUNCTION__,
            __LINE__,
            Context,
            PageCount,
            listHead));
    }

    return listHead;
}

VOID
EmclpReleaseBouncePages(
    IN  EMCL_CONTEXT *Context,
    IN  PEMCL_BOUNCE_PAGE BounceListHead
    )
/*++

Routine Description:

    Return EMCL_BOUNCE_PAGES from a linked list to their 'home'
    EMCL_BOUNCE_BLOCK lists. Effectively frees these temporary pages for use
    by another I/O.


Arguments:

    Context - Pointer to the EMCL Context.

    BounceListHead - Linked list of one more EMCL_BOUNCE_PAGE structures.

Return Value:

    none.

--*/
{
    PEMCL_BOUNCE_PAGE page;
    UINT32 count = 0;

    while (BounceListHead)
    {
        page = BounceListHead;
        BounceListHead = BounceListHead->NextBouncePage;

        page->BounceBlock->InUsePageCount--;
        count++;

        page->NextBouncePage = page->BounceBlock->FreePageListHead;
        page->BounceBlock->FreePageListHead = page;
    }

    DEBUG((EFI_D_VERBOSE,
        "%a(%d) Context=%p released PageCount=%d\n",
        __FUNCTION__,
        __LINE__,
        Context,
        count));
}



VOID
EmclpCopyBouncePagesToExternalBuffer(
    IN  EFI_EXTERNAL_BUFFER *ExternalBuffer,
    IN  PEMCL_BOUNCE_PAGE BouncePageList,
    IN  BOOLEAN CopyToBounce
    )
/*++

Routine Description:

    Copy between the memory pages in the bounce buffers and the client's
    buffer respecting the page offsets of the client's buffer. This function
    will zero the partial pages at the beginning and end of the BouncePageList.

Arguments:

    ExternalBuffer - The EFI client's data buffer. Can start at any

    BouncePageList - List of bounce pages (shared with host)

    CopyToBounce - If TRUE, copy from ExternalBuffer into the BouncePageList
                 - If FALSE, copy from the BouncePageList into ExternalBuffer.

Return Value:

    EFI_STATUS.

--*/
{
    UINT64 pageOffset;
    PEMCL_BOUNCE_PAGE bouncePage;
    UINT8* bounceBuffer;
    UINT8* bounceBufferEnd;
    UINT8* extBuffer;
    UINT32 transferToGo;
    UINT32 copySize;

    DEBUG((EFI_D_INFO,
        "%a(%d) ExternalBuffer.Buffer=%p Size=0x%x BouncePageList=%p CopyToBounce=%d\n",
        __FUNCTION__,
        __LINE__,
        ExternalBuffer->Buffer,
        ExternalBuffer->BufferSize,
        BouncePageList,
        CopyToBounce));

    ASSERT(BouncePageList);

    bouncePage = BouncePageList;
    pageOffset = (UINT64)ExternalBuffer->Buffer % EFI_PAGE_SIZE;

    extBuffer = ExternalBuffer->Buffer;
    transferToGo = ExternalBuffer->BufferSize;

    while (transferToGo)
    {
        ASSERT(bouncePage);

        bounceBuffer = (UINT8*)bouncePage->PageVA;

        // Zero any unused space in buffer we are sharing with the host.
        if (CopyToBounce && pageOffset)
        {
            DEBUG((EFI_D_VERBOSE,
                "%a(%d) Zero %p size=0x%x\n",
                __FUNCTION__,
                __LINE__,
                bouncePage->PageVA,
                pageOffset));
            ZeroMem(bouncePage->PageVA, pageOffset);
        }

        // First page offset
        bounceBuffer += pageOffset;
        copySize = EFI_PAGE_SIZE - (UINT32)pageOffset;
        pageOffset = 0; // no more offsets

        copySize = MIN(copySize, transferToGo);
        bounceBufferEnd = bounceBuffer + copySize;

        if (CopyToBounce)
        {
            DEBUG((EFI_D_VERBOSE,
                "%a(%d) CopyToBounce dst=%p src=%p size=0x%x\n",
                __FUNCTION__,
                __LINE__,
                bounceBuffer,
                extBuffer,
                copySize));

            CopyMem(bounceBuffer, extBuffer, copySize);
        }
        else
        {
            DEBUG((EFI_D_VERBOSE,
                "%a(%d) CopyToExtBuffer dst=%p src=%p size=0x%x\n",
                __FUNCTION__,
                __LINE__,
                extBuffer,
                bounceBuffer,
                copySize));

            CopyMem(extBuffer, bounceBuffer, copySize);
        }

        transferToGo -= copySize;
        extBuffer += copySize;

        // Zero any unused space in buffer we are sharing with the host.
        if (transferToGo == 0 &&
            CopyToBounce &&
            ((UINT64)bounceBufferEnd % EFI_PAGE_SIZE))
        {
            UINT32 endOffset = (UINT64)bounceBufferEnd % EFI_PAGE_SIZE;
            UINT32 zeroSize = EFI_PAGE_SIZE - endOffset;

            DEBUG((EFI_D_VERBOSE,
                "%a(%d) Zero %p size=0x%x (from offset=0x%x)\n",
                __FUNCTION__,
                __LINE__,
                bounceBufferEnd,
                zeroSize,
                endOffset));

            ZeroMem(bounceBufferEnd, zeroSize);
        }
        bouncePage = bouncePage->NextBouncePage;
    }

    ASSERT(bouncePage == NULL); // should be all done
}


VOID
EmclpZeroBouncePageList(
    IN  PEMCL_BOUNCE_PAGE BouncePageList
    )
{
    PEMCL_BOUNCE_PAGE bouncePage = BouncePageList;
    UINT32 pageCount = 0;

    while (bouncePage)
    {
        ZeroMem(bouncePage->PageVA, EFI_PAGE_SIZE);
        bouncePage = bouncePage->NextBouncePage;
        pageCount++;
    }
    DEBUG((EFI_D_VERBOSE, "%a(%d) BouncePageList=%p zeroed %d pages\n",
        __FUNCTION__,
        __LINE__,
        BouncePageList,
        pageCount));
}