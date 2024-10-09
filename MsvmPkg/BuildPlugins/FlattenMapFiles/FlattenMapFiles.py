## @file FlattenMapFiles.py
# Plugin to support copying all Map files to 1 directory.
# This makes debugging easier when source level debugging is not available.
#
##
# Copyright (c) Microsoft Corporation. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause-Patent
##
###
from edk2toolext.environment.plugintypes.uefi_build_plugin import IUefiBuildPlugin
import logging
import shutil
import os

class FlattenMapFiles(IUefiBuildPlugin):

    def do_post_build(self, thebuilder):
        #Path to Build output
        BuildPath = thebuilder.env.GetValue("BUILD_OUTPUT_BASE")
        #Path to where the Map files will be stored
        MapFilesPath = os.path.join(BuildPath, "MAP")

        IgnoreMapFiles = []  #make lower case

        try:
            if not os.path.isdir(MapFilesPath):
                os.mkdir(MapFilesPath)
        except:
            logging.critical("Error making Map Files directory")

        logging.critical("Copying Map Files to flat directory")
        for dirpath, dirnames, filenames in os.walk(BuildPath):
            if MapFilesPath in dirpath:
                continue
            for filename in filenames:
                fnl = filename.strip().lower()
                if(fnl.endswith(".map")):
                    if(any(e for e in IgnoreMapFiles if e in fnl)):
                        # too much info. logging.debug("Flatten  - Ignore Map files: %s" % filename)
                        pass
                    else:
                        shutil.copy(os.path.join(dirpath, filename), os.path.join(MapFilesPath, filename))
        return 0