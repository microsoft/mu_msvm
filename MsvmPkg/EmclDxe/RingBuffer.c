/** @file
  This file implements VMBus's ring buffers.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Synchronization.h>

#include "transportp.h"

#define MAXIMUM_EXPECTED_INTERRUPT_COUNT 64
#define UINT32_MAX 0xffffffff


VOID
PkpExpectInterrupt(
    IN  PPACKET_LIB_CONTEXT PkLibContext,
    IN  BOOLEAN IsIncoming
    );

static
BOOLEAN
PkpValidatePointer(
    IN  UINT32 DataBytesInRing,
    IN  UINT32 Pointer
    );

/// Initializes and validates the ring buffer pointer caches in the packet
/// context from the public data in the ring control structure.
///
/// \param Context Context to finish initializing.
EFI_STATUS
PkpInitRingBufferControl(
    IN OUT  PPACKET_LIB_CONTEXT Context
    )
{
    UINT32 incomingIn;
    UINT32 incomingOut;
    UINT32 outgoingIn;
    UINT32 outgoingOut;

    //
    // Fetch and validate the in/out pointers.
    //

    incomingIn = ReadNoFence((UINT32*)&Context->Incoming.Control->In);
    incomingOut = ReadNoFence((UINT32*)&Context->Incoming.Control->Out);
    outgoingIn = ReadNoFence((UINT32*)&Context->Outgoing.Control->In);
    outgoingOut = ReadNoFence((UINT32*)&Context->Outgoing.Control->Out);
    if (!PkpValidatePointer(Context->Incoming.DataBytesInRing, incomingIn) ||
        !PkpValidatePointer(Context->Incoming.DataBytesInRing, incomingOut) ||
        !PkpValidatePointer(Context->Outgoing.DataBytesInRing, outgoingIn) ||
        !PkpValidatePointer(Context->Outgoing.DataBytesInRing, outgoingOut))
    {
        return EFI_RING_CORRUPT_ERROR;
    }

    //
    // Store the validated information.
    //

    Context->IncomingInCache = incomingIn;
    Context->IncomingOut = incomingOut;
    Context->OutgoingIn = outgoingIn;
    Context->OutgoingOutCache = outgoingOut;

    //
    // Disable ring-full interrupts and enable ring-empty interrupts.
    //

    Context->Outgoing.Control->PendingSendSize = 0;
    Context->Incoming.Control->InterruptMask = FALSE;

    //
    // Set feature bits.
    //

    Context->Outgoing.Control->FeatureBits.Value = 0;
    Context->Outgoing.Control->FeatureBits.SupportsPendingSendSize = TRUE;

    //
    // The opposite endpoint is in an unknown state and may send an
    // interrupt for each direction.
    //

    PkpExpectInterrupt(Context, TRUE);
    PkpExpectInterrupt(Context, FALSE);
    return EFI_SUCCESS;
}

/// This function reduces a specified value using a specfied modulus. This
/// function replaces very expensive modulo division operations when it is known
/// that the number of iterations is at most one and usually zero.
///
/// \param Value Supplies the value to be reduced.
/// \param Modulus Supplies the modulus value.
///
/// \returns The reduced value is returned as the function value.
static
UINT32
PkModuloReduce(
    IN  UINT32 Value,
    IN  UINT32 Modulus
    )
{
    if (Value >= Modulus)
    {
        Value -= Modulus;
        ASSERT(Value < Modulus);
    }

    return Value;
}


/// Validates a ring pointer. It must be less than the number of bytes in the
/// ring and be aligned to a 64-bit value.
///
/// \param DataBytesInRing The number of bytes in the ring.
/// \param Pointer The ring pointer to validate.
///
/// \returns TRUE if the pointer is valid.
static
BOOLEAN
PkpValidatePointer(
    IN  UINT32 DataBytesInRing,
    IN  UINT32 Pointer
    )
{
    return Pointer < DataBytesInRing && (Pointer % sizeof(UINT64)) == 0;
}


/// Determine the number of bytes of data available in the ring.  That is how
/// much data has been placed in the ring by the other end and is available for
/// this end to process.
///
/// We don't want to "capture" the In and Out values in this function because
/// the caller will make decisions about what to do based on the results of this
/// function.  So we need the caller to be using the same values of In and Out
/// that we're using here.  Remember that the opposite endpoint can change the
/// In and Out values at any time.  Some of those changes are benign, expected
/// and normal. Some might be malicious.
///
/// \param DataBytesInRing The total number of bytes in the ring.
/// \param PreviouslyCapturedIn Value for In.
/// \param PreviouslyCapturedOut Value for Out.
///
/// \returns Returns the number of bytes of data available.
static
UINT32
PkpDataAvailable(
    IN  UINT32 DataBytesInRing,
    IN  UINT32 PreviouslyCapturedIn,
    IN  UINT32 PreviouslyCapturedOut
    )
{
    UINT32 bytesAvailable;

    ASSERT(PreviouslyCapturedIn < DataBytesInRing);
    ASSERT(PreviouslyCapturedOut < DataBytesInRing);

    bytesAvailable = PreviouslyCapturedIn - PreviouslyCapturedOut;
    if (bytesAvailable < DataBytesInRing)
    {
        //            1         2         3
        //  0123456789012345678901234567890123456789
        // +----------------------------------------+
        // |            aaaabbbb                    |
        // +----------------------------------------+
        //  ^           ^       ^                   ^
        //  Buffer      Out     In                  RingLength
        //
        // In this example, In >= Out, (remember that == means the
        // buffer is empty) there are 8 bytes of available data
        // represented by aaaabbbb.
        //
        return bytesAvailable;
    }
    else
    {
        //            1         2         3
        //  0123456789012345678901234567890123456789
        // +----------------------------------------+
        // |ffffgggghhhh        aaaabbbbccccddddeeee|
        // +----------------------------------------+
        //  ^           ^       ^                   ^
        //  Buffer      In      Out                 RingLength
        //
        // In this example, In < Out, there are 32 bytes of available
        // data, 20 at the end of the buffer and another 12 at the
        // beginning.
        //
        return DataBytesInRing + bytesAvailable;
    }
}


/// Determine the amount of free space in the ring.  If In equals Out, then the
/// buffer is empty.  One data byte can't be used, or a full buffer looks like
/// an empty buffer.
///
/// \param DataBytesInRing The total number of bytes in the ring.
/// \param PreviouslyCapturedIn Value of In when this transaction began.
///     (Remember that reading it again would open us up to atacks where the
///     opposite end- point changed it maliciously after the first reading.)
/// \param PreviouslyCapturedOut Value of In when this transaction began.
///
/// \returns Returns the number of bytes free.
static
UINT32
PkpFreeBytes(
    IN  UINT32 DataBytesInRing,
    IN  UINT32 PreviouslyCapturedIn,
    IN  UINT32 PreviouslyCapturedOut
    )
{
    UINT32 bytesFree;

    ASSERT(PreviouslyCapturedIn < DataBytesInRing);
    ASSERT(PreviouslyCapturedOut < DataBytesInRing);

    bytesFree = PreviouslyCapturedOut - PreviouslyCapturedIn - 1;
    if (bytesFree < DataBytesInRing)
    {
        //            1         2         3
        //  0123456789012345678901234567890123456789
        // +----------------------------------------+
        // |ffffgggghhhh        aaaabbbbccccddddeeee|
        // +----------------------------------------+
        //  ^           ^       ^                   ^
        //  Buffer      In      Out                 RingLength
        //
        // In this example, In < Out, there are 32 bytes of available
        // data, 20 at the end of the buffer and another 12 at the
        // beginning.
        //

        return bytesFree;
    }
    else
    {
        //            1         2         3
        //  0123456789012345678901234567890123456789
        // +----------------------------------------+
        // |            aaaabbbb                    |
        // +----------------------------------------+
        //  ^           ^       ^                   ^
        //  Buffer      Out     In                  RingLength
        //
        // In this example, In >= Out, (remember that == means the
        // buffer is empty) there are 8 bytes of available data
        // represented by aaaabbbb.
        //

        return DataBytesInRing + bytesFree;
    }
}

/// Checks to see if the ougoing ring buffer has enough space for the outgoing
/// packet. If not, updates the PendingSendSize in the ring control.
///
/// \param PkLibContext Pointer to the packet library context structure.
/// \param TotalPacketSize Size of the packet to be be placed in the ring
///     buffer.
/// \param In Current cached value of the In pointer.
/// \param Out Current cached value of the Out pointer.
/// \param DataBytesInRing How many data bytes are in the ring buffer.
///
/// \retval EFI_SUCCESS The ring buffer has enough space for the packet.
/// \retval EFI_RING_CORRUPT_ERROR The ring buffer itself has become corrupt.
///     No further processing on it is likely to succeed.
/// \retval EFI_INVALID_PARAMETER The packet is larger than the size of the
///     ring buffer.
/// \retval EFI_BUFFER_TOO_SMALL The ring buffer is currently full.
EFI_STATUS
PkpCheckSendBufferFreeBytes(
    IN  PPACKET_LIB_CONTEXT PkLibContext,
    IN  UINT32              TotalPacketSize,
    IN  UINT32              In,
    IN  UINT32              Out,
    IN  UINT32              DataBytesInRing
    )
{
    PVMRCB control;
    EFI_STATUS status = EFI_SUCCESS;
    UINT32 pendingSendSize;

    if (PkpFreeBytes(DataBytesInRing, In, Out) < TotalPacketSize)
    {
        control = PkLibContext->Outgoing.Control;

        //
        // The cached value of the Out pointer did not yield enough space. Fetch
        // the public version and check again.
        //

        Out = ReadNoFence((UINT32*)&control->Out);
        if (!PkpValidatePointer(DataBytesInRing, Out))
        {
            status = EFI_RING_CORRUPT_ERROR;
            goto Cleanup;
        }

        PkLibContext->OutgoingOutCache = Out;
        if (PkpFreeBytes(DataBytesInRing, In, Out) < TotalPacketSize)
        {
            //
            // There is still not enough free space to send this packet. Verify
            // that this isn't because the requested packet size is larger
            // than the ring size.
            //

            if (TotalPacketSize >= DataBytesInRing)
            {
                status = EFI_INVALID_PARAMETER;
                goto Cleanup;
            }

            //
            // Update the pending send size in the control region and then
            // check one more time to avoid a race where enough space was freed
            // just after setting the pending size.
            //
            // Note that the currently pending commit is added to the total
            // packet size in order to avoid the race where a packet is
            // removed  by the other endpoint before the next commit occurs.
            // As a result, there may be a longer delay than absolutely
            // necessary before the signal arrives.
            //
            // FUTURE-jostarks: Consider changing callers and the contract
            // so that this is not an issue.
            //

            if (PkLibContext->PendingSendSize == 0)
            {
                PkpExpectInterrupt(PkLibContext, FALSE);
            }

            pendingSendSize = PkpDataAvailable(DataBytesInRing,
                                               In,
                                               PkLibContext->OutgoingIn) +
                              TotalPacketSize;

            if (pendingSendSize >= DataBytesInRing)
            {
                pendingSendSize = DataBytesInRing - 1;
            }

            WriteNoFence((UINT32*)&control->PendingSendSize, pendingSendSize);

            //
            // Store the actual send size so that it can be retrieved by
            // users of the library.
            //

            PkLibContext->PendingSendSize = TotalPacketSize -
                sizeof(PREVIOUS_PACKET_OFFSET);

            //
            // A memory barrier is necessary to ensure that PendingSendSize
            // is set before re-reading the Out portion of the control
            // region. Otherwise a processor (or compiler) reordering could
            // occur.
            //

            MemoryBarrier();
            Out = ReadNoFence((UINT32*)&control->Out);
            if (!PkpValidatePointer(DataBytesInRing, Out))
            {
                status = EFI_RING_CORRUPT_ERROR;
                goto Cleanup;
            }

            PkLibContext->OutgoingOutCache = Out;
            if (PkpFreeBytes(DataBytesInRing, In, Out) < TotalPacketSize)
            {
                //
                // The ring buffer is really full.
                //

                status = EFI_BUFFER_TOO_SMALL;
                goto Cleanup;
            }

            //
            // The ring buffer is no longer full. Note that now we may receive
            // an extra interrupt, but this is a small enough race that it
            // is acceptable.
            //
        }
    }

Cleanup:

    return status;
}


/// Update the outgoing ring's In pointer (offset) and checks to see if the
/// opposite endpoint must be notified. This entails checking the Out pointer
/// after the In pointer has been set; this requires a memory barrier.
///
/// \param PkLibContext Pointer to the packet library context structure.
/// \param NewIn New In value.
///
/// \retval EFI_SUCCESS The packet was successfully inserted into the ring
///     buffer and the number of bytes that were in the ring before insertion
///     was larger than NonEmptyThreshold.
/// \retval EFI_RING_SIGNAL_OPPOSITE_ENDPOINT The packet was successfully
///     inserted into the ring buffer and the number of bytes that were in the
///     ring buffer before insertion was less than or equal to NonEmptyThreshold
///     and the number of bytes after insertion was greater than
///     NonEmptyThreshold and the opposite endpoint's InterruptMask was clear.
/// \retval EFI_RING_CORRUPT_ERROR The ring buffer itself has become corrupt.
///     No further processing on it is likely to succeed.
EFI_STATUS
PkCompleteInsertion(
    IN   PPACKET_LIB_CONTEXT   PkLibContext,
    IN   UINT32                NewIn
    )
{
    UINT32 oldIn;
    UINT32 currentOut;
    PVMRCB control;
    UINT32 dataBytesInRing;
    UINT32 interruptMask;

    control = PkLibContext->Outgoing.Control;
    dataBytesInRing = PkLibContext->Outgoing.DataBytesInRing;

    ASSERT(PkpValidatePointer(dataBytesInRing, NewIn));

    //
    // Update the stored In pointer.
    //

    oldIn = PkLibContext->OutgoingIn;
    PkLibContext->OutgoingIn = NewIn;

    //
    // Update the public In pointer.
    //
    // NB: This must be a release operation (i.e. no reads or writes will be
    // reordered after this write) so that all writes to the packet are completed
    // before updating the In pointer and the other endpoint seeing the change.
    //

    WriteRelease((UINT32*)&control->In, NewIn);

    //
    // Ensure that the write to the public In pointer is visible before
    // reading the Out pointer. This is necessary to avoiding missing sending
    // a notification.
    //

    MemoryBarrier();

    //
    // Read the interrupt mask bit.
    //

    interruptMask = ReadNoFence((UINT32*)&control->InterruptMask);

    //
    // Read and cache the public Out pointer.
    //

    currentOut = ReadNoFence((UINT32*)&control->Out);
    if (!PkpValidatePointer(dataBytesInRing, currentOut))
    {
        return EFI_RING_CORRUPT_ERROR;
    }

    PkLibContext->OutgoingOutCache = currentOut;

    //
    // Determine if the ring buffer may have been previously empty. If
    // the old In pointer exactly the current Out pointer, send a signal.
    // Otherwise, the opposite endpoint still has data to consume or has
    // already consumed past the insertion point; in either case, no signal
    // is necessary.
    //

    if (oldIn == currentOut)
    {
        if (interruptMask == 0)
        {
            return EFI_RING_SIGNAL_OPPOSITE_ENDPOINT;
        }
        else
        {
            (*PkLibContext->InterruptMaskSkips) += 1;
            return EFI_SUCCESS;
        }
    }
    else
    {
        return EFI_SUCCESS;
    }
}


/// Update the ring's Out pointer (offset). Analogous to PkCompletionInsertion;
/// see comments there for synchronization notes.
///
/// \param PkLibContext Pointer to the packet library context structure.
/// \param NewOut New Out value.
///
/// \retval EFI_SUCCESS The packet was successfully removed from the ring
///     buffer and the number of bytes that were in the ring before removal was
///     smaller than NonFullThreshold.
/// \retval EFI_RING_SIGNAL_OPPOSITE_ENDPOINT The packet was successfully
///     removed from the ring buffer and the number of bytes that were in the
///     ring buffer before removal was greater than or equal to NonFullThreshold
///     and the number of bytes after insertion was less than NonFullThreshold
///     and the opposite endpoint's InterruptMask was clear. This code is not an
///     error, i.e. EFI_ERROR(EFI_RING_SIGNAL_OPPOSITE_ENDPOINT) is false.
/// \retval EFI_RING_NEWLY_EMPTY The incoming ring is now empty.  This can be
///     used as a hint by the client to avoid immediately trying to receive
///     another packet, thus avoiding a pointless lock acquisition.    This code
///     is not an error, i.e. EFI_ERROR(EFI_RING_NEWLY_EMPTY) is false.
EFI_STATUS
PkCompleteRemoval(
    IN   PPACKET_LIB_CONTEXT   PkLibContext,
    IN   UINT32                NewOut
    )
{
    UINT32 oldFreeBytes;
    UINT32 oldOut;
    UINT32 newFreeBytes;
    UINT32 currentIn;
    UINT32 dataBytesInRing;
    PVMRCB control;
    UINT32 pendingSendSize;

    control = PkLibContext->Incoming.Control;
    dataBytesInRing = PkLibContext->Incoming.DataBytesInRing;

    ASSERT(PkpValidatePointer(dataBytesInRing, NewOut));

    //
    // Mark that an interrupt is expected if the ring is now empty.
    //

    if ((UINT64)ReadNoFence((UINT32*)&control->In) == NewOut)
    {
        PkpExpectInterrupt(PkLibContext, TRUE);
    }

    //
    // Update the stored and public Out pointer.
    //

    oldOut = PkLibContext->IncomingOut;
    PkLibContext->IncomingOut = NewOut;
    WriteNoFence((UINT32*)&control->Out, NewOut);

    //
    // Flush the write to the public Out pointer to ensure that the subsequent
    // read of In will be up-to-date. This is necessary to avoid missing
    // notifications.
    //

    MemoryBarrier();

    //
    // Determine whether an interrupt may be necessary.
    //

    pendingSendSize = ReadNoFence((UINT32*)&control->PendingSendSize);

    //
    // Read and cache the public In pointer.
    //

    currentIn = ReadNoFence((UINT32*)&control->In);
    if (!PkpValidatePointer(dataBytesInRing, currentIn))
    {
        return EFI_RING_CORRUPT_ERROR;
    }

    PkLibContext->IncomingInCache = currentIn;

    //
    // Check to see if this removal frees up enough space for the opposite
    // endpoint to write into the ring.
    //

    if (pendingSendSize != 0)
    {
        //
        // N.B. If the opposite endpoint has produced past the insertion
        // point, the number of free bytes before the send will appear larger
        // than the number of free bytes before the send. In this case, no
        // signal is necessary since the opposite endpoint has already
        // "noticed" the extra free space.
        //

        oldFreeBytes = PkpFreeBytes(dataBytesInRing, currentIn, oldOut);
        newFreeBytes = PkpFreeBytes(dataBytesInRing, currentIn, NewOut);
        if (newFreeBytes >= pendingSendSize && oldFreeBytes < pendingSendSize)
        {
            return EFI_RING_SIGNAL_OPPOSITE_ENDPOINT;
        }
    }

    if (NewOut == currentIn)
    {
        return EFI_RING_NEWLY_EMPTY;
    }
    else
    {
        return EFI_SUCCESS;
    }
}


/// Retrieves the outgoing ring's current offset, appropriate for passing to
/// PkGetSendBuffer.
///
/// \param PkLibContext Packet library context.
///
/// \returns A 32-bit offset in bytes from where the next packet will be read.
UINT32
PkGetOutgoingRingOffset(
    IN  PPACKET_LIB_CONTEXT PkLibContext
    )
{
    return PkLibContext->OutgoingIn;
}


/// Retrieves the incoming ring's current offset, appropriate for passing to
/// PkGetReceiveBuffer.
///
/// \param PkLibContext Packet library context.
///
/// \returns A 32-bit offset in bytes from where the next packet will be read.
UINT32
PkGetIncomingRingOffset(
    IN  PPACKET_LIB_CONTEXT PkLibContext
    )
{
    return PkLibContext->IncomingOut;
}


/// Gets a pointer to a buffer in the outgoing ring to store a packet. Checks to
/// ensure enough space is available and prepares some control data.
///
/// \param PkLibContext Pointer to the packet library context structure.
/// \param Offset A pointer to the ring offset to write to. Returns the offset
///     of the next free location.
/// \param PacketSize Total number of bytes that will be consumed by the packet
///     that we want to insert into the ring buffer.
/// \param Buffer Returns a pointer to the buffer.
///
/// \returns EFI_STATUS value
EFI_STATUS
PkGetSendBuffer(
    IN      PPACKET_LIB_CONTEXT PkLibContext,
    IN OUT  UINT32* Offset,
    IN      UINT32 PacketSize,
    OUT     VOID* *Buffer
    )
{
    EFI_STATUS status;
    PVMRCB control;
    UINT32 totalPacketSize;
    UINT32 in;
    UINT32 out;
    volatile UINT64 *buffer;
    PREVIOUS_PACKET_OFFSET packetOffset;
    UINT32 dataBytesInRing;
    INT64 PacketOneBeforeEndOffset;
    INT64 PacketEndOffset;

    PacketSize = ALIGN_UP(PacketSize, UINT64);
    totalPacketSize = PacketSize + sizeof(PREVIOUS_PACKET_OFFSET);

    //
    // Grab the In/Out pointers from the cache.
    //

    dataBytesInRing = PkLibContext->Outgoing.DataBytesInRing;
    in = *Offset;
    out = PkLibContext->OutgoingOutCache;

    //
    // Get the correct buffer offset we will write into.
    //

    PacketOneBeforeEndOffset =
        ((INT64)(PkModuloReduce(in + PacketSize - sizeof(UINT64), dataBytesInRing)) - (INT64)in) / (INT64)sizeof(UINT64);
    PacketEndOffset =
        ((INT64)(PkModuloReduce(in + PacketSize, dataBytesInRing)) - (INT64)in) / (INT64)sizeof(UINT64);

    ASSERT(PacketOneBeforeEndOffset <= UINT32_MAX);
    ASSERT(PacketEndOffset <= UINT32_MAX);

    //
    // Pull the buffer into the processor cache.
    //

    buffer = (UINT64*)&PkLibContext->Outgoing.Data[in];
    PrefetchForWrite(&buffer[PacketOneBeforeEndOffset]);
    PrefetchForWrite(buffer);

    //
    // Check if there is enough space in the send buffer for this packet.
    //

    status = PkpCheckSendBufferFreeBytes(PkLibContext,
                                         totalPacketSize,
                                         in,
                                         out,
                                         dataBytesInRing);

    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Zero out the tail parts of the buffer in case of a non-multiple of 8
    // packet size.
    //

    buffer[PacketOneBeforeEndOffset] = 0;

    //
    // Record the original IN pointer for debugging.
    // N.B. Although this is only used in debugging, it must be there to
    // work on old chk-built VSPs and VSCs, which assert on it.
    //

    packetOffset.Reserved = 0;
    packetOffset.Offset = in;
    buffer[PacketEndOffset] = packetOffset.AsUINT64;

    //
    // No send is pending anymore.
    //

    if (PkLibContext->PendingSendSize != 0)
    {
        PkLibContext->PendingSendSize = 0;
        control = PkLibContext->Outgoing.Control;
        WriteNoFence((UINT32*)&control->PendingSendSize, 0);
    }

    //
    // Return the new offset and the buffer.
    //

    *Offset = PkModuloReduce(in + totalPacketSize, dataBytesInRing);
    *Buffer = (VOID*)buffer;
    status = EFI_SUCCESS;

Cleanup:
    return status;
}


/// Retrieves a pointer to a buffer where the next packet can be read from. The
/// caller must take care to avoid security holes due to double-fetching values
/// from this buffer; a malicious remote endpoint can change the data at any
/// time.
///
/// \param PkLibContext Packet library context.
/// \param Offset A pointer to the offset to read from, which can be retrieved
///     from PkGetIncomingRingOffset. Is updated on success with the next
///     packet's offset. Should be passed to PkGetReceiveBuffer to read more
///     packets or PkCompleteRemoval to finish reading.
/// \param Buffer Buffer pointing to ring data to receive.
/// \param Length Returns the length of the buffer.
EFI_STATUS
PkGetReceiveBuffer(
    IN      PPACKET_LIB_CONTEXT PkLibContext,
    IN OUT  UINT32 *Offset,
    OUT     VOID* *Buffer,
    OUT     UINT32 *Length
    )
{
    UINT32 in;
    UINT32 out;
    UINT32 bytesInRing;
    EFI_STATUS status;
    PVMPACKET_DESCRIPTOR header;
    UINT32 packetLength;
    UINT32 totalPacketSize;
    UINT32 dataBytesInRing;

    //
    // Grab the In/NewOut pointers from the cache.
    //

    dataBytesInRing = PkLibContext->Incoming.DataBytesInRing;
    in = PkLibContext->IncomingInCache;
    out = *Offset;

    ASSERT(out < dataBytesInRing);

    if (in == out)
    {
        //
        // The cached version of In did not yield enough space. Try again with
        // the public version of In.
        //
        // NB: This must be an acquire operation (i.e. no following reads or
        // writes will be reordered before this read) so that the header
        // contents below do not get prefetched with stale data.
        //

        in = ReadAcquire((UINT32*)&PkLibContext->Incoming.Control->In);
        if (!PkpValidatePointer(dataBytesInRing, in))
        {
            status = EFI_RING_CORRUPT_ERROR;
            goto Cleanup;
        }

        PkLibContext->IncomingInCache = in;
        if (in == out)
        {
            status = EFI_END_OF_FILE;
            goto Cleanup;
        }
    }

    bytesInRing = PkpDataAvailable(dataBytesInRing, in, out);
    header = (PVMPACKET_DESCRIPTOR)&PkLibContext->Incoming.Data[out];

    //
    // Since packets are aligned to the size of UINT64, as long as the packet length field
    // offset is less than sizeof(UINT64), we don't have to worry about wrapping around the
    // end of the ring buffer. We assert here to keep the assertion with the relevant code.
    //
    static_assert(OFFSET_OF(VMPACKET_DESCRIPTOR, Length8) < sizeof(UINT64), "");

    //
    // Capture the length field and shift it to a byte count.
    //
    // N.B. at this point it's not guaranteed that bytesInRing is bigger than
    // sizeof(VMPACKET_DESCRIPTOR), but the buffer is safe to read in any
    // case.
    //

    packetLength = header->Length8 * 8;

    //
    // Prevent double fetches of the packet length.
    //

    _ReadWriteBarrier();
    totalPacketSize = packetLength + sizeof(PREVIOUS_PACKET_OFFSET);

    //
    // Capture corruptions: length must cover at least the size of the
    // packet descriptor and should not be more than the buffer size.
    //

    if (packetLength < sizeof(VMPACKET_DESCRIPTOR) ||
        totalPacketSize > bytesInRing)
    {
        status = EFI_RING_CORRUPT_ERROR;
        goto Cleanup;
    }

    //
    // Verify the previous packet offset. Only perform this check in checked
    // builds, since the result does not affect anything (it's just a debugging
    // mechanism).
    //

#if DBG
    if (((PPREVIOUS_PACKET_OFFSET)((UINT8*)
        PkLibContext->Incoming.Data + PkModuloReduce((UINT32)((UINT8*)header - PkLibContext->Incoming.Data + packetLength), dataBytesInRing)
        ))->Offset != out)
    {
        status = EFI_RING_CORRUPT_ERROR;
        goto Cleanup;
    }
#endif

    *Buffer = header;
    *Length = packetLength;
    *Offset = PkModuloReduce(out + totalPacketSize, dataBytesInRing);
    status = EFI_SUCCESS;

Cleanup:
    return status;
}


/// This function writes to a single-mapped ring buffer, folding around if
/// necessary.
///
/// Assumes that the caller will synchronize access to the outgoing ring.
///
/// \param PkLibContext Pointer to the packet library context structure.
/// \param PacketBuf Pointer to a buffer containing the packet to be sent.
/// \param PacketBufSize Size of the buffer in bytes.
/// \param Offset The offset in the ring buffer to write. This offset should be
///     (the offset obtained by PkGetOutgoingRingOffset) + an offset into the
///     packet to write.
VOID
PkWritePacketSingleMapped(
    IN  PPACKET_LIB_CONTEXT PkLibContext,
    IN  VOID*               PacketBuf,
    IN  UINT32              PacketBufSize,
    IN  UINT32              Offset
    )
{
    UINT32 in;
    UINT32 dataBytesInRing;
    UINT32 ringBufferEndOffset;
    volatile UINT8 *buffer;

    buffer = PkLibContext->Outgoing.Data;

    dataBytesInRing = PkLibContext->Outgoing.DataBytesInRing;
    in = PkModuloReduce(Offset, dataBytesInRing);

    ringBufferEndOffset = dataBytesInRing - in;
    if (ALIGN_UP(PacketBufSize, UINT64) <= ringBufferEndOffset)
    {
        CopyMem((VOID*)(buffer + in),
                      PacketBuf,
                      PacketBufSize);
    }
    else
    {
        //
        // Handle the case where we must copy a packet around the end of the ring buffer.
        //
        CopyMem((VOID*)(buffer + in),
                      PacketBuf,
                      ringBufferEndOffset);

        CopyMem((VOID*)buffer,
                      (UINT8*)PacketBuf + ringBufferEndOffset,
                      PacketBufSize - ringBufferEndOffset);
    }
}


/// This function peeks a packet from the endpoint's incoming packet ring.
/// PkGetIncomingPacketSize must be called before this is called to get the
/// values for Out and PacketBufSize.
///
/// Assumes that the caller will synchronize access to the incoming ring.
///
/// \param PkLibContext Pointer to the packet library context structure.
/// \param PacketBuf Pointer to a buffer into which part of the incoming packet
///     can be copied.
/// \param PacketBufSize Size of the buffer in bytes.
/// \param Out Ring buffer pointer indicating where the next packet is located,
///     as retrieved from PkGetIncomingPacketSize.
VOID
PkReadPacketSingleMapped(
    IN  PPACKET_LIB_CONTEXT     PkLibContext,
    OUT VOID*                   PacketBuf,
    IN  UINT32                  PacketBufSize,
    IN  UINT32                  Out
    )
{
    volatile UINT8 *buffer;
    UINT32 dataBytesInRing;
    UINT32 ringBufferEndOffset;

    dataBytesInRing = PkLibContext->Incoming.DataBytesInRing;
    buffer = PkLibContext->Incoming.Data;

    Out = PkModuloReduce(Out, dataBytesInRing);

    //
    // See how much space is available up to the end of the ring buffer.
    //
    ringBufferEndOffset = dataBytesInRing - Out;

    //
    // Now grab the packet.
    //

    if (PacketBufSize <= ringBufferEndOffset)
    {
        CopyMem(PacketBuf, (VOID*)(buffer + Out), PacketBufSize);
    }
    else
    {
        //
        // Handle the case where we must copy a packet from around
        // the end of the ring buffer.
        //

        CopyMem(PacketBuf,
                      (VOID*)(buffer + Out),
                      ringBufferEndOffset);

        CopyMem((UINT8*)PacketBuf + ringBufferEndOffset,
                      (VOID*)buffer,
                      PacketBufSize - ringBufferEndOffset);

        ASSERT(PacketBufSize - ringBufferEndOffset + sizeof(PREVIOUS_PACKET_OFFSET) ==
            PkModuloReduce(Out + PacketBufSize + sizeof(PREVIOUS_PACKET_OFFSET), dataBytesInRing));
    }
}


/// This function inserts an entire raw packet into an endpoint's outgoing ring
/// buffer.  It does not use any side-band buffer management.  It does not
/// attempt to manage the packet's header.
///
/// Assumes that the caller will synchronize access to the outgoing ring.
///
/// \param PkLibContext Pointer to the packet library context structure.
/// \param PacketBuf Pointer to a buffer containing the packet to be sent.
/// \param PacketBufSize Size of the buffer in bytes.
///
/// \retval EFI_SUCCESS The packet was successfully inserted into the ring
///     buffer and the number of bytes that were in the ring before insertion
///     was larger than NonEmptyThreshold.
/// \retval EFI_RING_SIGNAL_OPPOSITE_ENDPOINT The packet was successfully
///     inserted into the ring buffer and the number of bytes that were in the
///     ring buffer before insertion was smaller than NonEmptyThreshold.
/// \retval EFI_INVALID_PARAMETER The packet is larger than the total size of
///     the ring.  This packet can never be sent through this ring.
/// \retval EFI_BUFFER_TOO_SMALL The packet is larger than the current free
///     space in the ring.  The packet could be sent through this ring later,
///     when the opposite endpoint consumes some or all of the currently waiting
///     packets.
/// \retval EFI_RING_CORRUPT_ERROR The ring buffer itself has become corrupt.
///     No further processing on it is likely to succeed.
EFI_STATUS
PkSendPacketRaw(
    IN  PPACKET_LIB_CONTEXT PkLibContext,
    IN  VOID*               PacketBuf,
    IN  UINT32              PacketBufSize
    )
{
    EFI_STATUS      status;
    UINT32          newIn;
    UINT8*          buffer;

    ASSERT(PacketBufSize > 0);

    newIn = PkLibContext->OutgoingIn;
    status = PkGetSendBuffer(PkLibContext, &newIn, PacketBufSize, &buffer);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    //
    // Copy the caller supplied data to the ring.
    //

    PkWriteRingBuffer(PkLibContext, buffer, PacketBuf, PacketBufSize);

    //
    // Finally, update the control structure so the data is visible to the
    // other end of the pipe.
    //

    status = PkCompleteInsertion(PkLibContext, newIn);

Cleanup:

    return status;
}


/// This function returns total number of bytes in the ring, regardless of
/// whether they are currently in use.
///
/// \param PkLibContext Pointer to the packet library context structure.
///
/// \returns The number of bytes in the ring.
UINT32
PkGetOutgoingRingSize(
    IN   PPACKET_LIB_CONTEXT PkLibContext
    )
{
    return PkLibContext->Outgoing.DataBytesInRing;
}


/// This function returns a snapshot of the number of bytes that are free within
/// the ring.  The number returned may be inaccurate by the time the function
/// returns, as packets may have been inserted or removed while the function was
/// executing.  Consequently, this function is only useful as a general gauge of
/// activity.
///
/// \param PkLibContext Pointer to the packet library context structure.
///
/// \returns The number of bytes that were free in the ring at some point during
///     the execution of this function.
UINT32
PkGetOutgoingRingFreeBytes(
    IN   PPACKET_LIB_CONTEXT PkLibContext
    )
{
    UINT32          currentOut;
    UINT32          currentIn;
    UINT32          dataBytesInRing;

    dataBytesInRing = PkLibContext->Outgoing.DataBytesInRing;
    currentIn = PkLibContext->OutgoingIn;
    currentOut = ReadNoFence((UINT32*)&PkLibContext->Outgoing.Control->Out);
    if (!PkpValidatePointer(dataBytesInRing, currentOut))
    {
        return 0;
    }

    return PkpFreeBytes(dataBytesInRing, currentIn, currentOut);
}


/// This routine computes the number of interrupts that are expected to arrive
/// from the opposite endpoint.
///
/// \param PkLibContext A pointer to the packet library context.
///
/// \returns TRUE if interrupts are currently masked.
UINT32
PkpExpectedInterruptCount(
    IN   PPACKET_LIB_CONTEXT PkLibContext
    )
{
    return PkLibContext->EmptyRingBufferCount +
           PkLibContext->FullRingBufferCount -
           PkLibContext->NonspuriousInterruptCount;

}


/// This routine increments the number of interrupts that are expected, up to an
/// upper bound of MAXIMUM_EXPECTED_INTERRUPT_COUNT. This count is decremented
/// in PkInterruptArrived.
///
/// \param PkLibContext Struct containing all of the lib's context.
/// \param IsIncoming If TRUE, expect an interrupt because the ring buffer is
///     empty. Otherwise, expect one because it is full.
VOID
PkpExpectInterrupt(
    IN   PPACKET_LIB_CONTEXT PkLibContext,
    IN   BOOLEAN IsIncoming
    )
{
    if (PkpExpectedInterruptCount(PkLibContext) < MAXIMUM_EXPECTED_INTERRUPT_COUNT)
    {
        if (IsIncoming)
        {
            PkLibContext->EmptyRingBufferCount += 1;
        }
        else
        {
            PkLibContext->FullRingBufferCount += 1;
        }
    }
}
