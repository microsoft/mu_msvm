/** @file
  Main SEC phase code.  Transitions to PEI.

  Copyright (c) 2008 - 2011, Intel Corporation. All rights reserved.
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Library/PeimEntryPoint.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/Armlib.h>
#include <Library/DebugAgentLib.h>
#include <Ppi/TemporaryRamSupport.h>


VOID
SetGuestOsId();

//
// The Temporary RAM support PPI data.
//

EFI_STATUS
EFIAPI
TemporaryRamMigration (
    IN  CONST EFI_PEI_SERVICES  **PeiServices,
        EFI_PHYSICAL_ADDRESS   TemporaryMemoryBase,
        EFI_PHYSICAL_ADDRESS   PermanentMemoryBase,
        UINTN                  CopySize
    );


EFI_PEI_TEMPORARY_RAM_SUPPORT_PPI mTemporaryRamSupportPpi =
{
    TemporaryRamMigration
};


EFI_PEI_PPI_DESCRIPTOR mPrivateDispatchTable[] =
{
    {
        (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
        &gEfiTemporaryRamSupportPpiGuid,
        &mTemporaryRamSupportPpi
    },
};


VOID
EFIAPI
SecStartupPhase2(
    IN  VOID    *Context
    )
/*++

Routine Description:

    The second phase of the SEC startup following debugger initialization.

Arguments:

    Context - A pointer the the context passed from the initial SEC startup code.

Return Value:

    None.

--*/
{
    EFI_SEC_PEI_HAND_OFF        *SecCoreData = (EFI_SEC_PEI_HAND_OFF *)Context;
    EFI_PEI_CORE_ENTRY_POINT    PeiCoreEntryPoint;

    DEBUG((DEBUG_VERBOSE,
           ">>> SecStartupPhase2 @ %p (%p)\n",
           SecStartupPhase2,
           Context));

    DEBUG((DEBUG_VERBOSE,
           "--- SecStartupPhase2: SecCoreData->BootFirmwareVolumeBase %p\n",
           SecCoreData->BootFirmwareVolumeBase));

    //
    // The PEI Core entry point is stored in the second entry of the FV reset vector.
    //
    PeiCoreEntryPoint =
        (EFI_PEI_CORE_ENTRY_POINT)*((UINT64*)((UINTN)(SecCoreData->BootFirmwareVolumeBase) + 8));


    DEBUG((DEBUG_VERBOSE,
           "<<< SecStartupPhase2: Calling PeiCoreEntryPoint %p\n",
           PeiCoreEntryPoint));
    //
    // Transfer the control to the PEI core.
    //
    // This passes a pointer to the TemporaryRamMigration function as a PPI.
    //
    (*PeiCoreEntryPoint)(SecCoreData, (EFI_PEI_PPI_DESCRIPTOR *)&mPrivateDispatchTable);

    //
    // If we get here then the PEI Core returned, which is not recoverable.
    //
    ASSERT(FALSE);
    CpuDeadLoop();
}


VOID
EFIAPI
SecStartupWithStack (
    IN  EFI_FIRMWARE_VOLUME_HEADER  *BootFv,
    IN  VOID                        *TopOfCurrentStack
    )
/*++

Routine Description:

    'C' Entrypoint in the SEC phase code.
    Called from the startup assembly code after establishing the stack.

Arguments:

    BootFv - A pointer to the boot firmware volume.

    TopOfCurrentStack - the top of the current stack

Return Value:

    None.

--*/
{
    EFI_SEC_PEI_HAND_OFF    SecCoreData;

    // Assume running putty so first "reset terminal" and "clear scrollback".
    static const CHAR8 sequence[] = {0x1B, 'c', 0x1b, '[', '3', 'J', 0};
    DEBUG((DEBUG_VERBOSE, sequence));

    DEBUG((DEBUG_VERBOSE,
           ">>> SecStartupWithStack @ %p (%p, %p)\n",
           SecStartupWithStack,
           BootFv,
           TopOfCurrentStack
           ));

    //
    // Initialize floating point operating environment
    // to be compliant with UEFI spec.
    //
    ArmEnableVFP();

    //
    // |-------------|       <-- TopOfCurrentStack
    // |   Stack     | 64k
    // |-------------|
    // |    Heap     | 64k
    // |-------------|       <-- SecCoreData.TemporaryRamBase
    //

    //
    // Initialize SEC hand-off state
    //
    SecCoreData.DataSize = sizeof(EFI_SEC_PEI_HAND_OFF);

    // TemporaryRam is the stack *and* heap

    SecCoreData.TemporaryRamSize       = SIZE_128KB;
    SecCoreData.TemporaryRamBase       =
        (VOID*)((UINT8 *)TopOfCurrentStack - SecCoreData.TemporaryRamSize);

    SecCoreData.PeiTemporaryRamBase    = SecCoreData.TemporaryRamBase;
    SecCoreData.PeiTemporaryRamSize    = SecCoreData.TemporaryRamSize >> 1;

    SecCoreData.StackBase              =
        (UINT8 *)SecCoreData.TemporaryRamBase + SecCoreData.PeiTemporaryRamSize;
    SecCoreData.StackSize              = SecCoreData.TemporaryRamSize >> 1;

    SecCoreData.BootFirmwareVolumeBase = BootFv;
    SecCoreData.BootFirmwareVolumeSize = (UINTN) BootFv->FvLength;

    //
    // Set the guest OS ID so that hypercalls are possible.
    //
    SetGuestOsId();

    //
    // Initialize Debug Agent to support source level debug in SEC/PEI phases
    //
    InitializeDebugAgent(DEBUG_AGENT_INIT_PREMEM_SEC, &SecCoreData, SecStartupPhase2);
}


