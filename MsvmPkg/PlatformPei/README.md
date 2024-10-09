# KDNET support

KDNET support comes in the form of a binary that is checked in.  PEI will load
the binary if it is present (from the architecture-specific kduefi.preefi) and
pass the configuration along to DXE for use when the debugger is finally
initialized.

The binary comes from the daily Windows build.  It is "kduefi.dll" in the root
of the bin directory.
