## @file
#  EFI/Framework Microsoft Virtual Machine Firmware (MSVM) platform
#
#  Copyright (c) Microsoft.
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
  OUTPUT_DIRECTORY              = Build/MsvmAARCH64
  SUPPORTED_ARCHITECTURES       = AARCH64
  BUILD_TARGETS                 = DEBUG|RELEASE
  SKUID_IDENTIFIER              = DEFAULT
  FLASH_DEFINITION              = MsvmPkg/MsvmPkgAARCH64.fdf

#
# Defines for the pre-built BaseCryptLib
#
  PEI_CRYPTO_SERVICES           = TINY_SHA
  DXE_CRYPTO_SERVICES           = STANDARD
  PEI_CRYPTO_ARCH               = AARCH64
  DXE_CRYPTO_ARCH               = AARCH64

################################################################################
#
# BuildOptions Section - extra build flags
#
################################################################################
[BuildOptions]
  *_*_AARCH64_GENFW_FLAGS = --keepexceptiontable
  DEBUG_*_*_CC_FLAGS = -D DEBUG_PLATFORM

# ARM64 has a UEFI spec requirement that RuntimeServiceCode/Data is 64k aligned
[BuildOptions.common.EDKII.DXE_RUNTIME_DRIVER]
  MSFT:*_*_AARCH64_DLINK_FLAGS = /ALIGN:0x10000

[BuildOptions.common.EDKII.SEC, BuildOptions.common.EDKII.PEIM, BuildOptions.common.EDKII.PEI_CORE]
  MSFT:*_*_*_DLINK_FLAGS = /ALIGN:4096 /FILEALIGN:4096

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
  ArmLib|ArmPkg/Library/ArmLib/ArmBaseLib.inf
  ArmMmuLib|ArmPkg/Library/ArmMmuLib/ArmMmuBaseLib.inf
  ArmMonitorLib|ArmPkg/Library/ArmMonitorLib/ArmMonitorLib.inf
  ArmSmcLib|ArmPkg/Library/ArmSmcLib/ArmSmcLib.inf
  ArmTrngLib|MdePkg/Library/BaseArmTrngLibNull/BaseArmTrngLibNull.inf
  AssertLib|AdvLoggerPkg/Library/AssertLib/AssertLib.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  BiosDeviceLib|MsvmPkg/Library/BiosDeviceLib/BiosDeviceLib.inf
  CacheMaintenanceLib|ArmPkg/Library/ArmCacheMaintenanceLib/ArmCacheMaintenanceLib.inf
  CapsuleLib|MdeModulePkg/Library/DxeCapsuleLibNull/DxeCapsuleLibNull.inf
  CrashLib|MsvmPkg/Library/CrashLib/CrashLib.inf
  CpuExceptionHandlerLib|ArmPkg/Library/ArmExceptionLib/ArmExceptionLib.inf
  CpuLib|MdePkg/Library/BaseCpuLib/BaseCpuLib.inf
  DebugLib|AdvLoggerPkg/Library/BaseDebugLibAdvancedLogger/BaseDebugLibAdvancedLogger.inf
  DebugAgentLib|MdeModulePkg/Library/DebugAgentLibNull/DebugAgentLibNull.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  DefaultExceptionHandlerLib|MsvmPkg/Library/DefaultExceptionHandlerLib/DefaultExceptionHandlerLib.inf
  DeviceStateLib|MdeModulePkg/Library/DeviceStateLib/DeviceStateLib.inf
  DisplayDeviceStateLib|MsGraphicsPkg/Library/ColorBarDisplayDeviceStateLib/ColorBarDisplayDeviceStateLib.inf
  EmclLib|MsvmPkg/Library/EmclLib/EmclLib.inf
  FdtLib|EmbeddedPkg/Library/FdtLib/FdtLib.inf
  FltUsedLib|MdePkg/Library/FltUsedLib/FltUsedLib.inf
  FrameBufferBltLib|MdeModulePkg/Library/FrameBufferBltLib/FrameBufferBltLib.inf
  HwResetSystemLib|ArmPkg/Library/ArmPsciResetSystemLib/ArmPsciResetSystemLib.inf
  ImagePropertiesRecordLib|MdeModulePkg/Library/ImagePropertiesRecordLib/ImagePropertiesRecordLib.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsicArmVirt.inf
  IsolationLib|MsvmPkg/Library/IsolationLib/IsolationLib.inf
  MathLib|MsCorePkg/Library/MathLib/MathLib.inf
  MmuLib|ArmPkg/Library/MmuLib/BaseMmuLib.inf
  MmUnblockMemoryLib|MdePkg/Library/MmUnblockMemoryLib/MmUnblockMemoryLibNull.inf
  MsBootPolicyLib|MsvmPkg/Library/MsBootPolicyLib/MsBootPolicyLib.inf
  OrderedCollectionLib|MdePkg/Library/BaseOrderedCollectionRedBlackTreeLib/BaseOrderedCollectionRedBlackTreeLib.inf
  PanicLib|MdePkg/Library/BasePanicLibNull/BasePanicLibNull.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  PeCoffExtraActionLib|MdePkg/Library/BasePeCoffExtraActionLibNull/BasePeCoffExtraActionLibNull.inf
  PeCoffGetEntryPointLib|MdePkg/Library/BasePeCoffGetEntryPointLib/BasePeCoffGetEntryPointLib.inf
  PeCoffLib|MdePkg/Library/BasePeCoffLib/BasePeCoffLib.inf
  PL011UartLib|ArmPlatformPkg/Library/PL011UartLib/PL011UartLib.inf
  PlatformPeiLib|ArmPlatformPkg/PlatformPei/PlatformPeiLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  ResetUtilityLib|MdeModulePkg/Library/ResetUtilityLib/ResetUtilityLib.inf
  SortLib|MdeModulePkg/Library/BaseSortLib/BaseSortLib.inf
  StackCheckFailureHookLib|MdePkg/Library/StackCheckFailureHookLibNull/StackCheckFailureHookLibNull.inf
  StackCheckLib|MdePkg/Library/StackCheckLib/StackCheckLib.inf
  SecurityLockAuditLib|MdeModulePkg/Library/SecurityLockAuditDebugMessageLib/SecurityLockAuditDebugMessageLib.inf ##MSCHANGE
  SerialPortLib|MdePkg/Library/BaseSerialPortLibNull/BaseSerialPortLibNull.inf
  SynchronizationLib|MdePkg/Library/BaseSynchronizationLib/BaseSynchronizationLib.inf
  Tpm2CommandLib|SecurityPkg/Library/Tpm2CommandLib/Tpm2CommandLib.inf
  TimerLib|MsvmPkg/Library/HvTimerLib/HvTimerLib.inf
  UefiBootManagerLib|MdeModulePkg/Library/UefiBootManagerLib/UefiBootManagerLib.inf
  UefiDecompressLib|MdePkg/Library/BaseUefiDecompressLib/BaseUefiDecompressLib.inf
  UefiScsiLib|MdePkg/Library/UefiScsiLib/UefiScsiLib.inf
  UiRectangleLib|MsGraphicsPkg/Library/BaseUiRectangleLib/BaseUiRectangleLib.inf
  VariablePolicyHelperLib|MdeModulePkg/Library/VariablePolicyHelperLib/VariablePolicyHelperLib.inf

