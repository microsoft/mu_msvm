/** @file -- DxePhaseVariables.h
Contains definitions for locating and working with the DXE phase
indication variables.

Copyright (c) 2015, Microsoft Corporation. All rights reserved.

**/

#ifndef _DXE_PHASE_VARIABLES_H_
#define _DXE_PHASE_VARIABLES_H_

// GUID Declaration
extern EFI_GUID   gMsDxePhaseVariablesGuid;

typedef BOOLEAN   PHASE_INDICATOR;

// NOTE: Variable Attributes
// When locating one of the phase indication variables, the attributes should be compared.
// If the attributes don't match, that's a security error and the indicator cannot be trusted.

#define DXE_PHASE_INDICATOR_ATTR            (EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS)     // Volatile, BS and RT

// EndOfDxe Indicator
#define END_OF_DXE_INDICATOR_VAR_NAME       L"EndOfDxeSignalled"
#define END_OF_DXE_INDICATOR_VAR_ATTR       DXE_PHASE_INDICATOR_ATTR

// ReadyToBoot Indicator
#define READY_TO_BOOT_INDICATOR_VAR_NAME    L"ReadyToBootSignalled"
#define READY_TO_BOOT_INDICATOR_VAR_ATTR    DXE_PHASE_INDICATOR_ATTR

#endif // _DXE_PHASE_VARIABLES_H_
