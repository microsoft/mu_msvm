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
  DXE_CRYPTO_SERVICES           = ALL
  PEI_CRYPTO_ARCH               = X64
  DXE_CRYPTO_ARCH               = X64

!include MsvmPkg/MsvmPkgX64.dsc.inc

[BuildOptions]
  *_*_*_CC_FLAGS = -D MS_HYP_CHANGE_BEGIN -D MS_HYP_CHANGE -D MS_HYP -D MU_CHANGE
  DEBUG_*_*_CC_FLAGS = -D DEBUG_PLATFORM

[Components]
  MdeModulePkg/Core/Dxe/DxeMain.inf {
    <LibraryClasses>
      CpuExceptionHandlerLib|UefiCpuPkg/Library/CpuExceptionHandlerLib/DxeCpuExceptionHandlerLib.inf
      DebugAgentLib|DebuggerFeaturePkg/Library/DebugAgent/DebugAgentDxe.inf
      DebugTransportLib|MsvmPkg/Library/DebugTransportLibMsvm/DebugTransportLibMsvm.inf
      SerialPortLib|PcAtChipsetPkg/Library/SerialIoLib/SerialIoLib.inf
      TransportLogControlLib|DebuggerFeaturePkg/Library/TransportLogControlLibNull/TransportLogControlLibNull.inf
  }
  MsvmPkg/CpuDxe/CpuDxe.inf
