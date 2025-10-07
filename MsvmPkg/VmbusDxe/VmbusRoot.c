/** @file
  Provides the root controller and bus implementation for the VMBus driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <IsolationTypes.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/SynchronizationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <PiDxe.h>
#include <Protocol/InternalEventServices.h>

#include "VmbusP.h"

#define VMBUS_SUPPORTED_FEATURE_FLAGS (VMBUS_FEATURE_FLAG_CLIENT_ID)
#define VMBUS_SUPPORTED_FEATURE_FLAGS_PARAVISOR \
    (VMBUS_FEATURE_FLAG_CONFIDENTIAL_CHANNELS)

typedef struct _VMBUS_HOT_MESSAGE
{
    LIST_ENTRY Link;
    VMBUS_MESSAGE Message;

} VMBUS_HOT_MESSAGE;


struct _VMBUS_ROOT_CONTEXT
{
    UINT32 Signature;

    EFI_EVENT WaitForMessage;
    EFI_EVENT ExitBootEvent;

    EFI_EVENT HotAllocationEvent;
    EFI_EVENT HotEvent;
    LIST_ENTRY HotMessageList;

    BOOLEAN Confidential;
    BOOLEAN SintConnected;
    BOOLEAN ContactInitiated;
    BOOLEAN OffersDelivered;
    VMBUS_MESSAGE_RESPONSE GpadlTable[VMBUS_MAX_GPADLS];

    VMBUS_CHANNEL_CONTEXT *Channels[VMBUS_MAX_CHANNELS];
    UINT32 MaxInterruptUsed;
    UINT32 FeatureFlags;
};

INTERNAL_EVENT_SERVICES_PROTOCOL *mInternalEventServices = NULL;

// Values from MsvmPkg.dec
VMBUS_ROOT_ALLOWED_GUIDS gAllowedGuids[VMBUS_NUMBER_OF_ALLOWED_GUIDS] = 
{
    {TRUE, { 0xba6163d9, 0x04a1, 0x4d29, {0xb6, 0x05, 0x72, 0xe2, 0xff, 0xb1, 0xdc, 0x7f} }},   // StorvscDxe
    {TRUE, { 0xf8615163, 0xdf3e, 0x46c5, {0x91, 0x3f, 0xf2, 0xd2, 0xf9, 0x65, 0xed, 0xe} }},    // NetvscDxe
    {TRUE, { 0x44c4f61d, 0x4444, 0x4400, {0x9d, 0x52, 0x80, 0x2e, 0x27, 0xed, 0xe1, 0x9f} }},   // VpcivscDxe
    {FALSE, { 0xda0a7802, 0xe377, 0x4aac, {0x8e, 0x77, 0x05, 0x58, 0xeb, 0x10, 0x73, 0xf8} }},  // VideoDxe
    {FALSE, { 0xc376c1c3, 0xd276, 0x48d2, {0x90, 0xa9, 0xc0, 0x47, 0x48, 0x07, 0x2c, 0x60} }},  // VmbfsDxe
    {FALSE, { 0xf912ad6d, 0x2b17, 0x48ea, {0xbd, 0x65, 0xf9, 0x27, 0xa6, 0x1c, 0x76, 0x84} }}   // SynthKeyDxe
};

EFI_GUID gVmbfsChannelGuid =  {0xc376c1c3, 0xd276, 0x48d2, {0x90, 0xa9, 0xc0, 0x47, 0x48, 0x07, 0x2c, 0x60}};

VOID
EFIAPI
VmbusRootSintNotify(
    IN  VOID *Context
    );

VOID
VmbusRootScanEventFlags(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  volatile HV_SYNIC_EVENT_FLAGS *Flags
    );

BOOLEAN
VmbusRootDispatchMessage(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  HV_MESSAGE *HvMessage
    );

VOID
EFIAPI
VmbusRootExitBootServices(
    IN  EFI_EVENT Event,
    IN  VOID *Context
    );

EFI_STATUS
VmbusRootInitializeContext(
    IN  VMBUS_ROOT_CONTEXT *RootContext
    );

EFI_STATUS
VmbusRootDestroyContext(
    IN  VMBUS_ROOT_CONTEXT *RootContext
    );

EFI_STATUS
VmbusRootDestroyChannel(
    IN  VMBUS_CHANNEL_CONTEXT *ChannelContext
    );

VOID
VmbusRootWaitForMessage(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  BOOLEAN PollForMessage,
    OUT VMBUS_MESSAGE *Message
    );

EFI_STATUS
VmbusRootInitiateContact(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  UINT32 RequestedVersion
    );

EFI_STATUS
VmbusRootNegotiateVersion(
    IN  VMBUS_ROOT_CONTEXT *RootContext
    );

VOID
VmbusRootSendUnload(
    IN  VMBUS_ROOT_CONTEXT *RootContext
    );

EFI_STATUS
VmbusRootCreateChannel(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  VMBUS_CHANNEL_OFFER_CHANNEL *OfferMessage,
    OUT VMBUS_CHANNEL_CONTEXT **ChannelContext OPTIONAL
    );

BOOLEAN
VmbusRootIsChannelAllowed(
    IN  VMBUS_CHANNEL_OFFER_CHANNEL *OfferMessage
);

EFI_STATUS
VmbusRootEnumerateChildren(
    IN  VMBUS_ROOT_CONTEXT *RootContext
    );

VOID
EFIAPI
VmbusRootHotAddAllocation(
    IN  EFI_EVENT Event,
    IN  VOID *Context
    );

VOID
EFIAPI
VmbusRootHotAdd(
    IN  EFI_EVENT Event,
    IN  VOID *Context
    );

EFI_STATUS
EFIAPI
VmbusComponentNameGetDriverName (
    IN  EFI_COMPONENT_NAME_PROTOCOL *This,
    IN  CHAR8 *Language,
    OUT CHAR16 **DriverName
    );

EFI_STATUS
EFIAPI
VmbusComponentNameGetControllerName(
    IN  EFI_COMPONENT_NAME_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_HANDLE ChildHandle OPTIONAL,
    IN  CHAR8 *Language,
    OUT CHAR16 **ControllerName
    );

EFI_STATUS
VmbusRootConnectSint(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  BOOLEAN Reconnect
    );

//
// UEFI does not use any features of the versions in between Win8.1 and Copper,
// so there is no reason to try to request them.
//

static const UINT32 gVmbusSupportedVersions[] =
{
    VMBUS_VERSION_COPPER,
    VMBUS_VERSION_WIN8_1
};

VMBUS_ROOT_CONTEXT mRootContext;

EFI_HANDLE mRootDevice;
EFI_HANDLE mVmbusImageHandle;
HV_CONNECTION_ID gVmbusConnectionId = {VMBUS_MESSAGE_CONNECTION_ID};
EFI_GUID *mVmbusLegacyProtocolGuid;

VMBUS_ROOT_DEVICE_PATH gVmbusRootDevicePath;

VMBUS_ROOT_NODE gVmbusRootNode =
{
    {
        {
            ACPI_DEVICE_PATH,
            ACPI_EXTENDED_DP,
            {
                (UINT8) (sizeof (VMBUS_ROOT_NODE)),
                (UINT8) ((sizeof (VMBUS_ROOT_NODE)) >> 8)
            }
        },
        0,
        0,
        0
    },
    VMBUS_ROOT_NODE_HID_STR,
    '\0',
    '\0'
};

EFI_DEVICE_PATH_PROTOCOL gEfiEndNode =
{
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
        (UINT8) (END_DEVICE_PATH_LENGTH),
        (UINT8) ((END_DEVICE_PATH_LENGTH) >> 8)
    }
};


EFI_STATUS
VmbusRootInitializeContext(
    IN  VMBUS_ROOT_CONTEXT *RootContext
    )
/**
    This routine initializes a root context.

    @param RootContext Pointer to the root context to initialize.

    @returns EFI_STATUS.

**/
{
    EFI_STATUS status;

    ZeroMem(RootContext, sizeof(*RootContext));
    RootContext->Signature = VMBUS_ROOT_CONTEXT_SIGNATURE;
    InitializeListHead(&RootContext->HotMessageList);

    //
    // When hardware isolation is in use, VmBus must first attempt to connect
    // to the paravisor using encrypted memory. If this fails, VmBus will
    // fall back to using isolated hypercalls and host-visible memory.
    //

    RootContext->Confidential = IsHardwareIsolated() && IsParavisorPresent();
    RootContext->SintConnected = FALSE;
    RootContext->ContactInitiated = FALSE;
    RootContext->OffersDelivered = FALSE;

    status =
        gBS->CreateEvent(
            0,
            0,
            NULL,
            NULL,
            &RootContext->WaitForMessage);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to create event for WaitForMessage - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    //
    // Set the hot event to the lowest TPL possible so any driver unbindings
    // triggered by hot-remove can safely stop the EMCL channel.
    //
    status =
        gBS->CreateEvent(
            EVT_NOTIFY_SIGNAL,
            TPL_APPLICATION + 1,
            VmbusRootHotAdd,
            (VOID*) RootContext,
            &RootContext->HotEvent);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to create event for HotEvent - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    //
    // Set the hot allocation event to the highest TPL that allows us to
    // allocate memory for the hot message.
    //

    status =
        gBS->CreateEvent(
            EVT_NOTIFY_SIGNAL,
            TPL_NOTIFY,
            VmbusRootHotAddAllocation,
            (VOID*) RootContext,
            &RootContext->HotAllocationEvent);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to create event for hot allocation event - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    status = EFI_SUCCESS;

Cleanup:
    if (EFI_ERROR(status))
    {
        VmbusRootDestroyContext(RootContext);
    }

    return status;
}


