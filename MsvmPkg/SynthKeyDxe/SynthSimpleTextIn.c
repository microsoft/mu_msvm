/** @file
  UEFI SIMPLE_TEXT_INPUT_PROTOCOL and SIMPLE_TEXT_INPUT_PROTOCOL_EX implementation for
  Hyper-V synthetic keyboard. This contains hardware agnostic functionality for interacting
  with UEFI applications. It also provides a simple API for a connectivity/hardware
  layer (e.g. vmbus) to queue keystrokes.

  This code is derived from:
    IntelFrameworkModulePkg\Bus\Isa\Ps2KeyboardDxe\Ps2Keyboard.h

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/


#include "SynthKeyDxe.h"
#include "SynthSimpleTextIn.h"
#include "SynthKeyChannel.h"
#include "SynthKeyLayout.h"

//
// Private function prototypes and types
//

#define SYNTH_KEYBOARD_EX_NOTIFY_SIGNATURE      SIGNATURE_32 ('S', 'k', 'e', 'n')

//
// Tracks registered notification functions that will be called when the
// requested key press event occurs.
//
typedef struct _SYNTH_KEYBOARD_EX_NOTIFY
{
  UINTN                               Signature;
  LIST_ENTRY                          NotifyEntry;

  EFI_HANDLE                          NotifyHandle;
  EFI_KEY_NOTIFY_FUNCTION             KeyNotificationFn;
  EFI_KEY_DATA                        KeyData;
} SYNTH_KEYBOARD_EX_NOTIFY, *PSYNTH_KEYBOARD_EX_NOTIFY;

VOID
KeyNotifyFire(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice,
    IN          EFI_KEY_DATA               *pKey
    );

VOID
KeyNotifyCleanup(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice
    );

BOOLEAN
KeyNotifyIsKeyRegistered(
    IN          EFI_KEY_DATA               *RegisteredData,
    IN          EFI_KEY_DATA               *InputData
    );

BOOLEAN
KeyNotifyIsPartialKey(
    IN          EFI_INPUT_KEY              *Key
    );

VOID
KeyBufferInitialize(
    IN          EFI_KEY_BUFFER             *Queue
    );

VOID
KeyBufferInsert(
    IN          EFI_KEY_BUFFER             *Queue,
    IN          EFI_KEY_DATA               *KeyData
    );

EFI_STATUS
KeyBufferRemove(
    IN          EFI_KEY_BUFFER             *Queue,
    OUT         EFI_KEY_DATA               *KeyData OPTIONAL
    );

BOOLEAN
KeyBufferIsEmpty(
    IN          EFI_KEY_BUFFER             *Queue
    );


VOID
SimpleTextInQueueKey(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice,
    IN          EFI_KEY_DATA               *Key
    )
/*++

Routine Description:

    Queues the given translated key press data into the key buffer, processing
    any special keystrokes and registered keystroke notifications in the process.

Arguments:

    pDevice     Device to queue the keystroke on.
    Key         Keystroke data to queue.


Return Value:

    None.

--*/
{

    //
    // Reset on Ctrl-Alt-Del
    // Note that the scan code here is the UEFI defined scan code and *not*
    // the value from the PS2 keyboard.
    //
    if ((EFI_KEY_CTRL_ACTIVE(pDevice->State.KeyState.KeyShiftState)) &&
        (EFI_KEY_ALT_ACTIVE(pDevice->State.KeyState.KeyShiftState)) &&
        (Key->Key.ScanCode == SCAN_DELETE))
    {
        gRT->ResetSystem (EfiResetWarm, EFI_SUCCESS, 0, NULL);
    }

    KeyNotifyFire(pDevice, Key);
    KeyBufferInsert(&pDevice->EfiKeyQueue, Key);
}


EFI_STATUS
SimpleTextInDequeueKey(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice,
    OUT         EFI_KEY_DATA               *KeyData
    )
