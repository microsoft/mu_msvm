/** @file
  Implementation of the Azure Integrated HSM DXE driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#ifndef __AZIHSMHCI_H__
#define __AZIHSMHCI_H__

#include <Uefi.h>

#include "AziHsmDxe.h"

/**
 * AziHsmHciWrSqTailDbReg: Write the SQ Tail doorbell register with New value
 * This function is used when the commands are fired.
 *
 * @param[in] PciIo    Pointer to the EFI_PCI_IO_PROTOCOL Struct.
 *
 * @param[in] SubQId    The ID Of The Submission Queue that you want to post the command to.
 * @param[in] DbValue   The new value of the DB To be Written
 */
EFI_STATUS
EFIAPI
AziHsmHciWrSqTailDbReg (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT16               SubQId,
  IN UINT16               DbValue
  );

/**
 * AziHsmHciWrCqHeadReg: Write the CQ Head doorbell register with New value
 * This function is used when the commands are fired.
 *
 * @param[in] PciIo    Pointer to the EFI_PCI_IO_PROTOCOL Struct.
 *
 * @param[in] CqId    The ID Of The Completion Queue that you want to post the command to.
 * @param[in] DbValue   The new value of the DB To be Written
 */
EFI_STATUS
EFIAPI
AziHsmHciWrCqHeadReg (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT16               CqId,
  IN UINT16               DbValue
  );

/**
 * Initialize the Azure Integrated HSM HCI driver.
 *
 * @param[in]  State  Pointer to the controller state.
 *
 * @retval EFI_SUCCESS  The driver was initialized successfully.
 * @retval Others       An error occurred while initializing the driver.
 */
EFI_STATUS
EFIAPI
AziHsmHciInitialize (
  IN AZIHSM_CONTROLLER_STATE  *State
  );

/**
 * Uninitialize the Azure Integrated HSM HCI driver.
 *
 * @param[in]  State  Pointer to the controller state.
 *
 * @retval EFI_SUCCESS  The driver was uninitialized successfully.
 * @retval Others       An error occurred while uninitializing the driver.
 */
EFI_STATUS
EFIAPI
AziHsmHciUninitialize (
  IN AZIHSM_CONTROLLER_STATE  *State
  );

#endif // __AZIHSMHCI_H__
