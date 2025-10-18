/** @file
  Implementation of the Azure Integrated HSM DXE driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include "AziHsmHci.h"
#include "AziHsmQueue.h"
#include "AziHsmAdmin.h"
#include "AziHsmCp.h"

#include <Library/DebugLib.h>

#define AZIHSM_SQ_TAIL_DB_OFFSET(QID)  ( (2 * QID) * 4)
#define AZIHSM_CQ_HEAD_DB_OFFSET(QID)  ( ((2 * QID) + 1) * 4)

#define AZIHSM_CTRL_PCI_BAR_INDEX  0
#define AZIHSM_CTRL_DB_BAR_INDEX   2

/**
 * Hardware Controller register offsets
 */
#define AZIHSM_CTRL_CAP_REG_OFFSET  0x0000 // Controller Capabilities
#define AZIHSM_CTRL_VER_REG_OFFSET  0x0008 // Version
#define AZIHSM_CTRL_CFG_REG_OFFSET  0x0014 // Controller Configuration
#define AZIHSM_CTRL_STS_REG_OFFSET  0x001c // Controller Status
#define AZIHSM_CTRL_AQA_REG_OFFSET  0x0024 // Admin Queue Attributes
#define AZIHSM_CTRL_ASQ_REG_OFFSET  0x0028 // Admin Submission Queue Base Address
#define AZIHSM_CTRL_ACQ_REG_OFFSET  0x0030 // Admin Completion Queue Base Address

/**
 * Capabilities Register
 */
typedef union _AZIHSM_CTRL_CAP_REG {
  UINT64    Val;
  struct {
    UINT16    Mqes;       // Maximum Queue Entries Supported
    UINT8     Cqr    : 1; // Contiguous Queues Required
    UINT8     Ams    : 2; // Arbitration Mechanism Supported
    UINT8     Rsvd1  : 5;
    UINT8     To;         // Timeout
    UINT16    Dstrd  : 4; // Doorbell Stride
    UINT16    Ssrs   : 1; // Subsystem Reset Supported SSRS
    UINT16    Css    : 8; // Command Sets Supported - Bit 37
    UINT16    Rsvd3  : 3;
    UINT8     MpsMin : 4; // Memory Page Size Minimum
    UINT8     MpsMax : 4; // Memory Page Size Maximum
    UINT8     Rsvd4;
  } Bits;
} AZIHSM_CTRL_CAP_REG;
STATIC_ASSERT (sizeof (AZIHSM_CTRL_CAP_REG) == sizeof (UINT64), "AZIHSM_CTRL_CAP_REG invalid size.");

/**
 * Version Register
 */
typedef union _AZIHSM_CTRL_VER_REG {
  UINT32    Val;

  struct {
    UINT8     Ter; // Tertiary version number
    UINT8     Mnr; // Minor version number
    UINT16    Mjr; // Major version number
  } Bits;
} AZIHSM_CTRL_VER_REG;
STATIC_ASSERT (sizeof (AZIHSM_CTRL_VER_REG) == sizeof (UINT32), "AZIHSM_CTRL_VER_REG invalid size.");

/**
 * Configuration Register
 */
typedef union _AZIHSM_CTRL_CFG_REG {
  UINT32    Val;
  struct {
    UINT16    En      : 1; // Enable
    UINT16    Rsvd1   : 3;
    UINT16    Css     : 3; // I/O Command Set Selected
    UINT16    Mps     : 4; // Memory Page Size
    UINT16    Ams     : 3; // Arbitration Mechanism Selected
    UINT16    Shn     : 2; // Shutdown Notification
    UINT8     HsmSqes : 4; // HSM Submission Queue Entry Size
    UINT8     HsmCqes : 4; // HSM Completion Queue Entry Size
    UINT8     AesSqes : 4; // AES Submission Queue Entry Size
    UINT8     AesCqes : 4; // AES Completion Queue Entry Size
  } Bits;
} AZIHSM_CTRL_CFG_REG;
STATIC_ASSERT (sizeof (AZIHSM_CTRL_CFG_REG) == sizeof (UINT32), "AZIHSM_CTRL_CFG_REG invalid size.");

