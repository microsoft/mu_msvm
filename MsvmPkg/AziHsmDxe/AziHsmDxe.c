/** @file
  Implementation of the Azure Integrated HSM DXE driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "AziHsmDdi.h"
#include "AziHsmDdiApi.h"
#include "AziHsmDxe.h"
#include "AziHsmPci.h"
#include "AziHsmHci.h"
#include "AziHsmCp.h"
#include "AziHsmBKS3.h"
#include "AziHsmAdmin.h"

#include <Library/DebugLib.h>

#include <Protocol/DevicePath.h>
#include <Protocol/DriverSupportedEfiVersion.h>
#include <Library/UefiLib.h>
#include <Guid/EventGroup.h>
#include <Guid/UnableToBootEvent.h>
#include <Protocol/ReportStatusCodeHandler.h>
#include <Pi/PiStatusCode.h>

STATIC EFI_EVENT  mAziHsmReadyToBootEvent = NULL;
STATIC EFI_EVENT  mAziHsmUnableToBootEvent = NULL;

//
// Global Platform Sealed Key - shared across all HSM devices
//
STATIC BOOLEAN        mAziHsmPHSealedKeyInit = FALSE;
STATIC AZIHSM_BUFFER  mAziHsmPHSealedKey     = { 0 };

//
// Function to cleanup sensitive data
//
VOID
EFIAPI
AziHsmCleanupSensitiveData (
  VOID
  );

//
// Driver Binding Instance
//
EFI_DRIVER_BINDING_PROTOCOL  gDriverBinding = {
  .Supported           = AziHsmBindingSupported,
  .Start               = AziHsmDriverBindingStart,
  .Stop                = AziHsmDriverBindingStop,
  .Version             = 0x10,
  .ImageHandle         = NULL,
  .DriverBindingHandle = NULL
};

EFI_COMPONENT_NAME_PROTOCOL  gComponentName = {
  .GetDriverName      = AziHsmGetDriverName,
  .GetControllerName  = AziHsmGetControllerName,
  .SupportedLanguages = "eng"
};

EFI_COMPONENT_NAME2_PROTOCOL  gComponentName2 = {
  .GetDriverName      = (EFI_COMPONENT_NAME2_GET_DRIVER_NAME)AziHsmGetDriverName,
  .GetControllerName  = (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME)AziHsmGetControllerName,
  .SupportedLanguages = "en"
};

EFI_UNICODE_STRING_TABLE  gDriverNameTable[] = {
  { .Language = "eng;en", .UnicodeString = L"Azure Integrated HSM Driver" },
  { .Language = NULL,     .UnicodeString = NULL                           }
};

EFI_UNICODE_STRING_TABLE  gControllerNameTable[] = {
  { .Language = "eng;en", .UnicodeString = L"Azure Integrated HSM Controller" },
  { .Language = NULL,     .UnicodeString = NULL                                }
};

EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL  gDriverSupportedEfiVersion = {
  .Length          = sizeof (EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL),
  .FirmwareVersion = 0x00010000
};

// AZIHSM_CONTROLLER_PRIVATE_DATA  *gPrivateData = NULL;

/**
  Tests to see if this driver supports a given controller. If a child device is provided,
  it further tests to see if this driver supports creating a handle for the specified child device.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to test.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.

  @retval EFI_SUCCESS              The device specified by ControllerHandle and
                                   RemainingDevicePath is supported by the driver specified by This.
  @retval EFI_ALREADY_STARTED      The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by the driver
                                   specified by This.
  @retval EFI_ACCESS_DENIED        The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by a different
                                   driver or an application that requires exclusive access.
                                   Currently not implemented.
  @retval EFI_UNSUPPORTED          The device specified by ControllerHandle and
                                   RemainingDevicePath is not supported by the driver specified by This.
**/
EFI_STATUS
EFIAPI
AziHsmBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *ParentDevicePath;
  EFI_PCI_IO_PROTOCOL       *PciIo;
  UINT16                    VendorId;
  UINT16                    DeviceId;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&ParentDevicePath,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );

  if (Status == EFI_ALREADY_STARTED) {
    Status = EFI_SUCCESS;
    goto Exit;
  }

  if (EFI_ERROR (Status)) {
    // DEBUG ((DEBUG_ERROR, "AziHsm: Failed to open Device Path protocol. Status: %r\n", Status));
    goto Exit;
  }

  gBS->CloseProtocol (Controller, &gEfiDevicePathProtocolGuid, This->DriverBindingHandle, Controller);

  // Attempt to Open PCI I/O Protocol
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );

  if (Status == EFI_ALREADY_STARTED) {
    Status = EFI_SUCCESS;
    goto Exit;
  }

  if (EFI_ERROR (Status)) {
    // DEBUG ((DEBUG_ERROR, "AziHsm: Failed to open PCI I/O protocol. Status: %r\n", Status));
    goto Exit;
  }

  Status = AziHsmPciReadVendorId (PciIo, &VendorId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to read PCI vendor ID. Status: %r\n", Status));
    goto Cleanup;
  }

  Status = AziHsmPciReadDeviceId (PciIo, &DeviceId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to read PCI device ID. Status: %r\n", Status));
    goto Cleanup;
  }

  if ((VendorId != AZIHSM_PCI_VENDOR_ID) || (DeviceId != AZIHSM_PCI_DEVICE_ID)) {
    Status = EFI_UNSUPPORTED;
    // DEBUG ((DEBUG_WARN, "AziHsm: Unsupported device. VendorId: 0x%04x, DeviceId: 0x%04x\n", VendorId, DeviceId));
  } else {
    Status = EFI_SUCCESS;
    DEBUG ((DEBUG_INFO, "AziHsm: Device found. VendorId: 0x%04x, DeviceId: 0x%04x\n", VendorId, DeviceId));
  }