!if $(DEBUGLIB_SERIAL) == 1
  SerialPortLib|ArmPlatformPkg/Library/PL011SerialPortLib/PL011SerialPortLib.inf
  PL011UartClockLib|ArmPlatformPkg/Library/PL011UartClockLib/PL011UartClockLib.inf
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

  MsUiThemeLib|MsGraphicsPkg/Library/MsUiThemeLib/Dxe/MsUiThemeLib.inf

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
  ArmMmuLib|ArmPkg/Library/ArmMmuLib/ArmMmuPeiLib.inf
  ExtractGuidedSectionLib|MdePkg/Library/BaseExtractGuidedSectionLib/BaseExtractGuidedSectionLib.inf
  MemoryAllocationLib|MdePkg/Library/PeiMemoryAllocationLib/PeiMemoryAllocationLib.inf
  PeiServicesLib|MdePkg/Library/PeiServicesLib/PeiServicesLib.inf
  PeiServicesTablePointerLib|ArmPkg/Library/PeiServicesTablePointerLib/PeiServicesTablePointerLib.inf
  StackCheckLib|MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf

#
# Library instance overrides just for SEC
#
[LibraryClasses.common.SEC]
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf

#
# Library instance overrides for PEI
#
[LibraryClasses.common.PEI_CORE, LibraryClasses.common.PEIM]
  FrameBufferMemDrawLib|MsGraphicsPkg/Library/FrameBufferMemDrawLib/FrameBufferMemDrawLibPei.inf
  HvHypercallLib|MsvmPkg/Library/HvHypercallLib/PeiHvHypercallLib.inf
  HobLib|MdePkg/Library/PeiHobLib/PeiHobLib.inf
  OemHookStatusCodeLib|MdeModulePkg/Library/OemHookStatusCodeLibNull/OemHookStatusCodeLibNull.inf
  ReportStatusCodeLib|MdeModulePkg/Library/PeiReportStatusCodeLib/PeiReportStatusCodeLib.inf
  PcdLib|MdePkg/Library/PeiPcdLib/PeiPcdLib.inf
  WatchdogTimerLib|MsvmPkg/Library/WatchdogTimerLib/WatchdogTimerLib.inf
  ResetSystemLib|MdeModulePkg/Library/PeiResetSystemLib/PeiResetSystemLib.inf

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
  AdvancedLoggerLib|AdvLoggerPkg/Library/AdvancedLoggerLib/Pei/AdvancedLoggerLib.inf
  DebugLib|AdvLoggerPkg/Library/PeiDebugLibAdvancedLogger/PeiDebugLibAdvancedLogger.inf
  PeimEntryPoint|MdePkg/Library/PeimEntryPoint/PeimEntryPoint.inf
  ResourcePublicationLib|MdePkg/Library/PeiResourcePublicationLib/PeiResourcePublicationLib.inf
  PcdDatabaseLoaderLib|MdeModulePkg/Library/PcdDatabaseLoaderLib/Pei/PcdDatabaseLoaderLibPei.inf  # MU_CHANGE
  RngLib|MdePkg/Library/BaseRngLibNull/BaseRngLibNull.inf

  MsUiThemeLib|MsGraphicsPkg/Library/MsUiThemeLib/Pei/MsUiThemeLib.inf