/**
 * Status Register
 */
typedef union _AZIHSM_CTRL_STS_REG {
  UINT32    Val;
  struct {
    UINT32    Rdy   : 1; // Ready
    UINT32    Cfs   : 1; // Controller Fatal Status
    UINT32    Shst  : 2; // Shutdown Status
    UINT32    Ssro  : 1; // Subsystem Reset Occurred
    UINT32    Rsvd1 : 27;
  } Bits;
} AZIHSM_CTRL_STS_REG;
STATIC_ASSERT (sizeof (AZIHSM_CTRL_STS_REG) == sizeof (UINT32), "AZIHSM_CTRL_STS_REG Size invalid size.");

/**
 * Admin Queue Attributes Register
 */
typedef union _AZIHSM_CTRL_AQA_REG {
  UINT32    Val;
  struct {
    UINT16    Asqs; // Submission Queue Size
    UINT16    Acqs; // Completion Queue Size
  } Bits;
} AZIHSM_CTRL_AQA_REG;
STATIC_ASSERT (sizeof (AZIHSM_CTRL_AQA_REG) == sizeof (UINT32), "AZIHSM_CTRL_AQA_REG Size invalid size.");

/**
 * Admin Submission Queue Base Address Register
 */
typedef union _AZIHSM_CTRL_ASQ_REG {
  UINT64    Val;
  struct {
    UINT64    BaseAddr; // Base Address
  } Bits;
} AZIHSM_CTRL_ASQ_REG;
STATIC_ASSERT (sizeof (AZIHSM_CTRL_ASQ_REG) == sizeof (UINT64), "AZIHSM_CTRL_ASQ_REG Size invalid size.");

/**
 * Admin Completion Queue Base Address Register
 */
typedef union _AZIHSM_CTRL_ACQ_REG {
  UINT64    Val;
  struct {
    UINT64    BaseAddr; // Base Address
  } Bits;
} AZIHSM_CTRL_ACQ_REG;
STATIC_ASSERT (sizeof (AZIHSM_CTRL_ACQ_REG) == sizeof (UINT64), "AZIHSM_CTRL_ACQ_REG Size invalid size.");

/**
 * Read the controller capabilities.
 *
 * @param[in]  PciIo  Pointer to the PCI I/O protocol instance.
 * @param[out] CapReg Pointer to the capabilities register structure to fill.
 *
 * @retval EFI_SUCCESS           The capabilities were read successfully.
 * @retval EFI_DEVICE_ERROR      An error occurred while reading the capabilities.
 */