Cleanup:

  gBS->CloseProtocol (Controller, &gEfiPciIoProtocolGuid, This->DriverBindingHandle, Controller);

Exit:

  return Status;
}

/**
  Starts a device controller or a bus controller.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  Controller           The handle of the controller to start.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.

  @retval EFI_SUCCESS              The device was started.
  @retval EFI_DEVICE_ERROR         The device could not be started due to a device error.Currently not implemented.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a lack of resources.
  @retval Others                   The driver failded to start the device.

**/
EFI_STATUS
EFIAPI
AziHsmDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *ParentDevicePath;
  EFI_PCI_IO_PROTOCOL       *PciIo;
  AZIHSM_CONTROLLER_STATE   *State;
  AZI_HSM_CTRL_IDEN         HsmIdenData;
  BOOLEAN                   IsHsmIdValid = FALSE;
  UINT8                     WrappedKey[AZIHSM_BUFFER_MAX_SIZE];
  AZIHSM_DDI_API_REV        ApiRevisionMin;
  AZIHSM_DDI_API_REV        ApiRevisionMax;
  AZIHSM_DERIVED_KEY        DerivedKey;
  AZIHSM_BUFFER             UnsealedKey;

  ZeroMem (&UnsealedKey, sizeof (UnsealedKey));
  ZeroMem (&DerivedKey, sizeof (DerivedKey));
  ZeroMem (WrappedKey, AZIHSM_BUFFER_MAX_SIZE);
  ZeroMem ((VOID *)&HsmIdenData, sizeof (HsmIdenData));
  ZeroMem ((VOID *)&ApiRevisionMin, sizeof (ApiRevisionMin));
  ZeroMem ((VOID *)&ApiRevisionMax, sizeof (ApiRevisionMax));

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&ParentDevicePath,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to open Device Path protocol. Status: %r\n", Status));
    goto Exit;
  }

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to open PCI I/O protocol. Status: %r\n", Status));
    goto CleanupParentDevicePath;
  }

  State = AllocateZeroPool (sizeof (AZIHSM_CONTROLLER_STATE));
  if (State == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to allocate memory for controller state. Status: %r\n", Status));
    goto CleanupPciIo;
  }

  State->Signature           = AZIHSM_CONTROLLER_SIGNATURE;
  State->ControllerHandle    = Controller;
  State->ImageHandle         = This->DriverBindingHandle;
  State->DriverBindingHandle = This->DriverBindingHandle;
  State->ParentDevicePath    = ParentDevicePath;
  State->PciIo               = PciIo;

  Status = PciIo->Attributes (PciIo, EfiPciIoAttributeOperationGet, 0, &State->PciAttributes);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to get PCI attributes. Status: %r\n", Status));
    goto CleanupState;
  }

  // Enable 64-bit DMA support in the PCI layer.
  Status = PciIo->Attributes (PciIo, EfiPciIoAttributeOperationEnable, EFI_PCI_IO_ATTRIBUTE_DUAL_ADDRESS_CYCLE, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to enable 64-bit DMA support. Status: %r\n", Status));
    goto CleanupState;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (&Controller, &gMsvmAziHsmProtocolGuid, &State->AziHsmProtocol, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to install AziHsm protocol. Status: %r\n", Status));
    goto CleanupState;
  }

  Status = AziHsmHciInitialize (State);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to initialize HCI driver. Status: %r\n", Status));
    goto CleanupAziHsmProtocol;
  }

  Status = AziHsmInitHsm (State);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Hsm Initialization Failed Failed. Status: %r\n", Status));
    goto CleanupAziHsmProtocol;
  }

  DEBUG ((DEBUG_INFO, "AziHsm: Starting BKS3 key derivation workflow\n"));
  // Get Unique ID for the HSM
  Status = AziHsmAdminIdentifyCtrl (State, (UINT8 *)&HsmIdenData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Identify Controller Failed. Status: %r\n", Status));
    goto CleanupAziHsmProtocol;
  } else {
    //
    // Check if HSM serial number is all zeros
    //
    IsHsmIdValid = !IsZeroBuffer (HsmIdenData.Sn, AZIHSM_CTRL_IDENT_SN_LEN);

    if (!IsHsmIdValid) {
      DEBUG ((DEBUG_ERROR, "AziHsm: Identify Controller Failed. Invalid HSM ID: All zeros\n"));
      Status = EFI_DEVICE_ERROR;
      goto CleanupAziHsmProtocol;
    }

    DEBUG ((DEBUG_INFO, "AziHsm: Identify Controller Success. HSM Ctrl_Id: %d\n", HsmIdenData.Ctrl_Id));
  }

  //
  // Get API revision
  //
  Status         = AziHsmGetApiRevision (State, &ApiRevisionMin, &ApiRevisionMax);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to get API revision: %r\n", Status));
    goto CleanupAziHsmProtocol;
  }

  if (mAziHsmPHSealedKeyInit) {
    //
    // Unseal the sealed blob using null hierarchy
    //
    Status = AziHsmUnsealNullHierarchy (&mAziHsmPHSealedKey, &UnsealedKey);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "AziHsm: Failed to unseal platform key sealed blob using null hierarchy: %r\n", Status));
      goto CleanupAziHsmProtocol;
    }

    Status = AziHsmDeriveSecretFromBlob (&UnsealedKey, (UINT8 *)(HsmIdenData.Sn), sizeof (HsmIdenData.Sn), &DerivedKey);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "AziHsm: Failed to derive BKS3 key from unsealed blob: %r\n", Status));
      goto CleanupAziHsmProtocol;
    }
  } else {
    DEBUG ((DEBUG_ERROR, "AziHsm: Platform hierarchy secret not available.\n"));
    goto CleanupAziHsmProtocol;
  }

  UINT16  WrappedKeySize = (UINT16)AZIHSM_BUFFER_MAX_SIZE;

  Status = AziHsmInitBks3 (
             State,
             ApiRevisionMax,
             DerivedKey.KeyData,
             sizeof (DerivedKey.KeyData),
             WrappedKey,
             &WrappedKeySize
             );

  //
  // Call Manticore API to get wrapped key
  //
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to get wrapped key from Manticore\n"));
    goto CleanupAziHsmProtocol;
  }

  DEBUG ((DEBUG_INFO, "AziHsm: Successfully retrieved wrapped key from Manticore\n"));

  goto Exit;