EFI_STATUS
VmbusRootDestroyChannel(
    IN  VMBUS_CHANNEL_CONTEXT *ChannelContext
    )
/**
    This routine destroys a channel handle by uninstalling the VMBus and Device
    Path protocols and then destroying the channel context.

    @param ChannelContext Pointer to the channel context to destroy.

    @returns EFI_STATUS.

*/
{
    EFI_STATUS status;

    DEBUG((EFI_D_INFO, "%a(%d) channelContext = %p ChannelId 0x%x\n",
        __FUNCTION__,
        __LINE__,
        ChannelContext,
        ChannelContext->ChannelId));

    status = gBS->UninstallMultipleProtocolInterfaces(
        ChannelContext->Handle,
        &gEfiVmbusProtocolGuid,
        &ChannelContext->VmbusProtocol,
        &gEfiDevicePathProtocolGuid,
        &ChannelContext->DevicePath,
        NULL);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: could not uninstall VmBus protocol - %r \n", __FUNCTION__, status));
        return status;
    }

    status = gBS->UninstallMultipleProtocolInterfaces(
        ChannelContext->Handle,
        mVmbusLegacyProtocolGuid,
        &ChannelContext->LegacyVmbusProtocol,
        NULL);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: could not uninstall legacy VmBus protocol - %r \n", __FUNCTION__, status));
        return status;
    }

    gBS->CloseProtocol(mRootDevice,
                       &gEfiVmbusRootProtocolGuid,
                       mVmbusImageHandle,
                       ChannelContext->Handle);

    ASSERT(ChannelContext->RootContext->Channels[ChannelContext->ChannelId] != NULL);

    ChannelContext->RootContext->Channels[ChannelContext->ChannelId] = NULL;
    VmbusChannelDestroyContext(ChannelContext);
    FreePool(ChannelContext);
    return EFI_SUCCESS;
}


EFI_STATUS
VmbusRootDestroyContext (
    IN  VMBUS_ROOT_CONTEXT *RootContext
    )
/**
    This routine destroys a root context.

    @param RootContext Pointer to the root context to destroy.

    @returns  EFI_STATUS.

**/
{
    EFI_STATUS status;
    UINT32 index;
    VMBUS_HOT_MESSAGE *hotMessage;

    DEBUG((EFI_D_INFO, "%a(%d) RootContext = %p\n",
        __FUNCTION__,
        __LINE__,
        RootContext));

    if (RootContext->ContactInitiated)
    {
        VmbusRootSendUnload(RootContext);
        RootContext->ContactInitiated = FALSE;
        RootContext->OffersDelivered = FALSE;
    }

    if (RootContext->SintConnected)
    {
        mHv->DisconnectSint(mHv, FixedPcdGet8(PcdVmbusSintIndex));
        RootContext->SintConnected = FALSE;
    }

    for (index = 0; index < VMBUS_MAX_CHANNELS; ++index)
    {
        if (RootContext->Channels[index] != NULL)
        {
            status = VmbusRootDestroyChannel(RootContext->Channels[index]);
            if (EFI_ERROR(status))
            {
                DEBUG((EFI_D_ERROR, "--- %a: failed to destroy channel - %r \n", __FUNCTION__, status));
                return status;
            }
            ASSERT(RootContext->Channels[index] == NULL);
        }
    }

    if (RootContext->WaitForMessage != NULL)
    {
        gBS->CloseEvent(RootContext->WaitForMessage);
        RootContext->WaitForMessage = NULL;
    }

    if (RootContext->ExitBootEvent != NULL)
    {
        gBS->CloseEvent(RootContext->ExitBootEvent);
        RootContext->ExitBootEvent = NULL;
    }

    if (RootContext->HotEvent != NULL)
    {
        gBS->CloseEvent(RootContext->HotEvent);
        RootContext->HotEvent = NULL;
    }

    if (RootContext->HotAllocationEvent != NULL)
    {
        gBS->CloseEvent(RootContext->HotAllocationEvent);
        RootContext->HotAllocationEvent = NULL;
    }

    while (!IsListEmpty(&RootContext->HotMessageList))
    {
        hotMessage = BASE_CR(RemoveEntryList(GetFirstNode(&RootContext->HotMessageList)),
                             VMBUS_HOT_MESSAGE,
                             Link);

        FreePool(hotMessage);
    }

    for (index = 0; index < VMBUS_MAX_GPADLS; ++index)
    {

        //
        // All drivers above should have released all GPADLs by now.
        //
        ASSERT(!VmbusRootValidateGpadl(RootContext, index));

        VmbusRootReclaimGpadl(RootContext, index);
    }

    return EFI_SUCCESS;
}


VOID
VmbusRootWaitForMessage(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  BOOLEAN PollForMessage,
    OUT VMBUS_MESSAGE *Message
    )
/**
    This routine waits for a message targeted at the root device.
    This routine must be called at TPL < TPL_NOTIFY.

    @params RootContext Pointer to the root context.

    @params PollForMessage poll for a message instead of waiting for event

    @params Message Returns the message received.

    @returns nothing.

**/
{   UINTN index;
    HV_MESSAGE *hvMessage;
    EFI_STATUS status;

    //
    // TPL must be less than TPL_NOTIFY, since hot add/remove messages are
    // processed in events at that TPL and will block all other messages.
    //
    ASSERT(EfiGetCurrentTpl() < TPL_NOTIFY);
    ASSERT(RootContext->SintConnected);

    if (!PollForMessage)
    {
        if (mInternalEventServices == NULL)
        {
            status = gBS->LocateProtocol(
                &gInternalEventServicesProtocolGuid,
                NULL,
                (VOID **)&mInternalEventServices);
            ASSERT_EFI_ERROR(status);
        }

        status = mInternalEventServices->WaitForEventInternal(1, &RootContext->WaitForMessage, &index);
        ASSERT_EFI_ERROR(status);
    }

    hvMessage = NULL;
    while (hvMessage == NULL)
    {
        hvMessage = mHv->GetSintMessage(mHv, FixedPcdGet8(PcdVmbusSintIndex), RootContext->Confidential);
    }

    //
    // Read the message size and store it before validation to avoid
    // double fetch.
    //
    Message->Size = hvMessage->Header.PayloadSize;

    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(Message->Size <= MAXIMUM_SYNIC_MESSAGE_BYTES);

    CopyMem(Message->Data, hvMessage->Payload, Message->Size);
    status = mHv->CompleteSintMessage(mHv, FixedPcdGet8(PcdVmbusSintIndex), RootContext->Confidential);
    ASSERT_EFI_ERROR(status);
}


