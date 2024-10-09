/** @file
  This file contains architecture specific functions for debugging a failure.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

#define MSVM_PKG_CRASH_ID 1

//
// Value present in RAX upon triple fault
// signals that this is a guest requested memory dump
// 32 bit modes will split the signature across eax & edx
// with the high DWORD in eax and the low DWORD in edx.
//
#define GUESTDUMP_TRIPLEFAULT_SIGNATURE 0x504d445453455547  // "GUESTDMP"
#define GUESTDUMP_TRIPLEFAULT_SIGNATURE_LOW_DWORD 0x53455547    // put in edx
#define GUESTDUMP_TRIPLEFAULT_SIGNATURE_HIGH_DWORD 0x504d4454   // put in eax