#
# Library instance overrides for DXE
#
[LibraryClasses.common.DXE_CORE, LibraryClasses.common.DXE_DRIVER, LibraryClasses.common.DXE_RUNTIME_DRIVER, LibraryClasses.common.UEFI_DRIVER, LibraryClasses.common.UEFI_APPLICATION]
  BootEventLogLib|MsvmPkg/Library/BootEventLogLib/BootEventLogLib.inf
  ConfigLib|MsvmPkg/Library/ConfigLib/ConfigLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  DpcLib|NetworkPkg/Library/DxeDpcLib/DxeDpcLib.inf
  DxeMemoryProtectionHobLib|MdeModulePkg/Library/MemoryProtectionHobLibNull/DxeMemoryProtectionHobLibNull.inf
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
  EventLogLib|MsvmPkg/Library/EventLogLib/EventLogLib.inf
  ExtractGuidedSectionLib|MdePkg/Library/DxeExtractGuidedSectionLib/DxeExtractGuidedSectionLib.inf
  FileExplorerLib|MdeModulePkg/Library/FileExplorerLib/FileExplorerLib.inf
  FrameBufferMemDrawLib|MsGraphicsPkg/Library/FrameBufferMemDrawLib/FrameBufferMemDrawLibDxe.inf
  HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  IpIoLib|NetworkPkg/Library/DxeIpIoLib/DxeIpIoLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  MemoryTypeInformationChangeLib|MdeModulePkg/Library/MemoryTypeInformationChangeLibNull/MemoryTypeInformationChangeLibNull.inf
  MmioAllocationLib|MsvmPkg/Library/MmioAllocationLib/MmioAllocationLib.inf
  NetLib|NetworkPkg/Library/DxeNetLib/DxeNetLib.inf
  PcdDatabaseLoaderLib|MdeModulePkg/Library/PcdDatabaseLoaderLib/Dxe/PcdDatabaseLoaderLibDxe.inf  # MU_CHANGE
  PcdLib|MdePkg/Library/DxePcdLib/DxePcdLib.inf
  ReportStatusCodeLib|MdeModulePkg/Library/DxeReportStatusCodeLib/DxeReportStatusCodeLib.inf
  ResetSystemLib|MdeModulePkg/Library/DxeResetSystemLib/DxeResetSystemLib.inf
  RngLib|MsvmPkg/Library/MsvmRngLib/MsvmRngLib.inf
  SynchronizationLib|MdePkg/Library/BaseSynchronizationLib/BaseSynchronizationLib.inf
  UdpIoLib|NetworkPkg/Library/DxeUdpIoLib/DxeUdpIoLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  WatchdogTimerLib|MsvmPkg/Library/WatchdogTimerLib/WatchdogTimerLib.inf