VMBUS_MESSAGE*
VmbusRootWaitForChannelResponse(
    IN  VMBUS_CHANNEL_CONTEXT *ChannelContext
    )
/**
    This routine waits for a message targeted at a specific channel.
    This routine must be called at TPL < TPL_NOTIFY.

    @param ChannelContext The channel to which the message is targeted to.

    @returns The message received.

**/
{
    EFI_STATUS status;
    UINTN index;

    //
    // TPL must be less than TPL_NOTIFY, since hot add/remove messages are
    // processed in events at that TPL and will block all other messages.
    //
    ASSERT(EfiGetCurrentTpl() < TPL_NOTIFY);

    if (mInternalEventServices == NULL)
    {
        status = gBS->LocateProtocol(
                    &gInternalEventServicesProtocolGuid,
                    NULL,
                    (VOID **)&mInternalEventServices);
        ASSERT_EFI_ERROR(status);
    }

    //
    // This can be called from TPL_CALLBACK. Use WaitForEventInternal instead of gBS->WaitForEvent
    // which enforces a TPL check for TPL_APPLICATION.
    //
    status = mInternalEventServices->WaitForEventInternal(1, &ChannelContext->Response.Event, &index);

    ASSERT_EFI_ERROR(status);

    return &ChannelContext->Response.Message;
}


EFI_STATUS
VmbusRootWaitForGpadlResponse(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  UINT32 GpadlHandle,
    OUT VMBUS_MESSAGE **Message
    )
/**
    This routine waits for a message targeted at a specific GPADL.
    This routine must be called at TPL < TPL_NOTIFY.

    @param RootContext Pointer to the root context.

    @param GpadlHandle GPADL handle to wait on.

    @param Message Returns the message.

    @returns EFI_STATUS.

**/
{
    EFI_STATUS status;
    UINTN index;

    //
    // TPL must be less than TPL_NOTIFY, since hot add/remove messages are
    // processed in events at that TPL and will block all other messages.
    //
    ASSERT(EfiGetCurrentTpl() < TPL_NOTIFY);

    if (RootContext->GpadlTable[GpadlHandle].Event == NULL)
    {
        status = EFI_INVALID_PARAMETER;
        DEBUG((EFI_D_ERROR, "--- %a: invalid handle event for the GPADL - %r \n", __FUNCTION__, status));
        return status;
    }

   if (mInternalEventServices == NULL)
   {
        status = gBS->LocateProtocol(
                    &gInternalEventServicesProtocolGuid,
                    NULL,
                    (VOID **)&mInternalEventServices);
        ASSERT_EFI_ERROR(status);
    }

    //
    // This can be called from TPL_CALLBACK. Use WaitForEventInternal instead of gBS->WaitForEvent
    // which enforces a TPL check for TPL_APPLICATION.
    //
    status = mInternalEventServices->WaitForEventInternal(
                                        1,
                                        &RootContext->GpadlTable[GpadlHandle].Event,
                                        &index);

    ASSERT_EFI_ERROR(status);

    *Message = &RootContext->GpadlTable[GpadlHandle].Message;
    return EFI_SUCCESS;
}


VOID
VmbusRootInitializeMessage(
    IN OUT  VMBUS_MESSAGE *Message,
    IN      VMBUS_CHANNEL_MESSAGE_TYPE Type,
    IN      UINT32 Size
    )
/**
    This routine initializes a VMBus message.

    @param Message Pointer to the VMBus message to initialize.

    @param Type Type of the message.

    @param Size Size of the message.

    @returns nothing.

**/
{
    ZeroMem(Message, sizeof(*Message));
    Message->Size = Size;
    Message->Header.MessageType = Type;
}


EFI_STATUS
VmbusRootSendMessage(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  VMBUS_MESSAGE *Message
    )
/**
    This routine synchronously sends a VMBus message to the opposite endpoint.

    @param Message VMBus message to send.

    @returns nothing.

**/
{
    EFI_STATUS status;

    do
    {
        status = mHv->PostMessage(mHv,
                                  gVmbusConnectionId,
                                  VMBUS_MESSAGE_TYPE,
                                  Message->Data,
                                  Message->Size,
                                  RootContext->Confidential);

    } while (status == EFI_NOT_READY);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "Vmbus failed to send message, confidential=%d\n", RootContext->Confidential));
    }

    return status;
}


VOID
EFIAPI
VmbusRootSintNotify (
    IN  VOID *Context
    )
/**
    This interrupt callback scans event flags and dispatches VMBus messages when
    a VMBus SINT is received.

    @param Context Pointer to the interrupt context, which is a pointer to the root
    context.

    @returns nothing.

**/
{
    VMBUS_ROOT_CONTEXT *rootContext;
    HV_MESSAGE *hvMessage;
    EFI_STATUS status;

    rootContext = (VMBUS_ROOT_CONTEXT*)Context;

    VmbusRootScanEventFlags(rootContext,
                            mHv->GetSintEventFlags(mHv, FixedPcdGet8(PcdVmbusSintIndex), FALSE));

#if defined(MDE_CPU_X64)

    //
    // If a confidential connection is used, the paravisor's event flags page
    // must also be scanned.
    //
    if (rootContext->Confidential)
    {
        VmbusRootScanEventFlags(rootContext,
                                mHv->GetSintEventFlags(mHv, FixedPcdGet8(PcdVmbusSintIndex), TRUE));
    }

#endif

    hvMessage = mHv->GetSintMessage(mHv, FixedPcdGet8(PcdVmbusSintIndex), rootContext->Confidential);

    if (hvMessage != NULL)
    {
        if (VmbusRootDispatchMessage(rootContext, hvMessage))
        {
            status = mHv->CompleteSintMessage(mHv, FixedPcdGet8(PcdVmbusSintIndex), rootContext->Confidential);
            ASSERT_EFI_ERROR(status);
        }
    }
}


VOID
VmbusRootScanEventFlags(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  volatile HV_SYNIC_EVENT_FLAGS *Flags
    )
/**
    This routine scans the hypervisor event flags and signals interrupt events
    that channels have registered.

    This routine must be called at TPL == TPL_HIGH_LEVEL.

    @param RootContext Pointer to the root context.

    @param Flags Hypervisor flags to scan.

    @returns nothing.

**/
{
    UINT64 *flags;
    UINT32 wordIndex;
    UINT32 bitIndex;
    UINT64 currentWord;
    UINT32 wordCount;

    flags = (UINT64*)Flags->Flags32;

    //
    // Scan through all the words up to and including largest interrupt flag
    // used.
    //
    wordCount = RootContext->MaxInterruptUsed / 64 + 1;
    for (wordIndex = 0; wordIndex < wordCount; ++wordIndex)
    {
        // Removing intrinsic function because of MSVC 14.44 linker error
        // currentWord = (UINT64) _InterlockedExchange64((INT64*) &flags[wordIndex], (INT64)0);
        currentWord = InterlockedCompareExchange64((INT64*) &flags[wordIndex],(INT64) flags[wordIndex], (INT64)0);
        while(_BitScanForward64(&bitIndex, currentWord) != 0)
        {
            currentWord &= ~((UINT64)1 << bitIndex);
            gBS->SignalEvent(
                    RootContext->Channels[wordIndex * 64 + bitIndex]->Interrupt);
        }
    }
}


BOOLEAN
VmbusRootDispatchMessage(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  HV_MESSAGE *HvMessage
    )
