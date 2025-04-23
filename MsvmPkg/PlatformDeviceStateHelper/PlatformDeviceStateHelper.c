/** @file
  Sets up the device state variable for use on displaying the device state

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Guid/GlobalVariable.h>

#include <Library/BiosDeviceLib.h>
#include <Library/CrashLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceStateLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>

#include <IsolationTypes.h>
#include <Uefi/UefiBaseType.h>
#include <AdvancedLoggerInternal.h>
#include <BiosInterface.h>

extern EFI_GUID gAdvancedLoggerHobGuid;

/**
    Check if secure boot is enabled

    @retval     TRUE  Secure boot is enabled
                FALSE Secure boot is disabled
**/
BOOLEAN
IsSecureBootOn()
{
    EFI_STATUS  Status = EFI_DEVICE_ERROR;
    UINT8      *Value = NULL;
    UINTN       Size = 0;

    //
    // For now, no hardware isolated platforms without a paravisor support secure boot.
    //
    if (IsHardwareIsolatedNoParavisor()) {
        return FALSE;
    }

    Status = GetVariable2(L"SecureBoot", &gEfiGlobalVariableGuid, (VOID **)&Value, &Size);
    if (EFI_ERROR (Status) || (Value == NULL)) {
        DEBUG ((DEBUG_ERROR, "%a - Failed to read SecureBoot variable.  Status = %r\n", __FUNCTION__, Status));
        return FALSE;
    }

    ASSERT(Size == 1);

    if(*Value == 1) {
        DEBUG((DEBUG_INFO, "%a - Secure boot on\n", __FUNCTION__));
        FreePool (Value);
        return TRUE;
    }

    DEBUG((DEBUG_INFO, "%a - Secure boot off\n", __FUNCTION__));
    FreePool (Value);
    return FALSE;
}

/**
    Set up the device state variable for use later in displaying the device state

    @retval     EFI_SUCCESS  Always returns success.
**/
EFI_STATUS
EFIAPI
PlatformDeviceStateHelperInit(
    IN EFI_HANDLE                   ImageHandle,
    IN EFI_SYSTEM_TABLE             *SystemTable
  )
{
    ADVANCED_LOGGER_PTR *advancedLoggerPtr;
    EFI_HOB_GUID_TYPE *guidHob;
    DEVICE_STATE CoreNotifications = GetDeviceState();

    DEBUG((DEBUG_INFO, "Starting %a \n", __FUNCTION__));

    //
    // Validate/set secure boot state
    //
    if (IsSecureBootOn())
    {
        // It is illegal to enable debugging with secure boot
        FAIL_FAST_UNEXPECTED_HOST_BEHAVIOR_IF_FALSE(((CoreNotifications & DEVICE_STATE_SOURCE_DEBUG_ENABLED) == 0));
    }
    else
    {
        CoreNotifications |= DEVICE_STATE_SECUREBOOT_OFF;
        AddDeviceState(CoreNotifications);
    }

    //
    // Set the GPA of the advanced logger info header for the host.
    //
    // NOTE: This GPA should contain the full logs.
    //
    guidHob = GetFirstGuidHob(&gAdvancedLoggerHobGuid);
    if (guidHob == NULL) 
    {
        DEBUG((DEBUG_ERROR, "%a: Advanced Logger HOB not found. Setting GPA to 0.\n", __FUNCTION__));
        WriteBiosDevice(BiosConfigSetEfiDiagnosticsGpa, 0);
    } 
    else 
    {
        // Get and validate the Advanced Logger pointer.
        advancedLoggerPtr = (ADVANCED_LOGGER_PTR *)GET_GUID_HOB_DATA (guidHob);
        if (advancedLoggerPtr == NULL) 
        {
            DEBUG((DEBUG_ERROR, "%a: Advanced Logger Ptr is NULL. Setting GPA to 0.\n", __FUNCTION__));
            WriteBiosDevice(BiosConfigSetEfiDiagnosticsGpa, 0);
        }
        else if (advancedLoggerPtr->LogBuffer >= MAX_UINT32)
        {
            DEBUG((DEBUG_ERROR, "%a: Advanced Logger buffer address 0x%llx >= 4GB. Setting GPA to 0.\n", __FUNCTION__, advancedLoggerPtr->LogBuffer));
            WriteBiosDevice(BiosConfigSetEfiDiagnosticsGpa, 0);
        }
        else 
        {
            // Get the Advanced Logger info header and set the proper GPA.
            ADVANCED_LOGGER_INFO* advancedLoggerInfo = (ADVANCED_LOGGER_INFO *)advancedLoggerPtr->LogBuffer;          
            DEBUG((DEBUG_INFO, "%a: Advanced Logger buffer address 0x%016llx\n", __FUNCTION__, advancedLoggerPtr->LogBuffer));
            DEBUG((DEBUG_INFO, "%a: Advanced Logger buffer size 0x%08x\n", __FUNCTION__, advancedLoggerInfo->LogBufferSize));
            WriteBiosDevice(BiosConfigSetEfiDiagnosticsGpa, (UINT32)(advancedLoggerPtr->LogBuffer));
        }
    }

    return EFI_SUCCESS;
}