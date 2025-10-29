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

STATIC EFI_EVENT  mAziHsmReadyToBootEvent = NULL;
STATIC EFI_EVENT  mAziHsmUnableToBootEvent = NULL;

//
// Global Platform Sealed Key - shared across all HSM devices
//
STATIC BOOLEAN        mAziHsmSealedPlatormSecretDerived = FALSE;
STATIC AZIHSM_BUFFER  mAziHsmSealedPlatformSecret     = { 0 };

// Forward declarations for internal helper functions

STATIC
EFI_STATUS
AziHsmAes256CbcEncrypt (
  IN  UINT8   *InputData,
  IN  UINTN   InputDataSize,
  OUT UINT8   *OutputData,
  OUT UINTN   *OutputDataSize,
  IN  UINT8   *Key,
  IN  UINTN   KeySize,
  IN  UINT8   *Iv,
  IN  UINTN   IvSize
);

STATIC
EFI_STATUS
AziHsmPerformBks3SealingWorkflow (
  IN  AZIHSM_CONTROLLER_STATE  *State,
  IN  AZIHSM_DDI_API_REV       ApiRevisionMax,
  IN  UINT8                    *HsmSerialData,
  IN  UINTN                    HsmSerialDataLength
);

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
    DEBUG((DEBUG_INFO, "AziHsmDxe: Controller already started, checking if supported\n"));
    Status = EFI_SUCCESS;
    goto Exit;
  }

  if (EFI_ERROR (Status)) {
   // No need to log here, as failure to open Device Path likely means unsupported device
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
    DEBUG((DEBUG_INFO, "AziHsmDxe: PCI I/O already started, checking if supported\n"));
    Status = EFI_SUCCESS;
    goto Exit;
  }

  if (EFI_ERROR (Status)) {
   // No need to log here, as failure to open PCI I/O likely means unsupported device
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
    DEBUG ((DEBUG_WARN, "AziHsm: Unsupported device. VendorId: 0x%04x, DeviceId: 0x%04x\n", VendorId, DeviceId));
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
  AZIHSM_DDI_API_REV        ApiRevisionMin;
  AZIHSM_DDI_API_REV        ApiRevisionMax;
  AZIHSM_DERIVED_KEY        DerivedKey;
  AZIHSM_BUFFER             UnsealedKey;

  ZeroMem (&UnsealedKey, sizeof (UnsealedKey));
  ZeroMem (&DerivedKey, sizeof (DerivedKey));
  ZeroMem ((VOID *)&HsmIdenData, sizeof (HsmIdenData));

  DEBUG ((DEBUG_INFO, "AziHsm: DriverBindingStart called for Controller: %p\n", Controller));

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
    goto CleanupHci;
  }

  DEBUG ((DEBUG_INFO, "AziHsm: Starting BKS3 key derivation workflow\n"));
  // Get Unique ID for the HSM
  Status = AziHsmAdminIdentifyCtrl (State, (UINT8 *)&HsmIdenData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Identify Controller Failed. Status: %r\n", Status));
    goto CleanupHci;
  } else {
    //
    // Check if HSM serial number is all zeros
    //
    IsHsmIdValid = !IsZeroBuffer (HsmIdenData.Sn, AZIHSM_CTRL_IDENT_SN_LEN);

    if (!IsHsmIdValid) {
      DEBUG ((DEBUG_ERROR, "AziHsm: Identify Controller Failed. Invalid HSM ID: All zeros\n"));
      Status = EFI_DEVICE_ERROR;
      goto CleanupHci;
    }

    DEBUG ((DEBUG_INFO, "AziHsm: Identify Controller Success. HSM Ctrl_Id: %d\n", HsmIdenData.Ctrl_Id));
  }

  //
  // Get API revision
  //
  ApiRevisionMin = (AZIHSM_DDI_API_REV){ 0 };
  ApiRevisionMax = (AZIHSM_DDI_API_REV){ 0 };
  Status         = AziHsmGetApiRevision (State, &ApiRevisionMin, &ApiRevisionMax);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to get API revision: %r\n", Status));
    Status = EFI_UNSUPPORTED;
    goto CleanupHci;
  }

  // Perform complete BKS3 derivation and sealing workflow: Use HSM serial number as input
  Status = AziHsmPerformBks3SealingWorkflow (State, ApiRevisionMax, (UINT8 *)(&HsmIdenData.Sn), sizeof (HsmIdenData.Sn));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: BKS3 derivation and sealing workflow failed. Status: %r\n", Status));
    Status = EFI_UNSUPPORTED;
    goto CleanupHci;
  }

  // Successfully started the controller
  goto Exit;