STATIC EFI_STATUS
ReadCapReg (
  IN EFI_PCI_IO_PROTOCOL   *PciIo,
  OUT AZIHSM_CTRL_CAP_REG  *CapReg
  )
{
  EFI_STATUS  Status;

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint64,
                        AZIHSM_CTRL_PCI_BAR_INDEX,
                        AZIHSM_CTRL_CAP_REG_OFFSET,
                        sizeof (AZIHSM_CTRL_CAP_REG) / sizeof (UINT64),
                        CapReg
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to read controller capabilities. Status: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
 * Read the controller version.
 *
 * @param[in]  PciIo  Pointer to the PCI I/O protocol instance.
 * @param[out] VerReg Pointer to the version register structure to fill.
 *
 * @retval EFI_SUCCESS           The version was read successfully.
 * @retval EFI_DEVICE_ERROR      An error occurred while reading the version.
 */
STATIC EFI_STATUS
ReadVerReg (
  IN EFI_PCI_IO_PROTOCOL   *PciIo,
  OUT AZIHSM_CTRL_VER_REG  *VerReg
  )
{
  EFI_STATUS  Status;

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        AZIHSM_CTRL_PCI_BAR_INDEX,
                        AZIHSM_CTRL_VER_REG_OFFSET,
                        sizeof (AZIHSM_CTRL_VER_REG) / sizeof (UINT32),
                        VerReg
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to read controller version. Status: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC EFI_STATUS
ReadConfigReg (
  IN EFI_PCI_IO_PROTOCOL   *PciIo,
  OUT AZIHSM_CTRL_CFG_REG  *CfgReg
  )
{
  EFI_STATUS  Status;

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        AZIHSM_CTRL_PCI_BAR_INDEX,
                        AZIHSM_CTRL_CFG_REG_OFFSET,
                        sizeof (AZIHSM_CTRL_CFG_REG) / sizeof (UINT32),
                        CfgReg
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to read controller configuration. Status: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC EFI_STATUS
WriteConfigReg (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN AZIHSM_CTRL_CFG_REG  *CfgReg
  )
{
  EFI_STATUS  Status;

  Status = PciIo->Mem.Write (
                        PciIo,
                        EfiPciIoWidthUint32,
                        AZIHSM_CTRL_PCI_BAR_INDEX,
                        AZIHSM_CTRL_CFG_REG_OFFSET,
                        sizeof (AZIHSM_CTRL_CFG_REG) / sizeof (UINT32),
                        CfgReg
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to write controller configuration. Status: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
 * Read the controller status.
 *
 * @param[in]  PciIo  Pointer to the PCI I/O protocol instance.
 * @param[out] StsReg Pointer to the status register structure to fill.
 *
 * @retval EFI_SUCCESS           The status was read successfully.
 * @retval EFI_DEVICE_ERROR      An error occurred while reading the status.
 */
STATIC EFI_STATUS
ReadStatusReg (
  IN EFI_PCI_IO_PROTOCOL   *PciIo,
  OUT AZIHSM_CTRL_STS_REG  *StsReg
  )
{
  EFI_STATUS  Status;

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        AZIHSM_CTRL_PCI_BAR_INDEX,
                        AZIHSM_CTRL_STS_REG_OFFSET,
                        sizeof (AZIHSM_CTRL_STS_REG) / sizeof (UINT32),
                        StsReg
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to read controller status. Status: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
 * Read the admin queue attributes.
 *
 * @param[in]  PciIo  Pointer to the PCI I/O protocol instance.
 * @param[out] AqaReg Pointer to the admin queue attributes register structure to fill.
 *
 * @retval EFI_SUCCESS           The admin queue attributes were read successfully.
 * @retval EFI_DEVICE_ERROR      An error occurred while reading the admin queue attributes.
 */
STATIC EFI_STATUS
ReadAqaReg (
  IN EFI_PCI_IO_PROTOCOL   *PciIo,
  OUT AZIHSM_CTRL_AQA_REG  *AqaReg
  )
{
  EFI_STATUS  Status;

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        AZIHSM_CTRL_PCI_BAR_INDEX,
                        AZIHSM_CTRL_AQA_REG_OFFSET,
                        sizeof (AZIHSM_CTRL_AQA_REG) / sizeof (UINT32),
                        AqaReg
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to read admin queue attributes. Status: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
 * Write the admin queue attributes.
 *
 * @param[in]  PciIo  Pointer to the PCI I/O protocol instance.
 * @param[in]  AqaReg Pointer to the admin queue attributes register structure to write.
 *
 * @retval EFI_SUCCESS           The admin queue attributes were written successfully.
 * @retval EFI_DEVICE_ERROR      An error occurred while writing the admin queue attributes.
 */
STATIC EFI_STATUS
WriteAqaReg (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN AZIHSM_CTRL_AQA_REG  *AqaReg
  )
{
  EFI_STATUS  Status;

  Status = PciIo->Mem.Write (
                        PciIo,
                        EfiPciIoWidthUint32,
                        AZIHSM_CTRL_PCI_BAR_INDEX,
                        AZIHSM_CTRL_AQA_REG_OFFSET,
                        sizeof (AZIHSM_CTRL_AQA_REG) / sizeof (UINT32),
                        AqaReg
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to write admin queue attributes. Status: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
 * Read the admin submission queue base address.
 *
 * @param[in]  PciIo  Pointer to the PCI I/O protocol instance.
 * @param[out] AsqReg Pointer to the admin submission queue base address register structure to fill.
 *
 * @retval EFI_SUCCESS           The admin submission queue base address was read successfully.
 * @retval EFI_DEVICE_ERROR      An error occurred while reading the admin submission queue base address.
 */
STATIC EFI_STATUS
ReadAsqReg (
  IN EFI_PCI_IO_PROTOCOL   *PciIo,
  OUT AZIHSM_CTRL_ASQ_REG  *AsqReg
  )
{
  EFI_STATUS  Status;

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint64,
                        AZIHSM_CTRL_PCI_BAR_INDEX,
                        AZIHSM_CTRL_ASQ_REG_OFFSET,
                        sizeof (AZIHSM_CTRL_ASQ_REG) / sizeof (UINT64),
                        AsqReg
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to read admin submission queue base address. Status: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
 * Write the admin submission queue base address.
 *
 * @param[in]  PciIo  Pointer to the PCI I/O protocol instance.
 * @param[in]  AsqReg Pointer to the admin submission queue base address register structure to write.
 *
 * @retval EFI_SUCCESS           The admin submission queue base address was written successfully.
 * @retval EFI_DEVICE_ERROR      An error occurred while writing the admin submission queue base address.
 */
STATIC EFI_STATUS
WriteAsqReg (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN AZIHSM_CTRL_ASQ_REG  *AsqReg
  )
{
  EFI_STATUS  Status;

  Status = PciIo->Mem.Write (
                        PciIo,
                        EfiPciIoWidthUint64,
                        AZIHSM_CTRL_PCI_BAR_INDEX,
                        AZIHSM_CTRL_ASQ_REG_OFFSET,
                        sizeof (AZIHSM_CTRL_ASQ_REG) / sizeof (UINT64),
                        AsqReg
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to write admin submission queue base address. Status: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
 * Read the admin completion queue base address.
 *
 * @param[in]  PciIo  Pointer to the PCI I/O protocol instance.
 * @param[out] AcqReg Pointer to the admin completion queue base address register structure to fill.
 *
 * @retval EFI_SUCCESS           The admin completion queue base address was read successfully.
 * @retval EFI_DEVICE_ERROR      An error occurred while reading the admin completion queue base address.
 */
STATIC EFI_STATUS
ReadAcqReg (
  IN EFI_PCI_IO_PROTOCOL   *PciIo,
  OUT AZIHSM_CTRL_ACQ_REG  *AcqReg
  )
{
  EFI_STATUS  Status;

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint64,
                        AZIHSM_CTRL_PCI_BAR_INDEX,
                        AZIHSM_CTRL_ACQ_REG_OFFSET,
                        sizeof (AZIHSM_CTRL_ACQ_REG) / sizeof (UINT64),
                        AcqReg
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to read admin completion queue base address. Status: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
 * Write the admin completion queue base address.
 *
 * @param[in]  PciIo  Pointer to the PCI I/O protocol instance.
 * @param[in]  AcqReg Pointer to the admin completion queue base address register structure to write.
 *
 * @retval EFI_SUCCESS           The admin completion queue base address was written successfully.
 * @retval EFI_DEVICE_ERROR      An error occurred while writing the admin completion queue base address.
 */
STATIC EFI_STATUS
WriteAcqReg (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN AZIHSM_CTRL_ACQ_REG  *AcqReg
  )
{
  EFI_STATUS  Status;

  Status = PciIo->Mem.Write (
                        PciIo,
                        EfiPciIoWidthUint64,
                        AZIHSM_CTRL_PCI_BAR_INDEX,
                        AZIHSM_CTRL_ACQ_REG_OFFSET,
                        sizeof (AZIHSM_CTRL_ACQ_REG) / sizeof (UINT64),
                        AcqReg
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to write admin completion queue base address. Status: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
AziHsmHciWrSqTailDbReg (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT16               SubQId,
  IN UINT16               DbValue
  )
{
  EFI_STATUS  Status;

  if ((PciIo == NULL) || (SubQId > AZIHSM_MAX_QUE_ID)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = PciIo->Mem.Write (
                        PciIo,
                        EfiPciIoWidthUint32,
                        AZIHSM_CTRL_DB_BAR_INDEX,
                        AZIHSM_SQ_TAIL_DB_OFFSET (SubQId),
                        1,
                        &DbValue
                        );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "AziHsm: [%a]: PciIo mem write error: %r\n", __FUNCTION__, Status));
  }

  return Status;
}

EFI_STATUS
EFIAPI
AziHsmHciWrCqHeadReg (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT16               CqId,
  IN UINT16               DbValue
  )
{
  EFI_STATUS  Status;

  if ((PciIo == NULL) || (CqId > AZIHSM_MAX_QUE_ID)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = PciIo->Mem.Write (
                        PciIo,
                        EfiPciIoWidthUint32,
                        AZIHSM_CTRL_DB_BAR_INDEX,
                        AZIHSM_CQ_HEAD_DB_OFFSET (CqId),
                        1,
                        &DbValue
                        );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "AziHsm: [%a]: PciIo mem write error: %r\n", __FUNCTION__, Status));
  }

  return Status;
}

/**
 * Print the contents of the capabilities register.
 *
 * @param[in]  Cap  Pointer to the capabilities register structure to print.
 */
STATIC VOID
PrintCapReg (
  IN AZIHSM_CTRL_CAP_REG  *Cap
  )
{
  DEBUG ((DEBUG_INFO, "AziHsm: CAP.MQES: %u\n", Cap->Bits.Mqes));
  DEBUG ((DEBUG_INFO, "AziHsm: CAP.CQR: %u\n", Cap->Bits.Cqr));
  DEBUG ((DEBUG_INFO, "AziHsm: CAP.AMS: %u\n", Cap->Bits.Ams));
  DEBUG ((DEBUG_INFO, "AziHsm: CAP.TO: %u\n", Cap->Bits.To));
  DEBUG ((DEBUG_INFO, "AziHsm: CAP.DSTRD: %u\n", Cap->Bits.Dstrd));
  DEBUG ((DEBUG_INFO, "AziHsm: CAP.SSRS: %u\n", Cap->Bits.Ssrs));
  DEBUG ((DEBUG_INFO, "AziHsm: CAP.CSS: %u\n", Cap->Bits.Css));
  DEBUG ((DEBUG_INFO, "AziHsm: CAP.MPS_MIN: %u\n", Cap->Bits.MpsMin));
  DEBUG ((DEBUG_INFO, "AziHsm: CAP.MPS_MAX: %u\n", Cap->Bits.MpsMax));
}

/**
 * Print the contents of the version register.
 *
 * @param[in]  Ver  Pointer to the version register structure to print.
 */
STATIC VOID
PrintVerReg (
  IN AZIHSM_CTRL_VER_REG  *Ver
  )
{
  DEBUG ((DEBUG_INFO, "AziHsm: VER.MJR: %u\n", Ver->Bits.Mjr));
  DEBUG ((DEBUG_INFO, "AziHsm: VER.MNR: %u\n", Ver->Bits.Mnr));
  DEBUG ((DEBUG_INFO, "AziHsm: VER.TER: %u\n", Ver->Bits.Ter));
}

STATIC VOID
PrintConfigReg (
  IN AZIHSM_CTRL_CFG_REG  *Cfg
  )
{
  DEBUG ((DEBUG_INFO, "AziHsm: [CFG.EN: %u] [CFG.Val: %llx]\n", Cfg->Bits.En, Cfg->Val));
}

STATIC VOID
PrintStatusReg (
  IN AZIHSM_CTRL_STS_REG  *Sts
  )
{
  DEBUG ((DEBUG_INFO, "AziHsm: [STS.RDY: %u] [Sts.Val: %llx]\n", Sts->Bits.Rdy, Sts->Val));
}

STATIC VOID
PrintAqaReg (
  IN AZIHSM_CTRL_AQA_REG  *Aqa
  )
{
  DEBUG ((DEBUG_INFO, "AziHsm: AQA.ASQS: %u\n", Aqa->Bits.Asqs));
  DEBUG ((DEBUG_INFO, "AziHsm: AQA.ACQS: %u\n", Aqa->Bits.Acqs));
}

STATIC VOID
PrintAsqReg (
  IN AZIHSM_CTRL_ASQ_REG  *Asq
  )
{
  DEBUG ((DEBUG_INFO, "AziHsm: ASQ.BASE: %llx\n", Asq->Bits.BaseAddr));
}

STATIC VOID
PrintAcqReg (
  IN AZIHSM_CTRL_ACQ_REG  *Acq
  )
{
  DEBUG ((DEBUG_INFO, "AziHsm: ACQ.BASE: %llx\n", Acq->Bits.BaseAddr));
}

STATIC EFI_STATUS
EnableController (
  IN AZIHSM_CONTROLLER_STATE  *State,
  IN AZIHSM_CTRL_CAP_REG      *Cap
  )
{
  EFI_STATUS           Status;
  AZIHSM_CTRL_STS_REG  Sts;
  AZIHSM_CTRL_CFG_REG  Cfg;

  Status = ReadStatusReg (State->PciIo, &Sts);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "AziHsm: Failed to read status register. Status: %r\n", Status));
    goto Exit;
  }

  if (Sts.Bits.Rdy) {
    // Controller is already enabled
    DEBUG ((DEBUG_INFO, "AziHsm: Controller is already enabled. Status: %r\n", Status));
    goto Exit;
  }

  Status = ReadConfigReg (State->PciIo, &Cfg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "AziHsm: Failed to read config register. Status: %r\n", Status));
    goto Exit;
  }

  Cfg.Bits.En = 1;

  Status = WriteConfigReg (State->PciIo, &Cfg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "AziHsm: Failed to write config register. Status: %r\n", Status));
    goto Exit;
  }

  // Wait for the controller to be ready
  for (UINT32 Index = 0; Index < 1000; Index++) {
    gBS->Stall (1000);

    Status = ReadStatusReg (State->PciIo, &Sts);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "AziHsm: Failed to read status register. Status: %r\n", Status));
      goto Exit;
    }

    if (Sts.Bits.Rdy) {
      DEBUG ((DEBUG_INFO, "AziHsm: Controller is ready in %u microseconds\n", Index * 1000));
      break;
    }
  }

  if (!Sts.Bits.Rdy) {
    DEBUG ((DEBUG_INFO, "AziHsm: Controller is not ready after timeout.\n"));
    Status = EFI_DEVICE_ERROR;
  }

  PrintConfigReg (&Cfg);
  PrintStatusReg (&Sts);

Exit:
  DEBUG ((DEBUG_INFO, "AziHsm controller is enabled completed. Status: %r\n", Status));
  return Status;
}

STATIC EFI_STATUS
DisableController (
  IN AZIHSM_CONTROLLER_STATE  *State,
  IN AZIHSM_CTRL_CAP_REG      *Cap
  )
{
  EFI_STATUS           Status;
  AZIHSM_CTRL_STS_REG  Sts;
  AZIHSM_CTRL_CFG_REG  Cfg;
  UINT8                Timeout;
  UINT32               Index;

  Status = ReadStatusReg (State->PciIo, &Sts);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "AziHsm: Failed to read status register. Status: %r\n", Status));
    goto Exit;
  }

  if (!Sts.Bits.Rdy) {
    DEBUG ((DEBUG_INFO, "AziHsm: Controller is already disabled %r.\n", Status));
    goto Exit;
  }

  Status = ReadConfigReg (State->PciIo, &Cfg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "AziHsm: Failed to read config register. Status: %r\n", Status));
    goto Exit;
  }

  Cfg.Bits.En = 0;

  Status = WriteConfigReg (State->PciIo, &Cfg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "AziHsm: Failed to write config register. Status: %r\n", Status));
    goto Exit;
  }

  if (Cap->Bits.To == 0) {
    Timeout = 1;
  } else {
    Timeout = Cap->Bits.To;
  }

  for (Index = (Timeout * 500); Index != 0; --Index) {
    gBS->Stall (1000);

    Status = ReadStatusReg (State->PciIo, &Sts);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "AziHsm: Failed to read status register. Status: %r\n", Status));
      goto Exit;
    }

    if (Sts.Bits.Rdy == 0) {
      DEBUG ((DEBUG_INFO, "AziHsm: Controller is disabled in %u microseconds\n", Index * 1000));
      break;
    }
  }

  if (Index == 0) {
    DEBUG ((DEBUG_INFO, "AziHsm: Controller is not disabled after timeout.\n"));
    Status = EFI_DEVICE_ERROR;
  }

  return Status;

