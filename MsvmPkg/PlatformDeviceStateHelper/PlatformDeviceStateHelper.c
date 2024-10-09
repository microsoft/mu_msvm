/** @file
  Sets up the device state variable for use on displaying the device state

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Guid/GlobalVariable.h>

#include <Library/CrashLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceStateLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>

#include <IsolationTypes.h>



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

    return EFI_SUCCESS;
}