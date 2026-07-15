/** @file
  Private declarations shared by the EfiHvDxe implementation files.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#pragma once

#include <PiDxe.h>
#include <IsolationTypes.h>
#include <Hv/HvGuestHypercall.h>
#include <Hv/HvStatus.h>
#include <Protocol/Cpu.h>
#include <Protocol/EfiHv.h>
#include <Guid/EventGroup.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CrashLib.h>
#include <Library/DebugLib.h>
#include <Library/HostVisibilityLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/HvHypercallLib.h>
#if defined(MDE_CPU_X64)
#include <Library/LocalApicLib.h>
#endif
#if defined(MDE_CPU_AARCH64)
#include <Protocol/HardwareInterrupt.h>
#endif

#define WINHVP_MAX_REPS_PER_HYPERCALL  0xFFF

typedef struct _EFI_HV_SINT_CONFIGURATION
{
  EFI_HV_INTERRUPT_HANDLER InterruptHandler;
  VOID *Context;
  UINT8 Vector;
} EFI_HV_SINT_CONFIGURATION, *PEFI_HV_SINT_CONFIGURATION;

typedef struct _EFI_HV_PAGES
{
  UINT8 HypercallInputPage[EFI_PAGE_SIZE];
  UINT8 HypercallOutputPage[EFI_PAGE_SIZE];
  HV_SYNIC_EVENT_FLAGS_PAGE EventFlagsPage;
  HV_MESSAGE_PAGE MessagePage;

#if defined(MDE_CPU_X64)

  //
  // Additional pages needed to configure the paravisor in an isolated VM, to
  // allow for encrypted communication with the paravisor.
  //

  HV_SYNIC_EVENT_FLAGS_PAGE ParavisorEventFlagsPage;
  HV_MESSAGE_PAGE ParavisorMessagePage;

#endif

} EFI_HV_PAGES, *PEFI_HV_PAGES;

typedef struct _EFI_HV_PROTECTION_OBJECT
{
  LIST_ENTRY ListEntry;
  UINT64 GpaPageBase;
  UINT32 NumberOfPages;
} EFI_HV_PROTECTION_OBJECT, *PEFI_HV_PROTECTION_OBJECT;

typedef struct _EFI_HV_PIN_OBJECT
{
  LIST_ENTRY ListEntry;
  UINT64 RequestGpaPageBase;
  UINT32 RequestNumberOfPages;
  UINT64 GpaPageBase;
  UINT32 NumberOfPages;
} EFI_HV_PIN_OBJECT, *PEFI_HV_PIN_OBJECT;

extern HV_HYPERCALL_CONTEXT mHvContext;
extern HV_HYPERCALL_CONTEXT mHvBypassContext;
extern BOOLEAN mUseBypassContext;
extern BOOLEAN mBypassOnly;
extern PEFI_HV_PAGES mHvPages;
extern VOID *mHvInputPage;
extern EFI_HANDLE mHvHandle;
extern BOOLEAN mSynicConnected;
extern EFI_EVENT mExitBootServicesEvent;
extern BOOLEAN mAutoEoi;
extern BOOLEAN mDirectTimerSupported;
extern LIST_ENTRY mHostVisiblePageList;
extern LIST_ENTRY mPinnedPageList;
extern UINT64 mSharedGpaBoundary;
extern UINT64 mCanonicalizationMask;
extern UINT32 mIsolationType;
extern VOID *mSvsmCallingArea;
extern EFI_HV_SINT_CONFIGURATION mSintConfiguration[HV_SYNIC_SINT_COUNT];
extern UINT8 mVectorSint[256];
extern EFI_HV_INTERRUPT_HANDLER mDirectTimerInterruptHandlers[256];
extern HV_X64_MSR_STIMER_CONFIG_CONTENTS mTimerConfiguration[HV_SYNIC_STIMER_COUNT];
extern EFI_HV_PROTOCOL mHv;

#if defined(MDE_CPU_X64)
extern UINT8 *mHypercallPage;
extern EFI_CPU_ARCH_PROTOCOL *mCpu;
#elif defined(MDE_CPU_AARCH64)
extern EFI_HARDWARE_INTERRUPT_PROTOCOL *mHwInt;
#endif

UINTN
EfiHvpSharedPa(
  VOID *Address
  );

VOID *
EfiHvpSharedVa(
  VOID *Address
  );

UINTN
EfiHvpBasePa(
  UINTN Address
  );

HV_STATUS
EfiHvIssueHypercall(
  IN  HV_CALL_CODE CallCode,
  IN  BOOLEAN Fast,
  IN  UINT64 FirstRegister,
  IN  UINT64 SecondRegister
  );

EFI_STATUS
EfiHvConvertStatus(
  IN  HV_STATUS Status
  );

EFI_STATUS
EfiHvConnectToHypervisor(
  VOID
  );

VOID
EfiHvDisconnectFromHypervisor(
  VOID
  );

EFI_STATUS
EfiHvConnectToSynic(
  VOID
  );

VOID
EfiHvDisconnectFromSynic(
  VOID
  );

EFI_STATUS
EFIAPI
EfiHvConnectSint(
  IN  EFI_HV_PROTOCOL *This,
  IN  HV_SYNIC_SINT_INDEX SintIndex,
  IN  UINT8 Vector,
  IN  BOOLEAN NoProxy,
  IN  EFI_HV_INTERRUPT_HANDLER InterruptHandler,
  IN  VOID *Context
  );

EFI_STATUS
EFIAPI
EfiHvConnectSintToEvent(
  IN  EFI_HV_PROTOCOL *This,
  IN  HV_SYNIC_SINT_INDEX SintIndex,
  IN  UINT8 Vector,
  IN  EFI_EVENT Event
  );

VOID
EFIAPI
EfiHvDisconnectSint(
  IN  EFI_HV_PROTOCOL *This,
  IN  HV_SYNIC_SINT_INDEX SintIndex
  );

HV_MESSAGE *
EFIAPI
EfiHvGetSintMessage(
  IN  EFI_HV_PROTOCOL *This,
  IN  HV_SYNIC_SINT_INDEX SintIndex,
  IN  BOOLEAN Direct
  );

EFI_STATUS
EFIAPI
EfiHvCompleteSintMessage(
  IN  EFI_HV_PROTOCOL *This,
  IN  HV_SYNIC_SINT_INDEX SintIndex,
  IN  BOOLEAN Direct
  );

volatile HV_SYNIC_EVENT_FLAGS *
EFIAPI
EfiHvGetSintEventFlags(
  IN  EFI_HV_PROTOCOL *This,
  IN  HV_SYNIC_SINT_INDEX SintIndex,
  IN  BOOLEAN Direct
  );

UINT64
EFIAPI
EfiHvGetReferenceTime(
  IN  EFI_HV_PROTOCOL *This
  );

UINT32
EFIAPI
EfiHvGetCurrentVpIndex(
  IN  EFI_HV_PROTOCOL *This
  );

VOID
EFIAPI
EfiHvSetTimer(
  IN  EFI_HV_PROTOCOL *This,
  IN  UINT32 TimerIndex,
  IN  UINT64 Expiration
  );

BOOLEAN
EFIAPI
EfiHvDirectTimerSupported(
  VOID
  );

EFI_STATUS
EFIAPI
EfiHvConfigureTimer(
  IN          EFI_HV_PROTOCOL             *This,
  IN          UINT32                      TimerIndex,
  IN          HV_SYNIC_SINT_INDEX         SintIndex,
  IN          BOOLEAN                     Periodic,
  IN          BOOLEAN                     DirectMode,
  IN          UINT8                       Vector,
  IN OPTIONAL EFI_HV_INTERRUPT_HANDLER    InterruptHandler
  );

EFI_STATUS
EFIAPI
EfiHvPostMessage(
  IN  EFI_HV_PROTOCOL *This,
  IN  HV_CONNECTION_ID ConnectionId,
  IN  HV_MESSAGE_TYPE MessageType,
  IN  VOID *Payload,
  IN  UINT32 PayloadSize,
  IN  BOOLEAN DirectHypercall
  );

EFI_STATUS
EFIAPI
EfiHvSignalEvent(
  IN  EFI_HV_PROTOCOL *This,
  IN  HV_CONNECTION_ID ConnectionId,
  IN  UINT16 FlagNumber
  );

EFI_STATUS
EFIAPI
EfiHvStartApplicationProcessor(
  IN  EFI_HV_PROTOCOL *This,
  IN  UINT64 VpIndex,
  IN  PHV_INITIAL_VP_CONTEXT VpContext
  );

EFI_STATUS
EFIAPI
EfiHvpModifySparseGpaPageHostVisibility(
  IN              HV_MAP_GPA_FLAGS    MapFlags,
  IN              UINT32              PageCount,
  IN              HV_GPA_PAGE_NUMBER  GpaPageBase,
  OUT OPTIONAL    UINT32              *PageCountProcessed
  );

EFI_STATUS
EFIAPI
EfiHvMakeAddressRangeHostVisible(
  IN              EFI_HV_IVM_PROTOCOL         *This,
  IN              HV_MAP_GPA_FLAGS            MapFlags,
  IN              VOID                        *BaseAddress,
  IN              UINT32                      ByteCount,
  IN              BOOLEAN                     ZeroPages,
  OUT OPTIONAL    EFI_HV_PROTECTION_HANDLE    *ProtectionHandle
  );

VOID
EFIAPI
EfiHvMakeAddressRangeNotHostVisible(
  IN      EFI_HV_IVM_PROTOCOL *This,
  IN OUT  EFI_HV_PROTECTION_HANDLE *ProtectionHandle
  );

EFI_STATUS
EfiHvpPinUnpinGpaPageRanges(
  IN              BOOLEAN             Pin,
  IN              UINT32              PageCount,
  IN              HV_GPA_PAGE_NUMBER  GpaPageBase,
  OUT OPTIONAL    UINT32              *PageCountProcessed
  );

EFI_STATUS
EFIAPI
EfiHvPinAddressRange(
  IN              EFI_HV_IVM_PROTOCOL *This,
  IN              VOID                *BaseAddress,
  IN              UINT32              ByteCount,
  OUT OPTIONAL    BOOLEAN             *PinApplied
  );

VOID
EFIAPI
EfiHvUnpinAddressRange(
  IN  EFI_HV_IVM_PROTOCOL *This,
  IN  VOID                *BaseAddress,
  IN  UINT32              ByteCount
  );
