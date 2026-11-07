/** @file

    This file contains types and constants shared between
    HyperV host and ARM64 UEFI firmware.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
--*/
#pragma once

// Legacy ARM64 interrupts.

#define GIC_LEGACY_PPI_OVERFLOW_INTERRUPT_FROM_CNTP   19
#define GIC_LEGACY_PPI_OVERFLOW_INTERRUPT_FROM_CNTV   20
#define GIC_LEGACY_PPI_OVERFLOW_INTERRUPT_FROM_CNTHP  21
#define GIC_LEGACY_PPI_OVERFLOW_INTERRUPT_FROM_CNTHV  0
#define GIC_LEGACY_PPI_GIC_MAINTENANCE_INTERRUPT      0
// Legacy value is same as standard.
#define GIC_LEGACY_PPI_PERFORMANCE_MONITORS_INTERRUPT  GIC_BSA_PPI_PERFORMANCE_MONITORS_INTERRUPT

// Standard ARM64 interrupts (BSA, SBSA, VBSA).

#define GIC_BSA_PPI_OVERFLOW_INTERRUPT_FROM_CNTP    30
#define GIC_BSA_PPI_OVERFLOW_INTERRUPT_FROM_CNTV    27
#define GIC_BSA_PPI_OVERFLOW_INTERRUPT_FROM_CNTHP   26
#define GIC_BSA_PPI_OVERFLOW_INTERRUPT_FROM_CNTHV   28
#define GIC_BSA_PPI_GIC_MAINTENANCE_INTERRUPT       25
#define GIC_BSA_PPI_PERFORMANCE_MONITORS_INTERRUPT  23

//
// See ARM base system architecture.
//
typedef struct _VM_CONFIG_ARM64_INTERRUPTS {
  // -----------------------------------------------------------------------------------------
  // RB_PPI_01 Table 3: PPI assignments for operating system
  // These are historically unused or not assigned according to standard.
  // -----------------------------------------------------------------------------------------
  UINT8    CNTP;    // Overflow interrupt from CNTP  Non-secure physical timer interrupt.
  UINT8    CNTV;    // Overflow interrupt from CNTV  Virtual timer interrupt.
  UINT8    CTIIRQ;  // Cross Trigger Interface (CTI) interrupt.
  UINT8    Perf;    // Indicates an overflow condition in the Performance monitoring unit.
  UINT8    Debug;   // COMMIRQ Debug Communication Channel (DCC) interrupt.
  UINT8    Profile; // PMBIRQ Statistical profiling Interrupt if Statistical Profiling Extensions are implemented.
  // -----------------------------------------------------------------------------------------
  UINT8    Trace; // An implementation might need to reserve a PPI for trace buffer overflow.
  // -----------------------------------------------------------------------------------------
  // RB_PPI_02 Table 4: PPI assignments for Hypervisor
  // These are historically unused and must be standard compliant for nested virtualization.
  // -----------------------------------------------------------------------------------------
  // Virtual before Physical is inconsistent but is the documentation order.
  UINT8    CNTHV;          // Overflow interrupt from CNTHV  Non-secure EL2 virtual timer
                           // interrupt if PEs are Armv8.1 or greater.
  UINT8    CNTHP;          // Overflow interrupt from CNTHP  Non-secure EL2 physical timer interrupt.
  UINT8    GICMaintenance; // The virtual PE interface list register overflow interrupt.
  // -----------------------------------------------------------------------------------------
  // RB_PPI_03 Table 5: PPI assignments for platform security functionality
  // This is for completeness but not expected to be used.
  // -----------------------------------------------------------------------------------------
  UINT8    CNTPS;  // Overflow interrupt from CNTPS  Secure Physical timer interrupt.
  UINT8    CNTHPS; // Secure EL2 physical timer interrupt if Secure EL2 is implemented.
  UINT8    CNTHVS; // Secure EL2 virtual timer interrupt if Secure EL2 is implemented.
  // -----------------------------------------------------------------------------------------
  // Above is BSA.
  // Below is other interrupts used in HyperV-UEFI and perhaps needing more flexibility (host awareness/assignment).
  // -----------------------------------------------------------------------------------------
  // Synthetic Timer Configuration
  UINT8    SynicTimerSintIndex;
  UINT8    SynicTimerTimerIndex;
  UINT8    SynicTimerVector;
  // Vmbus Config
  UINT8    VmbusSintVector; // VmbusSintVector vs. VmbusVector: for UEFI vs. for guest
  UINT8    VmbusSintIndex;
  // PPI for Linux. Older public kernels used by WSL2 had 16 and 17 hardcoded, so
  // use the next available, 18.
  UINT8    VmbusVector; // VmbusSintVector vs. VmbusVector: for UEFI vs. for guest
  // Generation Counter Device
  UINT8    GenCountEventVector;
  // Battery Device
  UINT8    BatteryEventVector;
  // UART Devices
  UINT8    Com1Vector;
  UINT8    Com2Vector;
  // real time clock (RTC)
  UINT8    RtcVector;
  // PMEM (NVDIMM)
  UINT8    PmemEventVector;
  // Processor Aggregator
  UINT8    ProcIdleEventVector;
  UINT8    PaddingForAlignment[6];
} VM_CONFIG_ARM64_INTERRUPTS;
