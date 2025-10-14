/** @file
  GUID definition for the Unable To Boot Event

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __UNABLE_TO_BOOT_EVENT_H__
#define __UNABLE_TO_BOOT_EVENT_H__

//
// GUID for the Unable To Boot Event
// This event is signaled when DeviceBootManagerUnableToBoot() is called,
// allowing other drivers to be notified when the system cannot find
// bootable devices/options.
//
#define UNABLE_TO_BOOT_EVENT_GUID \
  { \
    0x3c8d08a8, 0x42c5, 0x4c33, { 0xa6, 0xd8, 0x13, 0xf4, 0xda, 0x4a, 0xa6, 0x85 } \
  }

extern EFI_GUID gMsvmUnableToBootEventGuid;

#endif