Exit:
  DEBUG ((DEBUG_INFO, "AziHsm controller is disabled completed. Status: %r\n", Status));
  return Status;
}

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
  )
{
  EFI_STATUS           Status;
  AZIHSM_CTRL_CAP_REG  Cap;
  AZIHSM_CTRL_VER_REG  Ver;
  AZIHSM_CTRL_AQA_REG  Aqa;
  AZIHSM_CTRL_ASQ_REG  Asq;
  AZIHSM_CTRL_ACQ_REG  Acq;

  Status = ReadCapReg (State->PciIo, &Cap);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = ReadVerReg (State->PciIo, &Ver);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = DisableController (State, &Cap);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = AziHsmQueuePairInitialize (
             &State->AdminQueue,
             State->PciIo,
             AZIHSM_QUEUE_ID_ADMIN,
             AZIHSM_QUEUE_SIZE,
             AZIHSM_SQE_SIZE,
             AZIHSM_CQE_SIZE,
             Cap.Bits.Dstrd
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to initialize admin queue pair. Status: %r\n", Status));
    goto Exit;
  }

  Aqa.Bits.Asqs = AZIHSM_QUEUE_SIZE;
  Aqa.Bits.Acqs = AZIHSM_QUEUE_SIZE;

  DEBUG ((
    DEBUG_INFO,
    "AziHsm: ASQ [DeviceAddr:%lx] [HostAddr:%lx]\n",
    State->AdminQueue.SubmissionQueue.Buffer.DeviceAddress,
    State->AdminQueue.SubmissionQueue.Buffer.HostAddress
    ));

  DEBUG ((
    DEBUG_INFO,
    "AziHsm: ACQ [DeviceAddr:%lx] [HostAddr:%lx]\n",
    State->AdminQueue.CompletionQueue.Buffer.DeviceAddress,
    State->AdminQueue.CompletionQueue.Buffer.HostAddress
    ));

  Asq.Bits.BaseAddr = State->AdminQueue.SubmissionQueue.Buffer.DeviceAddress;
  Acq.Bits.BaseAddr = State->AdminQueue.CompletionQueue.Buffer.DeviceAddress;

  Status = WriteAqaReg (State->PciIo, &Aqa);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed To Write Aqa. Status: %r\n", Status));
    goto Exit;
  }

  Status = WriteAsqReg (State->PciIo, &Asq);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed To Write Asq. Status: %r\n", Status));
    goto Exit;
  }

  Status = WriteAcqReg (State->PciIo, &Acq);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed To Write Acq. Status: %r\n", Status));
    goto Exit;
  }

  Status = EnableController (State, &Cap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed To Enable Controller. Status: %r\n", Status));
    goto Exit;
  }

  PrintCapReg (&Cap);
  PrintVerReg (&Ver);
  PrintAqaReg (&Aqa);
  PrintAsqReg (&Asq);
  PrintAcqReg (&Acq);

  Status = AziHsmAdminIdentifyCtrl (State, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Identify Controller Failed. Status: %r\n", Status));
    goto Exit;
  }

Exit:
  return Status;
}

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
  )
{
  EFI_STATUS           Status;
  AZIHSM_CTRL_CAP_REG  Cap;

  Status = AziHsmAdminDeleteDeviceIoQueuePair (State, &State->HsmQueue);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = ReadCapReg (State->PciIo, &Cap);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = DisableController (State, &Cap);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = AziHsmQueuePairUninitialize (&State->AdminQueue);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to free admin queue: %r\n", Status));
  }

  Status = AziHsmQueuePairUninitialize (&State->HsmQueue);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AziHsm: Failed to free hsm queue: %r\n", Status));
  }

Exit:
  return Status;
}