/**
    This routine dispatches a hypervisor message based on its type, notifying
    either the root device, a channel device, or a GPADL handle.

    This routine must be called at TPL == TPL_HIGH_LEVEL.

    This routine receives a message from the host and therefore
    must validate this message before using it.

    @param RootContext Pointer to the root context.

    @param HvMessage Hypervisor message to dispatch.

    @returns TRUE if hypervisor message should be completed, FALSE otherwise.

**/
{
    VMBUS_MESSAGE *message;
    VMBUS_MESSAGE_RESPONSE *response;
    BOOLEAN completeMessage;
    UINT32 childId;
    UINT32 gpadl;

    completeMessage = TRUE;
    response = NULL;

    childId = 0;
    gpadl = 0;

    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(HvMessage->Header.MessageType == VMBUS_MESSAGE_TYPE);

    message = BASE_CR(HvMessage->Payload, VMBUS_MESSAGE, Header);

    switch (message->Header.MessageType)
    {
    case ChannelMessageOfferChannel:

        //
        // Hot add events need to drop TPL to allocate memory and should queue
        // up messages behind them, so don't complete this message.
        //
        if (RootContext->OffersDelivered)
        {
            gBS->SignalEvent(RootContext->HotAllocationEvent);
            completeMessage = FALSE;
            break;
        }
        // fallthrough

    case ChannelMessageVersionResponse:
        // fallthrough

    case ChannelMessageAllOffersDelivered:
        // fallthrough

    case ChannelMessageUnloadComplete:

        //
        // These messages are dealt with differently, since they arrive
        // synchronously during initialization and are not channel or GPADL-
        // specific.
        //
        gBS->SignalEvent(RootContext->WaitForMessage);
        completeMessage = FALSE;
        break;

    case ChannelMessageOpenChannelResult:

        //
        // Store the channel ID before validating to avoid a double fetch.
        //
        childId = message->OpenResult.ChildRelId;
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(childId < VMBUS_MAX_CHANNELS);
        response = &RootContext->Channels[childId]->Response;

        break;

    case ChannelMessageGpadlTorndown:

        //
        // Store the GPADL before validating to avoid a double fetch.
        //
        gpadl = message->GpadlTorndown.Gpadl;
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(gpadl < VMBUS_MAX_GPADLS);
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(VmbusRootValidateGpadl(RootContext, gpadl));
        response = &RootContext->GpadlTable[gpadl];

        break;

    case ChannelMessageGpadlCreated:

        //
        // Store the GPADL before validating to avoid a double fetch.
        //
        gpadl = message->GpadlCreated.Gpadl;
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(gpadl < VMBUS_MAX_GPADLS);
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(VmbusRootValidateGpadl(RootContext, gpadl));
        response = &RootContext->GpadlTable[gpadl];

        break;

    case ChannelMessageRescindChannelOffer:

        //
        // Hot remove is not supported because UEFI makes it difficult to
        // guarantee a channel will not be used once it is gone. Silently accept
        // rescind messages but never send a RelIdReleased in response.
        //
        break;

    default:
        ASSERT(!"Vmbus received unexpected message");

        break;
    }

    if (response != NULL)
    {
        // Validate the payload size coming in from the host.
        // Validate a locally stored value to avoid a double fetch.
        response->Message.Size = HvMessage->Header.PayloadSize;
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(response->Message.Size <= MAXIMUM_SYNIC_MESSAGE_BYTES);

        CopyMem(response->Message.Data,
                HvMessage->Payload,
                response->Message.Size);

        gBS->SignalEvent(response->Event);
    }

    return completeMessage;
}


VOID
EFIAPI
VmbusRootHotAddAllocation(
    IN  EFI_EVENT Event,
    IN  VOID * Context
    )
/**
    This routine allocates space for hot add messages and copies the message
    from the SINT queue, to be processed by VmbusRootHotAdd.

    This routine receives a message from the host and therefore
    must validate this message before using it.

    @param Event The event that was signalled.

    @param Context A pointer to the event context, which is the root context.

    @returns nothing.

**/
{
    VMBUS_ROOT_CONTEXT *context;
    HV_MESSAGE *hvMessage;
    VMBUS_HOT_MESSAGE *hotMessage;
    EFI_STATUS status;

    ASSERT(EfiGetCurrentTpl() == TPL_NOTIFY);

    context = (VMBUS_ROOT_CONTEXT*)Context;
    hvMessage = mHv->GetSintMessage(mHv, FixedPcdGet8(PcdVmbusSintIndex), context->Confidential);
    if (hvMessage == NULL)
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to get hot message\n", __FUNCTION__));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    hotMessage = AllocatePool(sizeof(*hotMessage));
    if (hotMessage == NULL)
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to allocate hot message - %r \n", __FUNCTION__, EFI_OUT_OF_RESOURCES));
        goto Cleanup;
    }

    ZeroMem(hotMessage, sizeof(*hotMessage));

    hotMessage->Message.Size = hvMessage->Header.PayloadSize;

    CopyMem(hotMessage->Message.Data,
            hvMessage->Payload,
            sizeof(hotMessage->Message.OfferChannel));
            
    if (hotMessage->Message.Header.MessageType != ChannelMessageOfferChannel ||
        hotMessage->Message.Size != sizeof(hotMessage->Message.OfferChannel) ||
        hotMessage->Message.OfferChannel.ChildRelId >= VMBUS_MAX_CHANNELS)
    {
        DEBUG((EFI_D_ERROR, "--- %a: invalid offer message: %#x (size %d), rel ID %d",
            __FUNCTION__,
            hotMessage->Message.Header.MessageType,
            hotMessage->Message.Size,
            hotMessage->Message.OfferChannel.ChildRelId));
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR();
    }

    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(hotMessage->Message.Header.MessageType == ChannelMessageOfferChannel);

    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(hotMessage->Message.OfferChannel.ChildRelId < VMBUS_MAX_CHANNELS);

    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(context->Channels[hotMessage->Message.OfferChannel.ChildRelId] == NULL);

    //
    // Do not proceed if this channel is not allowed during UEFI boot.
    //
    if (!VmbusRootIsChannelAllowed(&hotMessage->Message.OfferChannel))
    {
        FreePool(hotMessage);
        goto Cleanup;
    }

    InsertTailList(&context->HotMessageList, &hotMessage->Link);
    gBS->SignalEvent(context->HotEvent);

Cleanup:
    status = mHv->CompleteSintMessage(mHv, FixedPcdGet8(PcdVmbusSintIndex), context->Confidential);
    ASSERT_EFI_ERROR(status);
}


VOID
EFIAPI
VmbusRootHotAdd(
    IN  EFI_EVENT Event,
    IN  VOID *Context
    )
/**
    This routine processes hot-add messages. Hot-remove is not supported under UEFI,
    as we cannot guarantee that a channel isn't being used (or block on it) when
    it's being removed.

    @param Event The event that was signalled.

    @param Context A pointer to the event context, which is the root context.

    @returns nothing.

**/
{
    EFI_STATUS status;
    EFI_TPL tpl;
    LIST_ENTRY list;
    VMBUS_ROOT_CONTEXT *context;
    VMBUS_CHANNEL_CONTEXT *channelContext;
    VMBUS_HOT_MESSAGE *hotMessage;

    context = (VMBUS_ROOT_CONTEXT*)Context;
    channelContext = NULL;
    InitializeListHead(&list);

    tpl = gBS->RaiseTPL(TPL_NOTIFY);

    //
    // While TPL is raised, copy list of messages locally.
    //
    if (!IsListEmpty(&context->HotMessageList))
    {
        list = context->HotMessageList;
        list.ForwardLink->BackLink = &list;
        list.BackLink->ForwardLink = &list;
        InitializeListHead(&context->HotMessageList);
    }

    gBS->RestoreTPL(tpl);
    while (!IsListEmpty(&list))
    {
        hotMessage = BASE_CR(GetFirstNode(&list), VMBUS_HOT_MESSAGE, Link);

        //
        // The offer message is validated before adding it to the list.
        //
        ASSERT(hotMessage->Message.Header.MessageType == ChannelMessageOfferChannel);
        ASSERT(hotMessage->Message.Size == sizeof(hotMessage->Message.OfferChannel));

        status = VmbusRootCreateChannel(context,
                                        &hotMessage->Message.OfferChannel,
                                        &channelContext);

        if (EFI_ERROR(status))
        {
            DEBUG((EFI_D_ERROR, "--- %a: failed to create the channel - %r \n", __FUNCTION__, status));
        }
        else
        {

            //
            // ConnectController must be manually called to hook this channel up to
            // any drivers that can manage it.
            //
            gBS->ConnectController(channelContext->Handle,
                                   NULL,
                                   NULL,
                                   TRUE);
        }

        RemoveEntryList(&hotMessage->Link);
        FreePool(hotMessage);
    }
}


