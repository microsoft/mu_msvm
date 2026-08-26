## @file
# Minimal Stuart CI settings for MsvmPkg source checks.
#
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

import logging
import os

from edk2toolext.invocables.edk2_ci_build import CiBuildSettingsManager
from edk2toolext.invocables.edk2_update import UpdateSettingsManager


class Settings(CiBuildSettingsManager, UpdateSettingsManager):
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

    def GetPackagesPath(self):
        workspace = self.GetWorkspaceRoot()
        package_roots = ("", "MU_BASECORE", "Common/MU", "Feature/DEBUGGER")
        return [os.path.join(workspace, path) for path in package_roots]

    def GetWorkspaceRoot(self):
        return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    def GetName(self):
        return "MuMsvmSourceChecks"