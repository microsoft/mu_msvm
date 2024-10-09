/** @file
  Platform BDS library definition. A platform can implement
  instances to support platform-specific behavior.

  Copyright (c) 2008 - 2010, Intel Corporation. All rights reserved.
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#ifndef __MS_PLAT_BDS_LIB_H_
#define __MS_PLAT_BDS_LIB_H_

/**
  Platform Bds initialization. Includes the platform firmware vendor, revision
  and so crc check.

**/
VOID
EFIAPI
PlatformBdsInit (
  VOID
  );

#endif