/*++

Routine Description:

    Returns the next available key stroke (if any) with proper synchronization.

Arguments:

    pDevice     Keyboard device to read from.

    KeyData     On success, contains the next key stroke.

Return Value:

    EFI_SUCCESS if a key was available and returned.
    EFI_NOT_READY if there was no keystroke data availiable.

--*/
{
    EFI_STATUS  status;
    EFI_TPL     oldTpl;

    oldTpl = gBS->RaiseTPL(TPL_KEYBOARD_NOTIFY);

    if (!pDevice->State.ChannelConnected)
    {
        status = EFI_DEVICE_ERROR;
    }
    else
    {
        status = KeyBufferRemove(&pDevice->EfiKeyQueue, KeyData);
    }

    gBS->RestoreTPL(oldTpl);

    return status;
}


EFI_STATUS
EFIAPI
SimpleTextInReset(
    IN          EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
    IN          BOOLEAN                         ExtendedVerification
    )
/*++

Routine Description:

    SIMPLE_TEXT_INPUT_PROTOCOL Reset Implementation.
    Resets the keyboard clearing all buffered keystrokes.

Arguments:

    This                    EFI_SIMPLE_TEXT_INPUT_PROTOCOL for this device instance.

    ExtendedVerification    UNUSED - Indicates if extended verification should be performed.

Return Value:

    EFI_SUCCESS on success
    EFI_DEVICE_ERROR if a device error occurred.
    Other EFI_STATUS on error.

--*/
{
    PSYNTH_KEYBOARD_DEVICE  pDevice = SYNTH_KEYBOARD_DEVICE_FROM_THIS(This);
    EFI_STATUS              status = EFI_SUCCESS;
    EFI_TPL                 oldTpl;

    SynthKeyReportStatus(pDevice, EFI_PROGRESS_CODE, EFI_P_PC_RESET);

    oldTpl = gBS->RaiseTPL(TPL_KEYBOARD_NOTIFY);

    if (!pDevice->State.ChannelConnected)
    {
        status = EFI_DEVICE_ERROR;
    }

    //
    // UEFI SIMPLE_TEXT_INPUT_PROTOCOLPUT specification require the key buffer to
    // be cleared on reset.
    //
    KeyBufferInitialize(&pDevice->EfiKeyQueue);

    //
    // Clear the shift and toggle states.
    // Shift and toggle state are always valid (even if nothing is set),
    // Indicate that here and forget about them.
    //
    pDevice->State.KeyState.KeyShiftState  = EFI_SHIFT_STATE_VALID;
    pDevice->State.KeyState.KeyToggleState = EFI_TOGGLE_STATE_VALID;

    gBS->RestoreTPL(oldTpl);

    return status;
}


EFI_STATUS
EFIAPI
SimpleTextInResetEx(
    IN          EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL   *This,
    IN          BOOLEAN                              ExtendedVerification
    )
/*++

Routine Description:

    SIMPLE_TEXT_INPUT_EX_PROTOCOL Reset implementation.
    Reset's the input device and optionaly run diagnostics.

Arguments:

    This                     Protocol instance pointer.

    ExtendedVerification     UNUSED - If TRUE, Driver may perform diagnostics on reset.

Return Value:

    EFI_SUCCESS on success
    EFI_DEVICE_ERROR if a device error occurred.
    Other EFI_STATUS on error.

--*/
{
  SYNTH_KEYBOARD_DEVICE *pDevice = SYNTH_KEYBOARD_DEVICE_FROM_THIS_EX(This);

  return SimpleTextInReset(&pDevice->ConIn, ExtendedVerification);
}


EFI_STATUS
EFIAPI
SimpleTextInReadKeyStroke(
    IN          EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *This,
    OUT         EFI_INPUT_KEY                   *Key
    )