EFI_STATUS
VmbusRootGetFreeGpadl(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    OUT UINT32 *GpadlHandle
    )
/**
    This routine allocates a new GPADL and returns its handle.

    This routine must be called at TPL <= TPL_VMBUS.

    @param RootContext Pointer to the root context.

    @param GpadlHandle Handle of the GPADL that was created.

    @returns EFI_STATUS.

**/
{
    EFI_STATUS status;
    EFI_TPL tpl;
    EFI_EVENT event;
    UINT32 index;

    event = NULL;

    status = gBS->CreateEvent(0,
                              0,
                              NULL,
                              NULL,
                              &event);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to create event - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    tpl = gBS->RaiseTPL(TPL_VMBUS);

    //
    // The whole GPADL array is scanned for a free entry.
    // TODO: Make this more efficient.
    //
    for (index = 1; index < VMBUS_MAX_GPADLS; ++index)
    {
        if (RootContext->GpadlTable[index].Event == NULL)
        {
            *GpadlHandle = index;

            //
            // Create a new event to mark it as taken.
            //
            RootContext->GpadlTable[index].Event = event;
            break;
        }
    }

    gBS->RestoreTPL(tpl);

    if (index == VMBUS_MAX_GPADLS)
    {
        status = EFI_OUT_OF_RESOURCES;
        DEBUG((EFI_D_ERROR, "--- %a: failed to find an available GPADL - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    status = EFI_SUCCESS;

Cleanup:
    if (EFI_ERROR(status))
    {
        if (event != NULL)
        {
            gBS->CloseEvent(event);
        }
    }

    return status;
}


VOID
VmbusRootReclaimGpadl(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  UINT32 GpadlHandle
    )
/**
    This routine releases a GPADL to be reused.

    @param RootContext Pointer to the root context.

    @param GpadlHandle GPADL handle to release.

    @returns nothing.

**/
{
    VMBUS_MESSAGE_RESPONSE *gpadlEntry;

    gpadlEntry = &RootContext->GpadlTable[GpadlHandle];
    if (gpadlEntry->Event != NULL)
    {
        gBS->CloseEvent(gpadlEntry->Event);
        gpadlEntry->Event = NULL;
    }
}

BOOLEAN
VmbusRootValidateGpadl(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  UINT32 GpadlHandle
    )
/**
    This routine verifies if the provided GPADL handle is valid.

    @param RootContext Pointer to the root context.

    @param GpadlHandle GPADL handle to verify.

    @returns TRUE if the GPADL has been created previously, FALSE otherwise.

**/
{
    return RootContext->GpadlTable[GpadlHandle].Event != NULL;
}

VOID
VmbusRootSetInterruptEntry(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  UINT32 ChannelId,
    IN  EFI_EVENT Event
    )

/**
    This routine registers an interrupt event for a channel.

    @param RootContext Pointer to the root context.

    @param ChannelId Index of the interrupt to set.

    @param Event The event to signal when the interrupt occurs.

    @returns nothing.

**/

{
    EFI_TPL tpl;

    ASSERT(ChannelId < VMBUS_MAX_CHANNELS);

    tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);
    if (ChannelId > RootContext->MaxInterruptUsed)
    {
        RootContext->MaxInterruptUsed = ChannelId;
    }

    RootContext->Channels[ChannelId]->Interrupt = Event;
    gBS->RestoreTPL(tpl);
}


VOID
VmbusRootClearInterruptEntry(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  UINT32 ChannelId
    )
/**
    This routine unregisters an interrupt for a channel.

    @param RootContext Pointer to the root context.

    @param ChannelId Index of the interrupt to clear.

    @returns nothing.

**/
{
    EFI_TPL tpl;
    UINT32 index;

    ASSERT(ChannelId < VMBUS_MAX_CHANNELS);

    tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);

    RootContext->Channels[ChannelId]->Interrupt = NULL;
    if (ChannelId == RootContext->MaxInterruptUsed)
    {
        //
        // Scan backwards for the first set interrupt.
        //

        for (index = RootContext->MaxInterruptUsed; index > 0; --index)
        {
            if ((RootContext->Channels[index] != NULL) &&
                (RootContext->Channels[index]->Interrupt != NULL))
            {
                break;
            }
        }

        RootContext->MaxInterruptUsed = index;
    }

    gBS->RestoreTPL(tpl);
}


VOID
EFIAPI
VmbusRootExitBootServices(
    IN  EFI_EVENT Event,
    IN  VOID *Context
    )
/**
    This event notification sends an unload message when ExitBootServices is
    called.

    @param Event The event that was signalled.

    @param Context A pointer to the event context, which is the root context.

    @returns nothing.

**/
{
    VMBUS_ROOT_CONTEXT *rootContext;
    int i;
    int orphanedGpadlCount = 0;

    rootContext = (VMBUS_ROOT_CONTEXT*)Context;

    for (i = 0; i < VMBUS_MAX_GPADLS; i++);
    {
        if (rootContext->GpadlTable[i].Event)
        {
            DEBUG((EFI_D_WARN,
                "%a (%d) GPADL 0x%x not cleaned up.\n",
                __FUNCTION__,
                __LINE__,
                i));
            orphanedGpadlCount++;
        }
    }

    DEBUG((EFI_D_WARN, "%a (%d) orphaned %d GPADLs (IsolationArchitecture=%d)\n",
        __FUNCTION__,
        __LINE__,
        orphanedGpadlCount,
        GetIsolationType()));

    VmbusRootSendUnload(rootContext);
}


EFI_STATUS
VmbusRootNegotiateVersion(
    IN  VMBUS_ROOT_CONTEXT *RootContext
    )
/**
    This routine initiates contact with the host endpoint and negotiates the
    VMBus version.

    This function must be called at TPL < TPL_HIGH_LEVEL.

    @param RootContext Pointer to the root context.

    @returns EFI_STATUS.

**/
{
    EFI_STATUS status = EFI_PROTOCOL_ERROR;
    UINTN index;
    UINT32 version;

    for (index = 0; index < ARRAY_SIZE(gVmbusSupportedVersions); index++)
    {
        version = gVmbusSupportedVersions[index];
        status = VmbusRootInitiateContact(RootContext, version);
        if (status != EFI_PROTOCOL_ERROR)
        {
            break;
        }

        DEBUG((EFI_D_WARN, "--- %a: host did not support version 0x%x\n", __FUNCTION__, version));
    }

    if (!EFI_ERROR(status))
    {
        DEBUG((EFI_D_INFO, "--- %a: negotiated version 0x%x\n", __FUNCTION__, version));
    }

    return status;
}


EFI_STATUS
VmbusRootInitiateContact(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  UINT32 RequestedVersion
    )
/**
    This routine initiates contact with the host endpoint using the requested
    version.

    This function must be called at TPL < TPL_HIGH_LEVEL.

    This routine receives a message from the host and therefore
    must validate this message before using it.

    @param RootContext Pointer to the root context.

    @param RequestedVersion The protocol version to request from the host.

    @returns EFI_STATUS.

**/
{
    VMBUS_MESSAGE message;
    UINT32 size;
    EFI_STATUS status;

    ASSERT(RootContext->SintConnected);

    size = (RequestedVersion >= VMBUS_VERSION_COPPER)
        ? sizeof(message.InitiateContact)
        : VMBUS_CHANNEL_INITIATE_CONTACT_MIN_SIZE;

    VmbusRootInitializeMessage(&message, ChannelMessageInitiateContact, size);
    message.InitiateContact.VMBusVersionRequested = RequestedVersion;
    message.InitiateContact.TargetMessageVp = mHv->GetCurrentVpIndex(mHv);
    if (RequestedVersion >= VMBUS_VERSION_COPPER)
    {
        message.InitiateContact.ClientId = gMsvmVmbusClientGuid;
        message.InitiateContact.FeatureFlags = VMBUS_SUPPORTED_FEATURE_FLAGS;
        if (RootContext->Confidential)
        {
            message.InitiateContact.FeatureFlags |= VMBUS_SUPPORTED_FEATURE_FLAGS_PARAVISOR;
        }
    }

    RootContext->FeatureFlags = message.InitiateContact.FeatureFlags;
    status = VmbusRootSendMessage(RootContext, &message);
    if (EFI_ERROR(status))
    {
        if (!RootContext->Confidential)
        {
            return status;
        }

        DEBUG((EFI_D_WARN, "--- %a: Retrying without confidential control plane\n", __FUNCTION__));
        RootContext->Confidential = FALSE;
        status = VmbusRootConnectSint(RootContext, TRUE);
        if (EFI_ERROR(status))
        {
            return status;
        }

        //
        // Clear feature flags only supported for confidential connections.
        //

        if (RequestedVersion >= VMBUS_VERSION_COPPER)
        {
            message.InitiateContact.FeatureFlags = VMBUS_SUPPORTED_FEATURE_FLAGS;
            RootContext->FeatureFlags = message.InitiateContact.FeatureFlags;
        }

        status = VmbusRootSendMessage(RootContext, &message);
        if (EFI_ERROR(status))
        {
            return status;
        }
    }

    //
    // We may have leftover messages if this driver was stopped previously.
    //
    do
    {
        VmbusRootWaitForMessage(RootContext, FALSE, &message);
    } while (message.Header.MessageType != ChannelMessageVersionResponse);

    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(message.Size >= VMBUS_CHANNEL_VERSION_RESPONSE_MIN_SIZE);

    if (!message.VersionResponse.VersionSupported ||
        message.VersionResponse.ConnectionState
            != VmbusChannelConnectionSuccessful)
    {
        status = EFI_PROTOCOL_ERROR;

    }
    else
    {
        RootContext->ContactInitiated = TRUE;
        if (RequestedVersion >= VMBUS_VERSION_COPPER)
        {
            FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(message.Size >= sizeof(message.VersionResponse));
            RootContext->FeatureFlags &= message.VersionResponse.SupportedFeatures;
        }

        status = EFI_SUCCESS;
    }
    return status;
}


VOID
VmbusRootSendUnload(
    IN  VMBUS_ROOT_CONTEXT *RootContext
    )
/**
    This routine sends an unload message and synchronously waits for a response
    from the root.

    This function must be called at TPL < TPL_HIGH_LEVEL.

    This routine receives a message from the host and therefore
    must validate this message before using it.

    @param RootContext  Pointer to the root context.

    @returns nothing.

**/
{
    VMBUS_MESSAGE message;

    VmbusRootInitializeMessage(&message,
                               ChannelMessageUnload,
                               sizeof(message.Header));

    VmbusRootSendMessage(RootContext, &message);

    //
    // Ignore all messages until the unload response comes back.
    //
    do
    {
        VmbusRootWaitForMessage(RootContext, TRUE, &message);
    } while (message.Header.MessageType != ChannelMessageUnloadComplete);

    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(message.Size == sizeof(message.Header));
}


EFI_STATUS
VmbusRootCreateChannel(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  VMBUS_CHANNEL_OFFER_CHANNEL *OfferMessage,
    OUT VMBUS_CHANNEL_CONTEXT **ChannelContext OPTIONAL
    )
/**
    This routine constructs a channel from an offer message.

    @param RootContext Pointer to the root context.

    @param OfferMessage The offer message received.

    @param ChannelContext Optionally returns a pointer to the newly constructed
                            channel context.

    @returns EFI_STATUS.

**/
{
    EFI_STATUS status;
    VMBUS_CHANNEL_CONTEXT *channelContext;
    VOID *protocol;
    EFI_TPL tpl;

    channelContext = AllocatePool(sizeof(VMBUS_CHANNEL_CONTEXT));
    if (channelContext == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        DEBUG((EFI_D_ERROR, "--- %a: failed to create event for WaitForMessage - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    VmbusChannelInitializeContext(channelContext,
                                  OfferMessage,
                                  RootContext);

    //
    // The following validations should have been done when the channel offer
    // was received. However, it is possible that the host can send multiple
    // channel offers with the same channel ID which would not be caught
    // unless an entry for this ID was made into the Channels list.
    //
    ASSERT(OfferMessage->ChildRelId < VMBUS_MAX_CHANNELS);

    tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);
    FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(RootContext->Channels[channelContext->ChannelId] == NULL);
    RootContext->Channels[channelContext->ChannelId] = channelContext;
    gBS->RestoreTPL(tpl);

    //
    // Install the Device Path and VMBus protocols onto a new child handle.
    //
    status = gBS->InstallMultipleProtocolInterfaces(&channelContext->Handle,
                                                    &gEfiDevicePathProtocolGuid,
                                                    &channelContext->DevicePath,
                                                    &gEfiVmbusProtocolGuid,
                                                    &channelContext->VmbusProtocol,
                                                    NULL);

    ASSERT_EFI_ERROR(status);

    status = gBS->InstallMultipleProtocolInterfaces(&channelContext->Handle,
                                                    mVmbusLegacyProtocolGuid,
                                                    &channelContext->LegacyVmbusProtocol,
                                                    NULL);

    ASSERT_EFI_ERROR(status);

    //
    // Open the root VMBus tag protocol BY_CHILD_CONTROLLER so EFI can track
    // this relation.
    //
    status = gBS->OpenProtocol(mRootDevice,
                               &gEfiVmbusRootProtocolGuid,
                               &protocol,
                               mVmbusImageHandle,
                               channelContext->Handle,
                               EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to open the VmBus protocol - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    if (ChannelContext != NULL)
    {
        *ChannelContext = channelContext;
    }

    status = EFI_SUCCESS;

Cleanup:
    if (EFI_ERROR(status))
    {
        if (channelContext != NULL)
        {
            VmbusRootDestroyChannel(channelContext);
        }
    }

    return status;
}


BOOLEAN
VmbusRootIsChannelAllowed(
    IN  VMBUS_CHANNEL_OFFER_CHANNEL *OfferMessage
)
/**
    This routine determines if a VmBus channel is allowed or not.

    @param OfferMessage The offer message received that contains the channel details.

    @returns TRUE if the channel is allowed, FALSE otherwise.

**/
{
    int index = 0;
    int allowedGuidCount = 0;

    allowedGuidCount = sizeof(gAllowedGuids) / sizeof(gAllowedGuids[0]);

    for (index = 0; index < allowedGuidCount; index++)
    {
        if (IsIsolated())
        {
            if (gAllowedGuids[index].IsAllowedWhenIsolated == FALSE)
            {
                continue;
            }
        }

        if (CompareMem(&OfferMessage->InterfaceType, &gAllowedGuids[index].AllowedGuid, sizeof(EFI_GUID)) == 0)
        {
            DEBUG((DEBUG_INFO, "%a: Channel allowed during boot (%g).\n", __FUNCTION__, &OfferMessage->InterfaceType));
            return TRUE;
        }
    }

    if (IsIsolated())
    {
        if (PcdGetBool(PcdEnableIMCWhenIsolated))
        {

            //
            // Decide if this is the IMC channel and if it should be allowed.
            //
            if (CompareMem(&OfferMessage->InterfaceType, &gVmbfsChannelGuid, sizeof(EFI_GUID)) == 0)
            {
                DEBUG((DEBUG_INFO, "%a: IMC Channel allowed during boot (%g).\n", __FUNCTION__, &OfferMessage->InterfaceType));
                return TRUE;
            }
        }
    }

    DEBUG((DEBUG_WARN, "%a: Channel not allowed during boot (%g).\n", __FUNCTION__, &OfferMessage->InterfaceType));
    return FALSE;

}


EFI_STATUS
VmbusRootEnumerateChildren(
    IN  VMBUS_ROOT_CONTEXT *RootContext
    )
/**
    This routine receives all VMBus offers from the root, creates a child
    handle for each one, and installs the VMBus and Device Path protocols onto
    them.

    This function must be called at TPL < TPL_HIGH_LEVEL.

    This routine receives a message from the host and therefore
    must validate this message before using it.

    @param RootContext Pointer to the root context.

    @returns EFI_STATUS.

**/
{
    EFI_STATUS status;
    VMBUS_MESSAGE message;

    VmbusRootInitializeMessage(&message,
                               ChannelMessageRequestOffers,
                               sizeof(message.Header));

    VmbusRootSendMessage(RootContext, &message);
    for (;;)
    {
        VmbusRootWaitForMessage(RootContext, FALSE, &message);
        if (message.Size == sizeof(message.Header) &&
            message.Header.MessageType == ChannelMessageAllOffersDelivered)
        {
            RootContext->OffersDelivered = TRUE;
            break;
        }

        if (message.Size != sizeof(message.OfferChannel) ||
            message.Header.MessageType != ChannelMessageOfferChannel)
        {
            status = EFI_PROTOCOL_ERROR;
            DEBUG((EFI_D_ERROR, "--- %a: unexpected VMBus message received from root - %r \n", __FUNCTION__, status));
            return status;
        }

        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(message.OfferChannel.ChildRelId < VMBUS_MAX_CHANNELS);
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(RootContext->Channels[message.OfferChannel.ChildRelId] == NULL);

        //
        // Do not proceed if this channel is not allowed during UEFI boot.
        //
        if (!VmbusRootIsChannelAllowed(&message.OfferChannel))
        {

            //
            // Do nothing for this channel creation.
            //
            continue;
        }

        status = VmbusRootCreateChannel(RootContext,
                                        &message.OfferChannel,
                                        NULL);

        if (EFI_ERROR(status))
        {
            DEBUG((EFI_D_ERROR, "--- %a: failed to create the channel - %r \n", __FUNCTION__, status));
            return status;
        }
    }

    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
VmbusRootDriverSupported (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    )
/**
    Supported routine for VMBus driver binding protocol.

    @param This Pointer to the driver binding protocol.

    @param ControllerHandle Device handle to check if supported.

    @param RemainingDevicePath Device path of child to start.

    @returns EFI_STATUS.

**/
{
    EFI_STATUS status;
    VOID *protocol;

    //
    // Check for the root controller tag GUID and make sure this driver is not
    // already managing this device.
    //
    status = gBS->OpenProtocol(ControllerHandle,
                               &gEfiVmbusRootProtocolGuid,
                               &protocol,
                               This->DriverBindingHandle,
                               ControllerHandle,
                               EFI_OPEN_PROTOCOL_BY_DRIVER);

    if (EFI_ERROR(status))
    {
        return status;
    }

    gBS->CloseProtocol(ControllerHandle,
                       &gEfiVmbusRootProtocolGuid,
                       This->DriverBindingHandle,
                       ControllerHandle);

    return EFI_SUCCESS;
}


EFI_STATUS
VmbusRootConnectSint(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  BOOLEAN Reconnect
    )
{
    EFI_STATUS status;

    //
    // Disconnect first if the SINT was previously connected. This is the case
    // on fallback from attempting a confidential connection to the paravisor.
    //
    if (Reconnect)
    {
        mHv->DisconnectSint(mHv, FixedPcdGet8(PcdVmbusSintIndex));
    }

    status = mHv->ConnectSint(mHv,
                              FixedPcdGet8(PcdVmbusSintIndex),
                              FixedPcdGet8(PcdVmbusSintVector),
                              RootContext->Confidential,
                              VmbusRootSintNotify,
                              RootContext);
    DEBUG((DEBUG_VERBOSE, "--- %a after ConnectSint status %r\n", __FUNCTION__, status));
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to connect SINT - %r \n", __FUNCTION__, status));
    }

    return status;
}


EFI_STATUS
EFIAPI
VmbusRootDriverStart (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
    )
/**
    Start routine for VMBus driver binding protocol.

    @param This Pointer to the driver binding protocol.

    @param ControllerHandle Device handle on which to start.

    @param RemainingDevicePath Device path of device being started.

    @returns EFI_STATUS.

**/
{
    EFI_STATUS status;
    VOID *protocol;
    DEBUG((DEBUG_VERBOSE, ">>> %a\n", __FUNCTION__));

    ASSERT(ControllerHandle == mRootDevice);

    status = gBS->LocateProtocol(&gEfiHvProtocolGuid, NULL, (VOID **)&mHv);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to locate the EfiHv protocol - %r \n", __FUNCTION__, status));
        return status;
    }

    status = gBS->LocateProtocol(&gEfiHvIvmProtocolGuid, NULL, (VOID **)&mHvIvm);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to locate the EfiHvIvm protocol - %r \n", __FUNCTION__, status));
        return status;
    }

    mSharedGpaBoundary = (UINTN)PcdGet64(PcdIsolationSharedGpaBoundary);
    mCanonicalizationMask = PcdGet64(PcdIsolationSharedGpaCanonicalizationBitmask);

    status = VmbusRootInitializeContext(&mRootContext);
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to initialize context - %r \n", __FUNCTION__, status));
        return status;
    }
    DEBUG((DEBUG_VERBOSE, "--- %a after VmbusRootInitializeContext\n", __FUNCTION__));

    status = VmbusRootConnectSint(&mRootContext, FALSE);
    if (EFI_ERROR(status))
    {
        goto Cleanup;
    }

    mRootContext.SintConnected = TRUE;

    status = VmbusRootNegotiateVersion(&mRootContext);
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to initiate contact - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }
    DEBUG((DEBUG_VERBOSE, "--- %a after VmbusRootInitiateContact status %r\n", __FUNCTION__, status));

    status = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,
                                TPL_CALLBACK,
                                VmbusRootExitBootServices,
                                &mRootContext,
                                &gEfiEventExitBootServicesGuid,
                                &mRootContext.ExitBootEvent);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to create the exit boot services event - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    status = VmbusRootEnumerateChildren(&mRootContext);
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to enumerate children - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }
    DEBUG((DEBUG_VERBOSE, "--- %a after VmbusRootEnumerateChildren status %r\n", __FUNCTION__, status));
    status = gBS->OpenProtocol(ControllerHandle,
                               &gEfiVmbusRootProtocolGuid,
                               &protocol,
                               This->DriverBindingHandle,
                               ControllerHandle,
                               EFI_OPEN_PROTOCOL_BY_DRIVER);

    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to open the VMBus protocol - %r \n", __FUNCTION__, status));
        goto Cleanup;
    }

    status = EFI_SUCCESS;

