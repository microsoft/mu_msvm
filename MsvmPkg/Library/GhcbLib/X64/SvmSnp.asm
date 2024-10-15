/** @file
  Asm implementations of SNP instructions that will become compiler
  intrinsics.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

include macamd64.inc

;*++
;
; VOID
; _sev_vmgexit(
;     VOID
;     );
;
; Routine Description:
;
;   This routine performs an VMGEXIT.
;
; Arguments:
;
;   None.
;
; Return Value:
;
;   None.
;
;--*

        LEAF_ENTRY _sev_vmgexit, _TEXT$00

        db      0f3h                    ; VMGEXIT prefix
        vmmcall
        ret

        LEAF_END _sev_vmgexit, _TEXT$00

        end
