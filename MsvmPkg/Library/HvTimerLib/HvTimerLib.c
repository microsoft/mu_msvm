/** @file
  This file implements the TimerLib library class using the hypervisor
  reference time counter.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <Base.h>

#include <Hv/HvGuestMsr.h>
#include <Library/BaseLib.h>
#include <Library/TimerLib.h>
#include <Library/IoLib.h>
#include <Library/DebugLib.h>

#if defined(MDE_CPU_AARCH64)
#include <Library/HvHypercallLib.h>
#endif

VOID
EFIAPI
Stall100ns(
    IN UINT64 Time100ns
    )
/*++

Routine Description:

    Stalls the processor for the given amount of time by consulting the
    hypervisor reference time counter.

Arguments:

    Time100ns - The time to stall the processor for, in 100ns units.

Return Value:

    None.

--*/
{
    UINT64 end;
    UINT64 current;

    current = GetPerformanceCounter();
    end = current + Time100ns;
    do
    {

        CpuPause();
        current = GetPerformanceCounter();

    } while (current < end);
}


UINTN
EFIAPI
MicroSecondDelay (
    IN      UINTN                     MicroSeconds
    )
/*++

Routine Description:

    Stalls the processor for the given amount of time.

Arguments:

    MicroSeconds - The time to stall, in microseconds.

Return Value:

    The passed in value.

--*/
{
    Stall100ns(MicroSeconds * 10ui64);
    return MicroSeconds;
}


UINTN
EFIAPI
NanoSecondDelay (
    IN      UINTN                     NanoSeconds
    )
/*++

Routine Description:

    Stalls the processor for the given amount of time.

Arguments:

    MicroSeconds - The time to stall, in nanoseconds.

Return Value:

    The passed in value.

--*/
{
    Stall100ns(NanoSeconds / 100ui64);
    return NanoSeconds;
}


UINT64
EFIAPI
GetPerformanceCounter (
    VOID
    )
/*++

Routine Description:

    Gets the current value of the hypervisor reference time.

Arguments:

    None.

Return Value:

    The current reference time, in 100ns units.

--*/
{
#if defined (MDE_CPU_X64)

    return AsmReadMsr64(HvSyntheticMsrTimeRefCount);

#elif defined(MDE_CPU_AARCH64)

    HV_STATUS status;
    UINT64 RegisterValue;

    status = AsmGetVpRegister64(HvRegisterTimeRefCount, &RegisterValue);
    ASSERT(status == HV_STATUS_SUCCESS);

    return RegisterValue;

#else
#error unsupported architecture
#endif
}


UINT64
EFIAPI
GetPerformanceCounterProperties (
    OUT      UINT64                    *StartValue,  OPTIONAL
    OUT      UINT64                    *EndValue     OPTIONAL
    )
/*++

Routine Description:

    Gets the properties of the hypervisor reference time.

Arguments:

    StartValue - Returns the smallest possible time value.

    EndValue - Returns the largest possible time value.

Return Value:

    Returns the frequency of the timer, in Hz units.

--*/
{
    if (StartValue != NULL)
    {
        *StartValue = 0;
    }

    if (EndValue != NULL)
    {
        *EndValue = (UINT64)-1;
    }

    return 10ui64 * 1000ui64 * 1000ui64;
}


UINT64
EFIAPI
GetTimeInNanoSecond (
    IN      UINT64                     Ticks
    )
/*++

Routine Description:

    Converts ticks in hypervisor reference time units (100ns) to time in
    1ns units.

Arguments:

    Ticks - A time offset in 100ns units.

Return Value:

    A time offset in 1ns units.

--*/
{
    return Ticks * 100;
}

