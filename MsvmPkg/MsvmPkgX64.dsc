## @file
#  EFI/Framework Microsoft Virtual Machine Firmware (MSVM) platform
#
#  Copyright (C) Microsoft.
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                 = Msvm
  PLATFORM_GUID                 = 60d3fbae-b4ed-4a10-9145-f8185dd8b1bd
  PLATFORM_VERSION              = 0.1
  DSC_SPECIFICATION             = 0x00010005
  OUTPUT_DIRECTORY              = Build/MsvmX64
  SUPPORTED_ARCHITECTURES       = X64
  BUILD_TARGETS                 = DEBUG|RELEASE
  SKUID_IDENTIFIER              = DEFAULT
  FLASH_DEFINITION              = MsvmPkg/MsvmPkgX64.fdf

#
# Defines for the pre-built BaseCryptLib
#
  PEI_CRYPTO_SERVICES           = TINY_SHA
  DXE_CRYPTO_SERVICES           = STANDARD
  PEI_CRYPTO_ARCH               = X64
  DXE_CRYPTO_ARCH               = X64

################################################################################
#
# BuildOptions Section - extra build flags
#
################################################################################
[BuildOptions]
  *_*_X64_GENFW_FLAGS = --keepexceptiontable
  DEBUG_*_*_CC_FLAGS = -D DEBUG_PLATFORM

################################################################################
#
# SKU Identification section - list of all SKU IDs supported by this Platform.
#
################################################################################
[SkuIds]
  0|DEFAULT

################################################################################
#
# Library Class section
#
# Library class names used by this platform and the implementations of those
# libraries.  <name>|<inf location>
#
################################################################################

#
# Library instances to use by default for all modules and phases unless overridden below
#
[LibraryClasses]
  AdvancedLoggerAccessLib|AdvLoggerPkg/Library/AdvancedLoggerAccessLib/AdvancedLoggerAccessLib.inf
  AdvancedLoggerHdwPortLib|AdvLoggerPkg/Library/AdvancedLoggerHdwPortLib/AdvancedLoggerHdwPortLib.inf
  AssertLib|AdvLoggerPkg/Library/AssertLib/AssertLib.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLibRepStr/BaseMemoryLibRepStr.inf
  BiosDeviceLib|MsvmPkg/Library/BiosDeviceLib/BiosDeviceLib.inf
  CacheMaintenanceLib|MdePkg/Library/BaseCacheMaintenanceLib/BaseCacheMaintenanceLib.inf
  CapsuleLib|MdeModulePkg/Library/DxeCapsuleLibNull/DxeCapsuleLibNull.inf
  CcExitLib|UefiCpuPkg/Library/CcExitLibNull/CcExitLibNull.inf
  CrashLib|MsvmPkg/Library/CrashLib/CrashLib.inf
  CpuLib|MdePkg/Library/BaseCpuLib/BaseCpuLib.inf
  DebugLib|AdvLoggerPkg/Library/BaseDebugLibAdvancedLogger/BaseDebugLibAdvancedLogger.inf
  DebugAgentLib|MdeModulePkg/Library/DebugAgentLibNull/DebugAgentLibNull.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  DeviceStateLib|MdeModulePkg/Library/DeviceStateLib/DeviceStateLib.inf
  DisplayDeviceStateLib|MsGraphicsPkg/Library/ColorBarDisplayDeviceStateLib/ColorBarDisplayDeviceStateLib.inf
  FltUsedLib|MdePkg/Library/FltUsedLib/FltUsedLib.inf
  FrameBufferBltLib|MdeModulePkg/Library/FrameBufferBltLib/FrameBufferBltLib.inf
  Hash2CryptoLib|SecurityPkg/Library/DxeHash2CryptoLib/DxeHash2CryptoLib.inf
  HostVisibilityLib|MsvmPkg/Library/HostVisibilityLib/HostVisibilityLib.inf
  HwResetSystemLib|MsvmPkg/Library/ResetSystemLib/ResetSystemLib.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  ImagePropertiesRecordLib|MdeModulePkg/Library/ImagePropertiesRecordLib/ImagePropertiesRecordLib.inf
  IntrinsicLib|CryptoPkg/Library/IntrinsicLib/IntrinsicLib.inf
  IsolationLib|MsvmPkg/Library/IsolationLib/IsolationLib.inf
  LocalApicLib|UefiCpuPkg/Library/BaseXApicX2ApicLib/BaseXApicX2ApicLib.inf
  MathLib|MsCorePkg/Library/MathLib/MathLib.inf
  MmUnblockMemoryLib|MdePkg/Library/MmUnblockMemoryLib/MmUnblockMemoryLibNull.inf
  MtrrLib|UefiCpuPkg/Library/MtrrLib/MtrrLib.inf
  MpInitLib|UefiCpuPkg/Library/MpInitLibUp/MpInitLibUp.inf
  MsBootPolicyLib|MsvmPkg/Library/MsBootPolicyLib/MsBootPolicyLib.inf
  PanicLib|MdePkg/Library/BasePanicLibNull/BasePanicLibNull.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  PCUartLib|MsvmPkg/Library/PCUart/PCUart.inf
  PeCoffLib|MdePkg/Library/BasePeCoffLib/BasePeCoffLib.inf
  PeCoffExtraActionLib|MdePkg/Library/BasePeCoffExtraActionLibNull/BasePeCoffExtraActionLibNull.inf
  PeCoffGetEntryPointLib|MdePkg/Library/BasePeCoffGetEntryPointLib/BasePeCoffGetEntryPointLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  ResetUtilityLib|MdeModulePkg/Library/ResetUtilityLib/ResetUtilityLib.inf
  SecurityLockAuditLib|MdeModulePkg/Library/SecurityLockAuditDebugMessageLib/SecurityLockAuditDebugMessageLib.inf ##MSCHANGE
  SerialPortLib|MdePkg/Library/BaseSerialPortLibNull/BaseSerialPortLibNull.inf
  StackCheckFailureHookLib|MdePkg/Library/StackCheckFailureHookLibNull/StackCheckFailureHookLibNull.inf
  SynchronizationLib|MdePkg/Library/BaseSynchronizationLib/BaseSynchronizationLib.inf
  TimerLib|MsvmPkg/Library/HvTimerLib/HvTimerLib.inf
  SortLib|MdeModulePkg/Library/BaseSortLib/BaseSortLib.inf
  UefiDecompressLib|MdePkg/Library/BaseUefiDecompressLib/BaseUefiDecompressLib.inf
  UiRectangleLib|MsGraphicsPkg/Library/BaseUiRectangleLib/BaseUiRectangleLib.inf
  UefiBootManagerLib|MdeModulePkg/Library/UefiBootManagerLib/UefiBootManagerLib.inf
  VariablePolicyHelperLib|MdeModulePkg/Library/VariablePolicyHelperLib/VariablePolicyHelperLib.inf

!if $(DEBUGLIB_SERIAL) == 1
  SerialPortLib|PcAtChipsetPkg/Library/SerialIoLib/SerialIoLib.inf
!endif

!if $(DEBUGLIB_BIOS) == 1
  DebugLib|MsvmPkg/Library/BiosDeviceDebugLib/BiosDeviceDebugLib.inf