CleanupAziHsmProtocol:
  gBS->UninstallMultipleProtocolInterfaces (Controller, &gMsvmAziHsmProtocolGuid, &State->AziHsmProtocol, NULL);

CleanupState:
  FreePool (State);

CleanupPciIo:
  gBS->CloseProtocol (Controller, &gEfiPciIoProtocolGuid, This->DriverBindingHandle, Controller);

CleanupParentDevicePath:
  gBS->CloseProtocol (Controller, &gEfiDevicePathProtocolGuid, This->DriverBindingHandle, Controller);

Exit:
  ZeroMem (WrappedKey, sizeof (WrappedKey));
  ZeroMem (DerivedKey.KeyData, sizeof (DerivedKey.KeyData));
  ZeroMem (&UnsealedKey, sizeof (UnsealedKey));
  DEBUG ((DEBUG_INFO, "AziHsm: DriverBindingStart completed. Status: %r\n", Status));
  return Status;
}

/**
  Stops a device controller or a bus controller.

  @param[in]  This              A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle  A handle to the device being stopped.
  @param[in]  NumberOfChildren  The number of child device handles in ChildHandleBuffer.
  @param[in]  ChildHandleBuffer An array of child handles to be freed.

  @retval EFI_SUCCESS           The device was stopped.
  @retval EFI_DEVICE_ERROR      The device could not be stopped due to a device error.

**/
EFI_STATUS
EFIAPI
AziHsmDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Controller,
  IN  UINTN                        NumberOfChildren,
  IN  EFI_HANDLE                   *ChildHandleBuffer
  )
{
  EFI_STATUS               Status;
  EFI_STATUS               ProtocolCloseStatus;
  AZIHSM_PROTOCOL          *AziHsmProtocol;
  AZIHSM_CONTROLLER_STATE  *State;

  Status = gBS->HandleProtocol (Controller, &gMsvmAziHsmProtocolGuid, (VOID **)&AziHsmProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to get AziHsm protocol. Status: %r\n", Status));
    goto Exit;
  }

  State = AZIHSM_CONTROLLER_STATE_FROM_PROTOCOL (AziHsmProtocol);
  if (State == NULL) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Invalid AziHsm state. Status: %r\n", Status));
    goto Exit;
  }

  Status = AziHsmHciUninitialize (State);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to uninitialize HCI driver. Status: %r\n", Status));
  }

  //
  // Continue with cleanup even if uninitialization fails
  //
  ProtocolCloseStatus = gBS->UninstallMultipleProtocolInterfaces (
                          Controller,
                          &gMsvmAziHsmProtocolGuid,
                          &State->AziHsmProtocol,
                          NULL
                          );
  if (EFI_ERROR (ProtocolCloseStatus)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to uninstall AziHsm protocol. Status: %r\n", ProtocolCloseStatus));
  }

  Status = ProtocolCloseStatus;

  FreePool (State);

  ProtocolCloseStatus = gBS->CloseProtocol (
                          Controller,
                          &gEfiPciIoProtocolGuid,
                          This->DriverBindingHandle,
                          Controller
                          );
  if (EFI_ERROR (ProtocolCloseStatus)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to close PciIo protocol. Status: %r\n", ProtocolCloseStatus));
    if (!EFI_ERROR (Status)) {
      Status = ProtocolCloseStatus;
    }
  }

  ProtocolCloseStatus = gBS->CloseProtocol (
                          Controller,
                          &gEfiDevicePathProtocolGuid,
                          This->DriverBindingHandle,
                          Controller
                          );
  if (EFI_ERROR (ProtocolCloseStatus)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to close DevicePath protocol. Status: %r\n", ProtocolCloseStatus));
    if (!EFI_ERROR (Status)) {
      Status = ProtocolCloseStatus;
    }
  }

