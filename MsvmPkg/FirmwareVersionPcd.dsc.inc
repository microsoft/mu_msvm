## @file
#  Firmware version record PCD value bindings, shared by MsvmPkgX64.dsc and
#  MsvmPkgAARCH64.dsc so the list lives in one place.
#
#  Binds the firmware version PCDs (declared in MsvmPkg/MsvmPkg.dec) to the
#  build-time MSVM_FW_* macros injected by the FirmwareVersionBlob pre-build
#  plugin, which sources them from MsvmPkg/FirmwareVersion.toml and git state.
#  The macros are supplied by the plugin as "-D MSVM_FW_*=..."; nothing here
#  needs a default because the plugin always runs before the build.
#
#  Copyright (c) Microsoft Corporation.
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

[PcdsFixedAtBuild.common]
  gMsvmPkgTokenSpaceGuid.PcdMsvmFirmwareInterfaceVersionMajor|$(MSVM_FW_INTERFACE_MAJOR)
  gMsvmPkgTokenSpaceGuid.PcdMsvmFirmwareInterfaceVersionMinor|$(MSVM_FW_INTERFACE_MINOR)
  gMsvmPkgTokenSpaceGuid.PcdMsvmFirmwareBaseVersion|"$(MSVM_FW_BASE_VERSION)"
  gMsvmPkgTokenSpaceGuid.PcdMsvmFirmwareGitCommit|"$(MSVM_FW_GIT_COMMIT)"
  gMsvmPkgTokenSpaceGuid.PcdMsvmFirmwareFlags|$(MSVM_FW_FLAGS)