!endif

  ## MS_CHANGE_?
  # MeasuredBoot and Other TPM-Based Security
  Tpm2DeviceLib|SecurityPkg/Library/Tpm2DeviceLibTcg2/Tpm2DeviceLibTcg2.inf
  Tpm2CommandLib|SecurityPkg/Library/Tpm2CommandLib/Tpm2CommandLib.inf
  TpmMeasurementLib|SecurityPkg/Library/DxeTpmMeasurementLib/DxeTpmMeasurementLib.inf
  Tcg2PhysicalPresenceLib|SecurityPkg/Library/DxeTcg2PhysicalPresenceLib/DxeTcg2PhysicalPresenceLib.inf
  Tcg2PpVendorLib|SecurityPkg/Library/Tcg2PpVendorLibNull/Tcg2PpVendorLibNull.inf
  OemTpm2InitLib|SecurityPkg/Library/OemTpm2InitLibNull/OemTpm2InitLib.inf               ## MS_CHANGE_?
  Tpm2DebugLib|SecurityPkg/Library/Tpm2DebugLib/Tpm2DebugLibNull.inf
  Tcg2PreUefiEventLogLib|SecurityPkg/Library/Tcg2PreUefiEventLogLibNull/Tcg2PreUefiEventLogLibNull.inf
  ## MS_CHANGE_?

  # MsCore BDS & FrontPage Libs
  PlatformBootManagerLib|MsCorePkg/Library/PlatformBootManagerLib/PlatformBootManagerLib.inf
  DeviceBootManagerLib|MsvmPkg/Library/DeviceBootManagerLib/DeviceBootManagerLib.inf
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  MsLogoLib|MsvmPkg/Library/MsLogoLib/MsLogoLib.inf #point to MsLogoLib
  BmpSupportLib|MdeModulePkg/Library/BaseBmpSupportLib/BaseBmpSupportLib.inf
  SafeIntLib|MdePkg/Library/BaseSafeIntLib/BaseSafeIntLib.inf
  MsPlatBdsLib|MsvmPkg/Library/MsPlatBdsLib/MsPlatBdsLib.inf

  #
  # MsGraphicsPkg Libs
  #
  UIToolKitLib|MsGraphicsPkg/Library/SimpleUIToolKit/SimpleUIToolKit.inf
  MsColorTableLib|MsGraphicsPkg/Library/MsColorTableLib/MsColorTableLib.inf
  MsUiThemeCopyLib|MsGraphicsPkg/Library/MsUiThemeCopyLib/MsUiThemeCopyLib.inf
  PlatformThemeLib|MsvmPkg/Library/PlatformThemeLib/PlatformThemeLib.inf
  SwmDialogsLib|MsGraphicsPkg/Library/SwmDialogsLib/SwmDialogs.inf

  #
  # Platform Runtime Package (PRM) Libs
  #
  PrmContextBufferLib|PrmPkg/Library/DxePrmContextBufferLib/DxePrmContextBufferLib.inf
  PrmModuleDiscoveryLib|PrmPkg/Library/DxePrmModuleDiscoveryLib/DxePrmModuleDiscoveryLib.inf
  PrmPeCoffLib|PrmPkg/Library/DxePrmPeCoffLib/DxePrmPeCoffLib.inf

#
# Library instance overrides for SEC and PEI
#
[LibraryClasses.common.SEC, LibraryClasses.common.PEI_CORE, LibraryClasses.common.PEIM]
  ExtractGuidedSectionLib|MdePkg/Library/BaseExtractGuidedSectionLib/BaseExtractGuidedSectionLib.inf
  MemoryAllocationLib|MdePkg/Library/PeiMemoryAllocationLib/PeiMemoryAllocationLib.inf
  PeiServicesLib|MdePkg/Library/PeiServicesLib/PeiServicesLib.inf
  PeiServicesTablePointerLib|MdePkg/Library/PeiServicesTablePointerLibIdt/PeiServicesTablePointerLibIdt.inf

#
# Library instance overrides just for SEC
#
[LibraryClasses.common.SEC]
  AdvancedLoggerLib|AdvLoggerPkg/Library/AdvancedLoggerLib/Sec/AdvancedLoggerLib.inf

#
# Library instance overrides for PEI
#
[LibraryClasses.common.PEI_CORE, LibraryClasses.common.PEIM]
  FrameBufferMemDrawLib|MsGraphicsPkg/Library/FrameBufferMemDrawLib/FrameBufferMemDrawLibPei.inf
  HobLib|MdePkg/Library/PeiHobLib/PeiHobLib.inf
  OemHookStatusCodeLib|MdeModulePkg/Library/OemHookStatusCodeLibNull/OemHookStatusCodeLibNull.inf
  ReportStatusCodeLib|MdeModulePkg/Library/PeiReportStatusCodeLib/PeiReportStatusCodeLib.inf
  PcdLib|MdePkg/Library/PeiPcdLib/PeiPcdLib.inf
  WatchdogTimerLib|MsvmPkg/Library/WatchdogTimerLib/WatchdogTimerLib.inf
  ResetSystemLib|MdeModulePkg/Library/PeiResetSystemLib/PeiResetSystemLib.inf
  NULL|MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf

#
# Library instance overrides just for PEI CORE
#
[LibraryClasses.common.PEI_CORE]
  AdvancedLoggerLib|AdvLoggerPkg/Library/AdvancedLoggerLib/PeiCore/AdvancedLoggerLib.inf
  PeiCoreEntryPoint|MdePkg/Library/PeiCoreEntryPoint/PeiCoreEntryPoint.inf

#
# Library instance overrides just for PEIMs
#
[LibraryClasses.common.PEIM]
  PeimEntryPoint|MdePkg/Library/PeimEntryPoint/PeimEntryPoint.inf
  ResourcePublicationLib|MdePkg/Library/PeiResourcePublicationLib/PeiResourcePublicationLib.inf
  GhcbLib|MsvmPkg/Library/GhcbLib/PeiGhcbLib.inf
  HvHypercallLib|MsvmPkg/Library/HvHypercallLib/PeiHvHypercallLib.inf
  PcdDatabaseLoaderLib|MdeModulePkg/Library/PcdDatabaseLoaderLib/Pei/PcdDatabaseLoaderLibPei.inf  # MU_CHANGE
  RngLib|MdePkg/Library/BaseRngLibNull/BaseRngLibNull.inf
  CpuExceptionHandlerLib|MsvmPkg/Library/CpuExceptionHandlerLib/PeiCpuExceptionHandlerLib.inf

  MsUiThemeLib|MsGraphicsPkg/Library/MsUiThemeLib/Pei/MsUiThemeLib.inf

  AdvancedLoggerLib|AdvLoggerPkg/Library/AdvancedLoggerLib/Pei/AdvancedLoggerLib.inf
  DebugLib|AdvLoggerPkg/Library/PeiDebugLibAdvancedLogger/PeiDebugLibAdvancedLogger.inf

