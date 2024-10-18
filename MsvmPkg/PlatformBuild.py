##
## Script to Build Hyper-V UEFI firmware
##
## Copyright (C) Microsoft.
##  SPDX-License-Identifier: BSD-2-Clause-Patent
##
import os, sys, logging
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
            RequiredSubmodule("Common/MU_TIANO"),
            RequiredSubmodule("Feature/DEBUGGER"),
            RequiredSubmodule("Silicon/ARM/MU_TIANO"),
        ]

    def GetPackagesPath(self):
        pp = ('MU_BASECORE', 'Common/MU', 'Common/MU_TIANO', 'Feature/DEBUGGER', 'Silicon/ARM/MU_TIANO')
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

    def PlatformPostBuild(self):
        return 0


    #------------------------------------------------------------------
    #
    # Method to do stuff pre build.
    # This is part of the build flow.
    # Currently do nothing.
    #
    #------------------------------------------------------------------
    def PlatformPreBuild(self):
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