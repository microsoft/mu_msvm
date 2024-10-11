/** @file
  A simple handle table implementation.

  The handle table functions like a traditional handle table where an opaque handle value
  is used to lookup a structure or other bit of information.  This implementation also
  allows lookup of an object or handle by a user defined object key.

  Key lookup is accomplished by performing a memory comparision on between a caller
  supplied key and the first N bytes of the stored object. The maximum key size is fixed
  at the time of handle table initialization.
  The user of the table has the option to not enable keyed lookup by specifying an
  ObjectKeySize of zero as part of the EFI_HANDLE_TABLE_INFO structure when initializing the
  table.
  Note that if keyed lookup is enabled, all objects in the handle table must have a unique
  key.

  Handles can be keyed to a given table.  When the a handle table is initialized the user
  can specify an 8-bit key value that will be included in all handles allocated by the table.
  The key is used to quickly reject handles allocated from a different handle table.

  Important Notes:

  - The current implementation is limited to adding handles only.
  - There is no reference tracking or deletion/removal of handles.
  - Users must provide synchronization between lookup and allocation if needed.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "EventLogDxe.h"
#include "EfiHandleTable.h"

//
// Channel handles are divided into three parts.
// The bottom 16 bits are used as a table index biased by 1
// The next 8 bits are used as the handle key.
// The remaining bits are available for use by callers.
//
// User Flags 8 or more
// Key        8
// Index      16
//
#define HANDLE_TABLE_MAX_SIZE       (0xFFFF - 1)

#define HANDLE_TABLE_KEY_MASK       0x00FF0000

#define HANDLE_TABLE_INDEX_MASK     0x0000FFFF

//
// Encodes an index as a handle for use with the given table
//
#define HANDLE_TABLE_ENCODE(_Index, _Table) (EFI_HANDLE)(UINTN)(((_Index) | ((_Table)->TableKey)) + 1)

//
// Decodes a handle into an index removing the key, user flags, and bias.
//
#define HANDLE_TABLE_INDEX(_Handle)         (((UINT32)(UINTN)(_Handle) & HANDLE_TABLE_INDEX_MASK) - 1)

//
// Isolates the key from a handle. This produces an un-shifted key
//
#define HANDLE_TABLE_KEY(_Handle)           ((UINT32)(UINTN)(_Handle) & HANDLE_TABLE_KEY_MASK)

//
// Actual handle table
//
typedef struct
{
    EFI_HANDLE_TABLE_INFO   Info;
    UINT32                  Size;
    UINT32                  TableKey;
    UINT32                  NextFree;
    VOID*                   Handles[];
} EFI_HANDLE_TABLE;


EFI_STATUS
EfiHandleTableInitialize(
    IN      const EFI_HANDLE_TABLE_INFO    *Attributes,
    IN      const UINT32                    Size,
    IN      const UINT8                     TableKey,
    OUT     EFI_HANDLE                     *Table
    )
/*++

Routine Description:

    Initializes a handle table

Arguments:

    Attributes  EFI_HANDLE_TABLE_INFO for the handle table.

    Size        Size of the handle table in handles.

    TableKey    Identifier for this handle table.

    Table       On success, returns a handle representing the handle table.

Return Value:

    EFI_STATUS.

--*/
{
    EFI_HANDLE_TABLE *table = NULL;
    UINTN allocSize;
    EFI_STATUS status;

    if ((Table == NULL) ||
        (Size == 0) ||
        (Size > HANDLE_TABLE_MAX_SIZE) ||
        (Attributes == NULL) ||
        (Attributes->Allocate == NULL))
    {
        status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    allocSize = sizeof(EFI_HANDLE_TABLE) + (Size * sizeof(VOID*));
    table = Attributes->Allocate(allocSize);

    if (table == NULL)
    {
        status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    ZeroMem(table, allocSize);
    CopyMem(&table->Info, Attributes, sizeof(table->Info));
    table->Size     = Size;
    table->TableKey = TableKey << 16;

    status = EFI_SUCCESS;

Exit:

    *Table = table;
    return status;
}


EFI_STATUS
EfiHandleTableAllocateObject(
    IN          EFI_HANDLE          TableHandle,
    IN          const UINTN         ObjectSize,
    OUT         VOID              **Object,
    OUT         EFI_HANDLE         *Handle
    )
/*++

Routine Description:

    Allocates an object and stores it in the handle table.

Arguments:

    TableHandle     Handle to the handle table to allocate an object in

    ObjectSize      Size of the object to allocate

    Object          Pointer to return the allocated object in

    Handle          Handle representing the object

Return Value:

    EFI_STATUS.

--*/
{
    EFI_HANDLE_TABLE *table     = (EFI_HANDLE_TABLE*)TableHandle;
    EFI_HANDLE       *newHandle = EFI_INVALID_HANDLE;
    VOID             *object    = NULL;
    UINT32            index;
    EFI_STATUS        status;

    //
    // Prevent objects that are too small to hold a key.
    //
    if (ObjectSize < table->Info.ObjectKeySize)
    {
        status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    ASSERT(Object != NULL);
    ASSERT(Handle != NULL);

    //
    // Find a free table index, then allocate and set a new object
    // NextFree serves as a hint for where to start the search.
    // Most likely it will point to the exact index to use, but not always.
    //
    // Assume failure before starting.
    //
    status = EFI_OUT_OF_RESOURCES;

    for (index = table->NextFree; index < table->Size; index++)
    {
        if (table->Handles[index] == NULL)
        {
            break;
        }
    }

    //
    // If a free index was found, attempt to allocate an object.
    //
    if (index < table->Size)
    {
        object = table->Info.Allocate(ObjectSize);

        if (object != NULL)
        {
            table->Handles[index] = object;
            newHandle = HANDLE_TABLE_ENCODE(index, table);
            //
            // It is unknown where the next free index is
            // but it must be after this index.
            // (Concurrent alloc and free are not allowed
            //  so another handle could not have been freed)
            //
            table->NextFree++;
            status = EFI_SUCCESS;
        }
    }

Exit:
    *Handle = newHandle;
    *Object = object;

    return status;
}


VOID *
EfiHandleTableLookupByKey(
    IN              const EFI_HANDLE    TableHandle,
    IN              const VOID         *Key,
    IN              const UINT32        KeySize,
    OUT OPTIONAL    EFI_HANDLE         *Handle
    )
/*++

Routine Description:

    Attempts to find an object in the handle table from the object Key.

Arguments:

    TableHandle     Handle table to use.

    Key             Key data to lookup.

    KeySize         Size of the key data. This must be equal to or smaller than
                    the object key data size that the handle table was initialized with.

    Handle          Optional handle pointer. If an object is found the associated handle
                    can be returned.

Return Value:

    Object pointer or NULL if not found.

--*/
{
    const EFI_HANDLE_TABLE *table  = (EFI_HANDLE_TABLE*)TableHandle;
    VOID             *object = NULL;
    EFI_HANDLE        handle = EFI_INVALID_HANDLE;
    UINT32            index;

    //
    // If the passed in key size is acceptable
    // look through the table for a matching key.
    //
    // This does a simple linear search for a matching key.
    // It doesn't scale well to a large number of handles
    // but works fine for the current implementation.
    //
    if ((KeySize <= table->Info.ObjectKeySize) &&
        (KeySize > 0))
    {
        for (index = 0; index < table->Size; index++)
        {
            if (table->Handles[index] != NULL)
            {
                if (CompareMem((UINT8*)table->Handles[index], (UINT8*)Key, KeySize) == 0)
                {
                    object = table->Handles[index];
                    handle = HANDLE_TABLE_ENCODE(index, table);
                    break;
                }
            }
        }
    }

    if (Handle != NULL)
    {
        *Handle = handle;
    }

    return object;
}


VOID *
EfiHandleTableLookupByHandle(
    IN          const EFI_HANDLE          TableHandle,
    IN          const EFI_HANDLE          Handle
    )
/*++

Routine Description:

    Attempts to find an object in the handle table from the given handle.

Arguments:

    TableHandle     Handle table to use.

    Handle          Handle to look up.

Return Value:

    Object pointer or NULL if not found.

--*/
{
    const EFI_HANDLE_TABLE *table  = (EFI_HANDLE_TABLE*)TableHandle;
    VOID             *object = NULL;
    UINT32            index  = HANDLE_TABLE_INDEX(Handle);

    if ((Handle != EFI_INVALID_HANDLE) &&
        (HANDLE_TABLE_KEY(Handle) == table->TableKey) &&
        (index < table->Size))
    {
        object = table->Handles[index];
    }

    return object;
}


EFI_STATUS
EfiHandleTableEnumerateObjects(
    IN          const EFI_HANDLE                TableHandle,
    IN          const VOID                     *CallbackContext,
    IN          HANDLE_ENUMERATE_CALLBACK       Callback
    )
/*++

Routine Description:

    Enumerates all allocated objects in the handle table and invokes the provided
    callback function for each object.

    Callback functions can cancel further enumeration by returning an EFI error code
    otherwise they should return EFI_SUCCESS.

Arguments:

    TableHandle     Table to enumerate objects from.

    CallbackContext Caller defined context. This will be provided as is to the callback function.

    Calback         Callback function to invoke.

Return Value:

    EFI_SUCCESS
    EFI_STATUS.

--*/
{
    const EFI_HANDLE_TABLE *table = (EFI_HANDLE_TABLE*)TableHandle;
    UINT32            index;
    EFI_STATUS        status = EFI_SUCCESS;

    for (index = 0; index < table->Size; index++)
    {
        if (table->Handles[index] != NULL)
        {
            EFI_HANDLE handle = HANDLE_TABLE_ENCODE(index, table);
            status = Callback(TableHandle, (VOID*)CallbackContext, handle, table->Handles[index]);

            if (EFI_ERROR(status))
            {
                break;
            }
        }
    }

    return status;
}
