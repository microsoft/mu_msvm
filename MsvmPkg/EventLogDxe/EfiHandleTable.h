/** @file
  A simple handle table implementation.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once


#define EFI_INVALID_HANDLE (EFI_HANDLE)((UINTN)-1)

//
// Channel handles are divided into two parts, the lower bits
// are reserved for use by the table itself and the remaining bits
// are available for use by callers.
//
#define HANDLE_TABLE_RESERVED_MASK  0x00ffffff


/*++

Routine Description:

    Allocates memory for an object in a handle table or the table itself.

Arguments:

    Size    Number of bytes of memory to allocate.

Return Value:

    Pointer to allocated memory or NULL on error.

--*/
typedef
VOID *
(EFIAPI *HANDLE_MEMORY_ALLOCATE)(
    IN      UINTN           Size
    );


/*++

Routine Description:

    Frees memory previously allocated with HANDLE_MEMORY_ALLOCATE

Arguments:

    None.

Return Value:

    None

--*/
typedef
VOID
(EFIAPI *HANDLE_MEMORY_FREE)(
    );


/*++

Routine Description:

    Handle table object enumeration callback.

Arguments:

    TableHandle         Handle for EFI handle table that the enumerated object belongs to.

    CallbackContext     Context provided to the call to EfiHandleTableEnumerateObjects

    ObjectHandle        Handle representing the object

    Object              Object itself

Return Value:

    EFI_STATUS.
    Enumeration can be stopped by returning a non-success status.

--*/
typedef
EFI_STATUS
(EFIAPI *HANDLE_ENUMERATE_CALLBACK)(
    IN      const EFI_HANDLE                TableHandle,
    IN      VOID                           *CallbackContext,
    IN      EFI_HANDLE                      ObjectHandle,
    IN      VOID                           *Object
    );


//
// Describes information about a handle table
//
typedef struct
{
    HANDLE_MEMORY_ALLOCATE  Allocate;
    HANDLE_MEMORY_FREE      Free;
    UINTN                   ObjectKeySize;
} EFI_HANDLE_TABLE_INFO;


EFI_STATUS
EfiHandleTableInitialize(
    IN          const EFI_HANDLE_TABLE_INFO    *Attributes,
    IN          const UINT32                Size,
    IN          const UINT8                 TableKey,
    OUT         EFI_HANDLE                 *Table
    );


EFI_STATUS
EfiHandleTableAllocateObject(
    IN          EFI_HANDLE                  TableHandle,
    IN          const UINTN                 ObjectSize,
    OUT         VOID                      **Object,
    OUT         EFI_HANDLE                 *Handle
    );


VOID *
EfiHandleTableLookupByKey(
    IN              const EFI_HANDLE            TableHandle,
    IN              const VOID                 *Key,
    IN              const UINT32                KeySize,
    OUT OPTIONAL    EFI_HANDLE                 *Handle
    );


VOID *
EfiHandleTableLookupByHandle(
    IN          const EFI_HANDLE            TableHandle,
    IN          const EFI_HANDLE            Handle
    );


EFI_STATUS
EfiHandleTableEnumerateObjects(
    IN          const EFI_HANDLE            TableHandle,
    IN          const VOID                 *CallbackContext,
    IN          HANDLE_ENUMERATE_CALLBACK   Callback
    );