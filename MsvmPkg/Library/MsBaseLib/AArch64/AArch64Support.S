//
// Based on Silicon/ARM/MU_TIANO/ArmPkg/Library/ArmLib/AArch64/AArch64Support.masm.
//

//------------------------------------------------------------------------------
//
// Copyright (c) 2008 - 2010, Apple Inc. All rights reserved.<BR>
// Copyright (c) 2011 - 2014, ARM Limited. All rights reserved.
//
// This program and the accompanying materials
// are licensed and made available under the terms and conditions of the BSD License
// which accompanies this distribution.  The full text of the license may be found at
// http://opensource.org/licenses/bsd-license.php
//
// THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
// WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
//
//------------------------------------------------------------------------------

#include <AArch64/AArch64.h>
#include <AsmMacroLib.h>

#define CTRL_M_BIT       (1 << 0)
//#define CTRL_A_BIT     (1 << 1)
#define CTRL_C_BIT       (1 << 2)
//#define CTRL_SA_BIT    (1 << 3)
#define CTRL_I_BIT       (1 << 12)
//#define CTRL_V_BIT     (1 << 12)
//#define CPACR_VFP_BITS (3 << 20)

ASM_FUNC(ArmEnableCachesAndMmu)
    EL1_OR_EL2(x1)
1:
    mrs     x0, sctlr_el1        // Get control register EL1
    b       3f
2:
    mrs     x0, sctlr_el2        // Get control register EL2
3:
    mov     x1, #(CTRL_M_BIT | CTRL_C_BIT | CTRL_I_BIT)  // Enable MMU, D & I caches
    orr     x0, x0, x1
    EL1_OR_EL2(x1)
1:
    msr     sctlr_el1, x0        // Write back control register
    b       3f
2:
    msr     sctlr_el2, x0        // Write back control register
3:
    dsb     sy
    isb
    ret
