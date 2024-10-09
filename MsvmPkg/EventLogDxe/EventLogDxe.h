/** @file
  Internal include file the Event Log Runtime DXE Driver.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BootEventLogLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/EfiHv.h>
#include <Protocol/EventLog.h>