Exit:
  //
  // Clear sensitive data as backup cleanup trigger
  // This ensures cleanup happens if driver is stopped for any reason
  //
  DEBUG ((DEBUG_WARN, "[AziHsm] DriverBindingStop - triggering sensitive data cleanup\n"));
  AziHsmCleanupSensitiveData ();
  DEBUG ((DEBUG_INFO, "AziHsm: DriverBindingStop completed. Status: %r\n", Status));
  return Status;
}

/**
  Retrieves a Unicode string that is the user readable name of the driver.

  @param[in]  This              A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                                EFI_COMPONENT_NAME_PROTOCOL instance.
  @param[in]  Language          A pointer to a Null-terminated ASCII string array
                                indicating the language.
  @param[out]  DriverName       A pointer to the Unicode string to return.

  @retval EFI_SUCCESS           The Unicode string for the Driver specified by
                                This and the language specified by Language was
                                returned in DriverName.
  @retval EFI_INVALID_PARAMETER Language is NULL.
  @retval EFI_INVALID_PARAMETER DriverName is NULL.
  @retval EFI_UNSUPPORTED       The driver specified by This does not support
                                the language specified by Language.
**/
EFI_STATUS
EFIAPI
AziHsmGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **DriverName
  )
{
  EFI_STATUS  Status;

  Status = LookupUnicodeString2 (
             Language,
             This->SupportedLanguages,
             gDriverNameTable,
             DriverName,
             (BOOLEAN)(This == &gComponentName)
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to get driver name. Status: %r\n", Status));
    goto Exit;
  }

Exit:

  return Status;
}