#
# Library instance overrides for DXE
#
[LibraryClasses.common.DXE_CORE, LibraryClasses.common.DXE_DRIVER, LibraryClasses.common.DXE_RUNTIME_DRIVER, LibraryClasses.common.UEFI_DRIVER, LibraryClasses.common.UEFI_APPLICATION]
  BootEventLogLib|MsvmPkg/Library/BootEventLogLib/BootEventLogLib.inf
  ConfigLib|MsvmPkg/Library/ConfigLib/ConfigLib.inf
  CpuExceptionHandlerLib|MsvmPkg/Library/CpuExceptionHandlerLib/DxeCpuExceptionHandlerLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  DpcLib|NetworkPkg/Library/DxeDpcLib/DxeDpcLib.inf
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
  EmclLib|MsvmPkg/Library/EmclLib/EmclLib.inf
  EventLogLib|MsvmPkg/Library/EventLogLib/EventLogLib.inf
  ExtractGuidedSectionLib|MdePkg/Library/DxeExtractGuidedSectionLib/DxeExtractGuidedSectionLib.inf
  FileExplorerLib|MdeModulePkg/Library/FileExplorerLib/FileExplorerLib.inf
  FrameBufferMemDrawLib|MsGraphicsPkg/Library/FrameBufferMemDrawLib/FrameBufferMemDrawLibDxe.inf
  HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  HttpLib|NetworkPkg/Library/DxeHttpLib/DxeHttpLib.inf
  IpIoLib|NetworkPkg/Library/DxeIpIoLib/DxeIpIoLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  DxeMemoryProtectionHobLib|MdeModulePkg/Library/MemoryProtectionHobLibNull/DxeMemoryProtectionHobLibNull.inf
  MemoryTypeInformationChangeLib|MdeModulePkg/Library/MemoryTypeInformationChangeLibNull/MemoryTypeInformationChangeLibNull.inf
  MmioAllocationLib|MsvmPkg/Library/MmioAllocationLib/MmioAllocationLib.inf
  NetLib|NetworkPkg/Library/DxeNetLib/DxeNetLib.inf
  PcdDatabaseLoaderLib|MdeModulePkg/Library/PcdDatabaseLoaderLib/Dxe/PcdDatabaseLoaderLibDxe.inf  # MU_CHANGE
  PcdLib|MdePkg/Library/DxePcdLib/DxePcdLib.inf
  ReportStatusCodeLib|MdeModulePkg/Library/DxeReportStatusCodeLib/DxeReportStatusCodeLib.inf
  ResetSystemLib|MdeModulePkg/Library/DxeResetSystemLib/DxeResetSystemLib.inf
  RngLib|MsvmPkg/Library/MsvmRngLib/MsvmRngLib.inf
  UdpIoLib|NetworkPkg/Library/DxeUdpIoLib/DxeUdpIoLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  UefiScsiLib|MdePkg/Library/UefiScsiLib/UefiScsiLib.inf
  UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
  WatchdogTimerLib|MsvmPkg/Library/WatchdogTimerLib/WatchdogTimerLib.inf
  #
  # Provide StackCookie support lib so that we can link to /GS exports
  #
  NULL|MdePkg/Library/StackCheckLib/StackCheckLibStaticInit.inf

#
# Library instances overrides for just DXE CORE
#
[LibraryClasses.common.DXE_CORE]
  AdvancedLoggerLib|AdvLoggerPkg/Library/AdvancedLoggerLib/DxeCore/AdvancedLoggerLib.inf
  DxeCoreEntryPoint|MdePkg/Library/DxeCoreEntryPoint/DxeCoreEntryPoint.inf
  HobLib|MdePkg/Library/DxeCoreHobLib/DxeCoreHobLib.inf
  MemoryAllocationLib|MdeModulePkg/Library/DxeCoreMemoryAllocationLib/DxeCoreMemoryAllocationLib.inf
##MSChange Begin
[LibraryClasses.common.DXE_DRIVER]
  ResetSystemLib|MdeModulePkg/Library/DxeResetSystemLib/DxeResetSystemLib.inf
  HashLib|SecurityPkg/Library/HashLibBaseCryptoRouter/HashLibBaseCryptoRouterDxe.inf
  GhcbLib|MsvmPkg/Library/GhcbLib/GhcbLib.inf
  HvHypercallLib|MsvmPkg/Library/HvHypercallLib/HvHypercallLib.inf
  PolicyLib|PolicyServicePkg/Library/DxePolicyLib/DxePolicyLib.inf
##MSChange End
  Tcg2PhysicalPresencePromptLib|MsvmPkg/Library/Tcg2PhysicalPresencePromptLibApprove/Tcg2PhysicalPresencePromptLibApprove.inf   ## MS_CHANGE

#
# Library instance overrides for all DXE Drivers
#
[LibraryClasses.common.DXE_DRIVER, LibraryClasses.common.UEFI_DRIVER, LibraryClasses.common.DXE_RUNTIME_DRIVER]
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf

#
# Library instance overrides for DXE Drivers and Applications
#
[LibraryClasses.common.DXE_DRIVER, LibraryClasses.common.UEFI_DRIVER, LibraryClasses.common.UEFI_APPLICATION]
  AdvancedLoggerLib|AdvLoggerPkg/Library/AdvancedLoggerLib/Dxe/AdvancedLoggerLib.inf

#
# Library instance overrides for just DXE Runtime Drivers
#
[LibraryClasses.common.DXE_RUNTIME_DRIVER]
  AdvancedLoggerLib|AdvLoggerPkg/Library/AdvancedLoggerLib/Runtime/AdvancedLoggerLib.inf
  BiosDeviceLib|MsvmPkg/Library/BiosDeviceLib/BiosDeviceRuntimeLib.inf
  ReportStatusCodeLib|MdePkg/Library/BaseReportStatusCodeLibNull/BaseReportStatusCodeLibNull.inf
  ResetSystemLib|MdeModulePkg/Library/RuntimeResetSystemLib/RuntimeResetSystemLib.inf
  UefiRuntimeLib|MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf



[LibraryClasses.X64]
  MsUiThemeLib|MsGraphicsPkg/Library/MsUiThemeLib/Dxe/MsUiThemeLib.inf

# PERF MODULES START
!if $(PERF_TRACE_ENABLE) == TRUE
[PcdsFixedAtBuild]
    # Sets bits 0, 3 to enable measurement but skip binding supports
    gEfiMdePkgTokenSpaceGuid.PcdPerformanceLibraryPropertyMask|0x9
    # 16M should be enough to fit all the verbose measurements
    gEfiMdeModulePkgTokenSpaceGuid.PcdExtFpdtBootRecordPadSize|0x1000000

[LibraryClasses.common.PEI_CORE, LibraryClasses.common.PEIM]
    PerformanceLib|MdeModulePkg/Library/PeiPerformanceLib/PeiPerformanceLib.inf

[LibraryClasses.common.UEFI_APPLICATION, LibraryClasses.common.DXE_RUNTIME_DRIVER, LibraryClasses.common.DXE_DRIVER, LibraryClasses.common.UEFI_DRIVER]
    PerformanceLib|MdeModulePkg/Library/DxePerformanceLib/DxePerformanceLib.inf

[LibraryClasses.common.DXE_CORE]
    PerformanceLib|MdeModulePkg/Library/DxeCorePerformanceLib/DxeCorePerformanceLib.inf

#  [Components.common]
#      # FBPT Dump App:
#      # Note, this has a dependency on ShellLib, so can only build this if also building the shell
#      PerformancePkg/Application/FbptDump/FbptDump.inf
!else
[LibraryClasses.common]
    PerformanceLib|MdePkg/Library/BasePerformanceLibNull/BasePerformanceLibNull.inf
