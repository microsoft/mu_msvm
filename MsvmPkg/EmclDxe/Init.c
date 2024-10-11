/** @file
  This file implements setup and teardown parts of the VMBus transport
  library

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "transportp.h"

#define RING_BUFFER_POOL_TAG 'gnrV'

EFI_STATUS
PkInitializeSingleMappedRingBuffer(
    OUT PPACKET_LIB_CONTEXT Context,
    IN  VOID* IncomingControl,
    IN  VOID* IncomingDataPages,
    IN  UINT32 IncomingDataPageCount,
    IN  VOID* OutgoingControl,
    IN  VOID* OutgoingDataPages,
    IN  UINT32 OutgoingDataPageCount
    )
/*++

Routine Description:

    Initializes a single-mapped ring buffer structure. This functions differs from
    PkInitializeDoubleMappedRingBuffer only in the SAL annotation.

Arguments:

    Context - A pointer to the packet library context structure to initialize.

    IncomingControl - A pointer to the incoming control region.

    IncomingDataPages - A pointer to the incoming data pages.

    IncomingDataPageCount - The size of the incoming data buffer, measured in
        pages.

    OutgoingControl - A pointer to the outgoing control region.

    OutgoingDataPages - A pointer to the outgoing data pages.

    OutgoingDataPageCount - The size of the outgoing buffer, measured in
        pages.

Return Value:

    EFI_STATUS.

--*/
{
    ZeroMem(Context, sizeof(*Context));
    Context->Incoming.Control = (PVMRCB)IncomingControl;
    Context->Incoming.Data = IncomingDataPages;
    Context->Incoming.DataBytesInRing = IncomingDataPageCount * EFI_PAGE_SIZE;
    Context->Outgoing.Control = (PVMRCB)OutgoingControl;
    Context->Outgoing.Data = OutgoingDataPages;
    Context->Outgoing.DataBytesInRing = OutgoingDataPageCount * EFI_PAGE_SIZE;
    Context->InterruptMaskSkips = &Context->StaticInterruptMaskSkips;
    return PkpInitRingBufferControl(Context);
}