EFI_STATUS
EFIAPI
TemporaryRamMigration(
    IN  CONST EFI_PEI_SERVICES  **PeiServices,
        EFI_PHYSICAL_ADDRESS    TemporaryMemoryBase,
        EFI_PHYSICAL_ADDRESS    PermanentMemoryBase,
        UINTN                   CopySize
    )
/*++

Routine Description:

    This function is called from PEI core to move data from temporary RAM
    used in the SEC phase to RAM used by the PEI phase.

Arguments:

    PeiServices - Pointer to the PEI Services Table.

    TemporaryMemoryBase - Source address in temporary memory from which this function
                          will copy the temporary RAM contents.

    PermanentMemoryBase - Destination address in permanent memory into which this
                          function will copy the temporary RAM contents.

    CopySize - Amount of memory to migrate from temporary to permanent memory.

Return Value:

    EFI_SUCCESS (always)

--*/
{
    VOID                             *OldHeap;
    VOID                             *NewHeap;
    VOID                             *OldStack;
    VOID                             *NewStack;
    DEBUG_AGENT_CONTEXT_POSTMEM_SEC  DebugAgentContext;
    BOOLEAN                          OldStatus;
    BASE_LIBRARY_JUMP_BUFFER         JumpBuffer;

    DEBUG((DEBUG_VERBOSE, ">>> TemporaryRamMigration@0x%x(0x%x, 0x%x, 0x%x)\n",
         (UINT32)(UINTN)TemporaryRamMigration,
         (UINT32)(UINTN)TemporaryMemoryBase,
         (UINT32)(UINTN)PermanentMemoryBase,
         CopySize));

    OldHeap = (VOID*)(UINTN)TemporaryMemoryBase;
    NewHeap = (VOID*)((UINTN)PermanentMemoryBase + (CopySize >> 1));

    OldStack = (VOID*)((UINTN)TemporaryMemoryBase + (CopySize >> 1));
    NewStack = (VOID*)(UINTN)PermanentMemoryBase;

    DebugAgentContext.HeapMigrateOffset = (UINTN)NewHeap - (UINTN)OldHeap;
    DebugAgentContext.StackMigrateOffset = (UINTN)NewStack - (UINTN)OldStack;

    OldStatus = SaveAndSetDebugTimerInterrupt(FALSE);
    InitializeDebugAgent(DEBUG_AGENT_INIT_POSTMEM_SEC, (VOID *) &DebugAgentContext, NULL);

    //
    // Migrate Heap
    //
    CopyMem(NewHeap, OldHeap, CopySize >> 1);

    //
    // Migrate Stack
    //
    CopyMem(NewStack, OldStack, CopySize >> 1);

    //
    // Use SetJump()/LongJump() to switch to a new stack.
    //
    if (SetJump (&JumpBuffer) == 0)
    {
        JumpBuffer.IP0 = JumpBuffer.IP0 + DebugAgentContext.StackMigrateOffset;
        LongJump(&JumpBuffer, (UINTN)-1);
    }

    SaveAndSetDebugTimerInterrupt(OldStatus);

    DEBUG((DEBUG_VERBOSE,
           "<<< TemporaryRamMigration(0x%x, 0x%x, 0x%x)\n",
           (UINTN)TemporaryMemoryBase,
           (UINTN)PermanentMemoryBase,
           CopySize));

    return EFI_SUCCESS;
}