#
# Library instances overrides for just DXE CORE
#
[LibraryClasses.common.DXE_CORE]
  AdvancedLoggerLib|AdvLoggerPkg/Library/AdvancedLoggerLib/DxeCore/AdvancedLoggerLib.inf
  DxeCoreEntryPoint|MdePkg/Library/DxeCoreEntryPoint/DxeCoreEntryPoint.inf
  HobLib|MdePkg/Library/DxeCoreHobLib/DxeCoreHobLib.inf
  MemoryAllocationLib|MdeModulePkg/Library/DxeCoreMemoryAllocationLib/DxeCoreMemoryAllocationLib.inf
  HvHypercallLib|MsvmPkg/Library/HvHypercallLib/HvHypercallLib.inf
##MSChange Begin
[LibraryClasses.common.DXE_DRIVER]
  ResetSystemLib|MdeModulePkg/Library/DxeResetSystemLib/DxeResetSystemLib.inf
  HashLib|SecurityPkg/Library/HashLibBaseCryptoRouter/HashLibBaseCryptoRouterDxe.inf
  PolicyLib|PolicyServicePkg/Library/DxePolicyLib/DxePolicyLib.inf
##MSChange End
  Tcg2PhysicalPresencePromptLib|MsvmPkg/Library/Tcg2PhysicalPresencePromptLibApprove/Tcg2PhysicalPresencePromptLibApprove.inf   ## MS_CHANGE

#
# Library instance overrides for all DXE Drivers
#
[LibraryClasses.common.DXE_DRIVER, LibraryClasses.common.UEFI_DRIVER, LibraryClasses.common.DXE_RUNTIME_DRIVER]
  HvHypercallLib|MsvmPkg/Library/HvHypercallLib/HvHypercallLib.inf
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
  # runtime drivers shouldn't use UEFI debugging, especially after ExitBootServices()
  DebugAgentLib|MdeModulePkg/Library/DebugAgentLibNull/DebugAgentLibNull.inf
  ReportStatusCodeLib|MdePkg/Library/BaseReportStatusCodeLibNull/BaseReportStatusCodeLibNull.inf
  ResetSystemLib|MdeModulePkg/Library/RuntimeResetSystemLib/RuntimeResetSystemLib.inf
  UefiRuntimeLib|MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf

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
  gMsvmPkgTokenSpaceGuid.PcdSynicTimerVector|17             # use PPI for SINTs
  gMsvmPkgTokenSpaceGuid.PcdSynicTimerDefaultPeriod|100000

  # Vmbus Config
  gMsvmPkgTokenSpaceGuid.PcdVmbusSintVector|18              # use PPI for SINTs
  gMsvmPkgTokenSpaceGuid.PcdVmbusSintIndex|0x2
  # PPI for Linux. Older public kernels used by WSL2 had 16 and 17 hardcoded, so
  # use the next available, 18.
  gMsvmPkgTokenSpaceGuid.PcdVmbusVector|18

  # BIOS Device
  gMsvmPkgTokenSpaceGuid.PcdBiosBaseAddress|0xEFFED000

  # Generation Counter Device
  gMsvmPkgTokenSpaceGuid.PcdGenCountEventVector|35          # SPI

  # Battery Device
  gMsvmPkgTokenSpaceGuid.PcdBatteryBase|0xEFFEA000
  gMsvmPkgTokenSpaceGuid.PcdBatteryEventVector|36           # SPI

  # UART Devices
  gMsvmPkgTokenSpaceGuid.PcdCom1RegisterBase|0xEFFEC000
  gMsvmPkgTokenSpaceGuid.PcdCom1Vector|33                   # SPI
  gMsvmPkgTokenSpaceGuid.PcdCom2RegisterBase|0xEFFEB000
  gMsvmPkgTokenSpaceGuid.PcdCom2Vector|34                   # SPI

  # RTC (clock)
  gMsvmPkgTokenSpaceGuid.PcdRtcRegisterBase|0x70
  gMsvmPkgTokenSpaceGuid.PcdRtcVector|8

  # PMEM (NVDIMM)
  gMsvmPkgTokenSpaceGuid.PcdPmemRegisterBase|0xEFFE9000
  gMsvmPkgTokenSpaceGuid.PcdPmemEventVector|37              # SPI

  # Processor Aggregator
  gMsvmPkgTokenSpaceGuid.PcdProcIdleBase|0xEFFE8000
  gMsvmPkgTokenSpaceGuid.PcdProcIdleEventVector|38           # SPI

  # Networking
  gEfiMdePkgTokenSpaceGuid.PcdEnforceSecureRngAlgorithms|FALSE    #Opt out of secure RNG until ARM64 has a way of doing TRNG.

  # GTDT for AArch64. Currently these aren't exposed to guests, and 0 is a valid
  # value to configure. Linux will attempt to configure them, so assign valid
  # interrupt lines.
  gMsvmPkgTokenSpaceGuid.PcdNonSecureEL1TimerGSIV|19
  gMsvmPkgTokenSpaceGuid.PcdVirtualEL1TimerGSIV|20
  gMsvmPkgTokenSpaceGuid.PcdNonSecureEL2TimerGSIV|21

  #
  # Static initial memory config - presumes minimum 64MB in VM
  # Page table, stack, and heap are hard-coded in host worker process.
  # ARM uses 128KB for stack and heap because it uses 4k pages which leads
  # to more entries in the translation table.
  #
  # Firmware:            0x00000000 to 0x00800000 8MB (Pcds from FDF file)
  # PageTable:           0x00800000 to 0x00804000 4KB (starts on 2MB boundary)
  # Stack and Heap:      0x00804000 to 0x00824000 128KB
  # System Memory (PEI): 0x00824000 to 0x04000000 ~55MB
  #
  gMsvmPkgTokenSpaceGuid.PcdSystemMemoryBaseAddress|0x00824000
  gMsvmPkgTokenSpaceGuid.PcdSystemMemorySize|0x037EC000

  #
  # The runtime state of these two Debug PCDs can be modified in the debugger by
  # modifying EfiKdDebugPrintGlobalMask and EfiKdDebugPrintComponentMask.
  #
