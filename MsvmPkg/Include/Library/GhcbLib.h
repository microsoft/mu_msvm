/** @file
  Definitions for functionality available through GHCB calls to the host.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

/*++

Routine Description:

    This routine executes the VMGEXIT instruction.

Arguments:

    None.

Return Value:

    None.

--*/
VOID
_sev_vmgexit(
    VOID
    );


/*++

Routine Description:

    This routine initializes the GHCB on an SNP system.

Arguments:

    None.

Return Value:

    Pointer to the GHCB.

--*/
VOID*
GhcbInitializeGhcb(
    VOID
    );


/*++

Routine Description:

    This routine writes an MSR using the GHCB protocol.

Arguments:

    Ghcb - Supplies a pointer to the GHCB.

    MsrNumber - Supplies the MSR number.

    RegisterValue - Supplies the value to write to the MSR.

Return Value:

    None.

--*/
VOID
GhcbWriteMsr(
    IN  VOID    *Ghcb,
        UINT64  MsrNumber,
        UINT64  RegisterValue
    );


/*++

Routine Description:

    This routine reads an MSR using the GHCB protocol.

Arguments:

    Ghcb - Supplies a pointer to the GHCB.

    MsrNumber - Supplies the MSR number.

    RegisterValue - Supplies a pointer to a variable that will receive the
        value of the MSR.

Return Value:

    None.

--*/
VOID
GhcbReadMsr(
    IN  VOID    *Ghcb,
        UINT64  MsrNumber,
    OUT UINT64  *RegisterValue
    );
