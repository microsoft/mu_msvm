/** @file
  CPU DXE Module extension for BdLIb.

  Copyright (c) 2008 - 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

// MS_HYP_CHANGE BEGIN

#include "CpuDxe.h"

#include <IsolationTypes.h>
#include <Hv/HvGuestMsr.h>
#include <Library/CrashLib.h>
#include <Library/PrintLib.h>


#define CPU_INTERRUPT_NUM  256


IA32_IDT_GATE_DESCRIPTOR  mOrigIdtEntry[CPU_INTERRUPT_NUM] = { 0 };

EFI_CPU_INTERRUPT_HANDLER ExternalVectorTable[0x100];
UINT16                    mOrigIdtEntryCount    = 0;


extern IA32_IDT_GATE_DESCRIPTOR  gIdtTable[CPU_INTERRUPT_NUM];
extern EFI_CPU_ARCH_PROTOCOL gCpu;

//
// Error code flag indicating whether or not an error code will be
// pushed on the stack if an exception occurs.
//
// 1 means an error code will be pushed, otherwise 0
//
// bit 0 - exception 0
// bit 1 - exception 1
// etc.
//
UINT32 mErrorCodeFlag = 0x00027d00;

//
// Local function prototypes
//

/**
  Label of base address of IDT vector 0.

  This is just a label of base address of IDT vector 0.

**/
VOID
EFIAPI
AsmIdtVector00 (
  VOID
  );

/**
  Restore original Interrupt Descriptor Table Handler Address.

  @param Index        The Index of the interrupt descriptor table handle.

**/
VOID
RestoreInterruptDescriptorTableHandlerAddress (
  IN UINTN       Index
  );

/**
  Set Interrupt Descriptor Table Handler Address.

  @param Index        The Index of the interrupt descriptor table handle.
  @param Handler      Handler address.

**/
VOID
SetInterruptDescriptorTableHandlerAddress (
  IN UINTN Index,
  IN VOID  *Handler  OPTIONAL
  );

//
// CPU Arch Protocol Functions
//