/*++

Routine Description:

    SIMPLE_TEXT_INPUT_PROTOCOL ReadKeyStroke Implementation.
    Returns the next available keystroke, if any.

Arguments:

    This        EFI_SIMPLE_TEXT_INPUT_PROTOCOL for this device instance.

    Key         On success, will contain the returned keystroke.


Return Value:

    EFI_SUCCESS if a keystroke is returned.
    EFI_DEVICE_ERROR if a device error occurred.
    EFI_DEVICE_NOT_READY if there are no keystrokes available.

--*/
{
    PSYNTH_KEYBOARD_DEVICE  pDevice = SYNTH_KEYBOARD_DEVICE_FROM_THIS(This);
    EFI_KEY_DATA            keyData;
    EFI_STATUS              status = EFI_SUCCESS;

    //
    // Get the next keystroke, looping to drop partial keystrokes.
    // Partial keystrokes (signified by ScanCode and UnicodeChar both being NULL)
    // are not returned by EFI_SIMPLE_TEXT_INPUT_PROTOCOL. If they are desired,
    // EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL should be used.
    //
    while (TRUE)
    {
        status = SimpleTextInDequeueKey(pDevice, &keyData);

        if (EFI_ERROR(status))
        {
            break;
        }

        if (!KeyNotifyIsPartialKey(&keyData.Key))
        {
            CopyMem(Key, &keyData.Key, sizeof(EFI_INPUT_KEY));
            break;
        }

        //
        // Partial keystroke, drop it and try again.
        //
    }

    return status;
}


EFI_STATUS
EFIAPI
SimpleTextInReadKeyStrokeEx(
    IN          EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *This,
    OUT         EFI_KEY_DATA                       *KeyData
    )
