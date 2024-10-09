/** @file
  This module wraps the VMBus packet library C files with definitions
  to allow it to compile in the UEFI environment.

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>

#define PrefetchForWrite(x)

#define ALIGN_UP(x, y) ALIGN_VALUE((x), sizeof(y))

#include "Init.c"
#include "RingBuffer.c"
