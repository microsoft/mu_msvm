/** @file

    Implementation of ExtScsiPassThru protocol for StorvscDxe.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "StorvscDxe.h"

EFI_STATUS
EFIAPI
StorvscExtScsiPassThruPassThru (
    IN      EFI_EXT_SCSI_PASS_THRU_PROTOCOL *This,
    IN      UINT8 *Target,
    IN      UINT64 Lun,
    IN OUT  EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET *Packet,
    IN      EFI_EVENT Event OPTIONAL
    )
/*++

Routine Description:

    Sends a SCSI Request Packet to a SCSI device that is attached to the SCSI channel.

Arguments:

    This -A pointer to the EFI_EXT_SCSI_PASS_THRU_PROTOCOL instance.

    Target - The Target is an array of size TARGET_MAX_BYTES and it represents
        the id of the SCSI device to send the SCSI Request Packet.

    Lun - The LUN of the SCSI device to send the SCSI Request Packet.

    Packet - A pointer to the SCSI Request Packet to send to the SCSI
        device specified by Target and Lun.

    Event - If Event is NULL, then blocking I/O is performed. If Event is not NULL,
        then nonblocking I/O is performed, and Event will be signaled when the SCSI
        Request Packet completes.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    STORVSC_ADAPTER_CONTEXT *instance;

    instance = STORVSC_ADAPTER_CONTEXT_FROM_EXT_SCSI_PASS_THRU_THIS(This);

    if (StorChannelSearchLunList(&instance->LunList, *Target, (UINT8)Lun) ==
            NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    if (Event != NULL)
    {
        //
        // This is a Non-Blocking IO request
        //
        status = StorChannelSendScsiRequest(
            instance->ChannelContext,
            Packet,
            Target,
            Lun,
            Event);
    }
    else
    {
        status = StorChannelSendScsiRequestSync(
            instance->ChannelContext,
            Packet,
            Target,
            Lun);
    }

    return status;
}


EFI_STATUS
EFIAPI
StorvscExtScsiPassThruGetNextTargetLun (
    IN      EFI_EXT_SCSI_PASS_THRU_PROTOCOL *This,
    IN OUT  UINT8 **Target,
    IN OUT  UINT64 *Lun
    )
/*++

Routine Description:

    Used to retrieve the list of legal Target IDs and LUNs for SCSI devices
    on a SCSI channel.

Arguments:

    This - A pointer to the EFI_EXT_SCSI_PASS_THRU_PROTOCOL instance.

    Target  - On input, a pointer to the Target ID (an array of size
        TARGET_MAX_BYTES) of a SCSI device present on the SCSI channel.
        On output, a pointer to the Target ID (an array of TARGET_MAX_BYTES)
        of the next SCSI device present on a SCSI channel.
        An input value of 0xF (all bytes in the array are 0xFF) in the Target array
        retrieves the Target ID of the first SCSI device present on a SCSI channel.

    Lun - On input, a pointer to the LUN of a SCSI device present on the SCSI
        channel. On output, a pointer to the LUN of the next SCSI device present
        on a SCSI channel.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    STORVSC_ADAPTER_CONTEXT *instance;
    EFI_TPL tpl;
    PTARGET_LUN entry;
    LIST_ENTRY *listEntry;
    BOOLEAN firstDevice = TRUE;
    UINT32 index;

    instance = STORVSC_ADAPTER_CONTEXT_FROM_EXT_SCSI_PASS_THRU_THIS(This);

    for (index = 0; index < TARGET_MAX_BYTES; index++)
    {
        if ((*Target)[index] != 0xFF)
        {
            firstDevice = FALSE;
            break;
        }
    }

    tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);

    if (IsListEmpty(&instance->LunList))
    {
        status = EFI_NOT_FOUND;
    }
    else if (firstDevice)
    {
        entry = BASE_CR(instance->LunList.ForwardLink, TARGET_LUN, ListEntry);
        **Target = entry->TargetId;
        *Lun = entry->Lun;

        status = EFI_SUCCESS;
    }
    else
    {
        listEntry = StorChannelSearchLunList(&instance->LunList, **Target, (UINT8)*Lun);

        if (listEntry != NULL)
        {
            if (listEntry->ForwardLink != &instance->LunList)
            {
                entry = BASE_CR(listEntry->ForwardLink, TARGET_LUN, ListEntry);
                **Target = entry->TargetId;
                *Lun = entry->Lun;

                status = EFI_SUCCESS;
            }
            else
            {
                status = EFI_NOT_FOUND;
            }
        }
        else
        {
            status = EFI_INVALID_PARAMETER;
        }
    }

    gBS->RestoreTPL(tpl);

    return status;
}


EFI_STATUS
EFIAPI
StorvscExtScsiPassThruBuildDevicePath (
    IN      EFI_EXT_SCSI_PASS_THRU_PROTOCOL *This,
    IN      UINT8 *Target,
    IN      UINT64 Lun,
    IN OUT  EFI_DEVICE_PATH_PROTOCOL **DevicePath
    )
/*++

Routine Description:

    Used to allocate and build a device path node for a SCSI device on a SCSI channel.

Arguments:

    This - A pointer to the EFI_EXT_SCSI_PASS_THRU_PROTOCOL instance.

    Target - The Target is an array of size TARGET_MAX_BYTES and it specifies
        the Target ID of the SCSI device for which a device path node is to be
        allocated and built.

    Lun - The LUN of the SCSI device for which a device path node is to be
        allocated and built.

    DevicePath - A pointer to a single device path node that describes the SCSI
        device specified by Target and Lun. This function is responsible for allocating
        the buffer DevicePath with the boot service AllocatePool(). It is the caller's
        responsibility to free DevicePath when the caller is finished with DevicePath.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    EFI_DEV_PATH *devicePathNode;
    STORVSC_ADAPTER_CONTEXT *instance;
    EFI_TPL tpl;
    LIST_ENTRY *listEntry;

    if (DevicePath == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    instance = STORVSC_ADAPTER_CONTEXT_FROM_EXT_SCSI_PASS_THRU_THIS(This);

    tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);

    listEntry = StorChannelSearchLunList(&instance->LunList, *Target, (UINT8)Lun);

    gBS->RestoreTPL(tpl);

    if (listEntry == NULL)
    {
        status = EFI_NOT_FOUND;
        goto Cleanup;
    }

    devicePathNode = AllocatePool(sizeof(SCSI_DEVICE_PATH));
    if (devicePathNode == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Cleanup;
    }

    devicePathNode->Scsi.Header.Type = MESSAGING_DEVICE_PATH;
    devicePathNode->Scsi.Header.SubType = MSG_SCSI_DP;
    devicePathNode->Scsi.Header.Length[0] = sizeof(SCSI_DEVICE_PATH) & 0xFF;
    devicePathNode->Scsi.Header.Length[1] = (sizeof(SCSI_DEVICE_PATH) >> 8) & 0xFF;

    devicePathNode->Scsi.Pun = *Target;
    devicePathNode->Scsi.Lun = (UINT16) Lun;

    *DevicePath = (EFI_DEVICE_PATH_PROTOCOL *) devicePathNode;

    status = EFI_SUCCESS;

Cleanup:
    return status;

}


EFI_STATUS
EFIAPI
StorvscExtScsiPassThruGetTargetLun (
    IN  EFI_EXT_SCSI_PASS_THRU_PROTOCOL *This,
    IN  EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    OUT UINT8 **Target,
    OUT UINT64 *Lun
    )
/*++

Routine Description:

    Used to translate a device path node to a Target ID and LUN.

Arguments:

    This - A pointer to the EFI_EXT_SCSI_PASS_THRU_PROTOCOL instance.

    DevicePath - A pointer to a single device path node that describes the SCSI device
        on the SCSI channel.

    Target - A pointer to the Target Array which represents the ID of a SCSI device
        on the SCSI channel.

    Lun - A pointer to the LUN of a SCSI device on the SCSI channel.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_DEV_PATH *devicePathNode;
    STORVSC_ADAPTER_CONTEXT *instance;
    EFI_TPL tpl;
    LIST_ENTRY* foundTargetLun;

    instance = STORVSC_ADAPTER_CONTEXT_FROM_EXT_SCSI_PASS_THRU_THIS(This);
    devicePathNode = (EFI_DEV_PATH *)DevicePath;

    //
    // Validate parameters passed in.
    //
    if (DevicePath == NULL || Target == NULL || Lun == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    if (*Target == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Check whether the DevicePath belongs to SCSI_DEVICE_PATH
    //
    if ((DevicePath->Type != MESSAGING_DEVICE_PATH) ||
        (DevicePath->SubType != MSG_SCSI_DP) ||
        (DevicePathNodeLength(DevicePath) != sizeof(SCSI_DEVICE_PATH)))
    {
        return EFI_UNSUPPORTED;
    }

    SetMem(*Target, TARGET_MAX_BYTES, 0xFF);

    tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);

    foundTargetLun = StorChannelSearchLunList(
        &instance->LunList,
        (UINT8) devicePathNode->Scsi.Pun,
        (UINT8) devicePathNode->Scsi.Lun);

    gBS->RestoreTPL(tpl);

    if (foundTargetLun == NULL)
    {
        return EFI_NOT_FOUND;
    }

    **Target = (UINT8) devicePathNode->Scsi.Pun;
    *Lun = devicePathNode->Scsi.Lun;

    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
StorvscExtScsiPassThruResetChannel (
    IN  EFI_EXT_SCSI_PASS_THRU_PROTOCOL *This
    )
/*++

Routine Description:

    Resets a SCSI channel. This operation resets all the SCSI devices connected
        to the SCSI channel.

Arguments:

    This - A pointer to the EFI_EXT_SCSI_PASS_THRU_PROTOCOL instance.

Return Value:

    EFI_STATUS.

--*/
{
    return EFI_UNSUPPORTED;
}