!endif
# PERF MODULES END

[PcdsFixedAtBuild.common]
  # Advanced Logger Config
  gAdvLoggerPkgTokenSpaceGuid.PcdAdvancedLoggerBase|0xFA000000   # Must be TemporaryRamBase
  gAdvLoggerPkgTokenSpaceGuid.PcdAdvancedLoggerPreMemPages|1     # Size is 4KB
  gAdvLoggerPkgTokenSpaceGuid.PcdAdvancedLoggerPages|1024        # Size is 4MB
!if $(DEBUGLIB_SERIAL) == 1
  gAdvLoggerPkgTokenSpaceGuid.PcdAdvancedLoggerHdwPortDebugPrintErrorLevel|0xFFFFFFFF
!else
  gAdvLoggerPkgTokenSpaceGuid.PcdAdvancedLoggerHdwPortDebugPrintErrorLevel|0x0
!endif

  # Feature Debugger Config
  DebuggerFeaturePkgTokenSpaceGuid.PcdInitialBreakpointTimeoutMs|0
!if $(DEBUGGER_ENABLED) == 1
  DebuggerFeaturePkgTokenSpaceGuid.PcdForceEnableDebugger|TRUE
!endif

  # Synthetic Timer Config
  gMsvmPkgTokenSpaceGuid.PcdSynicTimerSintIndex|0x1
  gMsvmPkgTokenSpaceGuid.PcdSynicTimerTimerIndex|0x0
  gMsvmPkgTokenSpaceGuid.PcdSynicTimerVector|0x40
  gMsvmPkgTokenSpaceGuid.PcdSynicTimerDefaultPeriod|100000

  # Vmbus Config
  gMsvmPkgTokenSpaceGuid.PcdVmbusSintIndex|0x2
  gMsvmPkgTokenSpaceGuid.PcdVmbusSintVector|0x41
  gMsvmPkgTokenSpaceGuid.PcdVmbusVector|0x5

  # BIOS Device
  gMsvmPkgTokenSpaceGuid.PcdBiosBaseAddress|0x28

  # Battery Device
  gMsvmPkgTokenSpaceGuid.PcdBatteryBase|0xFED3F000

  # UART Devices
  gMsvmPkgTokenSpaceGuid.PcdCom1RegisterBase|0x3F8
  gMsvmPkgTokenSpaceGuid.PcdCom1Vector|4
  gMsvmPkgTokenSpaceGuid.PcdCom2RegisterBase|0x2F8
  gMsvmPkgTokenSpaceGuid.PcdCom2Vector|3

  # RTC (clock)
  gMsvmPkgTokenSpaceGuid.PcdRtcRegisterBase|0x70
  gMsvmPkgTokenSpaceGuid.PcdRtcVector|8

  # PMEM (NVDIMM)
  gMsvmPkgTokenSpaceGuid.PcdPmemRegisterBase|0xFED3D000


  # Networking
  gEfiNetworkPkgTokenSpaceGuid.PcdAllowHttpConnections|TRUE

  # Processor Aggregator Device
  gMsvmPkgTokenSpaceGuid.PcdProcIdleBase|0xFED3C000

  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareRevision|0x00100032
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVendor|L"Microsoft"

  #
  # The runtime state of these two Debug PCDs can be modified in the debugger by
  # modifyting EfiBdDebugPrintGlobalMask and EfiBdDebugPrintComponentMask.
  #
!ifdef DEBUG_NOISY
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x804FEF4B
!else
  # This default turns on errors and warnings
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80000002
!endif

# Disable asserts when not building debug
# NOTE: Technically this is a lie, since BdDebugLib doesn't use this. But keep
#       it for parity with AArch64.
!if $(TARGET) == DEBUG
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x47
!else
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x06
!endif

  #
  # See REPORT_STATUS_CODE_PROPERTY_nnnnn in ReportStatusCodeLib.h
  #
  gEfiMdePkgTokenSpaceGuid.PcdReportStatusCodePropertyMask|0x07

  # Prevent reboots due to some memory variables being out of sync, seems
  # to only be relevant when supporting S4 (hibernate)
  # FUTURE: figure out what this is all about
  gEfiMdeModulePkgTokenSpaceGuid.PcdResetOnMemoryTypeInformationChange|FALSE

  # We are only supporting SMBIOS v3.1
  gEfiMdeModulePkgTokenSpaceGuid.PcdSmbiosVersion|0x0301

  # Default OEM ID for ACPI table creation, its length must be 0x6 bytes to follow ACPI specification.
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemId|"VRTUAL"

  # Default OEM Table ID for ACPI table creation, "MICROSFT", as a SIGNATURE_64 value.
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemTableId|0x5446534F5243494D

  # Default OEM Revision for ACPI table creation.
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemRevision|0x00000001

  # Default Creator ID for ACPI table creation, "MSFT", as a SIGNATURE_32 value.
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultCreatorId|0x5446534D

  # Default Creator Revision for ACPI table creation.
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultCreatorRevision|0x00000001

  # Default settings for serial ports
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultBaudRate|115200
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultDataBits|8
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultParity|1
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultStopBits|1

  # Default setting for serial console terminal type is UTF8
  gEfiMdePkgTokenSpaceGuid.PcdDefaultTerminalType|3

  # Override defaults to indicate only US english support
  gEfiMdePkgTokenSpaceGuid.PcdUefiVariableDefaultLangCodes|"engeng"
  gEfiMdePkgTokenSpaceGuid.PcdUefiVariableDefaultPlatformLangCodes|"en;en-US"

  # Configure max supported number of Logical Processorss
  gUefiCpuPkgTokenSpaceGuid.PcdCpuMaxLogicalProcessorNumber|0x00000001

  # Base addresses of memory mapped devices in MMIO space.
  gUefiCpuPkgTokenSpaceGuid.PcdCpuLocalApicBaseAddress|0xFEE00000
  gEfiSecurityPkgTokenSpaceGuid.PcdTpmBaseAddress|0xFED40000
  gPcAtChipsetPkgTokenSpaceGuid.PcdIoApicBaseAddress|0xFEC00000

  # Use 1GB page table entries in DXE page table when possible
  gEfiMdeModulePkgTokenSpaceGuid.PcdUse1GPageTable|TRUE

  # COM port used for feature debugger
  gMsvmPkgTokenSpaceGuid.PcdFeatureDebuggerPortUartBase|0x3F8  #COM1

  # COM port used for advanced logger output
  gPcAtChipsetPkgTokenSpaceGuid.PcdUartIoPortBaseAddress|0x2F8  #COM2

  # Change PcdBootManagerMenuFile to point to the FrontPage application
  gEfiMdeModulePkgTokenSpaceGuid.PcdBootManagerMenuFile|{ 0x8A, 0x70, 0x42, 0x40, 0x2D, 0x0F, 0x23, 0x48, 0xAC, 0x60, 0x0D, 0x77, 0xB3, 0x11, 0x18, 0x89 }

  gEfiMdeModulePkgTokenSpaceGuid.PcdBootManagerInBootOrder|FALSE

  gEfiSecurityPkgTokenSpaceGuid.PcdOptionRomImageVerificationPolicy|0x00000004        # DENY_EXECUTE_ON_SECURITY_VIOLATION
  gEfiSecurityPkgTokenSpaceGuid.PcdRemovableMediaImageVerificationPolicy|0x00000004   # DENY_EXECUTE_ON_SECURITY_VIOLATION
  gEfiSecurityPkgTokenSpaceGuid.PcdFixedMediaImageVerificationPolicy|0x00000004       # DENY_EXECUTE_ON_SECURITY_VIOLATION

  gEfiSecurityPkgTokenSpaceGuid.PcdForceReallocatePcrBanks|FALSE

  # Disable auto power off
  gMsGraphicsPkgTokenSpaceGuid.PcdPowerOffDelay|0xffffffff

  # Enable NVME changes for ASAP devices
  gEfiMdeModulePkgTokenSpaceGuid.PcdSupportAlternativeQueueSize|TRUE