/**
  Retrieves a Unicode string that is the user readable name of the controller
  that is being managed by a driver.

  @param[in]  This              A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                                EFI_COMPONENT_NAME_PROTOCOL instance.
  @param[in]  ControllerHandle  The handle of a controller that the driver
                                specified by This is managing.
  @param[in]  ChildHandle       The handle of the child controller to retrieve
                                the name of.
  @param[in]  Language          A pointer to a Null-terminated ASCII string
                                array indicating the language.
  @param[out]  ControllerName   A pointer to the Unicode string to return.

  @retval EFI_SUCCESS           The Unicode string for the user readable name in
                                the language specified by Language for the
                                driver specified by This was returned in
                                DriverName.
  @retval EFI_INVALID_PARAMETER ControllerHandle is NULL.
  @retval EFI_INVALID_PARAMETER ChildHandle is not NULL and it is not a valid
                                EFI_HANDLE.
  @retval EFI_INVALID_PARAMETER Language is NULL.
  @retval EFI_INVALID_PARAMETER ControllerName is NULL.
  @retval EFI_UNSUPPORTED       The driver specified by This is not currently
                                managing the controller specified by
                                ControllerHandle and ChildHandle.
  @retval EFI_UNSUPPORTED       The driver specified by This does not support
                                the language specified by Language.
**/
EFI_STATUS
EFIAPI
AziHsmGetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  EFI_HANDLE                   ControllerHandle,
  IN  EFI_HANDLE                   ChildHandle        OPTIONAL,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **ControllerName
  )
{
  EFI_STATUS  Status;

  // Ensure this driver is currently managing ControllerHandle
  Status = EfiTestManagedDevice (
             ControllerHandle,
             gDriverBinding.DriverBindingHandle,
             &gEfiPciIoProtocolGuid
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Driver is not managing the controller. Status: %r\n", Status));
    goto Exit;
  }

  Status = LookupUnicodeString2 (
             Language,
             This->SupportedLanguages,
             gControllerNameTable,
             ControllerName,
             (BOOLEAN)(This == &gComponentName)
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to get controller name. Status: %r\n", Status));
    goto Exit;
  }

Exit:

  return Status;
}