Cleanup:
    if (EFI_ERROR(status))
    {
        VmbusRootDestroyContext(&mRootContext);
    }

    DEBUG((DEBUG_VERBOSE, "<<< %a status %r\n", __FUNCTION__, status));
    return status;
}


EFI_STATUS
EFIAPI
VmbusRootDriverStop (
    IN  EFI_DRIVER_BINDING_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  UINTN NumberOfChildren,
    IN  EFI_HANDLE *ChildHandleBuffer
    )
/**
    Stop routine for VMBus driver binding protocol.

    @param This - Pointer to the driver binding protocol.

    @param ControllerHandle Pointer to the device handle which needs to be stopped.

    @param NumberOfChildren If 0, stop the root controller. Otherwise, the number of
        children in ChildHandleBuffer to be stopped.

    @param ChildHandleBuffer An array of child handles to stop.

    @returns EFI_STATUS.

**/
{
    EFI_STATUS status;
    UINTN childIndex;
    UINTN channelIndex;
    VMBUS_CHANNEL_CONTEXT *channelContext;

    if (NumberOfChildren == 0)
    {
        ASSERT(ControllerHandle == mRootDevice);

        gBS->CloseProtocol(ControllerHandle,
                           &gEfiVmbusRootProtocolGuid,
                           This->DriverBindingHandle,
                           ControllerHandle);

        status = VmbusRootDestroyContext(&mRootContext);
        if (EFI_ERROR(status))
        {
            DEBUG((EFI_D_ERROR, "--- %a: failed to destroy the context - %r \n", __FUNCTION__, status));
            return status;
        }
    }
    else
    {
        for (childIndex = 0; childIndex < NumberOfChildren; ++childIndex)
        {
            for (channelIndex = 0;
                 channelIndex < VMBUS_MAX_CHANNELS;
                 ++channelIndex)
            {
                channelContext = mRootContext.Channels[channelIndex];
                if (channelContext != NULL &&
                    channelContext->Handle == ChildHandleBuffer[childIndex])
                {
                    status = VmbusRootDestroyChannel(channelContext);
                    if (EFI_ERROR(status))
                    {
                        DEBUG((EFI_D_ERROR, "--- %a: failed to destroy the channel - %r \n", __FUNCTION__, status));
                        return status;
                    }

                    ASSERT(mRootContext.Channels[channelIndex] == NULL);
                    break;
                }
            }

            if (channelIndex == VMBUS_MAX_CHANNELS)
            {
                ASSERT(!"VMBus stop call received invalid child");
            }
        }
    }

    return EFI_SUCCESS;
}


