/** @file
  Just enough for VmBus protocol handling.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once


typedef long NTSTATUS;
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

//
// MessageId: STATUS_REVISION_MISMATCH
//
// MessageText:
//
// Indicates two revision levels are incompatible.
//
#define STATUS_REVISION_MISMATCH         ((NTSTATUS)0xC0000059L)

