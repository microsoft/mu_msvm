##
## Script to Build Hyper-V UEFI firmware
##
## Copyright (C) Microsoft.
##  SPDX-License-Identifier: BSD-2-Clause-Patent
##
import os, sys, logging, struct, subprocess
from edk2toolext.environment.uefi_build import UefiBuilder
from edk2toolext.invocables.edk2_platform_build import BuildSettingsManager
from edk2toolext.invocables.edk2_setup import SetupSettingsManager
from edk2toolext.invocables.edk2_update import UpdateSettingsManager
from edk2toollib.utility_functions import GetHostInfo
from edk2toolext.invocables.edk2_setup import RequiredSubmodule

#
#==========================================================================
# PLATFORM BUILD ENVIRONMENT CONFIGURATION
#
# MODULE_PKG_PATHS = ";".join(os.path.join(WORKSPACE_PATH, pkg_name) for pkg_name in MODULE_PKGS)

#--------------------------------------------------------------------------------------------------------
# Subclass the UEFI builder and add platform specific functionality.
#
class PlatformBuilder(UefiBuilder, UpdateSettingsManager, SetupSettingsManager, BuildSettingsManager):

    def GetWorkspaceRoot(self):
        return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    def GetActiveScopes(self):
        return ['hyperv', 'edk2-build']

    def GetPackagesSupported(self):
        return ("MsvmPkg", )

    def GetRequiredSubmodules(self):
        return [
            RequiredSubmodule("MU_BASECORE"),
            RequiredSubmodule("Common/MU"),
            RequiredSubmodule("Feature/DEBUGGER"),
            RequiredSubmodule("Common/PATINA_EDK2"),
        ]

    def GetPackagesPath(self):
        pp = ('MU_BASECORE', 'Common/MU', 'Feature/DEBUGGER', 'Common/PATINA_EDK2')
        ws = self.GetWorkspaceRoot()
        return [os.path.join(ws, x) for x in pp]

    def GetArchitecturesSupported(self):
        return ("AARCH64", "X64")

    def GetTargetsSupported(self):
        return ("DEBUG", "RELEASE")

    def SetPlatformEnv(self):
        logging.debug("PlatformBuilder SetPlatformEnv")

        self.env.SetValue("PRODUCT_NAME", "Hyper-V", "Platform Hardcoded")
        self.env.SetValue("TOOL_CHAIN_TAG", "VS2022", "Platform hardcoded")
        self.env.SetValue("BLD_*_BUILD_UNIT_TESTS", "FALSE", "Unit Test build off by default")
        self.env.SetValue("BLD_*_BUILD_APPS", "FALSE", "App Build off by default")
        self.env.SetValue("PE_VALIDATION_PATH", self.edk2path.GetAbsolutePathOnThisSystemFromEdk2RelativePath("MsvmPkg", "image_validation.cfg"), "Image validation ignore list")
        self.env.SetValue("BLD_*_USE_LEGACY_C_CORE", "TRUE", "For now default to the C core")

        #
        # Build AARCH64 by using BUILD_ARCH=AARCH64 with PlatformBuild.py
        #
        if self.env.GetValue("BUILD_ARCH") == "AARCH64":
            logging.debug("PlatformBuilder building AARCH64")
            self.env.SetValue("ACTIVE_PLATFORM", "MsvmPkg/MsvmPkgAARCH64.dsc", "Platform Hardcoded")
            self.env.SetValue("TARGET_ARCH", "AARCH64", "Platform Hardcoded")
            self.env.SetValue("ARCH", "AARCH64", "Platform hardcoded")
        else:
            logging.debug("PlatformBuilder building X64")
            self.env.SetValue("ACTIVE_PLATFORM", "MsvmPkg/MsvmPkgX64.dsc", "Platform Hardcoded")
            self.env.SetValue("TARGET_ARCH", "X64", "Platform Hardcoded")
            self.env.SetValue("ARCH", "X64", "Platform hardcoded")

        #self.env.SetValue("BLD_*_BUILDID", "72932128", "hardcoded for easy build file")
        self.env.SetValue("BLD_*_BUILDID_STRING", "17.1590.800", "hardcoded for easy build file")    # hack to make FdReport

        self.env.SetValue("LaunchBuildLogProgram", "Notepad", "default - will fail if already set", True)
        self.env.SetValue("LaunchLogOnSuccess", "True", "default - will fail if already set", True)
        self.env.SetValue("LaunchLogOnError", "True", "default - will fail if already set", False)

        return 0

    def SetPlatformEnvAfterTarget(self):
        logging.debug("PlatformBuilder SetPlatformEnvAfterTarget")
        return 0

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
    # Human-curated firmware/VMM compatibility contract version. Keep in sync
    # with MSVM_FIRMWARE_INTERFACE_VERSION_MAJOR/_MINOR in MsvmFirmwareVersion.h.
    FW_VERSION_INTERFACE_MAJOR = 1
    FW_VERSION_INTERFACE_MINOR = 0
    FW_VERSION_BASE_VERSION_SIZE = 16
    FW_VERSION_GIT_COMMIT_SIZE = 48
    FW_VERSION_STRUCT_FORMAT = "<IHHIHH16s48s"
    # Path of the generated blob, relative to the per-build output directory
    # (<OUTPUT_DIRECTORY>/<TARGET>_<TOOL_CHAIN_TAG>). The FDF references the same
    # location via $(OUTPUT_DIRECTORY)/$(TARGET)_$(TOOL_CHAIN_TAG).
    FW_VERSION_BLOB_SUBPATH = os.path.join("FwVersion", "FwVersionBlob.bin")

    def _GetOutputDirectory(self, workspace, arch):
        # The build output root is the DSC [Defines] OUTPUT_DIRECTORY value (it is
        # not necessarily "Build"). Read it from the active DSC so this stays in
        # sync with what GenFds uses; fall back to the BaseTools default of
        # Build/<PlatformName> when it cannot be determined.
        active_platform = self.env.GetValue("ACTIVE_PLATFORM")
        if active_platform:
            dsc_path = os.path.join(workspace, active_platform)
            try:
                with open(dsc_path, "r") as dsc:
                    for line in dsc:
                        stripped = line.strip()
                        if stripped.startswith("OUTPUT_DIRECTORY") and "=" in stripped:
                            return stripped.split("=", 1)[1].strip()
            except OSError as e:
                logging.warning(f"Could not read OUTPUT_DIRECTORY from {dsc_path}: {e}")
        return os.path.join("Build", f"Msvm{arch}")

    def _GetGitCommit(self, workspace):
        def _git(args):
            return subprocess.run(
                ["git"] + args,
                cwd=workspace,
                capture_output=True,
                text=True,
                check=True,
            ).stdout.strip()

        try:
            commit = _git(["rev-parse", "HEAD"])
            # Only tracked, uncommitted changes mark the build dirty; untracked
            # files (and gitignored build outputs) are intentionally ignored.
            dirty = bool(_git(["status", "--porcelain", "--untracked-files=no"]))
            return commit, dirty
        except Exception as e:
            logging.warning(f"Could not determine git commit for firmware version blob: {e}")
            return "unknown", False

    def _GenerateFirmwareVersionBlob(self):
        workspace = self.GetWorkspaceRoot()

        arch = "AARCH64" if self.env.GetValue("BUILD_ARCH") == "AARCH64" else "X64"
        target = self.env.GetValue("TARGET")
        toolchain = self.env.GetValue("TOOL_CHAIN_TAG")
        if not target or not toolchain:
            logging.error(
                "TARGET/TOOL_CHAIN_TAG not set; cannot place firmware version blob"
            )
            return -1

        # Static major.minor release prefix. The GitHub release workflow owns
        # this value (env BASE_VERSION) and assigns the patch number later, so
        # only the prefix is embedded here.
        base_version = os.environ.get("BASE_VERSION", "26.0")
        commit, dirty = self._GetGitCommit(workspace)
        flags = self.FW_VERSION_FLAG_DIRTY if dirty else 0

        blob = struct.pack(
            self.FW_VERSION_STRUCT_FORMAT,
            self.FW_VERSION_SIGNATURE,
            self.FW_VERSION_STRUCT_VERSION,
            struct.calcsize(self.FW_VERSION_STRUCT_FORMAT),
            flags,
            self.FW_VERSION_INTERFACE_MAJOR,
            self.FW_VERSION_INTERFACE_MINOR,
            base_version.encode("ascii", "replace"),
            commit.encode("ascii", "replace"),
        )

        # Emit into the per-build output directory so the artifact stays out of
        # the source tree. Must match the FDF SECTION RAW path, which is derived
        # from the same OUTPUT_DIRECTORY / TARGET / TOOL_CHAIN_TAG values.
        output_directory = self._GetOutputDirectory(workspace, arch)
        out_dir = os.path.join(workspace, output_directory, f"{target}_{toolchain}")
        out_path = os.path.join(out_dir, self.FW_VERSION_BLOB_SUBPATH)
        os.makedirs(os.path.dirname(out_path), exist_ok=True)
        with open(out_path, "wb") as f:
            f.write(blob)

        logging.info(
            f"Firmware version blob written: base={base_version} commit={commit}"
            f"{' (dirty)' if dirty else ''}"
            f" interface={self.FW_VERSION_INTERFACE_MAJOR}.{self.FW_VERSION_INTERFACE_MINOR}"
            f" -> {out_path}"
        )
        return 0

    def PlatformPreBuild(self):
        logging.debug("PlatformBuilder PlatformPreBuild")
        return self._GenerateFirmwareVersionBlob()

    def PlatformPostBuild(self):
        return 0

    #
    #==========================================================================
    #
    # Smallest 'main' possible. Please don't add unnecessary code.
    if __name__ == "__main__":
        import argparse
        import sys
        from edk2toolext.invocables.edk2_update import Edk2Update
        from edk2toolext.invocables.edk2_setup import Edk2PlatformSetup
        from edk2toolext.invocables.edk2_platform_build import Edk2PlatformBuild
        print("Invoking Stuart")
        print(r"     ) _     _")
        print(r"    ( (^)-~-(^)")
        print(r"__,-.\_( 0 0 )__,-.___")
        print(r"  'W'   \   /   'W'")
        print(r"         >o<")
        SCRIPT_PATH = os.path.relpath(__file__)
        parser = argparse.ArgumentParser(add_help=False)
        parse_group = parser.add_mutually_exclusive_group()
        parse_group.add_argument("--update", "--UPDATE",
                                 action='store_true', help="Invokes stuart_update")
        parse_group.add_argument("--setup", "--SETUP",
                                 action='store_true', help="Invokes stuart_setup")
        args, remaining = parser.parse_known_args()
        new_args = ["stuart", "-c", SCRIPT_PATH]
        new_args = new_args + remaining
        sys.argv = new_args
        if args.setup:
            print("Running stuart_setup -c " + SCRIPT_PATH)
            Edk2PlatformSetup().Invoke()
        elif args.update:
            print("Running stuart_update -c " + SCRIPT_PATH)
            Edk2Update().Invoke()
        else:
            print("Running stuart_build -c " + SCRIPT_PATH)
            Edk2PlatformBuild().Invoke()