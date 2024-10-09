/** @file
  This file contains the structures that are used internally within
  the packet management library.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

#include <Vmbus/VmbusPacketInterface.h>
#include <Vmbus/VmbusPacketFormat.h>

EFI_STATUS
PkpInitRingBufferControl(
    IN OUT PPACKET_LIB_CONTEXT Context
    );
