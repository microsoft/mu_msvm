## @file FirmwareVersionBlob.py
# Pre-build plugin that owns the firmware version data.
#
# It reads the single source of truth (MsvmPkg/FirmwareVersion.toml) plus the
# git state, then does two things from those same values so they can never
# drift:
#
#   1. Generates the machine-readable firmware version record
#      (MSVM_FIRMWARE_VERSION_INFO) embedded as a RAW file in the DXE FV, which
#      the host/VMM scans out of the loaded image ('MVFW' signature / file GUID).
#      See MsvmPkg/Include/MsvmFirmwareVersion.h and the SECTION RAW reference in
#      MsvmPkg/MsvmPkgX64.fdf / MsvmPkgAARCH64.fdf.
#
#   2. Injects the same values as build macros ("BLD_*_MSVM_FW_*") which the DSC
#      binds to FixedAtBuild PCDs, so the firmware itself can read the version at
#      runtime (e.g. to log it early). See the [PcdsFixedAtBuild] overrides in
#      the platform DSC files.
#
# Runs before the platform build (do_pre_build), after stuart has populated the
# build environment, so BUILD_OUTPUT_BASE is guaranteed to be set and the macros
# are set in time for the build command.
#
##
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
##
import logging
import os
import struct
import tomllib

from edk2toolext.environment import repo_resolver
from edk2toolext.environment.plugintypes.uefi_build_plugin import IUefiBuildPlugin


class FirmwareVersionBlob(IUefiBuildPlugin):
    #
    # Layout of MSVM_FIRMWARE_VERSION_INFO (see
    # MsvmPkg/Include/MsvmFirmwareVersion.h). All integers are little-endian;
    # string fields are NUL-terminated ASCII.
    #   UINT32 Signature ('MVFW'), UINT16 StructVersion, UINT16 HeaderSize,
    #   UINT32 Flags, UINT16 InterfaceVersionMajor, UINT16 InterfaceVersionMinor,
    #   CHAR8 BaseVersion[16], CHAR8 GitCommit[48]
    #
    FW_VERSION_SIGNATURE = 0x5746564D       # 'MVFW' little-endian
    FW_VERSION_STRUCT_VERSION = 1
    FW_VERSION_FLAG_DIRTY = 0x1             # MSVM_FIRMWARE_VERSION_FLAG_DIRTY
    FW_VERSION_FLAG_OFFICIAL = 0x2         # MSVM_FIRMWARE_VERSION_FLAG_OFFICIAL
    FW_VERSION_BASE_VERSION_SIZE = 16
    FW_VERSION_GIT_COMMIT_SIZE = 48
    FW_VERSION_STRUCT_FORMAT = "<IHHIHH16s48s"
    # Path of the generated blob, relative to the per-build output directory
    # (BUILD_OUTPUT_BASE). The FDF references the same location via
    # $(OUTPUT_DIRECTORY)/$(TARGET)_$(TOOL_CHAIN_TAG).
    FW_VERSION_BLOB_SUBPATH = os.path.join("FwVersion", "FwVersionBlob.bin")
    # Single source of truth for the version numbers, relative to the workspace.
    FW_VERSION_TOML_SUBPATH = os.path.join("MsvmPkg", "FirmwareVersion.toml")

    def _ReadVersionToml(self, workspace):
        toml_path = os.path.join(workspace, self.FW_VERSION_TOML_SUBPATH)
        with open(toml_path, "rb") as f:
            data = tomllib.load(f)
        major = int(data["interface"]["major"])
        minor = int(data["interface"]["minor"])
        # The CI BASE_VERSION env override wins so the release workflow can own
        # the released value; the TOML provides the in-tree default.
        release = os.environ.get("BASE_VERSION") or str(data["release"]["version"])
        return major, minor, release

    def _GetGitCommit(self, workspace):
        # Use edk2toolext's standard repo state rather than a bespoke check, so
        # "dirty" means the same thing here as everywhere else in the build
        # tooling (repo_resolver reports dirty for modified tracked files or
        # untracked files; gitignored build outputs do not count).
        try:
            details = repo_resolver.repo_details(workspace)
            return details["Head"]["HexSha"], bool(details["Dirty"])
        except Exception as e:
            logging.warning(f"Could not determine git state for firmware version blob: {e}")
            return "unknown", False

    def do_pre_build(self, thebuilder):
        workspace = thebuilder.GetWorkspaceRoot()

        major, minor, base_version = self._ReadVersionToml(workspace)
        commit, dirty = self._GetGitCommit(workspace)
        flags = 0
        if dirty:
            flags |= self.FW_VERSION_FLAG_DIRTY
        # Official builds are marked by the CI pipeline (e.g. building main, not
        # a PR) via the OFFICIAL_BUILD environment variable. Any non-empty value
        # other than "0"/"false" counts as official.
        if os.environ.get("OFFICIAL_BUILD", "").strip().lower() not in ("", "0", "false"):
            flags |= self.FW_VERSION_FLAG_OFFICIAL

        # (1) Expose the values to the firmware as FixedAtBuild PCDs. Setting
        # BLD_*_<NAME> makes stuart pass "-D <NAME>=<value>" to the build, which
        # the DSC binds to the PcdMsvmFirmware* PCDs.
        thebuilder.env.SetValue("BLD_*_MSVM_FW_INTERFACE_MAJOR", str(major), "FirmwareVersion.toml", True)
        thebuilder.env.SetValue("BLD_*_MSVM_FW_INTERFACE_MINOR", str(minor), "FirmwareVersion.toml", True)
        thebuilder.env.SetValue("BLD_*_MSVM_FW_BASE_VERSION", base_version, "FirmwareVersion.toml", True)
        thebuilder.env.SetValue("BLD_*_MSVM_FW_GIT_COMMIT", commit, "git", True)
        thebuilder.env.SetValue("BLD_*_MSVM_FW_FLAGS", str(flags), "git", True)

        # (2) Pack the same values into the host-facing FV record.
        blob = struct.pack(
            self.FW_VERSION_STRUCT_FORMAT,
            self.FW_VERSION_SIGNATURE,
            self.FW_VERSION_STRUCT_VERSION,
            struct.calcsize(self.FW_VERSION_STRUCT_FORMAT),
            flags,
            major,
            minor,
            base_version.encode("ascii", "replace"),
            commit.encode("ascii", "replace"),
        )

        # Emit into the per-build output directory so the artifact stays out of
        # the source tree. stuart computes BUILD_OUTPUT_BASE as
        # <OUTPUT_DIRECTORY>/<TARGET>_<TOOL_CHAIN_TAG>, which matches the FDF
        # SECTION RAW path.
        out_dir = thebuilder.env.GetValue("BUILD_OUTPUT_BASE")
        out_path = os.path.join(out_dir, self.FW_VERSION_BLOB_SUBPATH)
        os.makedirs(os.path.dirname(out_path), exist_ok=True)
        with open(out_path, "wb") as f:
            f.write(blob)

        logging.info(
            f"Firmware version blob written: base={base_version} commit={commit}"
            f"{' (dirty)' if dirty else ''}"
            f"{' (official)' if flags & self.FW_VERSION_FLAG_OFFICIAL else ''}"
            f" interface={major}.{minor}"
            f" -> {out_path}"
        )
        return 0