!ifdef DEBUG_NOISY
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x804FEF4B
!else
  # This default turns on errors and warnings
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80000002
!endif

# Disable asserts when not building debug
!if $(TARGET) == DEBUG
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x47
!else
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x06
!endif

  #
  # See REPORT_STATUS_CODE_PROPERTY_nnnnn in ReportStatusCodeLib.h
  #
  gEfiMdePkgTokenSpaceGuid.PcdReportStatusCodePropertyMask|0x00000007

  # Prevent reboots due to some memory variables being out of sync, seems
  # to only be relevant when supporting S4 (hibernate)
  # FUTURE: figure out what this is all about
  gEfiMdeModulePkgTokenSpaceGuid.PcdResetOnMemoryTypeInformationChange|FALSE

  # Support SMBIOS 3.1
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

  # COM port used for feature debugger
  gMsvmPkgTokenSpaceGuid.PcdFeatureDebuggerPortUartBase|0xEFFEC000  #COM1

  # COM port used for advanced logger output
  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialRegisterBase|0xEFFEB000 #COM2

  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultBaudRate|115200
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultDataBits|8
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultParity|1
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultStopBits|1

  # Default setting for serial console terminal type is UTF8
  gEfiMdePkgTokenSpaceGuid.PcdDefaultTerminalType|3

  # Override defaults to indicate only US english support
  gEfiMdePkgTokenSpaceGuid.PcdUefiVariableDefaultLangCodes|"engeng"
  gEfiMdePkgTokenSpaceGuid.PcdUefiVariableDefaultPlatformLangCodes|"en;en-US"

  # Base addresses of memory mapped devices in MMIO space.
  gEfiSecurityPkgTokenSpaceGuid.PcdTpmBaseAddress|0xFED40000

  # Disable TPM platform hierarchy by default
  gEfiSecurityPkgTokenSpaceGuid.PcdRandomizePlatformHierarchy|FALSE

  # Disable front page auto power off
  gMsGraphicsPkgTokenSpaceGuid.PcdPowerOffDelay|0xffffffff

  # Change PcdBootManagerMenuFile to point to the FrontPage application
  gEfiMdeModulePkgTokenSpaceGuid.PcdBootManagerMenuFile|{ 0x8A, 0x70, 0x42, 0x40, 0x2D, 0x0F, 0x23, 0x48, 0xAC, 0x60, 0x0D, 0x77, 0xB3, 0x11, 0x18, 0x89 }

  gEfiMdeModulePkgTokenSpaceGuid.PcdBootManagerInBootOrder|FALSE

[PcdsFeatureFlag.common]
  gEfiMdeModulePkgTokenSpaceGuid.PcdInternalEventServicesEnabled|TRUE

  gAdvLoggerPkgTokenSpaceGuid.PcdAdvancedLoggerFixedInRAM|FALSE
  gAdvLoggerPkgTokenSpaceGuid.PcdAdvancedFileLoggerForceEnable|TRUE

