/** @file
  The Crash Dump Agent Library provide crash dump generation in the event of an
  unrecoverable error.

**/
#ifndef __CRASH_DUMP_AGENT_LIB_H__
#define __CRASH_DUMP_AGENT_LIB_H__


/**
  Called to initialize the crash dump agent.

  @param[in] HobList     Pointer to the HOB list.

**/
VOID
EFIAPI
InitializeCrashDumpAgent(
    IN VOID               *HobList
    );


/**
  Called when a fatal error is detected and the system cannot continue.
  It is not expected that this function returns.

  @param  BugCheckCode  Reason for the fatal error.
  @param  Param1        Bugcheck code specific parameter.
  @param  Param2        Bugcheck code specific parameter.
  @param  Param3        Bugcheck code specific parameter.
  @param  Param4        Bugcheck code specific parameter.

**/
VOID
EFIAPI
EfiBugCheck(
    IN  UINT32             BugCheckCode,
    IN  UINTN              Param1,
    IN  UINTN              Param2,
    IN  UINTN              Param3,
    IN  UINTN              Param4
    );

#endif  // __CRASH_DUMP_AGENT_LIB_H__