//
// Driver name table
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE gVmbusDriverNameTable[] =
{
    { "eng;en", (CHAR16 *)L"Hyper-V VMBus Driver"},
    { NULL, NULL }
};


//
// Controller name table
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE gVmbusControllerNameTable[] =
{
    { "eng;en", (CHAR16 *)L"Hyper-V VMBus Controller"},
    { NULL, NULL }
};


//
// EFI Component Name Protocol
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME_PROTOCOL gVmbusComponentName =
{
    VmbusComponentNameGetDriverName,
    VmbusComponentNameGetControllerName,
    "eng"
};

//
// EFI Component Name 2 Protocol
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME2_PROTOCOL gVmbusComponentName2 =
{
    (EFI_COMPONENT_NAME2_GET_DRIVER_NAME) VmbusComponentNameGetDriverName,
    (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME) VmbusComponentNameGetControllerName,
    "en"
};


EFI_DRIVER_BINDING_PROTOCOL gVmbusDriverBindingProtocol =
{
    VmbusRootDriverSupported,
    VmbusRootDriverStart,
    VmbusRootDriverStop,
    VMBUS_DRIVER_VERSION,
    NULL,
    NULL
};


EFI_STATUS
EFIAPI
VmbusComponentNameGetDriverName (
    IN  EFI_COMPONENT_NAME_PROTOCOL *This,
    IN  CHAR8 *Language,
    OUT CHAR16 **DriverName
    )
