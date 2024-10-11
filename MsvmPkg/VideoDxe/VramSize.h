/** @file
  VRAM size definitions.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

#define DEFAULT_VRAM_SIZE_WIN7 (4 * 1024 * 1024)

//
// In Win8 the synthetic video device upgraded the color depth capabilty from
// 16 to 32 bits per pixel.
//
#define DEFAULT_VRAM_SIZE_WIN8 (2 * DEFAULT_VRAM_SIZE_WIN7)