[PcdsFixedAtBuild.X64]
!if $(PERF_TRACE_ENABLE) == TRUE
  # 16M should be enough to fit all the verbose measurements
  gEfiMdeModulePkgTokenSpaceGuid.PcdExtFpdtBootRecordPadSize|0x1000000
!endif

[PcdsFeatureFlag.common]
  gEfiMdeModulePkgTokenSpaceGuid.PcdDxeIplBuildPageTables|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwarePerformanceDataTableS3Support|FALSE
  gEfiMdeModulePkgTokenSpaceGuid.PcdInternalEventServicesEnabled|TRUE

  gAdvLoggerPkgTokenSpaceGuid.PcdAdvancedLoggerFixedInRAM|FALSE
  gAdvLoggerPkgTokenSpaceGuid.PcdAdvancedFileLoggerForceEnable|TRUE

[PcdsDynamicDefault]

  gEfiMdeModulePkgTokenSpaceGuid.PcdStatusCodeUseMemory|FALSE
  gEfiMdeModulePkgTokenSpaceGuid.PcdStatusCodeUseSerial|FALSE
  gEfiMdeModulePkgTokenSpaceGuid.PcdPlatformRecoverySupport|FALSE

  gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|0x0

  gEfiNetworkPkgTokenSpaceGuid.PcdDhcp6UidType|4              # 04 = UUID-Based DHCPv6 Unique Identifier (DUID-UUID)

  # UEFI Config information from the BiosDevice
  # UEFI_CONFIG_STRUCTURE_COUNT
  gMsvmPkgTokenSpaceGuid.PcdConfigBlobSize|0x0
  # UEFI_CONFIG_BIOS_INFORMATION
  gMsvmPkgTokenSpaceGuid.PcdLegacyMemoryMap|0x0

  # UEFI_CONFIG_MADT
  gMsvmPkgTokenSpaceGuid.PcdMadtPtr|0x0
  gMsvmPkgTokenSpaceGuid.PcdMadtSize|0x0

  # UEFI_CONFIG_SRAT
  gMsvmPkgTokenSpaceGuid.PcdSratPtr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSratSize|0x0

  # UEFI_CONFIG_SLIT
  gMsvmPkgTokenSpaceGuid.PcdSlitPtr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSlitSize|0x0

  # UEFI_CONFIG_PPTT
  gMsvmPkgTokenSpaceGuid.PcdPpttPtr|0x0
  gMsvmPkgTokenSpaceGuid.PcdPpttSize|0x0

  # UEFI_CONFIG_MCFG
  gMsvmPkgTokenSpaceGuid.PcdMcfgPtr|0x0
  gMsvmPkgTokenSpaceGuid.PcdMcfgSize|0x0

  # UEFI_CONFIG_SSDT
  gMsvmPkgTokenSpaceGuid.PcdSsdtPtr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSsdtSize|0x0

  # UEFI_CONFIG_HMAT
  gMsvmPkgTokenSpaceGuid.PcdHmatPtr|0x0
  gMsvmPkgTokenSpaceGuid.PcdHmatSize|0x0

  # UEFI_CONFIG_MEMORY_MAP
  gMsvmPkgTokenSpaceGuid.PcdMemoryMapPtr|0x0
  gMsvmPkgTokenSpaceGuid.PcdMemoryMapSize|0x0

  # UEFI_CONFIG_ENTROPY
  # Points to the actual entropy array, not the containing config structure
  gMsvmPkgTokenSpaceGuid.PcdEntropyPtr|0x0

  # UEFI_CONFIG_BIOS_GUID
  # Points to the actual GUID, not the containing config structure
  gMsvmPkgTokenSpaceGuid.PcdBiosGuidPtr|0x0

  # UEFI_CONFIG_SMBIOS_SYSTEM_MANUFACTURER
  # Points to a null terminated string of the specified size
  gMsvmPkgTokenSpaceGuid.PcdSmbiosSystemManufacturerStr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosSystemManufacturerSize|0x0

  # UEFI_CONFIG_SMBIOS_SYSTEM_PRODUCT_NAME
  # Points to a null terminated string of the specified size
  gMsvmPkgTokenSpaceGuid.PcdSmbiosSystemProductNameStr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosSystemProductNameSize|0x0

  # UEFI_CONFIG_SMBIOS_SYSTEM_VERSION
  # Points to a null terminated string of the specified size
  gMsvmPkgTokenSpaceGuid.PcdSmbiosSystemVersionStr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosSystemVersionSize|0x0

  # UEFI_CONFIG_SMBIOS_SYSTEM_SERIAL_NUMBER
  # Points to a null terminated string of the specified size
  gMsvmPkgTokenSpaceGuid.PcdSmbiosSystemSerialNumberStr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosSystemSerialNumberSize|0x0

  # UEFI_CONFIG_SMBIOS_SYSTEM_SKU_NUMBER
  # Points to a null terminated string of the specified size
  gMsvmPkgTokenSpaceGuid.PcdSmbiosSystemSKUNumberStr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosSystemSKUNumberSize|0x0

  # UEFI_CONFIG_SMBIOS_SYSTEM_FAMILY
  # Points to a null terminated string of the specified size
  gMsvmPkgTokenSpaceGuid.PcdSmbiosSystemFamilyStr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosSystemFamilySize|0x0

  # UEFI_CONFIG_SMBIOS_BASE_SERIAL_NUMBER
  gMsvmPkgTokenSpaceGuid.PcdSmbiosBaseSerialNumberStr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosBaseSerialNumberSize|0x0

  # UEFI_CONFIG_SMBIOS_CHASSIS_SERIAL_NUMBER
  gMsvmPkgTokenSpaceGuid.PcdSmbiosChassisSerialNumberStr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosChassisSerialNumberSize|0x0

  # UEFI_CONFIG_SMBIOS_CHASSIS_ASSET_TAG
  gMsvmPkgTokenSpaceGuid.PcdSmbiosChassisAssetTagStr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosChassisAssetTagSize|0x0

  # UEFI_CONFIG_SMBIOS_BIOS_LOCK_STRING
  gMsvmPkgTokenSpaceGuid.PcdSmbiosBiosLockStringStr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosBiosLockStringSize|0x0

  # UEFI_CONFIG_SMBIOS_3_1_PROCESSOR_INFORMATION
  # Defaults are set to Unknown unless otherwise noted
  # Processor Type defaults to Central Processor type (CPU)
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorType|0x3
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorID|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorVoltage|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorExternalClock|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorMaxSpeed|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorCurrentSpeed|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorStatus|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorUpgrade|0x1
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorCharacteristics|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorFamily2|0x2

  # UEFI_CONFIG_SMBIOS_SOCKET_DESIGNATION
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorSocketDesignationStr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorSocketDesignationSize|0x0

  # UEFI_CONFIG_SMBIOS_PROCESSOR_MANUFACTURER
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorManufacturerStr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorManufacturerSize|0x0

  # UEFI_CONFIG_SMBIOS_PROCESSOR_VERSION
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorVersionStr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorVersionSize|0x0

  # UEFI_CONFIG_SMBIOS_PROCESSOR_SERIAL_NUMBER
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorSerialNumberStr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorSerialNumberSize|0x0

  # UEFI_CONFIG_SMBIOS_PROCESSOR_ASSET_TAG
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorAssetTagStr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorAssetTagSize|0x0

  # UEFI_CONFIG_SMBIOS_PROCESSOR_PART_NUMBER
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorPartNumberStr|0x0
  gMsvmPkgTokenSpaceGuid.PcdSmbiosProcessorPartNumberSize|0x0

  # UEFI_CONFIG_FLAGS
  gMsvmPkgTokenSpaceGuid.PcdSerialControllersEnabled|FALSE
  gMsvmPkgTokenSpaceGuid.PcdPauseAfterBootFailure|FALSE
  gMsvmPkgTokenSpaceGuid.PcdPxeIpV6|FALSE
  gMsvmPkgTokenSpaceGuid.PcdDebuggerEnabled|FALSE
  gMsvmPkgTokenSpaceGuid.PcdLoadOempTable|FALSE
  gMsvmPkgTokenSpaceGuid.PcdTpmEnabled|FALSE
  gMsvmPkgTokenSpaceGuid.PcdHibernateEnabled|FALSE
  gMsvmPkgTokenSpaceGuid.PcdConsoleMode|0x0
  gMsvmPkgTokenSpaceGuid.PcdMemoryAttributesTableEnabled|FALSE
  gMsvmPkgTokenSpaceGuid.PcdVirtualBatteryEnabled|FALSE
  gMsvmPkgTokenSpaceGuid.PcdSgxMemoryEnabled|FALSE
  gMsvmPkgTokenSpaceGuid.PcdProcIdleEnabled|FALSE
  gMsvmPkgTokenSpaceGuid.PcdIsVmbfsBoot|FALSE
  gMsvmPkgTokenSpaceGuid.PcdDisableFrontpage|FALSE
  gMsvmPkgTokenSpaceGuid.PcdMediaPresentEnabledByDefault|FALSE
  gMsvmPkgTokenSpaceGuid.PcdEnableIMCWhenIsolated|FALSE
  gMsvmPkgTokenSpaceGuid.PcdWatchdogEnabled|FALSE
  gMsvmPkgTokenSpaceGuid.PcdHostEmulatorsWhenHardwareIsolated|FALSE
  gMsvmPkgTokenSpaceGuid.PcdTpmLocalityRegsEnabled|FALSE


  # UEFI_CONFIG_PROCESSOR_INFORMATION
  gMsvmPkgTokenSpaceGuid.PcdProcessorCount|0x0
  gMsvmPkgTokenSpaceGuid.PcdProcessorsPerVirtualSocket|0x0
  gMsvmPkgTokenSpaceGuid.PcdThreadsPerProcessor|0x0

  # UEFI_CONFIG_MMIO_DESCRIPTION
  # Currently only two mmio holes, low gap and high gap but we could
  # do more in the future.
  gMsvmPkgTokenSpaceGuid.PcdLowMmioGapBasePageNumber|0x0
  gMsvmPkgTokenSpaceGuid.PcdLowMmioGapSizeInPages|0x0
  gMsvmPkgTokenSpaceGuid.PcdHighMmioGapBasePageNumber|0x0
  gMsvmPkgTokenSpaceGuid.PcdHighMmioGapSizeInPages|0x0

  # UEFI_CONFIG_ACPI_TABLE
  gMsvmPkgTokenSpaceGuid.PcdAcpiMadtMpMailBoxAddress|0x0
  gMsvmPkgTokenSpaceGuid.PcdAcpiTablePtr|0x0
  gMsvmPkgTokenSpaceGuid.PcdAcpiTableSize|0x0

  # PcdTpm2HashMask
  # This mask is used to indicate which PCRs are intended to be supported by the *platform* (not UEFI software).
  # If a PCR is allocated that isn't in this mask, it will be deallocated by Tcg2Pei.
  # If a PCR is supported in this mask, but isn't supported by the TPM, the mask will be updated by Tcg2Pei.
  # This mask is adjusted for legacy VM versions for compatibility reasons.
  gEfiSecurityPkgTokenSpaceGuid.PcdTpm2HashMask|0x00000007               # HASH_ALG_SHA[384 | 256 | 1]

  # PcdTcg2HashAlgorithmBitmap
  # This bitmap is updated at runtime by HashLibBaseCryptoRouter.
  # It indicates the UEFI at boot with the current FW support for TPM PCR hashing algorithms.
  # For this implementation, we promise no support beyond what is provided by the HashLib instances.
  gEfiSecurityPkgTokenSpaceGuid.PcdTcg2HashAlgorithmBitmap|0x00000000

  # Default TCG2 stack will try to autodect TPM at startup.
  # Fix this to dTPM 2.0 and skip the autodetection.
  gEfiSecurityPkgTokenSpaceGuid.PcdTpmInstanceGuid|{0x5a, 0xf2, 0x6b, 0x28, 0xc3, 0xc2, 0x8c, 0x40, 0xb3, 0xb4, 0x25, 0xe6, 0x75, 0x8b, 0x73, 0x17}

  # As a test disable PCR4 measurements
  # future change should be to have worker process pass config for this value
  #  This should only be used to support upgrades/existing VMs
  gEfiSecurityPkgTokenSpaceGuid.TcgMeasureBootStringsInPcr4|FALSE
  gMsvmPkgTokenSpaceGuid.PcdExcludeFvMainFromMeasurements|TRUE

  ## This PCD defines minimum length(in bytes) of the system preboot TCG event log area(LAML).
  #  For PC Client Implementation spec up to and including 1.2 the minimum log size is 64KB.
  #  Increase to 128KB since Linux is measuring more information causing the 64KB buffer to run out.
  # @Prompt Minimum length(in bytes) of the system preboot TCG event log area(LAML).
  gEfiSecurityPkgTokenSpaceGuid.PcdTcgLogAreaMinLen|0x20000

  # UEFI_CONFIG_NVDIMM_COUNT
  gMsvmPkgTokenSpaceGuid.PcdNvdimmCount|0x0

  # UEFI_CONFIG_VPCI_INSTANCE_FILTER_GUID
  gMsvmPkgTokenSpaceGuid.PcdVpciInstanceFilterGuidPtr|0x0

  # Isolation configuration
  gMsvmPkgTokenSpaceGuid.PcdIsolationArchitecture|0x0
  gMsvmPkgTokenSpaceGuid.PcdIsolationParavisorPresent|FALSE
  gMsvmPkgTokenSpaceGuid.PcdIsolationSharedGpaBoundary|0x0
  gMsvmPkgTokenSpaceGuid.PcdIsolationSharedGpaCanonicalizationBitmask|0x0

  # UEFI_CONFIG_AMD_ASPT
  gMsvmPkgTokenSpaceGuid.PcdAsptPtr|0x0
  gMsvmPkgTokenSpaceGuid.PcdAsptSize|0x0