/**
    Retrieves a Unicode string that is the user readable name of the EFI Driver.

    This function retrieves the user readable name of a driver in the form of a
    Unicode string. If the driver specified by This has a user readable name in
    the language specified by Language, then a pointer to the driver name is
    returned in DriverName, and EFI_SUCCESS is returned. If the driver specified
    by This does not support the language specified by Language,
    then EFI_UNSUPPORTED is returned.

    @param This A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or EFI_COMPONENT_NAME_PROTOCOL instance.

    @param Language A pointer to a Null-terminated ASCII string array indicating the language.

    @param DriverName A pointer to the string to return.

    @returns EFI_STATUS.

**/
{
    return LookupUnicodeString2(
        Language,
        This->SupportedLanguages,
        gVmbusDriverNameTable,
        DriverName,
        (BOOLEAN)(This == &gVmbusComponentName));
}


EFI_STATUS
EFIAPI
VmbusComponentNameGetControllerName(
    IN  EFI_COMPONENT_NAME_PROTOCOL *This,
    IN  EFI_HANDLE ControllerHandle,
    IN  EFI_HANDLE ChildHandle OPTIONAL,
    IN  CHAR8 *Language,
    OUT CHAR16 **ControllerName
    )
/**
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

    @param This A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or EFI_COMPONENT_NAME_PROTOCOL instance.

    @param ControllerHandle The handle of a controller that the driver specified by This
           is managing.  This handle specifies the controller whose name is to be returned.

    @param ChildHandle The handle of the child controller to retrieve the name of. This is an
        optional parameter that may be NULL.  It will be NULL for device drivers.  It will
        also be NULL for a bus drivers that wish to retrieve the name of the bus controller.
        It will not be NULL for a bus  driver that wishes to retrieve the name of a
        child controller.

    @param Language A pointer to a Null-terminated ASCII string array indicating the language.
        This is the language of the driver name that the caller is requesting, and it
        must match one of the languages specified in SupportedLanguages. The number of
        languages supported by a driver is up to the driver writer. Language is specified in
        RFC 4646 or ISO 639-2 language code format.

    @param ControllerName A pointer to the Unicode string to return. This Unicode string is the
        name of the controller specified by ControllerHandle and ChildHandle in the language
        specified by Language from the point of view of the driver specified by This.

    @returns EFI_STATUS.
**/
{
    EFI_STATUS status;

    //
    // Make sure this driver is currently managing a ControllerHandle
    //
    status = EfiTestManagedDevice(
        ControllerHandle,
        gVmbusDriverBindingProtocol.DriverBindingHandle,
        &gEfiVmbusRootProtocolGuid
        );
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to get the managing controller - %r \n", __FUNCTION__, status));
        return status;
    }

    //
    // ChildHandle must be NULL for a Device Driver
    //
    if (ChildHandle != NULL)
    {
        status = EFI_UNSUPPORTED;
        DEBUG((EFI_D_ERROR, "--- %a: invalid child handle - %r \n", __FUNCTION__, status));
        return status;
    }

    return LookupUnicodeString2(
        Language,
        This->SupportedLanguages,
        gVmbusControllerNameTable,
        ControllerName,
        (BOOLEAN)(This == &gVmbusComponentName));
}


EFI_STATUS
EFIAPI
VmbusDriverInitialize (
    IN  EFI_HANDLE ImageHandle,
    IN  EFI_SYSTEM_TABLE *SystemTable
    )
/**
    Entry point into VMBus driver.

    @param ImageHandle Handle of the driver image.

    @param SystemTable EFI system table.

    @returns EFI_STATUS.

**/
{
    EFI_STATUS status;

    DEBUG((DEBUG_VERBOSE, ">>> %a\n", __FUNCTION__));

    mVmbusImageHandle = ImageHandle;

    //
    // Determine which GUID will be used for the legacy interface.  The legacy
    // protocol is available in all VMs, but the GUID used to expose it
    // differs between isolated and non-isolated VMs.  This is required to
    // ensure that isolated VMs are correclty opting into the required
    // isolation behavior of the legacy protocol.
    //
    if (!IsIsolated())
    {
        mVmbusLegacyProtocolGuid = &gEfiVmbusLegacyProtocolGuid;
    }
    else
    {
        mVmbusLegacyProtocolGuid = &gEfiVmbusLegacyProtocolIvmGuid;
    }

    //
    // Install the VMBus root controller tag and device path protocols onto a
    // new root device handle.
    //
    gVmbusRootDevicePath.VmbusRootNode = gVmbusRootNode;
    gVmbusRootDevicePath.End = gEfiEndNode;

    status = gBS->InstallMultipleProtocolInterfaces(&mRootDevice,
                                                    &gEfiVmbusRootProtocolGuid,
                                                    NULL,
                                                    &gEfiDevicePathProtocolGuid,
                                                    &gVmbusRootDevicePath,
                                                    NULL);
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to install the VMBus protocol - %r \n", __FUNCTION__, status));
        return status;
    }

    //
    // Install the DriverBinding and Component Name protocols onto the driver image handle.
    //
    status = EfiLibInstallDriverBindingComponentName2(ImageHandle,
                                                      SystemTable,
                                                      &gVmbusDriverBindingProtocol,
                                                      ImageHandle,
                                                      &gVmbusComponentName,
                                                      &gVmbusComponentName2
                                                      );
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR, "--- %a: failed to open the driver binding protocol - %r \n", __FUNCTION__, status));
        return status;
    }

    DEBUG((DEBUG_VERBOSE, "<<< %a\n", __FUNCTION__));
    return EFI_SUCCESS;
}

BOOLEAN
VmbusRootSupportsFeatureFlag(
    IN  VMBUS_ROOT_CONTEXT *RootContext,
    IN  UINT32 FeatureFlag
    )
{
    return (RootContext->FeatureFlags & FeatureFlag) != 0;
}