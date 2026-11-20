################################################################################
# Rules section.
[Rule.Common.SEC]
  FILE SEC = $(NAMED_GUID) {
    PE32     PE32    Align=4K   Xip=TRUE       $(INF_OUTPUT)/$(MODULE_NAME).efi
    UI       STRING ="$(MODULE_NAME)" Optional
    VERSION  STRING ="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.PEI_CORE]
  FILE PEI_CORE = $(NAMED_GUID) {
    PE32     PE32   Align=Auto   Xip=TRUE       $(INF_OUTPUT)/$(MODULE_NAME).efi
    UI       STRING ="$(MODULE_NAME)" Optional
    VERSION  STRING ="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.PEIM]
  FILE PEIM = $(NAMED_GUID) {
     PEI_DEPEX PEI_DEPEX Optional        $(INF_OUTPUT)/$(MODULE_NAME).depex
     PE32      PE32   Align=Auto   Xip=TRUE       $(INF_OUTPUT)/$(MODULE_NAME).efi
     UI       STRING="$(MODULE_NAME)" Optional
     VERSION  STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.PEIM.BINARY]
  FILE PEIM = $(NAMED_GUID) {
    PEI_DEPEX PEI_DEPEX Optional      |.depex
    PE32      PE32     Align = Auto   |.efi
    UI        STRING="$(MODULE_NAME)" Optional
    VERSION   STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.DXE_CORE]
  FILE DXE_CORE = $(NAMED_GUID) {
    PE32     PE32           $(INF_OUTPUT)/$(MODULE_NAME).efi
    UI       STRING="$(MODULE_NAME)" Optional
    VERSION  STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.DXE_DRIVER]
  FILE DRIVER = $(NAMED_GUID) {
    DXE_DEPEX    DXE_DEPEX Optional      $(INF_OUTPUT)/$(MODULE_NAME).depex
    PE32     PE32                    $(INF_OUTPUT)/$(MODULE_NAME).efi
    UI       STRING="$(MODULE_NAME)" Optional
    VERSION  STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
    RAW ACPI  Optional               |.acpi
    RAW ASL   Optional               |.aml
  }

[Rule.Common.DXE_DRIVER.BINARY]
  FILE DRIVER = $(NAMED_GUID) {
    DXE_DEPEX DXE_DEPEX Optional      |.depex
    PE32      PE32                    |.efi
    UI        STRING="$(MODULE_NAME)" Optional
    VERSION   STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.DXE_RUNTIME_DRIVER]
  FILE DRIVER = $(NAMED_GUID) {
    DXE_DEPEX    DXE_DEPEX Optional      $(INF_OUTPUT)/$(MODULE_NAME).depex
    PE32     PE32                    $(INF_OUTPUT)/$(MODULE_NAME).efi
    UI       STRING="$(MODULE_NAME)" Optional
    VERSION  STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.UEFI_DRIVER]
  FILE DRIVER = $(NAMED_GUID) {
    DXE_DEPEX    DXE_DEPEX Optional      $(INF_OUTPUT)/$(MODULE_NAME).depex
    PE32     PE32                    $(INF_OUTPUT)/$(MODULE_NAME).efi
    UI       STRING="$(MODULE_NAME)" Optional
    VERSION  STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.UEFI_DRIVER.BINARY]
  FILE DRIVER = $(NAMED_GUID) {
    DXE_DEPEX DXE_DEPEX Optional      |.depex
    PE32      PE32                    |.efi
    UI        STRING="$(MODULE_NAME)" Optional
    VERSION   STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.UEFI_APPLICATION]
  FILE APPLICATION = $(NAMED_GUID) {
    PE32     PE32                    $(INF_OUTPUT)/$(MODULE_NAME).efi
    UI       STRING="$(MODULE_NAME)" Optional
    VERSION  STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.UEFI_APPLICATION.BINARY]
  FILE APPLICATION = $(NAMED_GUID) {
    PE32      PE32                    |.efi
    UI        STRING="$(MODULE_NAME)" Optional
    VERSION   STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.USER_DEFINED.ACPITABLE]
  FILE FREEFORM = $(NAMED_GUID) {
    RAW ACPI               |.acpi
    RAW ASL                |.aml
  }

[Rule.Common.USER_DEFINED.CSM]
  FILE FREEFORM = $(NAMED_GUID) {
    RAW BIN                |.bin
  }

################################################################################
#
# This FV section describes the MAIN volume contents. It contains SEC, PEI,
# and DXE modules. The volume is force rebased, but only modules whose rules
# specify Xip=TRUE are rebased by BaseTools.
#
[FV.MAIN]
BlockSize          = 0x1000
FvAlignment        = 4K
FvForceRebase      = TRUE
ERASE_POLARITY     = 1
MEMORY_MAPPED      = TRUE
STICKY_WRITE       = TRUE
LOCK_CAP           = TRUE
LOCK_STATUS        = TRUE
WRITE_DISABLED_CAP = TRUE
WRITE_ENABLED_CAP  = TRUE
WRITE_STATUS       = TRUE
WRITE_LOCK_CAP     = TRUE
WRITE_LOCK_STATUS  = TRUE
READ_DISABLED_CAP  = TRUE
READ_ENABLED_CAP   = TRUE
READ_STATUS        = TRUE
READ_LOCK_CAP      = TRUE
READ_LOCK_STATUS   = TRUE

#
# SEC and PEI components
#
INF MsvmPkg/Sec/SecMain.inf
INF ArmPkg/Drivers/CpuPei/CpuPei.inf
INF DebuggerFeaturePkg/DebugConfigPei/DebugConfigPei.inf
INF MdeModulePkg/Universal/PCD/Pei/Pcd.inf
INF MdeModulePkg/Universal/ResetSystemPei/ResetSystemPei.inf
INF MdeModulePkg/Core/DxeIplPeim/DxeIpl.inf
INF MdeModulePkg/Core/Pei/PeiMain.inf
INF MsGraphicsPkg/MsUiTheme/Pei/MsUiThemePpi.inf
INF MsvmPkg/EfiDiagnosticsPei/EfiDiagnosticsPei.inf
INF MsvmPkg/PlatformPei/PlatformPei.inf
INF SecurityPkg/RandomNumberGenerator/RngPei/RngPei.inf
INF SecurityPkg/Tcg/Tcg2Pei/Tcg2Pei.inf

!include $(SHARED_CRYPTO_PATH)/Driver/Bin/CryptoDriver.PEI.inc.fdf

#
# DXE Components
#
!if $(USE_LEGACY_C_CORE) == TRUE
  INF MdeModulePkg/Core/Dxe/DxeMain.inf
!else
  FILE DXE_CORE = 23C9322F-2AF2-476A-BC4C-26BC88266C71 {
    !if $(TARGET) == RELEASE
      SECTION PE32 = $(PATINA_DXE_CORE_PATH)/release/msvm_aarch64_dxe_core.efi
    !else
      SECTION PE32 = $(PATINA_DXE_CORE_PATH)/debug/msvm_aarch64_dxe_core.efi
    !endif
    SECTION UI = "DxeCore"
  }
!endif

!if $(USE_LEGACY_C_CORE) == TRUE
  INF ArmPkg/Drivers/CpuDxe/CpuDxe.inf
  INF ArmPkg/Drivers/ArmGicDxe/ArmGicDxe.inf
!endif

INF ArmPkg/Drivers/TimerDxe/TimerDxe.inf

!if $(FILE_LOGGER) == 1
  INF AdvLoggerPkg/AdvancedFileLogger/AdvancedFileLogger.inf
!endif

INF FatPkg/EnhancedFatDxe/Fat.inf

INF MdeModulePkg/Bus/Pci/NvmExpressDxe/NvmExpressDxe.inf
INF MdeModulePkg/Bus/Scsi/ScsiDiskDxe/ScsiDiskDxe.inf
INF MdeModulePkg/Bus/Scsi/ScsiBusDxe/ScsiBusDxe.inf
INF UefiCpuPkg/CpuIo2Dxe/CpuIo2Dxe.inf
INF MdeModulePkg/Bus/Pci/PciHostBridgeDxe/PciHostBridgeDxe.inf
INF MdeModulePkg/Bus/Pci/PciBusDxe/PciBusDxe.inf
INF MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
INF MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf
INF MdeModulePkg/Universal/Acpi/BootGraphicsResourceTableDxe/BootGraphicsResourceTableDxe.inf
INF MdeModulePkg/Universal/BdsDxe/BdsDxe.inf
INF MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf
INF MdeModulePkg/Universal/Console/ConPlatformDxe/ConPlatformDxe.inf
INF MdeModulePkg/Universal/Console/ConSplitterDxe/ConSplitterDxe.inf
INF MdeModulePkg/Universal/Console/GraphicsConsoleDxe/GraphicsConsoleDxe.inf
INF MdeModulePkg/Universal/Console/TerminalDxe/TerminalDxe.inf
INF MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf
INF MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
INF MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
INF MdeModulePkg/Universal/Disk/RamDiskDxe/RamDiskDxe.inf
INF MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf
INF MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf
INF MdeModulePkg/Universal/Metronome/Metronome.inf
INF MdeModulePkg/Universal/MonotonicCounterRuntimeDxe/MonotonicCounterRuntimeDxe.inf
INF MdeModulePkg/Universal/PCD/Dxe/Pcd.inf
INF MdeModulePkg/Universal/ReportStatusCodeRouter/RuntimeDxe/ReportStatusCodeRouterRuntimeDxe.inf
INF MdeModulePkg/Universal/ResetSystemRuntimeDxe/ResetSystemRuntimeDxe.inf
INF MdeModulePkg/Universal/SecurityStubDxe/SecurityStubDxe.inf
INF MdeModulePkg/Universal/SetupBrowserDxe/SetupBrowserDxe.inf
INF MdeModulePkg/Universal/SmbiosDxe/SmbiosDxe.inf
INF MsGraphicsPkg/DisplayEngineDxe/DisplayEngineDxe.inf

!include $(SHARED_CRYPTO_PATH)/Driver/Bin/CryptoDriver.DXE.inc.fdf

INF MsvmPkg/AcpiPlatformDxe/AcpiPlatformDxe.inf
INF RuleOverride=ACPITABLE MsvmPkg/AcpiTables/AcpiTables.inf
INF MsvmPkg/AziHsmDxe/AziHsmDxe.inf
INF MsvmPkg/EfiHvDxe/EfiHvDxe.inf
INF MsvmPkg/EmclDxe/EmclDxe.inf
INF MsvmPkg/EventLogDxe/EventLogDxe.inf
INF MsvmPkg/MsvmPcAtRealTimeClockRuntimeDxe/PcatRealTimeClockRuntimeDxe.inf
INF MsvmPkg/NetvscDxe/NetvscDxe.inf
INF MsvmPkg/EfiDiagnosticsDxe/EfiDiagnosticsDxe.inf
INF MsvmPkg/IoMmuDxe/IoMmuDxe.inf
INF MsvmPkg/PlatformDeviceStateHelper/PlatformDeviceStateHelper.inf
INF MsvmPkg/SerialDxe/SerialDxe.inf
INF MsvmPkg/SmbiosPlatformDxe/SmbiosPlatformDxe.inf
INF MsvmPkg/StorvscDxe/StorvscDxe.inf
INF MsvmPkg/SynthKeyDxe/SynthKeyDxe.inf
INF MsvmPkg/VariableDxe/VariableDxe.inf
INF MsvmPkg/VideoDxe/VideoDxe.inf
INF MsvmPkg/VmbfsDxe/VmbfsDxe.inf
INF MsvmPkg/VmbusDxe/VmbusDxe.inf
INF MsvmPkg/VmMeasurementDxe/VmMeasurementDxe.inf
INF MsvmPkg/VpcivscDxe/VpcivscDxe.inf
INF MsvmPkg/WatchdogTimerDxe/WatchdogTimerDxe.inf

# VirtIo (virtio-blk over PCI) support, ported from OvmfPkg (VirtioPkg)
INF VirtioPkg/Virtio10Dxe/Virtio10.inf
INF VirtioPkg/VirtioBlkDxe/VirtioBlk.inf

!if $(PRM_ENABLE) == TRUE
INF PrmPkg/PrmConfigDxe/PrmConfigDxe.inf
INF PrmPkg/Samples/PrmSampleAcpiParameterBufferModule/PrmSampleAcpiParameterBufferModule.inf
INF PrmPkg/Samples/PrmSampleContextBufferModule/PrmSampleContextBufferModule.inf
INF PrmPkg/PrmLoaderDxe/PrmLoaderDxe.inf
INF PrmPkg/PrmSsdtInstallDxe/PrmSsdtInstallDxe.inf
!endif

INF SecurityPkg/Hash2DxeCrypto/Hash2DxeCrypto.inf

INF NetworkPkg/ArpDxe/ArpDxe.inf
INF NetworkPkg/Dhcp4Dxe/Dhcp4Dxe.inf
INF NetworkPkg/Dhcp6Dxe/Dhcp6Dxe.inf
INF NetworkPkg/DnsDxe/DnsDxe.inf
INF NetworkPkg/DpcDxe/DpcDxe.inf
INF NetworkPkg/HttpDxe/HttpDxe.inf
INF NetworkPkg/HttpUtilitiesDxe/HttpUtilitiesDxe.inf
INF NetworkPkg/Ip4Dxe/Ip4Dxe.inf
INF NetworkPkg/Ip6Dxe/Ip6Dxe.inf
INF NetworkPkg/MnpDxe/MnpDxe.inf
INF NetworkPkg/Mtftp4Dxe/Mtftp4Dxe.inf
INF NetworkPkg/Mtftp6Dxe/Mtftp6Dxe.inf
INF NetworkPkg/TcpDxe/TcpDxe.inf
INF NetworkPkg/TlsDxe/TlsDxe.inf
INF NetworkPkg/Udp4Dxe/Udp4Dxe.inf
INF NetworkPkg/Udp6Dxe/Udp6Dxe.inf
INF NetworkPkg/UefiPxeBcDxe/UefiPxeBcDxe.inf

INF SecurityPkg/RandomNumberGenerator/RngDxe/RngDxe.inf
INF SecurityPkg/Tcg/MemoryOverwriteControl/TcgMor.inf
INF SecurityPkg/Tcg/Tcg2Dxe/Tcg2Dxe.inf
INF MsvmPkg/MsvmTcg2PlatformDxe/MsvmTcg2PlatformDxe.inf

# UI Theme Protocol.
INF  MsGraphicsPkg/MsUiTheme/Dxe/MsUiThemeProtocol.inf
# Rendering Engine (SRE) driver.
INF  MsGraphicsPkg/RenderingEngineDxe/RenderingEngineDxe.inf
# Simple Window Manager (SWM) driver.
INF  MsGraphicsPkg/SimpleWindowManagerDxe/SimpleWindowManagerDxe.inf

# FrontPage application.
INF  MsvmPkg/FrontPage/FrontPage.inf

#INF  MdeModulePkg/Universal/Acpi/FirmwarePerformanceDataTableDxe/FirmwarePerformanceDxe.inf

FILE FREEFORM = PCD(gMsvmPkgTokenSpaceGuid.PcdLogoFile) {
    SECTION RAW = $(MSVM_BOOT_LOGO)
    SECTION UI = "Logo"
  }

FILE FREEFORM = PCD(gMsvmPkgTokenSpaceGuid.PcdFrontPageLogoFile) {
    SECTION RAW = $(MSVM_FRONTPAGE_LOGO)
}

FILE FREEFORM = PCD(gMsvmPkgTokenSpaceGuid.PcdBootFailIndicatorFile) {
    SECTION RAW = MsvmPkg/FrontPage/Resources/NoBoot.bmp
}

#
# Firmware version record (git commit + base release version). Generated at
# build time by MsvmPkg/PlatformBuild.py. The host/VMM locates this record by
# scanning the loaded image for the 'MVFW' signature or for this file GUID; see
# MsvmPkg/Include/MsvmFirmwareVersion.h for the layout.
#
FILE FREEFORM = B8F2D3A4-6C7E-4E1B-9A2F-1D3C5E7A9B0D {
    SECTION RAW = $(OUTPUT_DIRECTORY)/$(TARGET)_$(TOOL_CHAIN_TAG)/FwVersion/FwVersionBlob.bin
}