################################################################################
#
# Components Section - list of all Modules include for this Platform.
#
################################################################################

!include $(SHARED_CRYPTO_PATH)/Driver/Bin/CryptoDriver.inc.dsc

[Components]
  #
  # SEC Phase modules
  #
  MsvmPkg/Sec/SecMain.inf {
    <LibraryClasses>
      NULL|MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf
  }
  UefiCpuPkg/ResetVector/Vtf0/Vtf0.inf

  #
  # PEI Phase modules
  #
  MdeModulePkg/Core/DxeIplPeim/DxeIpl.inf
  MdeModulePkg/Core/Pei/PeiMain.inf
  MdeModulePkg/Universal/ResetSystemPei/ResetSystemPei.inf
  MdeModulePkg/Universal/PCD/Pei/Pcd.inf
  MsvmPkg/PlatformPei/PlatformPei.inf
  MsGraphicsPkg/MsUiTheme/Pei/MsUiThemePpi.inf
  MdeModulePkg/Universal/Acpi/FirmwarePerformanceDataTablePei/FirmwarePerformancePei.inf {
    <LibraryClasses>
      LockBoxLib|MdeModulePkg/Library/SmmLockBoxLib/SmmLockBoxPeiLib.inf
      PciLib|MdePkg/Library/BasePciLibCf8/BasePciLibCf8.inf
      PciCf8Lib|MdePkg/Library/BasePciCf8Lib/BasePciCf8Lib.inf
  }
  SecurityPkg/RandomNumberGenerator/RngPei/RngPei.inf {
    <LibraryClasses>
      RngLib|MdePkg/Library/BaseRngLibNull/BaseRngLibNull.inf
  }

  DebuggerFeaturePkg/DebugConfigPei/DebugConfigPei.inf

  #
  # DXE Phase modules
  #
  MdeModulePkg/Core/Dxe/DxeMain.inf {
    <LibraryClasses>
      CpuExceptionHandlerLib|UefiCpuPkg/Library/CpuExceptionHandlerLib/DxeCpuExceptionHandlerLib.inf
      DebugAgentLib|DebuggerFeaturePkg/Library/DebugAgent/DebugAgentDxe.inf
      DebugTransportLib|MsvmPkg/Library/DebugTransportLibMsvm/DebugTransportLibMsvm.inf
      SerialPortLib|PcAtChipsetPkg/Library/SerialIoLib/SerialIoLib.inf
      TransportLogControlLib|DebuggerFeaturePkg/Library/TransportLogControlLibNull/TransportLogControlLibNull.inf
  }
  MdeModulePkg/Universal/PCD/Dxe/Pcd.inf
  MdeModulePkg/Universal/SecurityStubDxe/SecurityStubDxe.inf {
    <LibraryClasses>
      SecurityManagementLib|MdeModulePkg/Library/DxeSecurityManagementLib/DxeSecurityManagementLib.inf
      NULL|MsvmPkg/Library/DxeImageVerificationLib/DxeImageVerificationLib.inf
      NULL|SecurityPkg/Library/DxeTpm2MeasureBootLib/DxeTpm2MeasureBootLib.inf
  }
  MsvmPkg/CpuDxe/CpuDxe.inf
  MdeModulePkg/Universal/Metronome/Metronome.inf
  MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf
  MdeModulePkg/Universal/SetupBrowserDxe/SetupBrowserDxe.inf

  MdeModulePkg/Universal/BdsDxe/BdsDxe.inf
  MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
  MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf {
    <LibraryClasses>
      CapsuleLib|MdeModulePkg/Library/DxeCapsuleLibNull/DxeCapsuleLibNull.inf
      LockBoxLib|MdeModulePkg/Library/LockBoxNullLib/LockBoxNullLib.inf
  }
  MdeModulePkg/Universal/MonotonicCounterRuntimeDxe/MonotonicCounterRuntimeDxe.inf
  MdeModulePkg/Universal/ResetSystemRuntimeDxe/ResetSystemRuntimeDxe.inf
  MdeModulePkg/Universal/SmbiosDxe/SmbiosDxe.inf
  MdeModulePkg/Universal/Console/ConPlatformDxe/ConPlatformDxe.inf
  MdeModulePkg/Universal/Console/ConSplitterDxe/ConSplitterDxe.inf
  MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf
  FatPkg/EnhancedFatDxe/Fat.inf
  MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf
  MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
  MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
  MdeModulePkg/Universal/Disk/RamDiskDxe/RamDiskDxe.inf
  MdeModulePkg/Universal/Console/GraphicsConsoleDxe/GraphicsConsoleDxe.inf {
    <LibraryClasses>
      UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
      HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
    <PcdsPatchableInModule>
      gEfiMdeModulePkgTokenSpaceGuid.PcdVideoHorizontalResolution|1024
      gEfiMdeModulePkgTokenSpaceGuid.PcdVideoVerticalResolution|768
      gEfiMdeModulePkgTokenSpaceGuid.PcdSetGraphicsConsoleModeOnStart|FALSE
  }
  MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf
  MdeModulePkg/Universal/Acpi/BootGraphicsResourceTableDxe/BootGraphicsResourceTableDxe.inf
  MdeModulePkg/Bus/Scsi/ScsiDiskDxe/ScsiDiskDxe.inf
  MdeModulePkg/Bus/Scsi/ScsiBusDxe/ScsiBusDxe.inf
  MdeModulePkg/Universal/Console/TerminalDxe/TerminalDxe.inf
  MdeModulePkg/Universal/ReportStatusCodeRouter/RuntimeDxe/ReportStatusCodeRouterRuntimeDxe.inf
  MdeModulePkg/Universal/MemoryTest/NullMemoryTestDxe/NullMemoryTestDxe.inf

  MsGraphicsPkg/DisplayEngineDxe/DisplayEngineDxe.inf

  # Security components
  SecurityPkg/Hash2DxeCrypto/Hash2DxeCrypto.inf

  # Networking components

  NetworkPkg/ArpDxe/ArpDxe.inf
  NetworkPkg/Dhcp4Dxe/Dhcp4Dxe.inf
  NetworkPkg/Dhcp6Dxe/Dhcp6Dxe.inf
  NetworkPkg/DnsDxe/DnsDxe.inf
  NetworkPkg/DpcDxe/DpcDxe.inf
  NetworkPkg/HttpDxe/HttpDxe.inf
  NetworkPkg/HttpUtilitiesDxe/HttpUtilitiesDxe.inf
  NetworkPkg/Ip4Dxe/Ip4Dxe.inf
  NetworkPkg/Ip6Dxe/Ip6Dxe.inf
  NetworkPkg/MnpDxe/MnpDxe.inf
  NetworkPkg/Mtftp4Dxe/Mtftp4Dxe.inf
  NetworkPkg/Mtftp6Dxe/Mtftp6Dxe.inf
  NetworkPkg/TcpDxe/TcpDxe.inf
  NetworkPkg/TlsDxe/TlsDxe.inf
  NetworkPkg/Udp4Dxe/Udp4Dxe.inf
  NetworkPkg/Udp6Dxe/Udp6Dxe.inf
  NetworkPkg/UefiPxeBcDxe/UefiPxeBcDxe.inf

  # Hyper-V platform specific components

  MsvmPkg/AcpiPlatformDxe/AcpiPlatformDxe.inf
  MsvmPkg/AcpiTables/AcpiTables.inf
  MsvmPkg/EfiHvDxe/EfiHvDxe.inf
  MsvmPkg/EmclDxe/EmclDxe.inf
  MsvmPkg/EventLogDxe/EventLogDxe.inf
  MsvmPkg/MsvmPcAtRealTimeClockRuntimeDxe/PcatRealTimeClockRuntimeDxe.inf
  MsvmPkg/NetvscDxe/NetvscDxe.inf
  MsvmPkg/NvmExpressDxe/NvmExpressDxe.inf
  MsvmPkg/PlatformDeviceStateHelper/PlatformDeviceStateHelper.inf
  MsvmPkg/SmbiosPlatformDxe/SmbiosPlatformDxe.inf
  MsvmPkg/StorvscDxe/StorvscDxe.inf
  MsvmPkg/SynicTimerDxe/SynicTimerDxe.inf
  MsvmPkg/SynthKeyDxe/SynthKeyDxe.inf
  MsvmPkg/VariableDxe/VariableDxe.inf
  MsvmPkg/VideoDxe/VideoDxe.inf
  MsvmPkg/VmbusDxe/VmbusDxe.inf
  MsvmPkg/VpcivscDxe/VpcivscDxe.inf
  MsvmPkg/WatchdogTimerDxe/WatchdogTimerDxe.inf
  MsvmPkg/SerialDxe/SerialDxe.inf
  MsvmPkg/VmbfsDxe/VmbfsDxe.inf
  MsvmPkg/VmMeasurementDxe/VmMeasurementDxe.inf

  # Advanced logger components
