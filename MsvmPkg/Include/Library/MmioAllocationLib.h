/** @file
  MMIO allocation library declarations

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

#include <Base.h>

//
// \brief      Allocate Mmio space from one of the mmio gaps, either high or low.
//
// \param[in]  NumberOfPages  The number of pages to allocate from the mmio gap
//
// \return     A pointer to the region of mmio space, or NULL on failure.
//
VOID*
AllocateMmioPages(UINT64 NumberOfPages);