[PcdsDynamicDefault]
  # GIC related config (legacy Hyper-V values as default)
  gArmTokenSpaceGuid.PcdGicDistributorBase|0xFFFF0000        # aka GICD
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0xEFFEE000     # aka GICR

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

  # UEFI_CONFIG_IORT
  gMsvmPkgTokenSpaceGuid.PcdIortPtr|0x0
  gMsvmPkgTokenSpaceGuid.PcdIortSize|0x0

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
  gMsvmPkgTokenSpaceGuid.PcdCxlMemoryEnabled|FALSE
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
  gMsvmPkgTokenSpaceGuid.PcdAcpiTablePtr|0x0
  gMsvmPkgTokenSpaceGuid.PcdAcpiTableSize|0x0

  # PcdTpm2HashMask
  # This mask is used to indicate which PCRs are intended to be supported by the *platform* (not UEFI software).
  # If a PCR is allocated that isn't in this mask, it will be deallocated by Tcg2Pei.
  # If a PCR is supported in this mask, but isn't supported by the TPM, the mask will be updated by Tcg2Pei.
  gEfiSecurityPkgTokenSpaceGuid.PcdTpm2HashMask|0x00000007               # HASH_ALG_SHA[384 | 256 | 1]

  # PcdTcg2HashAlgorithmBitmap
  # This bitmap is updated at runtime by HashLibBaseCryptoRouter.
  # It indicates the UEFI at boot with the current FW support for TPM PCR hashing algorithms.
  # For this implementation, we promise no support beyond what is provided by the HashLib instances.
  gEfiSecurityPkgTokenSpaceGuid.PcdTcg2HashAlgorithmBitmap|0x00000000

  # Default TCG2 stack will try to autodect TPM at startup.
  # Fix this to dTPM 2.0 and skip the autodetection.
  gEfiSecurityPkgTokenSpaceGuid.PcdTpmInstanceGuid|{0x5a, 0xf2, 0x6b, 0x28, 0xc3, 0xc2, 0x8c, 0x40, 0xb3, 0xb4, 0x25, 0xe6, 0x75, 0x8b, 0x73, 0x17}

  ## This PCD defines minimum length(in bytes) of the system preboot TCG event log area(LAML).
  #  For PC Client Implementation spec up to and including 1.2 the minimum log size is 64KB.
  #  Increase to 128KB since Linux is measuring more information causing the 64KB buffer to run out.
  # @Prompt Minimum length(in bytes) of the system preboot TCG event log area(LAML).
  gEfiSecurityPkgTokenSpaceGuid.PcdTcgLogAreaMinLen|0x20000

  # UEFI_CONFIG_NVDIMM_COUNT
  gMsvmPkgTokenSpaceGuid.PcdNvdimmCount|0x0

  # Isolation configuration
  gMsvmPkgTokenSpaceGuid.PcdIsolationArchitecture|0x0
  gMsvmPkgTokenSpaceGuid.PcdIsolationParavisorPresent|FALSE
  gMsvmPkgTokenSpaceGuid.PcdIsolationSharedGpaBoundary|0x0
  gMsvmPkgTokenSpaceGuid.PcdIsolationSharedGpaCanonicalizationBitmask|0x0

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
  MsvmPkg/Sec/SecMain.inf

  #
  # PEI Phase modules
  #
  ArmPkg/Drivers/CpuPei/CpuPei.inf
  MdeModulePkg/Core/DxeIplPeim/DxeIpl.inf
  MdeModulePkg/Core/Pei/PeiMain.inf
  MdeModulePkg/Universal/ResetSystemPei/ResetSystemPei.inf
  MdeModulePkg/Universal/PCD/Pei/Pcd.inf
  MsvmPkg/PlatformPei/PlatformPei.inf
  MsGraphicsPkg/MsUiTheme/Pei/MsUiThemePpi.inf
  SecurityPkg/RandomNumberGenerator/RngPei/RngPei.inf {
    <LibraryClasses>
      RngLib|MdePkg/Library/BaseRngLibNull/BaseRngLibNull.inf
  }

  DebuggerFeaturePkg/DebugConfigPei/DebugConfigPei.inf

  #
  # DXE Phase modules
  #
  ArmPkg/Drivers/CpuDxe/CpuDxe.inf
  ArmPkg/Drivers/ArmGicDxe/ArmGicDxe.inf
  FatPkg/EnhancedFatDxe/Fat.inf

  MdeModulePkg/Universal/BdsDxe/BdsDxe.inf
  MdeModulePkg/Bus/Scsi/ScsiDiskDxe/ScsiDiskDxe.inf
  MdeModulePkg/Bus/Scsi/ScsiBusDxe/ScsiBusDxe.inf
  MdeModulePkg/Core/Dxe/DxeMain.inf {
    <LibraryClasses>
      NULL|MsCorePkg/Library/DebugPortProtocolInstallLib/DebugPortProtocolInstallLib.inf
      ArmGenericTimerCounterLib|ArmPkg/Library/ArmGenericTimerVirtCounterLib/ArmGenericTimerVirtCounterLib.inf
      DebugTransportLib|MsvmPkg/Library/DebugTransportLibMsvm/DebugTransportLibMsvm.inf
      DebugAgentLib|DebuggerFeaturePkg/Library/DebugAgent/DebugAgentDxe.inf
      PL011UartClockLib|ArmPlatformPkg/Library/PL011UartClockLib/PL011UartClockLib.inf
      SerialPortLib|ArmPlatformPkg/Library/PL011SerialPortLib/PL011SerialPortLib.inf
      TimerLib|ArmPkg/Library/ArmArchTimerLib/ArmArchTimerLib.inf
      TransportLogControlLib|DebuggerFeaturePkg/Library/TransportLogControlLibNull/TransportLogControlLibNull.inf
  }
  MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
  MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf
  MdeModulePkg/Universal/Acpi/BootGraphicsResourceTableDxe/BootGraphicsResourceTableDxe.inf
  MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf {
    <LibraryClasses>
      CapsuleLib|MdeModulePkg/Library/DxeCapsuleLibNull/DxeCapsuleLibNull.inf
      LockBoxLib|MdeModulePkg/Library/LockBoxNullLib/LockBoxNullLib.inf
  }
  MdeModulePkg/Universal/Console/ConPlatformDxe/ConPlatformDxe.inf
  MdeModulePkg/Universal/Console/ConSplitterDxe/ConSplitterDxe.inf
  MdeModulePkg/Universal/Console/GraphicsConsoleDxe/GraphicsConsoleDxe.inf {
    <LibraryClasses>
      UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
      HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
    <PcdsPatchableInModule>
      gEfiMdeModulePkgTokenSpaceGuid.PcdVideoHorizontalResolution|1024
      gEfiMdeModulePkgTokenSpaceGuid.PcdVideoVerticalResolution|768
      gEfiMdeModulePkgTokenSpaceGuid.PcdSetGraphicsConsoleModeOnStart|FALSE
  }
  MdeModulePkg/Universal/Console/TerminalDxe/TerminalDxe.inf
  MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf
  MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
  MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
  MdeModulePkg/Universal/Disk/RamDiskDxe/RamDiskDxe.inf
  MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf
  MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf
  MdeModulePkg/Universal/Metronome/Metronome.inf
  MdeModulePkg/Universal/MonotonicCounterRuntimeDxe/MonotonicCounterRuntimeDxe.inf
  MdeModulePkg/Universal/ResetSystemRuntimeDxe/ResetSystemRuntimeDxe.inf
  MdeModulePkg/Universal/PCD/Dxe/Pcd.inf
  MdeModulePkg/Universal/ReportStatusCodeRouter/RuntimeDxe/ReportStatusCodeRouterRuntimeDxe.inf
  MdeModulePkg/Universal/SecurityStubDxe/SecurityStubDxe.inf {
  <LibraryClasses>
    SecurityManagementLib|MdeModulePkg/Library/DxeSecurityManagementLib/DxeSecurityManagementLib.inf
    NULL|MsvmPkg/Library/DxeImageVerificationLib/DxeImageVerificationLib.inf
    NULL|SecurityPkg/Library/DxeTpm2MeasureBootLib/DxeTpm2MeasureBootLib.inf
  }
  MdeModulePkg/Universal/SmbiosDxe/SmbiosDxe.inf
  MdeModulePkg/Universal/SetupBrowserDxe/SetupBrowserDxe.inf

  MsGraphicsPkg/DisplayEngineDxe/DisplayEngineDxe.inf

 # Networking components

  NetworkPkg/ArpDxe/ArpDxe.inf
  NetworkPkg/Dhcp4Dxe/Dhcp4Dxe.inf
  NetworkPkg/Dhcp6Dxe/Dhcp6Dxe.inf
  NetworkPkg/DpcDxe/DpcDxe.inf
  NetworkPkg/Ip4Dxe/Ip4Dxe.inf
  NetworkPkg/Ip6Dxe/Ip6Dxe.inf
  NetworkPkg/MnpDxe/MnpDxe.inf
  NetworkPkg/Mtftp4Dxe/Mtftp4Dxe.inf
  NetworkPkg/Mtftp6Dxe/Mtftp6Dxe.inf
  NetworkPkg/TcpDxe/TcpDxe.inf
  NetworkPkg/Udp4Dxe/Udp4Dxe.inf
  NetworkPkg/Udp6Dxe/Udp6Dxe.inf
  NetworkPkg/UefiPxeBcDxe/UefiPxeBcDxe.inf

  MsvmPkg/AcpiPlatformDxe/AcpiPlatformDxe.inf
  MsvmPkg/AcpiTables/AcpiTables.inf
  MsvmPkg/EfiHvDxe/EfiHvDxe.inf
  MsvmPkg/EmclDxe/EmclDxe.inf
  MsvmPkg/EventLogDxe/EventLogDxe.inf
  MsvmPkg/MsvmPcAtRealTimeClockRuntimeDxe/PcatRealTimeClockRuntimeDxe.inf
  MsvmPkg/NetvscDxe/NetvscDxe.inf
  MsvmPkg/NvmExpressDxe/NvmExpressDxe.inf
  MsvmPkg/PlatformDeviceStateHelper/PlatformDeviceStateHelper.inf
  MsvmPkg/SerialDxe/SerialDxe.inf
  MsvmPkg/SmbiosPlatformDxe/SmbiosPlatformDxe.inf
  MsvmPkg/StorvscDxe/StorvscDxe.inf
  MsvmPkg/SynthKeyDxe/SynthKeyDxe.inf
  MsvmPkg/SynicTimerDxe/SynicTimerDxe.inf
  MsvmPkg/VariableDxe/VariableDxe.inf
  MsvmPkg/VideoDxe/VideoDxe.inf
  MsvmPkg/VmbfsDxe/VmbfsDxe.inf
  MsvmPkg/VmbusDxe/VmbusDxe.inf
  MsvmPkg/VmMeasurementDxe/VmMeasurementDxe.inf
  MsvmPkg/VpcivscDxe/VpcivscDxe.inf
  MsvmPkg/WatchdogTimerDxe/WatchdogTimerDxe.inf

  # Advanced logger components