!if $(FILE_LOGGER) == 1
  AdvLoggerPkg/AdvancedFileLogger/AdvancedFileLogger.inf
!endif

  SecurityPkg/RandomNumberGenerator/RngDxe/RngDxe.inf {
    <LibraryClasses>
      RngLib|MsvmPkg/Library/MsvmRngLib/MsvmRngLib.inf
  }

  # TPM related components
  # TODO: Currently the PH is locked by the hypervisor.
  #       If this ever changes, will need a driver to lock the PH.

  SecurityPkg/Tcg/MemoryOverwriteControl/TcgMor.inf

  SecurityPkg/Tcg/Tcg2Dxe/Tcg2Dxe.inf {
    <LibraryClasses>
      Tpm2DeviceLib|MsvmPkg/Library/Tpm2DeviceLib/Tpm2DeviceLib.inf
      HashLib|SecurityPkg/Library/HashLibBaseCryptoRouter/HashLibBaseCryptoRouterDxe.inf
      NULL|SecurityPkg/Library/HashInstanceLibSha384/HashInstanceLibSha384.inf
      NULL|SecurityPkg/Library/HashInstanceLibSha256/HashInstanceLibSha256.inf
      NULL|SecurityPkg/Library/HashInstanceLibSha1/HashInstanceLibSha1.inf
      NULL|MsvmPkg/Library/Tcg2PreInitLib/Tcg2PreInitLibDxe.inf
  }

  SecurityPkg/Tcg/Tcg2Pei/Tcg2Pei.inf {
    <LibraryClasses>
      Tpm2DeviceLib|MsvmPkg/Library/Tpm2DeviceLib/Tpm2DeviceLib.inf
      HashLib|SecurityPkg/Library/HashLibBaseCryptoRouter/HashLibBaseCryptoRouterPei.inf
      NULL|SecurityPkg/Library/HashInstanceLibSha384/HashInstanceLibSha384.inf
      NULL|SecurityPkg/Library/HashInstanceLibSha256/HashInstanceLibSha256.inf
      NULL|SecurityPkg/Library/HashInstanceLibSha1/HashInstanceLibSha1.inf
      NULL|MsvmPkg/Library/Tcg2PreInitLib/Tcg2PreInitLibPei.inf
      #special library For HyperV so that boot doesn't measure Main FV
      NULL|MsvmPkg/Library/ExcludeMainFvFromMeasurementLib/ExcludeMainFvFromMeasurementLib.inf
  }

  # UI Theme Protocol
  MsGraphicsPkg/MsUiTheme/Dxe/MsUiThemeProtocol.inf

  # Simple Window Manager (SWM) driver.
  MsGraphicsPkg/SimpleWindowManagerDxe/SimpleWindowManagerDxe.inf

  # Rendering Engine (SRE) driver.
  MsGraphicsPkg/RenderingEngineDxe/RenderingEngineDxe.inf

  # FrontPage application.
  MsvmPkg/FrontPage/FrontPage.inf

  MdeModulePkg/Universal/Acpi/FirmwarePerformanceDataTableDxe/FirmwarePerformanceDxe.inf {
    <LibraryClasses>
      LockBoxLib|MdeModulePkg/Library/SmmLockBoxLib/SmmLockBoxDxeLib.inf
  }

