/** @file
  Macros, etc.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/


#define FIELD_SIZE(TYPE, Field) (sizeof(((TYPE *)0)->Field))

#define SIZEOF_THROUGH_FIELD(TYPE, Field) \
    (OFFSET_OF(TYPE, Field) + FIELD_SIZE(TYPE, Field))

//
//  CONTAINS_FIELD usage:
//
//      if (CONTAINS_FIELD(pBlock, pBlock->cbSize, dwMumble)) { // safe to use pBlock->dwMumble
//
#define CONTAINS_FIELD(Struct, Size, Field) \
    ( (((INT8*)(&(Struct)->Field)) + sizeof((Struct)->Field)) <= (((INT8*)(Struct))+(Size)) )