!if $(FILE_LOGGER) == 1
  AdvLoggerPkg/AdvancedFileLogger/AdvancedFileLogger.inf
!endif

  SecurityPkg/RandomNumberGenerator/RngDxe/RngDxe.inf {
    <LibraryClasses>
      RngLib|MsvmPkg/Library/MsvmRngLib/MsvmRngLib.inf
  }

  # TPM related components

  SecurityPkg/Tcg/MemoryOverwriteControl/TcgMor.inf

  SecurityPkg/Tcg/Tcg2Dxe/Tcg2Dxe.inf {
    <LibraryClasses>
      Tpm2DeviceLib|MsvmPkg/Library/Tpm2DeviceLib/Tpm2DeviceLib.inf
      HashLib|SecurityPkg/Library/HashLibBaseCryptoRouter/HashLibBaseCryptoRouterDxe.inf
      NULL|SecurityPkg/Library/HashInstanceLibSha256/HashInstanceLibSha256.inf
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
  }

  SecurityPkg/Tcg/Tcg2PlatformDxe/Tcg2PlatformDxe.inf {
    <LibraryClasses>
      Tpm2DeviceLib|MsvmPkg/Library/Tpm2DeviceLib/Tpm2DeviceLib.inf
      TpmPlatformHierarchyLib|SecurityPkg/Library/PeiDxeTpmPlatformHierarchyLib/PeiDxeTpmPlatformHierarchyLib.inf
      NULL|MsvmPkg/Library/Tcg2PreInitLib/Tcg2PreInitLibDxe.inf
  }

!if $(LEGACY_DEBUGGER) == 1
  MsKdDebugPkg2/KdDxe/KdDxe.inf {
    <LibraryClasses>
      PL011UartClockLib|ArmPlatformPkg/Library/PL011UartClockLib/PL011UartClockLib.inf
      SerialPortLib|MsvmPkg/Library/KdPL011SerialPortLib/KdPL011SerialPortLib.inf
      SourceDebugEnabledLib|MsvmPkg/Library/SourceDebugEnabled/SourceDebugEnabledLib.inf
  }
!endif

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
  PrmPkg/Samples/PrmSampleContextBufferModule/PrmSampleContextBufferModule.inf
!endif

[BuildOptions]
  # Generate PDBs on release builds with full debugging, with linker and CC flags
  # Force file alignment to 4K as required on AArch64
  MSFT:*_*_*_DLINK_FLAGS = /FILEALIGN:4096 /DEBUG:FULL /PDBALTPATH:$(MODULE_NAME).pdb
  MSFT:*_*_*_CC_FLAGS = /Zi