/*++

Routine Description:

    SIMPLE_TEXT_INPUT_EX_PROTOCOL ReadKeyStrokeEx Implementation.
    Reads the next keystroke from the input device. The WaitForKeyEx Event can
    be used to test for existance of a keystroke via WaitForEvent() call.

Arguments:

    This         Protocol instance pointer.

    KeyData      A pointer to a buffer that is filled in with the keystroke
                 state data for the key that was pressed.

Return Value:

    EFI_SUCCESS           The keystroke information was returned.
    EFI_NOT_READY         There was no keystroke data availiable.
    EFI_DEVICE_ERROR      The keystroke information was not returned due to
                          hardware errors.
    EFI_INVALID_PARAMETER KeyData is NULL.

--*/
{
    PSYNTH_KEYBOARD_DEVICE pDevice = SYNTH_KEYBOARD_DEVICE_FROM_THIS_EX(This);

    if (KeyData == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    return SimpleTextInDequeueKey(pDevice, KeyData);
}


VOID
EFIAPI
SimpleTextInWaitForKey(
    IN          EFI_EVENT                   Event,
    IN          VOID                       *Context
    )
/*++

Routine Description:

    Event notification function for SIMPLE_TEXT_INPUT_PROTOCOL.WaitForKey event.
    Called when an application issues a WaitForEvent on the WaitForKey event
    and the event is not signalled. If a key is available signal the event.

    Note that this routine will cause any queue partial keys to be dropped.


Arguments:

    Event       Event to signal if a key is available.

    Context     SYNTH_KEYBOARD_DEVICE device instance associated with the event.

Return Value:

    None

--*/
{
    PSYNTH_KEYBOARD_DEVICE      pDevice = (PSYNTH_KEYBOARD_DEVICE)Context;
    EFI_TPL                     oldTpl;

    if (!pDevice->State.ChannelConnected)
    {
        return;
    }

    oldTpl = gBS->RaiseTPL(TPL_KEYBOARD_NOTIFY);

    //
    // See if there is a key in the buffer, looping to remove and skip
    // partial keys since they are not supported in WaitforKey
    //
    while (!KeyBufferIsEmpty(&pDevice->EfiKeyQueue))
    {
        EFI_INPUT_KEY *nextKey = &pDevice->EfiKeyQueue.Buffer[pDevice->EfiKeyQueue.Head].Key;

        if (!KeyNotifyIsPartialKey(nextKey))
        {
            //
            // there is pending value key, signal the event.
            //
            gBS->SignalEvent(Event);
            break;
        }

        //
        // Drop the partial key.
        //
        KeyBufferRemove(&pDevice->EfiKeyQueue, NULL);

    }

    gBS->RestoreTPL (oldTpl);
}


VOID
EFIAPI
SimpleTextInWaitForKeyEx(
    IN          EFI_EVENT                   Event,
    IN          VOID                       *Context
    )
/*++

Routine Description:

    Event notification function for SIMPLE_TEXT_INPUT_PROTOCOL_EX.WaitForKeyEx event.
    Called when an application issues a WaitForEvent on the WaitForKeyEx event
    and the event is not signalled. If a key is available the event will be signaled.

    Note that this routine will cause any queue partial keys to be dropped.

Arguments:

    Event       Event to signal if a key is available.

    Context     SYNTH_KEYBOARD_DEVICE device instance associated with the event.

Return Value:

    None

--*/
{
    SimpleTextInWaitForKey(Event, Context);
}


EFI_STATUS
EFIAPI
SimpleTextInSetState(
    IN          EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *This,
    IN          EFI_KEY_TOGGLE_STATE               *KeyToggleState
    )
/*++

Routine Description:

    SIMPLE_TEXT_INPUT_EX_PROTOCOL SetState implementation.
    Sets the state on the input device.

Arguments:

    This               Protocol instance instance for the device.

    KeyToggleState     EFI_KEY_TOGGLE_STATE to set on the input device.

Return Value:

    EFI_SUCCESS           The device state was set successfully.
    EFI_DEVICE_ERROR      The device is not functioning correctly and could
                          not have the setting adjusted.
    EFI_UNSUPPORTED       The device does not have the ability to set its state.
    EFI_INVALID_PARAMETER KeyToggleState is NULL.

--*/
{
    PSYNTH_KEYBOARD_DEVICE  pDevice = SYNTH_KEYBOARD_DEVICE_FROM_THIS_EX(This);
    EFI_TPL                 oldTpl;
    EFI_STATUS              status;

    if (KeyToggleState == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    if ((*KeyToggleState & EFI_TOGGLE_STATE_VALID) != EFI_TOGGLE_STATE_VALID)
    {
        return EFI_UNSUPPORTED;
    }

    if (!pDevice->State.ChannelConnected)
    {
        return EFI_DEVICE_ERROR;
    }

    oldTpl = gBS->RaiseTPL(TPL_KEYBOARD_NOTIFY);

    //
    // Synchronize this with the key event processing callback.
    // This is to keep a set of consistent flags if the callback tests a flag
    // and then tests it again and expects both to be the same.
    //
    pDevice->State.KeyState.KeyToggleState = *KeyToggleState;

    gBS->RestoreTPL(oldTpl);

    status = SynthKeyChannelSetIndicators(pDevice);

    if (EFI_ERROR(status))
    {
        status = EFI_DEVICE_ERROR;
    }

    return status;
}


EFI_STATUS
EFIAPI
SimpleTextInRegisterKeyNotify(
    IN          EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *This,
    IN          EFI_KEY_DATA                       *KeyData,
    IN          EFI_KEY_NOTIFY_FUNCTION             KeyNotificationFunction,
    OUT         EFI_HANDLE                         *NotifyHandle
    )
/*++

Routine Description:

    SIMPLE_TEXT_INPUT_EX_PROTOCOL RegisterKeyNotify implementation.
    Register a notification function to be called when a particular keystroke
    is received.

Arguments:

    This                        Protocol instance instance for the device.

    KeyData                     Keystroke to be notified about including shift and toggle state.
                                Either KeyShiftState or KeyToggleState can be 0, in which case
                                the associated state is ignored. Non-zero values must match exactly
                                for the notification to be called.

    KeyNotificationFunction     Function to be called when the key sequence in KeyData is received.

    NotifyHandle                On return, contains the unique handle identifying the registered
                                notification. This must be unregistered using the
                                SIMPLE_TEXT_INPUT_EX_PROTOCOL UnregisterKeyNotify API.

Return Value:

    EFI_SUCCESS               The notification function was registered successfully.
    EFI_OUT_OF_RESOURCES      Unable to allocate resources for necesssary data structures.
    EFI_INVALID_PARAMETER     KeyData or NotifyHandle or KeyNotificationFunction is NULL.

--*/
{
    PSYNTH_KEYBOARD_DEVICE      pDevice = SYNTH_KEYBOARD_DEVICE_FROM_THIS_EX(This);
    PSYNTH_KEYBOARD_EX_NOTIFY   currentNotify;
    PSYNTH_KEYBOARD_EX_NOTIFY   newNotify = NULL;

    LIST_ENTRY *link;
    EFI_STATUS  status;
    EFI_TPL     oldTpl;

    if ((KeyData == NULL) ||
        (NotifyHandle == NULL) ||
        (KeyNotificationFunction == NULL))
    {
        return EFI_INVALID_PARAMETER;
    }

    oldTpl = gBS->RaiseTPL(TPL_KEYBOARD_NOTIFY);

    //
    // See if the same combination of keydata and function callback is
    // already registered. Return EFI_SUCCESS in that case.
    //
    for (link  = pDevice->NotifyList.ForwardLink;
         link != &pDevice->NotifyList;
         link  = link->ForwardLink)
    {
        currentNotify = CR(link,
                           SYNTH_KEYBOARD_EX_NOTIFY,
                           NotifyEntry,
                           SYNTH_KEYBOARD_EX_NOTIFY_SIGNATURE);

        if (KeyNotifyIsKeyRegistered(&currentNotify->KeyData, KeyData))
        {
            if (currentNotify->KeyNotificationFn == KeyNotificationFunction)
            {
                newNotify = currentNotify->NotifyHandle;
                status = EFI_SUCCESS;
                goto Exit;
            }
        }
    }

    //
    // No existing notfication matches the request,
    // allocate a notification and save the necessary information.
    //
    newNotify = (PSYNTH_KEYBOARD_EX_NOTIFY)AllocateZeroPool(sizeof(SYNTH_KEYBOARD_EX_NOTIFY));

    if (newNotify == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    CopyMem(&newNotify->KeyData, KeyData, sizeof(EFI_KEY_DATA));

    newNotify->Signature         = SYNTH_KEYBOARD_EX_NOTIFY_SIGNATURE;
    newNotify->KeyNotificationFn = KeyNotificationFunction;
    newNotify->NotifyHandle      = (EFI_HANDLE)newNotify;

    InsertTailList (&pDevice->NotifyList, &newNotify->NotifyEntry);

    status = EFI_SUCCESS;

Exit:

    *NotifyHandle = newNotify;
    gBS->RestoreTPL(oldTpl);

    return status;
}


EFI_STATUS
EFIAPI
SimpleTextInUnregisterKeyNotify(
    IN          EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *This,
    IN          EFI_HANDLE                          NotificationHandle
    )
/*++

Routine Description:

    SIMPLE_TEXT_INPUT_EX_PROTOCOL UnregisterKeyNotify Implementation
    Remove a registered notification function from a particular keystroke.

Arguments:

    This                    Protocol instance for the device

    NotificationHandle      Handle to unregister, returned by a previous call to
                            RegisterKeyNotify.

Return Value:

    EFI_SUCCESS if the handle was unregistered.
    EFI_INVALID_PARAMETER if the handle was not valid or registered.

--*/
{
    PSYNTH_KEYBOARD_DEVICE      pDevice = SYNTH_KEYBOARD_DEVICE_FROM_THIS_EX(This);
    PSYNTH_KEYBOARD_EX_NOTIFY   currentNotify;

    LIST_ENTRY *link;
    EFI_STATUS  status = EFI_INVALID_PARAMETER;
    EFI_TPL     oldTpl;

    if (NotificationHandle == NULL)
    {
        return status;
    }

    if (((PSYNTH_KEYBOARD_EX_NOTIFY)NotificationHandle)->Signature != SYNTH_KEYBOARD_EX_NOTIFY_SIGNATURE)
    {
        return status;
    }

    oldTpl = gBS->RaiseTPL(TPL_KEYBOARD_NOTIFY);

    //
    // Search the notification list for a matching registration,
    // removing and freeing it if found.
    //
    for (link  = pDevice->NotifyList.ForwardLink;
         link != &pDevice->NotifyList;
         link  = link->ForwardLink)
    {
        currentNotify = CR(link,
                           SYNTH_KEYBOARD_EX_NOTIFY,
                           NotifyEntry,
                           SYNTH_KEYBOARD_EX_NOTIFY_SIGNATURE);

        if (currentNotify->NotifyHandle == NotificationHandle)
        {
            RemoveEntryList(&currentNotify->NotifyEntry);

            gBS->FreePool(currentNotify);
            status = EFI_SUCCESS;
            break;
        }

    }

    gBS->RestoreTPL(oldTpl);

    return status;
}


EFI_STATUS
SimpleTextInInitialize(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice
    )
/*++

Routine Description:

    Initializes the necessary state and registers the EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL
    and SIMPLE_TEXT_INPUT_PROTOCOL protocols. This will also initialize the communication
    layer (i.e. VMBUS). SimpleTextInCleanup must be called upon successfull return.

Arguments:

    pDevice     Device instance to initialize and register protocols for.

Return Value:

    EFI_SUCCESS on success.
    EFI_STATUS on error

--*/
{
    EFI_STATUS  status;

    InitializeListHead (&pDevice->NotifyList);

    pDevice->ConIn.Reset                 = SimpleTextInReset;
    pDevice->ConIn.ReadKeyStroke         = SimpleTextInReadKeyStroke;

    pDevice->ConInEx.Reset               = SimpleTextInResetEx;
    pDevice->ConInEx.ReadKeyStrokeEx     = SimpleTextInReadKeyStrokeEx;
    pDevice->ConInEx.SetState            = SimpleTextInSetState;
    pDevice->ConInEx.RegisterKeyNotify   = SimpleTextInRegisterKeyNotify;
    pDevice->ConInEx.UnregisterKeyNotify = SimpleTextInUnregisterKeyNotify;

    status = gBS->CreateEvent(EVT_NOTIFY_WAIT,
                              TPL_KEYBOARD_NOTIFY,
                              SimpleTextInWaitForKey,
                              pDevice,
                              &pDevice->ConIn.WaitForKey);

    if (EFI_ERROR (status))
    {
        goto ErrorExit;
    }

    status = gBS->CreateEvent(EVT_NOTIFY_WAIT,
                              TPL_KEYBOARD_NOTIFY,
                              SimpleTextInWaitForKeyEx,
                              pDevice,
                              &pDevice->ConInEx.WaitForKeyEx);

    if (EFI_ERROR (status))
    {
        goto ErrorExit;
    }

    //
    // TODO: Rethink this, it could make adding to Reset() harder.
    // e.g. If Reset needs to perform some action that requires the channel to be open.
    //
    // Use the reset handler to get things to their initial
    // state. Ignore the return since this call will always fail
    // with EFI_DEVICE_ERROR as the channel is not up yet but we want to perform the rest of the
    // initialization.
    //
    SimpleTextInReset(&pDevice->ConIn, FALSE);

    status = SynthKeyChannelOpen(pDevice);

    if (EFI_ERROR(status))
    {
        goto ErrorExit;
    }

    //
    // Install protocol interfaces for the keyboard device.
    //
    status = gBS->InstallMultipleProtocolInterfaces(&pDevice->Handle,
                                                    &gEfiSimpleTextInProtocolGuid,
                                                    &pDevice->ConIn,
                                                    &gEfiSimpleTextInputExProtocolGuid,
                                                    &pDevice->ConInEx,
                                                    NULL);

    if (EFI_ERROR(status))
    {
        goto ErrorExit;
    }

    pDevice->State.SimpleTextInstalled = TRUE;

ErrorExit:

    if (EFI_ERROR(status))
    {
        SimpleTextInCleanup(pDevice);
    }

    return status;
}


VOID
SimpleTextInCleanup(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice
    )
/*++

Routine Description:

    Performs cleanup for the simple text in protocols.
    This must be able to handle partial or no initialization.

Arguments:

    pDevice     Device instance for which to cleanup.

Return Value:

    None.

--*/
{
    EFI_STATUS  status;
    EFI_TPL     oldTpl;

    //
    // Raise TPL so we don't race with the key press handler.
    //
    oldTpl = gBS->RaiseTPL(TPL_KEYBOARD_NOTIFY);

    //
    // Uninstall the SimpleTextIn and SimpleTextInEx protocols
    // InstallMultipleProtocolInterfaces guarantees that all
    // interfaces are installed on success, so if SimpleTextIn
    // was installed, we know we can uninstall both protocols.
    //
    if (pDevice->State.SimpleTextInstalled)
    {
        status = gBS->UninstallMultipleProtocolInterfaces(pDevice->Handle,
                                                          &gEfiSimpleTextInProtocolGuid,
                                                          &pDevice->ConIn,
                                                          &gEfiSimpleTextInputExProtocolGuid,
                                                          &pDevice->ConInEx,
                                                          NULL);

        if (EFI_ERROR(status))
        {
            goto Exit;
        }

        pDevice->State.SimpleTextInstalled = FALSE;
    }

    //
    // Cleanup the VMBUS channel
    //
    status = SynthKeyChannelClose(pDevice);

    if (EFI_ERROR(status))
    {
        goto Exit;
    }

    if (pDevice->ConIn.WaitForKey != NULL)
    {
        gBS->CloseEvent(pDevice->ConIn.WaitForKey);
        pDevice->ConIn.WaitForKey = NULL;
    }

    if (pDevice->ConInEx.WaitForKeyEx != NULL)
    {
        gBS->CloseEvent(pDevice->ConInEx.WaitForKeyEx);
        pDevice->ConInEx.WaitForKeyEx = NULL;
    }

    KeyNotifyCleanup(pDevice);

Exit:

    gBS->RestoreTPL(oldTpl);
}


VOID
KeyNotifyFire(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice,
    IN          EFI_KEY_DATA               *pKey
    )
/*++

Routine Description:

    Helper to search the notification list and invoke each registered notification handler
    that matches the given key.

Arguments:

    pDevice     Device instance to search

    pKey        Key data to match

Return Value:

    None.

--*/
{
    PSYNTH_KEYBOARD_EX_NOTIFY  currentNotify;
    LIST_ENTRY                *link;

    //
    // Invoke notification functions if any match this key
    //
    for (link  = pDevice->NotifyList.ForwardLink;
         link != &pDevice->NotifyList;
         link  = link->ForwardLink)
    {
        currentNotify = CR(link,
                           SYNTH_KEYBOARD_EX_NOTIFY,
                           NotifyEntry,
                           SYNTH_KEYBOARD_EX_NOTIFY_SIGNATURE);

        if (KeyNotifyIsKeyRegistered(&currentNotify->KeyData, pKey))
        {
            currentNotify->KeyNotificationFn(pKey);
        }
    }
}


VOID
KeyNotifyCleanup(
    IN          PSYNTH_KEYBOARD_DEVICE      pDevice
    )
/*++

Routine Description:

    Removes and frees all registered keystroke notifications on a keyboard instance.

Arguments:

    pDevice     Device instance to cleanup notifications for.

Return Value:

    None.

--*/
{
    LIST_ENTRY *list = &pDevice->NotifyList;

    while (!IsListEmpty(list))
    {
        PSYNTH_KEYBOARD_EX_NOTIFY notify = CR(list->ForwardLink,
                                              SYNTH_KEYBOARD_EX_NOTIFY,
                                              NotifyEntry,
                                              SYNTH_KEYBOARD_EX_NOTIFY_SIGNATURE);
        RemoveEntryList(list->ForwardLink);
        gBS->FreePool(notify);
    }
}


BOOLEAN
KeyNotifyIsPartialKey(
    IN          EFI_INPUT_KEY              *Key
    )
/*++

Routine Description:

    Helper to determine if a given EFI_KEY_DATA is a partial keystrok. Partial keystrokes are
    the ones with no scancode or unicode data but do contain shift and toggle state.

Arguments:

    Key     EFI_KEY_DATA to check.

Return Value:

    TRUE if the EFI_KEY_DATA is a partial key.
    FALSE if not.

--*/
{
    return (BOOLEAN)((Key->ScanCode == SCAN_NULL) && (Key->UnicodeChar == CHAR_NULL));
}


BOOLEAN
KeyNotifyIsKeyRegistered(
    IN          EFI_KEY_DATA               *RegisteredData,
    IN          EFI_KEY_DATA               *InputData
    )
/*++

Routine Description:

    Helper to determine if a given EFI_KEY_DATA matches the key data registered with
    RegisterKeyNotify.

Arguments:

    RegisteredData  EFI_KEY_DATA that was registered for notifications via RegisterKeyNotify

    InputData       EFI_KEY_DATA to match

Return Value:

    TRUE if the EFI_KEY_DATA structures match
    FALSE if not.

--*/
{
    ASSERT(RegisteredData != NULL);
    ASSERT(InputData != NULL);

    //
    // ScanCode and UnicodeChar always have to match.
    //
    if ((RegisteredData->Key.ScanCode    != InputData->Key.ScanCode) ||
        (RegisteredData->Key.UnicodeChar != InputData->Key.UnicodeChar))
    {
        return FALSE;
    }

    //
    // Assume values of zero in KeyShiftState or KeyToggleState indicate that
    // these states could be ignored (like a wildcard).
    //
    if ((RegisteredData->KeyState.KeyShiftState != 0) &&
        (RegisteredData->KeyState.KeyShiftState != InputData->KeyState.KeyShiftState))
    {
        return FALSE;
    }

    if ((RegisteredData->KeyState.KeyToggleState != 0) &&
        (RegisteredData->KeyState.KeyToggleState != InputData->KeyState.KeyToggleState))
    {
        return FALSE;
    }

    return TRUE;
}


VOID
KeyBufferInitialize(
    IN          EFI_KEY_BUFFER             *Queue
    )
/*++

Routine Description:

    Initializes the Key Buffer to an empty state.

Arguments:

    Queue       Key Buffer to initialize.

Return Value:

    None

--*/
{
    Queue->Head = Queue->Tail = 0;
}


VOID
KeyBufferInsert(
    IN          EFI_KEY_BUFFER             *Queue,
    IN          EFI_KEY_DATA               *KeyData
    )
/*++

Routine Description:

    Inserts the given key data into the given key buffer replacing the oldest entry
    if the buffer is full.

Arguments:

    Queue       Key buffer to add the key press to.

    KeyData     Key press information to queue.

Return Value:

    None.

--*/
{
    UINTN newTail = (Queue->Tail + 1) % SYNTHKEY_KEY_BUFFER_SIZE;

    if (newTail == Queue->Head)
    {
        //
        // Queue is full, drop the oldest item.
        // this is done by advancing the head by one thus making
        // room for one more entry.
        //
        Queue->Head = (Queue->Head + 1) % SYNTHKEY_KEY_BUFFER_SIZE;
    }

    CopyMem (&Queue->Buffer[Queue->Tail], KeyData, sizeof (EFI_KEY_DATA));
    Queue->Tail = newTail;
}


EFI_STATUS
KeyBufferRemove(
    IN          EFI_KEY_BUFFER             *Queue,
    OUT         EFI_KEY_DATA               *KeyData OPTIONAL
    )
/*++

Routine Description:

    Remove the oldest entry (if present) from the key buffer.

Arguments:

    Queue       Key Buffer to remove an entry from.

    KeyData     If provided, contains the removed key data on success.
                If NULL, the keystroke is removed but lost/dropped.

Return Value:

    EFI_SUCCESS on success.
    EFI_NOT_READY if the buffer is empty.

--*/
{
    if (KeyBufferIsEmpty(Queue))
    {
        return EFI_NOT_READY;
    }

    if (KeyData != NULL)
    {
        CopyMem(KeyData, &Queue->Buffer[Queue->Head], sizeof(EFI_KEY_DATA));
    }

    Queue->Head = (Queue->Head + 1) % SYNTHKEY_KEY_BUFFER_SIZE;

    return EFI_SUCCESS;
}


BOOLEAN
KeyBufferIsEmpty(
    IN      EFI_KEY_BUFFER                 *Queue
    )
/*++

Routine Description:

    Check whether the EFI key buffer is empty.

Arguments:

    Queue       Key buffer to check for emptiness.

Return Value:

    TRUE if empty
    FALSE if not

--*/
{
    return (BOOLEAN)(Queue->Head == Queue->Tail);
}