!if $(PRM_ENABLE) == TRUE
  #
  # PRM Configuration Driver
  #
  PrmPkg/PrmConfigDxe/PrmConfigDxe.inf {
    <LibraryClasses>
      NULL|PrmPkg/Samples/PrmSampleAcpiParameterBufferModule/Library/DxeAcpiParameterBufferModuleConfigLib/DxeAcpiParameterBufferModuleConfigLib.inf
      NULL|PrmPkg/Samples/PrmSampleContextBufferModule/Library/DxeContextBufferModuleConfigLib/DxeContextBufferModuleConfigLib.inf
      NULL|PrmPkg/Samples/PrmSampleHardwareAccessModule/Library/DxeHardwareAccessModuleConfigLib/DxeHardwareAccessModuleConfigLib.inf
  }

  #
  # PRM Module Loader Driver
  #
  PrmPkg/PrmLoaderDxe/PrmLoaderDxe.inf

  #
  # PRM SSDT Installation Driver
  #
  PrmPkg/PrmSsdtInstallDxe/PrmSsdtInstallDxe.inf

  #
  # PRM Sample Modules
  #
  PrmPkg/Samples/PrmSampleAcpiParameterBufferModule/PrmSampleAcpiParameterBufferModule.inf
  PrmPkg/Samples/PrmSampleHardwareAccessModule/PrmSampleHardwareAccessModule.inf
  PrmPkg/Samples/PrmSampleContextBufferModule/PrmSampleContextBufferModule.inf
!endif

[BuildOptions]
  # Generate PDBs on release builds with full debugging, with linker and CC flags
  MSFT:*_*_*_DLINK_FLAGS = /DEBUG:FULL /PDBALTPATH:$(MODULE_NAME).pdb
  MSFT:*_*_*_CC_FLAGS = /Zi

[BuildOptions.common.EDKII.DXE_CORE]
  MSFT:*_*_*_DLINK_FLAGS = /FILEALIGN:4096

[BuildOptions.common.EDKII.SEC, BuildOptions.common.EDKII.PEIM, BuildOptions.common.EDKII.PEI_CORE]
  MSFT:*_*_*_DLINK_FLAGS = /ALIGN:32 /FILEALIGN:32