EFI_STATUS
EFIAPI
StorvscExtScsiPassThruResetTargetLun (
    IN  EFI_EXT_SCSI_PASS_THRU_PROTOCOL *This,
    IN  UINT8 *Target,
    IN  UINT64 Lun
    )
/*++

Routine Description:

    Resets a SCSI logical unit that is connected to a SCSI channel.

Arguments:

    This - A pointer to the EFI_EXT_SCSI_PASS_THRU_PROTOCOL instance.

    Target - The Target is an array of size TARGET_MAX_BYTE and it represents the
        target port ID of the SCSI device containing the SCSI logical unit to reset.

    Lun - The LUN of the SCSI device to reset.

Return Value:

    EFI_STATUS.

--*/
{
    return EFI_UNSUPPORTED;
}


EFI_STATUS
EFIAPI
StorvscExtScsiPassThruGetNextTarget (
    IN      EFI_EXT_SCSI_PASS_THRU_PROTOCOL *This,
    IN OUT  UINT8 **Target
    )
/*++

Routine Description:

    Used to retrieve the list of legal Target IDs for SCSI devices on a SCSI channel.

Arguments:

    This - A pointer to the EFI_EXT_SCSI_PASS_THRU_PROTOCOL instance.

    Target - (TARGET_MAX_BYTES) of a SCSI device present on the SCSI channel.
        On output, a pointer to the Target ID (an array of TARGET_MAX_BYTES) of
        the next SCSI device present on a SCSI channel. An input value of 0xF (all bytes
        in the array are 0xF) in the Target array retrieves the Target ID of the first SCSI
        device present on a SCSI channel.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_STATUS status;
    STORVSC_ADAPTER_CONTEXT *instance;
    UINT8 nextTarget;
    INT16 currentTarget;
    EFI_TPL tpl;
    PTARGET_LUN entry;
    LIST_ENTRY *listEntry;
    BOOLEAN firstDevice = TRUE;
    UINT32 index;

    instance = STORVSC_ADAPTER_CONTEXT_FROM_EXT_SCSI_PASS_THRU_THIS(This);

    for (index = 0; index < TARGET_MAX_BYTES; index++)
    {
        if ((*Target)[index] != 0xFF)
        {
            firstDevice = FALSE;
            break;
        }
    }

    if (firstDevice)
    {
        currentTarget = -1;
    }
    else
    {
        currentTarget = **Target;
    }

    tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);

    if (IsListEmpty(&instance->LunList))
    {
        status = EFI_NOT_FOUND;
    }
    else
    {
        nextTarget = VMSTOR_MAX_TARGETS + 1;
        for (listEntry = instance->LunList.ForwardLink;
             listEntry->ForwardLink != &instance->LunList;
             listEntry = listEntry->ForwardLink)
        {
            entry = BASE_CR(listEntry, TARGET_LUN, ListEntry);
            if (entry->TargetId > currentTarget && entry->TargetId < nextTarget)
            {
                nextTarget = entry->TargetId;
            }
        }
        if (nextTarget <= VMSTOR_MAX_TARGETS)
        {
            **Target = nextTarget;

            status = EFI_SUCCESS;
        }
        else
        {
            status = EFI_NOT_FOUND;
        }
    }

    gBS->RestoreTPL(tpl);

    return status;
}