CleanupHci:
  AziHsmHciUninitialize (State);

CleanupAziHsmProtocol:
  gBS->UninstallMultipleProtocolInterfaces (Controller, &gMsvmAziHsmProtocolGuid, &State->AziHsmProtocol, NULL);

CleanupState:
  FreePool (State);

CleanupPciIo:
  gBS->CloseProtocol (Controller, &gEfiPciIoProtocolGuid, This->DriverBindingHandle, Controller);

CleanupParentDevicePath:
  gBS->CloseProtocol (Controller, &gEfiDevicePathProtocolGuid, This->DriverBindingHandle, Controller);

Exit:
  DEBUG ((DEBUG_INFO, "AziHsm: DriverBindingStart completed. Status: %r\n", Status));
  return Status;
}

/**
  Performs the complete BKS3 workflow including key derivation, wrapped key generation, AES encryption and TPM sealing.

  This function handles the complete workflow to derive and seal the BKS3 key:
  1. Derives BKS3 key from TPM using AziHsmDeriveBKS3FromTpm
  2. Calls AziHsmInitBks3 to get wrapped key from HSM using derived key
  3. Generates random AES key and IV using TPM
  4. Encrypts the wrapped key with AES-256-CBC
  5. Seals the AES key/IV to TPM NULL hierarchy
  6. Creates the final blob with sealed key data and encrypted wrapped key
  7. Sends the sealed blob to the HSM via AziHsmSetSealedBks3
  8. Securely clears all sensitive key material

  @param[in]  State             Pointer to the AziHsm controller state
  @param[in]  ApiRevisionMax    Maximum API revision supported by the HSM

  @retval EFI_SUCCESS           BKS3 workflow completed successfully
  @retval EFI_DEVICE_ERROR      TPM operations, key derivation or AES encryption failed
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failed
  @retval EFI_BUFFER_TOO_SMALL  Buffer size insufficient for the operation
  @retval Other                 Error returned by AziHsmDeriveBKS3FromTpm, AziHsmInitBks3, AziHsmSealToTpmNullHierarchy or AziHsmSetSealedBks3
**/
STATIC
EFI_STATUS
AziHsmPerformBks3SealingWorkflow (
  IN  AZIHSM_CONTROLLER_STATE  *State,
  IN  AZIHSM_DDI_API_REV       ApiRevisionMax,
  IN  UINT8                    *HsmSerialData,
  IN  UINTN                    HsmSerialDataLength
)
{
  EFI_STATUS          Status = EFI_INVALID_PARAMETER;
  AZIHSM_BUFFER       SealedBKS3Buffer;
  AZIHSM_BUFFER       SealedAesSecret;
  AZIHSM_BUFFER       TpmPlatformSecret;
  UINT8               *InputData = NULL;
  UINT8               *EncryptedData = NULL;
  UINTN               PadValue = 0;
  UINT8               Iv[AZIHSM_AES_IV_SIZE];
  UINT8               Aes256Key[AZIHSM_AES256_KEY_SIZE];
  UINTN               PaddedInputSize = 0;
  UINTN               EncryptedDataSize = 0;
  BOOLEAN             IsHSMSealSuccess = FALSE;
  UINT8               WrappedBKS3[AZIHSM_BUFFER_MAX_SIZE];
  UINT16              WrappedBKS3KeySize = (UINT16)AZIHSM_BUFFER_MAX_SIZE;
  AZIHSM_DERIVED_KEY  BKS3Key;
  AZIHSM_BUFFER       KeyIvBuffer;
  AZIHSM_KEY_IV_RECORD KeyIvRecord;
  UINT32              ExpectedSealedDataSize;
  UINT8               HsmGuid[AZIHSM_HSM_GUID_MAX_SIZE];
  UINT16              HsmGuidSize = AZIHSM_HSM_GUID_MAX_SIZE;
  AZIHSM_TCG_CONTEXT  TcgContext;


  if (State == NULL || HsmSerialData == NULL || HsmSerialDataLength == 0) {
    DEBUG ((DEBUG_ERROR, "AziHsm: AziHsmPerformBks3SealingWorkflow() Invalid parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&SealedBKS3Buffer, sizeof (SealedBKS3Buffer));
  ZeroMem (&SealedAesSecret, sizeof (SealedAesSecret));
  ZeroMem (WrappedBKS3, AZIHSM_BUFFER_MAX_SIZE);
  ZeroMem (&BKS3Key, sizeof (BKS3Key));
  ZeroMem (&TpmPlatformSecret, sizeof(TpmPlatformSecret));
  ZeroMem (HsmGuid, sizeof (HsmGuid));
  ZeroMem (&TcgContext, sizeof (TcgContext));

  DEBUG ((DEBUG_INFO, "AziHsm: Starting BKS3 key derivation workflow\n"));

  if (!mAziHsmSealedPlatormSecretDerived) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Sealed Platform hierarchy secret not available.\n"));
    Status = EFI_NOT_READY;
    goto Exit;
  }

  // Unseal the sealed blob using null hierarchy (ensures we can only unseal in current boot session)
  Status = AziHsmUnsealUsingTpmNullHierarchy (&mAziHsmSealedPlatformSecret, &TpmPlatformSecret);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to unseal platform key sealed blob using null hierarchy: %r\n", Status));
    goto Exit;
  }

  Status = AziHsmDeriveBKS3fromId (&TpmPlatformSecret, (UINT8 *)(HsmSerialData), HsmSerialDataLength, &BKS3Key);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to derive BKS3 key from unsealed blob: %r\n", Status));
    goto Exit;
  }

  // Call AziHsmInitBks3 to get wrapped key from HSM
  Status = AziHsmInitBks3 (
             State,
             ApiRevisionMax,
             BKS3Key.KeyData,
             sizeof (BKS3Key.KeyData),
             WrappedBKS3,
             &WrappedBKS3KeySize,
             HsmGuid,
             &HsmGuidSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to get wrapped key from HSM. Status: %r\n", Status));
    goto Exit;
  }

  if (HsmGuidSize != AZIHSM_GUID_SIZE) {
    DEBUG ((DEBUG_ERROR, "AziHsm: HSM GUID size is not as expected. Size: %d != %d\n", HsmGuidSize, AZIHSM_GUID_SIZE));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  // Use TPM to get random AES key and IV
  if (EFI_ERROR (AziHsmTpmGetRandom (sizeof (Aes256Key), Aes256Key))) {
    DEBUG ((DEBUG_ERROR, "AziHsm: AziHsmPerformBks3SealingWorkflow - TPM GetRandom failed for key\n"));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }
  if (EFI_ERROR (AziHsmTpmGetRandom (sizeof (Iv), Iv))) {
    DEBUG ((DEBUG_ERROR, "AziHsm: AziHsmPerformBks3SealingWorkflow - TPM GetRandom failed for IV\n"));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  // We need to add PKCS7 padding to ensure the data is block-aligned for AES encryption
  if (WrappedBKS3KeySize % AES_BLOCK_SIZE != 0) {
    PadValue = AES_BLOCK_SIZE - (WrappedBKS3KeySize % AES_BLOCK_SIZE);
  }

  PaddedInputSize = WrappedBKS3KeySize + PadValue;

  InputData = AllocatePool (PaddedInputSize);
  if (InputData == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  EncryptedData = AllocatePool (PaddedInputSize);
  if (EncryptedData == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Cleanup;
  }

  CopyMem (InputData, WrappedBKS3, WrappedBKS3KeySize);
  // SetMem signature: VOID *Buffer, UINTN Size, UINT8 Value
  SetMem (InputData + WrappedBKS3KeySize, PadValue, (UINT8)PadValue);

  // We encrypt the padded WrappedBKS3 with AES-256 CBC with the key/Iv we generated.
  Status = AziHsmAes256CbcEncrypt (
             InputData,
             PaddedInputSize,
             EncryptedData,
             &EncryptedDataSize,
             Aes256Key,
             sizeof (Aes256Key),
             Iv,
             sizeof (Iv)
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: AES256-CBC encryption failed : %r\n", Status));
    goto Cleanup;
  }

  // Now seal the AES key and IV to the TPM NULL hierarchy
  // Create a Key/IV record structure to hold the key and IV
  ZeroMem (&KeyIvRecord, sizeof (KeyIvRecord));
 
  KeyIvRecord.KeySize = (UINT8)sizeof (KeyIvRecord.Key);
  CopyMem (KeyIvRecord.Key, Aes256Key, sizeof (KeyIvRecord.Key));

  KeyIvRecord.IvSize = (UINT8)sizeof (KeyIvRecord.Iv);
  CopyMem (KeyIvRecord.Iv, Iv, sizeof (KeyIvRecord.Iv));

  KeyIvRecord.KeyVersion = AZIHSM_AES_KEY_VERSION;
  // RecordSize does not include the size of the RecordSize field itself
  KeyIvRecord.RecordSize = sizeof(AZIHSM_KEY_IV_RECORD) - sizeof(UINT16);

  
  ZeroMem (&KeyIvBuffer, sizeof (KeyIvBuffer));
  if (sizeof (KeyIvRecord) > sizeof (KeyIvBuffer.Data)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Key/IV record too large for AZIHSM_BUFFER\n"));
    Status = EFI_BUFFER_TOO_SMALL;
    goto Cleanup;
  }

  CopyMem (KeyIvBuffer.Data, &KeyIvRecord, sizeof (KeyIvRecord));
  KeyIvBuffer.Size = (UINT16)sizeof (KeyIvRecord);

  Status = AziHsmSealToTpmNullHierarchy (&KeyIvBuffer, &SealedAesSecret);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to get the sealed blob from TPM via AziHsmSealToTpmNullHierarchy. Status: %r\n", Status));
    goto Cleanup;
  }

  if (SealedAesSecret.Size >= AZIHSM_BUFFER_MAX_SIZE) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Size of sealedblob is greater than allocated buffer, %d > %d", SealedAesSecret.Size, AZIHSM_BUFFER_MAX_SIZE));
    Status = EFI_BUFFER_TOO_SMALL;
    goto Cleanup;
  }

  ExpectedSealedDataSize = (sizeof (SealedAesSecret.Size) + SealedAesSecret.Size + sizeof (UINT32) + (UINT32)EncryptedDataSize);
  // check if sealedblobsize+encrypted data size can fit in AZIHSM BUFFER
  if (ExpectedSealedDataSize > AZIHSM_BUFFER_MAX_SIZE) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Sealed blob size plus encrypted data size exceeds buffer size : %d > %d\n", ExpectedSealedDataSize, AZIHSM_BUFFER_MAX_SIZE));
    Status = EFI_BUFFER_TOO_SMALL;
    goto Cleanup;
  }

  DEBUG ((DEBUG_INFO, "AziHsm: Sealed blob size is : %u\n", SealedAesSecret.Size));

  // Sealing of AES key to TPM is success, now prepare data for HSM sealing
  // Copy size of SealedAesSecret to Wrapped data
  SealedBKS3Buffer.Size = 0;
  CopyMem (SealedBKS3Buffer.Data, (UINT8 *)&SealedAesSecret.Size, sizeof (SealedAesSecret.Size));
  SealedBKS3Buffer.Size += (UINT16)sizeof (SealedAesSecret.Size);
  // copy sealedblob data
  CopyMem (&SealedBKS3Buffer.Data[SealedBKS3Buffer.Size], SealedAesSecret.Data, SealedAesSecret.Size);
  SealedBKS3Buffer.Size += (UINT16)SealedAesSecret.Size;
  // copy size of encrypted data size
  CopyMem (&SealedBKS3Buffer.Data[SealedBKS3Buffer.Size], (UINT8 *)&EncryptedDataSize, sizeof (UINT32));
  SealedBKS3Buffer.Size += (UINT32)sizeof (UINT32);

  CopyMem (&SealedBKS3Buffer.Data[SealedBKS3Buffer.Size], EncryptedData, EncryptedDataSize);
  SealedBKS3Buffer.Size += (UINT32)EncryptedDataSize;

  // Check if we actually calculated correct size
  if (ExpectedSealedDataSize != SealedBKS3Buffer.Size) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Expected SealBks3 Blob size is not matching with calculated blob size. Expected %d, Got %d\n", ExpectedSealedDataSize, SealedBKS3Buffer.Size));
    Status = EFI_ABORTED;
    goto Cleanup;
  }

  DEBUG ((DEBUG_INFO, "AziHsm: SetSealBKS3 Blob size : %d\n", SealedBKS3Buffer.Size));

  Status = AziHsmSetSealedBks3 (State, ApiRevisionMax, SealedBKS3Buffer.Data, SealedBKS3Buffer.Size, &IsHSMSealSuccess);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to execute the HSM command. Status: %r\n", Status));
    goto Cleanup;
  }

  if (!IsHSMSealSuccess) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to set the sealed BKS3 key to HSM.\n"));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  // Measure HSM GUID to PCR 6
  CopyMem (TcgContext.Guid, HsmGuid, HsmGuidSize);

  Status = AziHsmMeasureGuidEvent(&TcgContext);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to measure HSM GUID to TPM PCR 6. Status: %r\n", Status));
    goto Cleanup;
  }

  DEBUG ((DEBUG_INFO, "AziHsm: HSM BKS3 key sealed to the device successfully\n"));
  Status = EFI_SUCCESS;

