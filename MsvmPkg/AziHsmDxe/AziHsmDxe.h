/** @file
  Header definitions for the Azure Integrated HSM DXE driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#ifndef __AZIHSMDXE_H__
#define __AZIHSMDXE_H__

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseCryptLib.h>  // For AES context and operations


#include <Protocol/DevicePath.h>
#include <Protocol/DriverSupportedEfiVersion.h>
#include <Protocol/PciIo.h>
#include <Protocol/AziHsm.h>

#include <IndustryStandard/Pci.h>

#include "AziHsmQueue.h"

#define AZIHSM_PCI_VENDOR_ID         0x1414
#define AZIHSM_PCI_DEVICE_ID         0xC003
#define AZIHSM_CONTROLLER_SIGNATURE  SIGNATURE_32 ('A','H','S','M')
#define AZIHSM_AES256_KEY_SIZE       32
#define AZIHSM_AES256_KEY_BITS       256
#define AZIHSM_AES_IV_SIZE           16
#define AZIHSM_AES_KEY_VERSION       1
#define AZIHSM_HSM_GUID_MAX_SIZE     32

#define AZIHSM_CONTROLLER_STATE_FROM_PROTOCOL(a) \
  CR (a, AZIHSM_CONTROLLER_STATE, AziHsmProtocol, AZIHSM_CONTROLLER_SIGNATURE)


// Marshal Key and IV into a compact struct to avoid manual offset math
#pragma pack(push, 1)
typedef struct {
  UINT16 RecordSize;   // RecordSize does not include the size of the RecordSize field itself
  UINT8  KeyVersion;
  UINT8  KeySize;                              // length of Key[] in bytes
  UINT8  Key[AZIHSM_AES256_KEY_SIZE];
  UINT8  IvSize;                               // length of Iv[] in bytes
  UINT8  Iv[AZIHSM_AES_IV_SIZE];
} AZIHSM_KEY_IV_RECORD;
#pragma pack(pop)

/**
 * Structure to hold the state information for the Azure Integrated HSM controller.
 */
typedef struct _AZIHSM_CONTROLLER_STATE {
  UINT32                      Signature;
  EFI_HANDLE                  ControllerHandle;
  EFI_HANDLE                  ImageHandle;
  EFI_HANDLE                  DriverBindingHandle;
  EFI_DEVICE_PATH_PROTOCOL    *ParentDevicePath;
  EFI_PCI_IO_PROTOCOL         *PciIo;
  UINT64                      PciAttributes;
  AZIHSM_IO_QUEUE_PAIR        AdminQueue;
  AZIHSM_IO_QUEUE_PAIR        HsmQueue;
  AZIHSM_PROTOCOL             AziHsmProtocol;
  BOOLEAN                     HsmQueuesCreated;
} AZIHSM_CONTROLLER_STATE;

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
  );

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
  );

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
  );

/**
  Retrieves a Unicode string that is the user readable name of the driver.

  @param  This[in]              A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                                EFI_COMPONENT_NAME_PROTOCOL instance.
  @param  Language[in]          A pointer to a Null-terminated ASCII string array
                                indicating the language.
  @param  DriverName[out]       A pointer to the Unicode string to return.

  @retval EFI_SUCCESS           The Unicode string for the Driver specified by
                                This and the language specified by Language was
                                returned in DriverName.
  @retval EFI_INVALID_PARAMETER Language is NULL.
  @retval EFI_INVALID_PARAMETER DriverName is NULL.
  @retval EFI_UNSUPPORTED       The driver specified by This does not support
                                the language specified by Languag
**/
EFI_STATUS
EFIAPI
AziHsmGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **DriverName
  );

/**
  Retrieves a Unicode string that is the user readable name of the controller
  that is being managed by a driver.

  @param  This[in]              A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                                EFI_COMPONENT_NAME_PROTOCOL instance.
  @param  ControllerHandle[in]  The handle of a controller that the driver
                                specified by This is managing.
  @param  ChildHandle[in]       The handle of the child controller to retrieve
                                the name of.
  @param  Language[in]          A pointer to a Null-terminated ASCII string
                                array indicating the language.
  @param  ControllerName[out]   A pointer to the Unicode string to return.

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
  );

#endif // __AZIHSMDXE_H__