/**
  Common exception handler.

  @param  InterruptType  Exception type
  @param  SystemContext  EFI_SYSTEM_CONTEXT

**/
VOID
EFIAPI
CommonExceptionHandlerMsvm (
  IN EFI_EXCEPTION_TYPE   InterruptType,
  IN EFI_SYSTEM_CONTEXT   SystemContext
  )
{
#if defined (MDE_CPU_X64)
  CHAR8 buffer[HV_CRASH_MAXIMUM_MESSAGE_SIZE];
  UINTN cursor = 0;

  cursor += AsciiSPrint(
    &buffer[cursor],
    (sizeof buffer) - (sizeof(buffer[0]) * cursor),
    "!!!! X64 Exception Type - %016lx !!!!\n",
    (UINT64)InterruptType
    );

  if ((mErrorCodeFlag & (1 << InterruptType)) != 0) {
    cursor += AsciiSPrint (
      &buffer[cursor],
      (sizeof buffer) - (sizeof(buffer[0]) * cursor),
      "ExceptionData - %016lx\n",
      SystemContext.SystemContextX64->ExceptionData
      );
  }

  cursor += AsciiSPrint (
    &buffer[cursor],
    (sizeof buffer) - (sizeof(buffer[0]) * cursor),
    "RIP - %016lx, RFL - %016lx\n",
    SystemContext.SystemContextX64->Rip,
    SystemContext.SystemContextX64->Rflags
    );
  cursor += AsciiSPrint (
    &buffer[cursor],
    (sizeof buffer) - (sizeof(buffer[0]) * cursor),
    "ExceptionData - %016lx\n",
    SystemContext.SystemContextX64->ExceptionData
    );
  cursor += AsciiSPrint (
    &buffer[cursor],
    (sizeof buffer) - (sizeof(buffer[0]) * cursor),
    "RAX - %016lx, RCX - %016lx, RDX - %016lx\n",
    SystemContext.SystemContextX64->Rax,
    SystemContext.SystemContextX64->Rcx,
    SystemContext.SystemContextX64->Rdx
    );
  cursor += AsciiSPrint (
    &buffer[cursor],
    (sizeof buffer) - (sizeof(buffer[0]) * cursor),
    "RBX - %016lx, RSP - %016lx, RBP - %016lx\n",
    SystemContext.SystemContextX64->Rbx,
    SystemContext.SystemContextX64->Rsp,
    SystemContext.SystemContextX64->Rbp
    );
  cursor += AsciiSPrint (
    &buffer[cursor],
    (sizeof buffer) - (sizeof(buffer[0]) * cursor),
    "RSI - %016lx, RDI - %016lx\n",
    SystemContext.SystemContextX64->Rsi,
    SystemContext.SystemContextX64->Rdi
    );
  cursor += AsciiSPrint (
    &buffer[cursor],
    (sizeof buffer) - (sizeof(buffer[0]) * cursor),
    "R8  - %016lx, R9  - %016lx, R10 - %016lx\n",
    SystemContext.SystemContextX64->R8,
    SystemContext.SystemContextX64->R9,
    SystemContext.SystemContextX64->R10
    );
  cursor += AsciiSPrint (
    &buffer[cursor],
    (sizeof buffer) - (sizeof(buffer[0]) * cursor),
    "R11 - %016lx, R12 - %016lx, R13 - %016lx\n",
    SystemContext.SystemContextX64->R11,
    SystemContext.SystemContextX64->R12,
    SystemContext.SystemContextX64->R13
    );
  cursor += AsciiSPrint (
    &buffer[cursor],
    (sizeof buffer) - (sizeof(buffer[0]) * cursor),
    "R14 - %016lx, R15 - %016lx\n",
    SystemContext.SystemContextX64->R14,
    SystemContext.SystemContextX64->R15
    );
  cursor += AsciiSPrint (
    &buffer[cursor],
    (sizeof buffer) - (sizeof(buffer[0]) * cursor),
    "CS  - %04lx, DS  - %04lx, ES  - %04lx, FS  - %04lx, GS  - %04lx, SS  - %04lx\n",
    SystemContext.SystemContextX64->Cs,
    SystemContext.SystemContextX64->Ds,
    SystemContext.SystemContextX64->Es,
    SystemContext.SystemContextX64->Fs,
    SystemContext.SystemContextX64->Gs,
    SystemContext.SystemContextX64->Ss
    );
  cursor += AsciiSPrint(
    &buffer[cursor],
    (sizeof buffer) - (sizeof(buffer[0]) * cursor),
    "GDT - %016lx; %04lx,                   IDT - %016lx; %04lx\n",
    SystemContext.SystemContextX64->Gdtr[0],
    SystemContext.SystemContextX64->Gdtr[1],
    SystemContext.SystemContextX64->Idtr[0],
    SystemContext.SystemContextX64->Idtr[1]
    );
  cursor += AsciiSPrint(
    &buffer[cursor],
    (sizeof buffer) - (sizeof(buffer[0]) * cursor),
    "LDT - %016lx, TR  - %016lx\n",
    SystemContext.SystemContextX64->Ldtr,
    SystemContext.SystemContextX64->Tr
    );
  cursor += AsciiSPrint(
    &buffer[cursor],
    (sizeof buffer) - (sizeof(buffer[0]) * cursor),
    "CR0 - %016lx, CR2 - %016lx, CR3 - %016lx\n",
    SystemContext.SystemContextX64->Cr0,
    SystemContext.SystemContextX64->Cr2,
    SystemContext.SystemContextX64->Cr3
    );
  cursor += AsciiSPrint(
    &buffer[cursor],
    (sizeof buffer) - (sizeof(buffer[0]) * cursor),
    "CR4 - %016lx, CR8 - %016lx\n",
    SystemContext.SystemContextX64->Cr4,
    SystemContext.SystemContextX64->Cr8
    );
  cursor += AsciiSPrint(
    &buffer[cursor],
    (sizeof buffer) - (sizeof(buffer[0]) * cursor),
    "DR0 - %016lx, DR1 - %016lx, DR2 - %016lx\n",
    SystemContext.SystemContextX64->Dr0,
    SystemContext.SystemContextX64->Dr1,
    SystemContext.SystemContextX64->Dr2
    );
  cursor += AsciiSPrint(
    &buffer[cursor],
    (sizeof buffer) - (sizeof(buffer[0]) * cursor),
    "DR3 - %016lx, DR6 - %016lx, DR7 - %016lx\n",
    SystemContext.SystemContextX64->Dr3,
    SystemContext.SystemContextX64->Dr6,
    SystemContext.SystemContextX64->Dr7
    );

  for (UINT32 index = 0; index < cursor; index += 0x100) // MAX_DEBUG_MESSAGE_LENGTH
  {
    DEBUG ((EFI_D_ERROR, "%a", &buffer[index]));
  }

#else
#error CPU type not supported for exception information dump!
#endif

  FailFast(
    InterruptType,
    SystemContext.SystemContextX64->ExceptionData,
    0,
    (UINTN)&buffer,
    cursor);
}