/**
  This is the unload handle for the HSM driver.

  Disconnect the driver specified by ImageHandle from the HSM device in the handle database.
  Uninstall all the protocols installed in the driver.

  @param[in]  ImageHandle       The drivers' driver image.

  @retval EFI_SUCCESS           The image is unloaded.
  @retval Others                Failed to unload the image.

**/
EFI_STATUS
EFIAPI
AziHsmDriverUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS                    Status;
  EFI_HANDLE                    *DeviceHandleBuffer;
  UINTN                         DeviceHandleCount;
  UINTN                         Index;
  EFI_COMPONENT_NAME_PROTOCOL   *ComponentName;
  EFI_COMPONENT_NAME2_PROTOCOL  *ComponentName2;

  DeviceHandleBuffer = NULL;

  // Get the list of the device handles managed by this driver.
  // If there is an error getting the list, then means the driver
  // doesn't manage any device. At this way, we would only close
  // those protocols installed at image handle.
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gMsvmAziHsmProtocolGuid,
                  NULL,
                  &DeviceHandleCount,
                  &DeviceHandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to locate device handles. Status: %r\n", Status));
    goto Exit;
  }

  // Disconnect the driver specified by ImageHandle from all
  // the devices in the handle database.
  for (Index = 0; Index < DeviceHandleCount; Index++) {
    Status = gBS->DisconnectController (DeviceHandleBuffer[Index], ImageHandle, NULL);
    if (EFI_ERROR (Status)) {
      goto Exit;
    }
  }

  //
  // Uninstall all the protocols installed in the driver entry point
  //
  Status = gBS->UninstallMultipleProtocolInterfaces (
                  ImageHandle,
                  &gEfiDriverBindingProtocolGuid,
                  &gDriverBinding,
                  &gEfiDriverSupportedEfiVersionProtocolGuid,
                  &gDriverSupportedEfiVersion,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to uninstall gEfiDriverBindingProtocolGuid. Status: %r\n", Status));
    goto Exit;
  }

  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiComponentNameProtocolGuid,
                  (VOID **)&ComponentName
                  );
  if (!EFI_ERROR (Status)) {
    gBS->UninstallProtocolInterface (
           ImageHandle,
           &gEfiComponentNameProtocolGuid,
           ComponentName
           );
  }

  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiComponentName2ProtocolGuid,
                  (VOID **)&ComponentName2
                  );
  if (!EFI_ERROR (Status)) {
    gBS->UninstallProtocolInterface (
           ImageHandle,
           &gEfiComponentName2ProtocolGuid,
           ComponentName2
           );
  }

  Status = EFI_SUCCESS;

Exit:
  DEBUG ((DEBUG_INFO, "AziHsm: Driver Unload completed. Status: %r\n", Status));

  return Status;
}

/**
  Ready to Boot event notification handler.

  @param[in] Event    The event that triggered this notification
  @param[in] Context  Context pointer (can be NULL)
**/
VOID
EFIAPI
AziHsmReadyToBootCallback (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  DEBUG ((DEBUG_INFO, "AziHsm: Ready to Boot event triggered - clearing sensitive data\n"));

  //
  // Clear sensitive data when ready to boot
  //
  AziHsmCleanupSensitiveData ();

  //
  // Close the event after handling
  //
  gBS->CloseEvent (Event);
}