Cleanup:
  // Clean up allocated resources and zero memory for security
  ZeroMem (&SealedBKS3Buffer, sizeof (SealedBKS3Buffer));
  ZeroMem (&SealedAesSecret, sizeof (SealedAesSecret));
  ZeroMem (Aes256Key, sizeof (Aes256Key));
  ZeroMem (Iv, sizeof (Iv));
  ZeroMem (WrappedBKS3, sizeof (WrappedBKS3));

  if (EncryptedData != NULL) {
    ZeroMem (EncryptedData, PaddedInputSize);
    FreePool (EncryptedData);
  }
  if (InputData != NULL) {
    ZeroMem (InputData, PaddedInputSize);
    FreePool (InputData);
  }

Exit:
  // Ensure derived key is always cleared before returning
  ZeroMem (&BKS3Key, sizeof (BKS3Key));
  ZeroMem (HsmGuid, sizeof (HsmGuid));
  ZeroMem (&TcgContext, sizeof (TcgContext));
  ZeroMem (&TpmPlatformSecret, sizeof(TpmPlatformSecret));
  return Status;
}

/**
 * Perform AES256-CBC encryption on the input data given a Key and IV.
 * 
 * @param[in]   InputData       Pointer to the input data buffer to encrypt.
 * @param[in]   InputDataSize   Size of the input data buffer in bytes (must be multiple of 16).
 * @param[out]  OutputData      Pointer to the output buffer to receive the encrypted data.
 * @param[out]  OutputDataSize  Pointer to the size of the output buffer in bytes.
 * @param[in]   Key             Pointer to the encryption key.
 * @param[in]   KeySize         Size of the encryption key in bytes (must be 32 for AES-256).
 * @param[in]   Iv              Pointer to the initialization vector (IV).
 * @param[in]   IvSize          Size of the IV in bytes (must be 16).
 * 
 * @retval EFI_SUCCESS              Encryption completed successfully.
 * @retval EFI_INVALID_PARAMETER    Invalid input parameters.
 * @retval EFI_OUT_OF_RESOURCES     Memory allocation failed.
 * @retval EFI_DEVICE_ERROR         Cryptographic operation failed.
 */
