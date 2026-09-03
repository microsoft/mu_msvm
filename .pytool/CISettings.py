## @file
# Minimal Stuart CI settings for MsvmPkg source checks.
#
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

import logging
from pathlib import Path

from edk2toolext.invocables.edk2_ci_build import CiBuildSettingsManager
from edk2toolext.invocables.edk2_setup import RequiredSubmodule, SetupSettingsManager
from edk2toolext.invocables.edk2_update import UpdateSettingsManager

WORKSPACE_ROOT = str(Path(__file__).parent.parent)


class Settings(CiBuildSettingsManager, SetupSettingsManager, UpdateSettingsManager):
    def __init__(self):
        self.ActualPackages = []
        self.ActualTargets = []
        self.ActualArchitectures = []

    def GetPackagesSupported(self):
        return ("MsvmPkg",)

    def GetArchitecturesSupported(self):
        return ("X64", "AARCH64")

    def GetTargetsSupported(self):
        return ("NO-TARGET",)

    def SetPackages(self, requested_packages):
        unsupported = set(requested_packages) - set(self.GetPackagesSupported())
        if unsupported:
            logging.critical("Unsupported packages requested: %s", " ".join(unsupported))
            raise Exception("Unsupported packages requested: " + " ".join(unsupported))
        self.ActualPackages = requested_packages

    def SetArchitectures(self, requested_architectures):
        unsupported = set(requested_architectures) - set(self.GetArchitecturesSupported())
        if unsupported:
            logging.critical("Unsupported architectures requested: %s", " ".join(unsupported))
            raise Exception("Unsupported architectures requested: " + " ".join(unsupported))
        self.ActualArchitectures = requested_architectures

    def SetTargets(self, requested_targets):
        unsupported = set(requested_targets) - set(self.GetTargetsSupported())
        if unsupported:
            logging.critical("Unsupported targets requested: %s", " ".join(unsupported))
            raise Exception("Unsupported targets requested: " + " ".join(unsupported))
        self.ActualTargets = requested_targets

    def GetActiveScopes(self):
        return ("cibuild",)

    def GetDependencies(self):
        return []

    def GetRequiredSubmodules(self):
        return (
            RequiredSubmodule("MU_BASECORE"),
            RequiredSubmodule("Common/MU"),
            RequiredSubmodule("Feature/DEBUGGER"),
            RequiredSubmodule("Common/PATINA_EDK2"),
        )

    def GetPackagesPath(self):
        return (".", "MU_BASECORE", "Common/MU", "Feature/DEBUGGER")

    def GetWorkspaceRoot(self):
        return WORKSPACE_ROOT

    def GetName(self):
        return "MuMsvmSourceChecks"