/**
  Initialize Interrupt Descriptor Table for interrupt handling.

**/
VOID
BdInitInterruptDescriptorTable (
  VOID
  )
{
  EFI_STATUS                Status;
  IA32_DESCRIPTOR           OldIdtPtr;
  IA32_IDT_GATE_DESCRIPTOR  *OldIdt;
  UINTN                     OldIdtSize;
  __declspec(align(16)) IA32_DESCRIPTOR IdtPtr;
  UINTN                     Index;
  UINT16                    CurrentCs;
  VOID                      *IntHandler;

  SetMem (ExternalVectorTable, sizeof(ExternalVectorTable), 0);

  //
  // Get original IDT address and size.
  //
  AsmReadIdtr ((IA32_DESCRIPTOR *) &OldIdtPtr);

  if ((OldIdtPtr.Base != 0) && ((OldIdtPtr.Limit & 7) == 7)) {
    OldIdt = (IA32_IDT_GATE_DESCRIPTOR*) OldIdtPtr.Base;
    OldIdtSize = (OldIdtPtr.Limit + 1) / sizeof (IA32_IDT_GATE_DESCRIPTOR);
    //
    // Save original IDT entry and IDT entry count.
    //
    CopyMem(mOrigIdtEntry, (VOID *) OldIdtPtr.Base, OldIdtPtr.Limit + 1);
    mOrigIdtEntryCount = (UINT16) OldIdtSize;
  } else {
    OldIdt = NULL;
    OldIdtSize = 0;
  }

  //
  // Intialize IDT
  //
  CurrentCs = AsmReadCs();
  for (Index = 0; Index < CPU_INTERRUPT_NUM; Index ++) {

    //
    // If the old IDT had a handler for this interrupt, then
    // preserve it.
    //
    if ((Index < OldIdtSize) &&
        (OldIdt[Index].Bits.GateType != 0)) {

      IntHandler =
        (VOID*) (
          OldIdt[Index].Bits.OffsetLow +
          (((UINTN) OldIdt[Index].Bits.OffsetHigh) << 16)
#if defined (MDE_CPU_X64)
            + (((UINTN) OldIdt[Index].Bits.OffsetUpper) << 32)
#endif
          );

        SetInterruptDescriptorTableHandlerAddress (Index, IntHandler);
    }
  }

  //
  // Load IDT Pointer
  //
  IdtPtr.Base = (UINT32)(((UINTN)(VOID*) gIdtTable) & (BASE_4GB-1));
  IdtPtr.Limit = (UINT16) (sizeof (gIdtTable) - 1);

  AsmWriteIdtr (&IdtPtr);

  //
  // Initialize Exception Handlers
  //
  for (Index = 0; Index < 32; Index++) {
    if (gIdtTable[Index].Bits.GateType == 0) {
      Status = CpuRegisterInterruptHandler (&gCpu, Index, CommonExceptionHandlerMsvm);
      ASSERT_EFI_ERROR (Status);
    }
  }
}

EFI_STATUS
EFIAPI
BdRegisterCpuInterruptHandler (
  IN EFI_EXCEPTION_TYPE         InterruptType,
  IN EFI_CPU_INTERRUPT_HANDLER  InterruptHandler
  )
{
  if (InterruptType < 0 || InterruptType > 0xff) {
    return EFI_UNSUPPORTED;
  }

  if (InterruptHandler == NULL && ExternalVectorTable[InterruptType] == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (InterruptHandler != NULL && ExternalVectorTable[InterruptType] != NULL) {
    return EFI_ALREADY_STARTED;
  }

  if (InterruptHandler != NULL) {
    SetInterruptDescriptorTableHandlerAddress ((UINTN)InterruptType, NULL);
  } else {
    //
    // Restore the original IDT handler address if InterruptHandler is NULL.
    //
    RestoreInterruptDescriptorTableHandlerAddress ((UINTN)InterruptType);
  }

  ExternalVectorTable[InterruptType] = InterruptHandler;
  return EFI_SUCCESS;
}


/**
  Set Interrupt Descriptor Table Handler Address.

  @param Index        The Index of the interrupt descriptor table handle.
  @param Handler      Handler address.

**/
VOID
SetInterruptDescriptorTableHandlerAddress (
  IN UINTN Index,
  IN VOID  *Handler  OPTIONAL
  )
{
  UINTN                     UintnHandler;

  if (Handler != NULL) {
    UintnHandler = (UINTN) Handler;
  } else {
    UintnHandler = ((UINTN) AsmIdtVector00) + (8 * Index);
  }

  gIdtTable[Index].Bits.Selector    = AsmReadCs();
  gIdtTable[Index].Bits.OffsetLow   = (UINT16)UintnHandler;
  gIdtTable[Index].Bits.Reserved_0  = 0;
  gIdtTable[Index].Bits.GateType    = IA32_IDT_GATE_TYPE_INTERRUPT_32;
  gIdtTable[Index].Bits.OffsetHigh  = (UINT16)(UintnHandler >> 16);
#if defined (MDE_CPU_X64)
  gIdtTable[Index].Bits.OffsetUpper = (UINT32)(UintnHandler >> 32);
  gIdtTable[Index].Bits.Reserved_1  = 0;
#endif
}

/**
  Restore original Interrupt Descriptor Table Handler Address.

  @param Index        The Index of the interrupt descriptor table handle.

**/
VOID
RestoreInterruptDescriptorTableHandlerAddress (
  IN UINTN       Index
  )
{
  if (Index < mOrigIdtEntryCount) {
    gIdtTable[Index].Bits.OffsetLow   = mOrigIdtEntry[Index].Bits.OffsetLow;
    gIdtTable[Index].Bits.OffsetHigh  = mOrigIdtEntry[Index].Bits.OffsetHigh;
#if defined (MDE_CPU_X64)
    gIdtTable[Index].Bits.OffsetUpper = mOrigIdtEntry[Index].Bits.OffsetUpper;
#endif
  }
}




// MS_HYP_CHANGE END