STATIC
EFI_STATUS
AziHsmAes256CbcEncrypt (
  IN  UINT8   *InputData,
  IN  UINTN   InputDataSize,
  OUT UINT8   *OutputData,
  OUT UINTN   *OutputDataSize,
  IN  UINT8   *Key,
  IN  UINTN   KeySize,
  IN  UINT8   *Iv,
  IN  UINTN   IvSize
)
{
  EFI_STATUS  Status;
  UINTN       CtxSize;
  VOID        *Ctx = NULL;
  BOOLEAN     CryptoOk;

  // Validate NULL parameters
  if (InputData == NULL || OutputData == NULL || OutputDataSize == NULL || Key == NULL || Iv == NULL) {
    DEBUG ((DEBUG_ERROR, "AziHsm: AES256-CBC Encrypt: Invalid NULL parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  // Validate input data size
  if (InputDataSize == 0) {
    DEBUG ((DEBUG_ERROR, "AziHsm: AES256-CBC Encrypt: Input data size is zero\n"));
    return EFI_INVALID_PARAMETER;
  }

  if (InputDataSize % AES_BLOCK_SIZE != 0) {
    DEBUG ((DEBUG_ERROR, "AziHsm: AES256-CBC Encrypt: Input data size %u not block-aligned (must be multiple of %u)\n", InputDataSize, AES_BLOCK_SIZE));
    return EFI_INVALID_PARAMETER;
  }

  // Validate key size
  if (KeySize != AZIHSM_AES256_KEY_SIZE) {
    DEBUG ((DEBUG_ERROR, "AziHsm: AES256-CBC Encrypt: Invalid key size %u, expected %u\n", KeySize, AZIHSM_AES256_KEY_SIZE));
    return EFI_INVALID_PARAMETER;
  }

  // Validate IV size
  if (IvSize != AZIHSM_AES_IV_SIZE) {
    DEBUG ((DEBUG_ERROR, "AziHsm: AES256-CBC Encrypt: Invalid IV size %u, expected %u\n", IvSize, AZIHSM_AES_IV_SIZE));
    return EFI_INVALID_PARAMETER;
  }

  // Allocate AES context
  CtxSize = AesGetContextSize ();
  Ctx     = AllocatePool (CtxSize);
  if (Ctx == NULL) {
    DEBUG ((DEBUG_ERROR, "AziHsm: AES256-CBC Encrypt: Failed to allocate AES context\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }
  ZeroMem (Ctx, CtxSize);

  // Initialize AES context
  CryptoOk = AesInit (Ctx, Key, AZIHSM_AES256_KEY_BITS);
  if (!CryptoOk) {
    DEBUG ((DEBUG_ERROR, "AziHsm: AziHsmAes256CbcEncrypt - AesInit failed\n"));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  // Perform CBC encryption
  CryptoOk = AesCbcEncrypt (Ctx, InputData, InputDataSize, Iv, OutputData);
  if (!CryptoOk) {
    DEBUG ((DEBUG_ERROR, "AziHsm: AziHsmAes256CbcEncrypt - AesCbcEncrypt failed\n"));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
    goto Exit;
  }

  *OutputDataSize = InputDataSize;
  Status = EFI_SUCCESS;

Exit:
  // Clean up sensitive AES context
  if (Ctx != NULL) {
    ZeroMem (Ctx, CtxSize);
    FreePool (Ctx);
  }

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
  DEBUG ((DEBUG_WARN, "AziHsm: DriverBindingStop - triggering sensitive data cleanup\n"));
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
  AZIHSM_DERIVED_KEY  TpmDerivedSecret;
  AZIHSM_BUFFER       TpmDerivedSecretBlob;
  AZIHSM_BUFFER       SealedSecretBlob;

  ZeroMem (&TpmDerivedSecret, sizeof (TpmDerivedSecret));
  ZeroMem (&TpmDerivedSecretBlob, sizeof (TpmDerivedSecretBlob));
  ZeroMem (&SealedSecretBlob, sizeof (SealedSecretBlob));

  //
  // Derive BKS3 key from TPM using the workflow
  //
  Status = AziHsmGetTpmPlatformSecret (&TpmDerivedSecret);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "AziHsm: BKS3 key derivation workflow failed: %r\n", Status));
    goto Exit;
  }

  //
  // Seal the derived key to the null hierarchy for secure storage
  //
  if (TpmDerivedSecret.KeySize > sizeof (TpmDerivedSecretBlob.Data)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Derived key size exceeds maximum buffer size of the TpmDerivedKeyBlob\n"));
    Status = EFI_BAD_BUFFER_SIZE;
    goto Exit;
  }

  CopyMem (TpmDerivedSecretBlob.Data, TpmDerivedSecret.KeyData, TpmDerivedSecret.KeySize);
  TpmDerivedSecretBlob.Size = (UINT32)TpmDerivedSecret.KeySize;

  // Seal the derived key to the TPM null hierarchy(to ensure it is associated with current boot) and
  // does not persist across reboots
  Status = AziHsmSealToTpmNullHierarchy (&TpmDerivedSecretBlob, &SealedSecretBlob);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Sealing to null hierarchy failed: %r\n", Status));
    goto Exit;
  }

  //
  // Store the sealed key globally for reuse across all HSM devices
  //
  CopyMem (&(mAziHsmSealedPlatformSecret.Data), &(SealedSecretBlob.Data), SealedSecretBlob.Size);
  mAziHsmSealedPlatformSecret.Size = SealedSecretBlob.Size;
  mAziHsmSealedPlatormSecretDerived  = TRUE;

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
  ZeroMem(&mAziHsmSealedPlatformSecret, sizeof(mAziHsmSealedPlatformSecret));
  mAziHsmSealedPlatormSecretDerived = FALSE;
Exit:
  ZeroMem (&TpmDerivedSecret, sizeof (TpmDerivedSecret));
  ZeroMem (&TpmDerivedSecretBlob, sizeof (TpmDerivedSecretBlob));
  ZeroMem (&SealedSecretBlob, sizeof (SealedSecretBlob));

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

  DEBUG ((DEBUG_INFO, "AziHsm: *** Starting sensitive data cleanup ***\n"));

  //
  // Clear sensitive data from HSM before OS takes control
  //
  ZeroMem (&mAziHsmSealedPlatformSecret, sizeof (AZIHSM_BUFFER));
  mAziHsmSealedPlatormSecretDerived = FALSE;
  DEBUG ((DEBUG_INFO, "AziHsm: Global Platform Hierarchy secret cleared\n"));

  mSensitiveDataCleared = TRUE;
  DEBUG ((DEBUG_INFO, "AziHsm: *** Sensitive data cleanup completed ***\n"));
}