/**
  Unable to Boot event notification handler.
  This is called when the system is unable to find bootable devices/options.

  @param[in] Event   The event that triggered this notification
  @param[in] Context Context pointer (can be NULL)
**/
VOID
EFIAPI
AziHsmUnableToBootCallback (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  DEBUG ((DEBUG_ERROR, "AziHsm: Unable to Boot event triggered - clearing sensitive data\n"));

  // Clear sensitive data when unable to boot
  AziHsmCleanupSensitiveData();
  
  // Close the event after handling
  gBS->CloseEvent (Event);
}
/**
  The entry point for HSM driver, used to install HSM driver on the ImageHandle.

  @param[in]  ImageHandle  Handle for this image
  @param[in]  SystemTable  Pointer to the system table

  @retval EFI_SUCCESS      Driver loaded successfully
  @retval Other            Error occurred during loading
**/
EFI_STATUS
EFIAPI
AziHsmDriverEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS          Status;
  AZIHSM_DERIVED_KEY  TpmDerivedKey;
  AZIHSM_BUFFER       TpmDerivedKeyBlob;
  AZIHSM_BUFFER       SealedKeyBlob;

  ZeroMem (&TpmDerivedKey, sizeof (TpmDerivedKey));
  ZeroMem (&TpmDerivedKeyBlob, sizeof (TpmDerivedKeyBlob));
  ZeroMem (&SealedKeyBlob, sizeof (SealedKeyBlob));

  //
  // Derive BKS3 key from TPM using the workflow
  //
  Status = AziHsmDeriveSecretFromTpm (&TpmDerivedKey);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "AziHsm: BKS3 key derivation workflow failed: %r\n", Status));
    goto Exit;
  }

  //
  // Seal the derived key to the null hierarchy for secure storage
  //
  if (TpmDerivedKey.KeySize > sizeof (TpmDerivedKeyBlob.Data)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Derived key size exceeds maximum buffer size of the TpmDerivedKeyBlob\n"));
    Status = EFI_BAD_BUFFER_SIZE;
    goto Exit;
  }

  CopyMem (TpmDerivedKeyBlob.Data, TpmDerivedKey.KeyData, TpmDerivedKey.KeySize);
  TpmDerivedKeyBlob.Size = (UINT32)TpmDerivedKey.KeySize;

  // Seal the derived key to the TPM null hierarchy(to ensure it is associated with current boot) and
  // does not persist across reboots
  Status = AziHsmSealToNullHierarchy (&TpmDerivedKeyBlob, &SealedKeyBlob);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Sealing to null hierarchy failed: %r\n", Status));
    goto Exit;
  }

  //
  // Store the sealed key globally for reuse across all HSM devices
  //
  CopyMem (&(mAziHsmPHSealedKey.Data), &(SealedKeyBlob.Data), SealedKeyBlob.Size);
  mAziHsmPHSealedKey.Size = SealedKeyBlob.Size;
  mAziHsmPHSealedKeyInit  = TRUE;

  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gDriverBinding,
             ImageHandle,
             &gComponentName,
             &gComponentName2
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Install driver binding failed. Status: %r\n", Status));
    goto Cleanup;
  }

  // Install EFI Driver Supported EFI Version Protocol required for
  // EFI drivers that are on PCI and other plug in cards.
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEfiDriverSupportedEfiVersionProtocolGuid,
                  &gDriverSupportedEfiVersion,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Install Driver Supported EFI Version failed. Status: %r\n", Status));
    goto Cleanup;
  }

  //
  // Register for Ready to Boot event to clear sensitive data before OS launch
  // This is the standard event that fires just before UEFI transfers control to OS
  //
  Status = EfiCreateEventReadyToBootEx (
             TPL_CALLBACK,
             AziHsmReadyToBootCallback,
             NULL,
             &mAziHsmReadyToBootEvent
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to create Ready to Boot event: %r\n", Status));
    goto Cleanup;
  }

  DEBUG ((DEBUG_INFO, "AziHsm: Ready to Boot event registered successfully\n"));

  //
  // Register to unable to boot event to cleanup sensitive data
  // This event is signaled when the system cannot find bootable devices/options
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  AziHsmUnableToBootCallback,
                  NULL,
                  &gMsvmUnableToBootEventGuid,
                  &mAziHsmUnableToBootEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to create Unable to Boot event: %r\n", Status));
    goto Cleanup;
  }

  DEBUG ((DEBUG_INFO, "AziHsm: Unable to Boot event registered successfully\n"));

  DEBUG ((DEBUG_INFO, "AziHsm: Driver loaded successfully\n"));
  goto Exit;

Cleanup:
  ZeroMem(&mAziHsmPHSealedKey, sizeof(mAziHsmPHSealedKey));
  mAziHsmPHSealedKeyInit = FALSE;
Exit:
  ZeroMem (&TpmDerivedKey, sizeof (TpmDerivedKey));
  ZeroMem (&TpmDerivedKeyBlob, sizeof (TpmDerivedKeyBlob));
  ZeroMem (&SealedKeyBlob, sizeof (SealedKeyBlob));

  return Status;
}

//
// Flag to prevent multiple cleanup calls
//
STATIC BOOLEAN  mSensitiveDataCleared = FALSE;

/**
  Cleanup sensitive data from HSM and memory.
  Called from multiple triggers to ensure cleanup happens regardless of boot outcome.
**/
VOID
EFIAPI
AziHsmCleanupSensitiveData (
  VOID
  )
{
  if (mSensitiveDataCleared) {
    DEBUG ((DEBUG_INFO, "AziHsm: Sensitive data already cleared, skipping\n"));
    return;
  }

  DEBUG ((DEBUG_ERROR, "AziHsm: *** Starting sensitive data cleanup ***\n"));

  //
  // Clear sensitive data from HSM before OS takes control
  //
  if (mAziHsmPHSealedKeyInit) {
    ZeroMem (&mAziHsmPHSealedKey, sizeof (AZIHSM_BUFFER));
    mAziHsmPHSealedKeyInit = FALSE;
    DEBUG ((DEBUG_ERROR, "AziHsm: Global Platform Hierarchy key cleared\n"));
  }

  mSensitiveDataCleared = TRUE;
  DEBUG ((DEBUG_ERROR, "AziHsm: *** Sensitive data cleanup completed ***\n"));
}

