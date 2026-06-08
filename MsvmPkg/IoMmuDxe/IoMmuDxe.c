/** @file
    Hyper-V IOMMU DXE driver.

    Implements the EDKII_IOMMU_PROTOCOL for Hyper-V isolated virtual machines.
    Provides bounce buffering so that DMA operations use host-visible memory.
    This driver only installs the protocol when running in an isolated VM;
    non-isolated VMs do not need IOMMU-based DMA translation.

    This is a generic implementation: any driver that uses PciIo or IoMmuLib
    for DMA (e.g., NvmExpressDxe, StorvscDxe) benefits from this bounce
    buffering transparently.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "IoMmuBounce.h"

#include <IsolationTypes.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>

#include <Protocol/IoMmu.h>

//
// External globals from IoMmuBounce.c
//
extern LIST_ENTRY  mAllocContextListHead;


/**
  EDKII_IOMMU_PROTOCOL.SetAttribute - Set IOMMU access attributes.

  For Hyper-V isolation, this is a no-op since visibility is managed
  explicitly through the bounce buffer mechanism rather than hardware
  page tables.

  @param[in]  This          Protocol instance.
  @param[in]  DeviceHandle  Device requesting access.
  @param[in]  Mapping       Mapping handle from Map().
  @param[in]  IoMmuAccess   Access flags (READ/WRITE).

  @retval EFI_SUCCESS       Always succeeds.
**/
STATIC
EFI_STATUS
EFIAPI
IoMmuSetAttribute (
    IN EDKII_IOMMU_PROTOCOL    *This,
    IN EFI_HANDLE              DeviceHandle,
    IN VOID                    *Mapping,
    IN UINT64                  IoMmuAccess
    )
{
    return EFI_SUCCESS;
}


/**
  EDKII_IOMMU_PROTOCOL.Map - Map a host address for DMA.

  For BusMasterCommonBuffer: the memory was already made host-visible by
  AllocateBuffer; returns the shared physical address directly.

  For BusMasterRead (host→device): acquires bounce pages, copies data into
  them, and returns the bounce physical address.

  For BusMasterWrite (device→host): acquires bounce pages, zeroes them,
  and returns the bounce physical address. Data is copied back on Unmap.

  @param[in]      This            Protocol instance.
  @param[in]      Operation       DMA operation type.
  @param[in]      HostAddress     System memory address to map.
  @param[in,out]  NumberOfBytes   Bytes to map / bytes mapped.
  @param[out]     DeviceAddress   Resulting DMA address for the device.
  @param[out]     Mapping         Opaque handle for Unmap().

  @retval EFI_SUCCESS             Mapping created successfully.
  @retval EFI_OUT_OF_RESOURCES    Could not allocate bounce pages.
**/
STATIC
EFI_STATUS
EFIAPI
IoMmuMap (
    IN     EDKII_IOMMU_PROTOCOL    *This,
    IN     EDKII_IOMMU_OPERATION   Operation,
    IN     VOID                    *HostAddress,
    IN OUT UINTN                   *NumberOfBytes,
    OUT    EFI_PHYSICAL_ADDRESS    *DeviceAddress,
    OUT    VOID                    **Mapping
    )
{
    if (!IoMmuIsBounceActive ()) {
        //
        // Non-isolated path. Mirrors RootBridgeIoMap()'s behavior when
        // IoMmuIsPresent() == FALSE: a non-64-bit Read/Write whose buffer
        // crosses the 4GB boundary is bounced into memory below 4GB so
        // 32-bit-only bus masters can address it. Everything else is an
        // identity mapping. CommonBuffer cannot be remapped here; the
        // memory has to already be addressable, which IoMmuAllocateBuffer
        // guarantees by allocating below 4GB whenever DUAL_ADDRESS_CYCLE
        // is not requested.
        //
        EFI_PHYSICAL_ADDRESS PhysicalAddress;

        PhysicalAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)HostAddress;

        if ((Operation != EdkiiIoMmuOperationBusMasterCommonBuffer)   &&
            (Operation != EdkiiIoMmuOperationBusMasterCommonBuffer64) &&
            (Operation != EdkiiIoMmuOperationBusMasterRead64)         &&
            (Operation != EdkiiIoMmuOperationBusMasterWrite64)        &&
            ((PhysicalAddress + *NumberOfBytes) > SIZE_4GB)) {

            PIOMMU_MAP_CONTEXT   MapContext;
            EFI_PHYSICAL_ADDRESS MappedAddress;
            UINTN                Pages;
            EFI_STATUS           Status;

            Pages         = EFI_SIZE_TO_PAGES (*NumberOfBytes);
            MappedAddress = SIZE_4GB - 1;
            Status = gBS->AllocatePages (
                             AllocateMaxAddress,
                             EfiBootServicesData,
                             Pages,
                             &MappedAddress
                             );
            if (EFI_ERROR (Status)) {
                *NumberOfBytes = 0;
                return Status;
            }

            //
            // Read = host->device: pre-populate the bounce buffer.
            //
            if ((Operation == EdkiiIoMmuOperationBusMasterRead) ||
                (Operation == EdkiiIoMmuOperationBusMasterRead64)) {
                CopyMem (
                    (VOID *)(UINTN)MappedAddress,
                    HostAddress,
                    *NumberOfBytes
                    );
            }

            MapContext = AllocatePool (sizeof (IOMMU_MAP_CONTEXT));
            if (MapContext == NULL) {
                gBS->FreePages (MappedAddress, Pages);
                *NumberOfBytes = 0;
                return EFI_OUT_OF_RESOURCES;
            }

            ZeroMem (MapContext, sizeof (*MapContext));
            MapContext->Signature       = IOMMU_MAP_CONTEXT_SIGNATURE;
            MapContext->Operation       = Operation;
            MapContext->HostAddress     = HostAddress;
            MapContext->NumberOfBytes   = *NumberOfBytes;
            MapContext->BounceBase      = (VOID *)(UINTN)MappedAddress;
            MapContext->BouncePageCount = (UINT32)Pages;
            //
            // BounceBlock == NULL discriminates this non-isolated <4GB
            // bounce path from the isolated bounce-block sub-allocation
            // path in IoMmuUnmap.
            //
            MapContext->BounceBlock     = NULL;
            MapContext->BounceStartPage = 0;

            *DeviceAddress = MappedAddress;
            *Mapping       = MapContext;
            return EFI_SUCCESS;
        }

        //
        // Either the device can DMA above 4GB or the buffer already lives
        // below 4GB; identity-map.
        //
        *DeviceAddress = PhysicalAddress;
        *Mapping       = NULL;
        return EFI_SUCCESS;
    }

    //
    // For BusMasterCommonBuffer, the memory was already made host-visible
    // via AllocateBuffer. Just return the shared PA.
    //
    if (Operation == EdkiiIoMmuOperationBusMasterCommonBuffer ||
        Operation == EdkiiIoMmuOperationBusMasterCommonBuffer64) {
        *DeviceAddress = IoMmuGetSharedPa (HostAddress);
        *Mapping = NULL;
        return EFI_SUCCESS;
    }

    //
    // For BusMasterRead/Write, sub-allocate a contiguous run of pages from
    // the pre-allocated bounce block pool. The owning block has already
    // been made host-visible at allocation time, so this avoids the
    // per-Map MakeAddressRangeHostVisible hypercall.
    //
    {
        PIOMMU_MAP_CONTEXT              MapContext;
        UINT32                          BouncePageCount;
        VOID                            *BounceBase;
        VOID                            *SharedVa;
        EFI_STATUS                      Status;
        PIOMMU_BOUNCE_BLOCK             BounceBlock;
        UINT32                          BounceStartPage;
        UINTN                           RequestedPages;

        RequestedPages = EFI_SIZE_TO_PAGES (*NumberOfBytes);

        //
        // Bounce-block bookkeeping (page indices, bitmap offsets,
        // BlockPageCount) is UINT32 throughout. Reject any request that
        // would not fit in 32 bits (>16 TiB) up front rather than silently
        // truncating. EDKII_IOMMU_PROTOCOL.Map has no realistic caller at
        // this size, but the explicit check makes the contract clear.
        //
        if (RequestedPages > MAX_UINT32) {
            DEBUG ((DEBUG_ERROR,
                "IoMmuMap: Request too large: %lu pages (max %u)\n",
                (UINT64)RequestedPages, MAX_UINT32));
            return EFI_OUT_OF_RESOURCES;
        }

        BouncePageCount = (UINT32)RequestedPages;

        //
        // The bounce block pool always allocates blocks below 4GB, so the
        // run returned here is safe for both 32-bit and 64-bit DMA devices.
        // Operation-based DmaMemoryTop checks (4GB cap) are therefore
        // implicitly satisfied.
        //
        Status = IoMmuAcquireBouncePages (
                     BouncePageCount,
                     &BounceBlock,
                     &BounceStartPage,
                     &BounceBase
                     );
        if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR,
                "IoMmuMap: Failed to acquire %d bounce pages: %r\n",
                BouncePageCount, Status));
            return EFI_OUT_OF_RESOURCES;
        }

        SharedVa = IoMmuGetSharedVa (BounceBase);

        //
        // Zero the entire bounce buffer. For BusMasterRead (host->device),
        // this prevents leaking guest data in unused page fragments. For
        // BusMasterWrite (device->host), this provides a clean buffer that
        // the device will write into (data copied back on Unmap).
        //
        ZeroMem (SharedVa, (UINTN)BouncePageCount * EFI_PAGE_SIZE);

        if (Operation == EdkiiIoMmuOperationBusMasterRead ||
            Operation == EdkiiIoMmuOperationBusMasterRead64) {
            //
            // Host->device transfer: copy data into the zeroed bounce buffer.
            //
            CopyMem (SharedVa, HostAddress, *NumberOfBytes);
        }

        //
        // Return the host-visible physical address.
        //
        *DeviceAddress = IoMmuGetSharedPa (BounceBase);

        //
        // Allocate mapping context for Unmap.
        //
        MapContext = AllocateZeroPool (sizeof (IOMMU_MAP_CONTEXT));
        if (MapContext == NULL) {
            IoMmuReleaseBouncePages (BounceBlock, BounceStartPage, BouncePageCount);
            return EFI_OUT_OF_RESOURCES;
        }

        MapContext->Signature        = IOMMU_MAP_CONTEXT_SIGNATURE;
        MapContext->Operation        = Operation;
        MapContext->HostAddress      = HostAddress;
        MapContext->NumberOfBytes    = *NumberOfBytes;
        MapContext->BounceBase       = BounceBase;
        MapContext->BouncePageCount  = BouncePageCount;
        MapContext->BounceBlock      = BounceBlock;
        MapContext->BounceStartPage  = BounceStartPage;

        *Mapping = MapContext;
    }

    return EFI_SUCCESS;
}


/**
  EDKII_IOMMU_PROTOCOL.Unmap - Complete a Map operation and release resources.

  For BusMasterWrite operations, copies data from the bounce buffer back
  to the original host address before releasing the bounce pages.

  @param[in]  This      Protocol instance.
  @param[in]  Mapping   Mapping handle from Map().

  @retval EFI_SUCCESS   Unmap completed.
**/
STATIC
EFI_STATUS
EFIAPI
IoMmuUnmap (
    IN EDKII_IOMMU_PROTOCOL    *This,
    IN VOID                    *Mapping
    )
{
    PIOMMU_MAP_CONTEXT  MapContext;

    if (Mapping == NULL) {
        //
        // NULL mapping: BusMasterCommonBuffer or no-op. Nothing to do.
        //
        return EFI_SUCCESS;
    }

    MapContext = (PIOMMU_MAP_CONTEXT)Mapping;
    ASSERT (MapContext->Signature == IOMMU_MAP_CONTEXT_SIGNATURE);

    //
    // Non-isolated <4GB bounce path. Mirrors RootBridgeIoUnmap()'s
    // no-IOMMU path: for Write (device->host) copy the bounce buffer
    // back into the original host buffer, then free the bounce pages
    // and the context.
    //
    if (MapContext->BounceBlock == NULL) {
        if ((MapContext->Operation == EdkiiIoMmuOperationBusMasterWrite) ||
            (MapContext->Operation == EdkiiIoMmuOperationBusMasterWrite64)) {
            CopyMem (
                MapContext->HostAddress,
                MapContext->BounceBase,
                MapContext->NumberOfBytes
                );
        }

        gBS->FreePages (
                 (EFI_PHYSICAL_ADDRESS)(UINTN)MapContext->BounceBase,
                 MapContext->BouncePageCount
                 );
        FreePool (MapContext);
        return EFI_SUCCESS;
    }

    //
    // Isolated bounce-block path.
    //
    // For BusMasterWrite (device->host), copy data from bounce to host.
    //
    if (MapContext->Operation == EdkiiIoMmuOperationBusMasterWrite ||
        MapContext->Operation == EdkiiIoMmuOperationBusMasterWrite64) {
        VOID  *SharedVa;

        SharedVa = IoMmuGetSharedVa (MapContext->BounceBase);
        CopyMem (
            MapContext->HostAddress,
            SharedVa,
            MapContext->NumberOfBytes
            );
    }

    //
    // Release the bounce pages back to the pool. The owning bounce block
    // remains host-visible for its lifetime, so no per-Unmap hypercall is
    // required.
    //
    if ((MapContext->Operation != EdkiiIoMmuOperationBusMasterCommonBuffer) &&
      (MapContext->Operation != EdkiiIoMmuOperationBusMasterCommonBuffer64))
    {
        IoMmuReleaseBouncePages (
            MapContext->BounceBlock,
            MapContext->BounceStartPage,
            MapContext->BouncePageCount
            );
        FreePool (MapContext);

    }

    return EFI_SUCCESS;
}


/**
  EDKII_IOMMU_PROTOCOL.AllocateBuffer - Allocate DMA-capable memory.

  Allocates pages and makes them host-visible so the device can DMA to them.
  Returns a canonicalized shared VA that maps above the shared GPA boundary.

  When EDKII_IOMMU_ATTRIBUTE_DUAL_ADDRESS_CYCLE is not set the allocation is
  constrained below 4GB and Type is forced to AllocateMaxAddress.  When the
  attribute is set, the caller-supplied Type is used as-is.

  @param[in]      This          Protocol instance.
  @param[in]      Type          Allocation type; overridden to AllocateMaxAddress
                                when dual-address-cycle is not set.
  @param[in]      MemoryType    Memory type (EfiBootServicesData or EfiRuntimeServicesData).
  @param[in]      Pages         Number of pages to allocate.
  @param[in,out]  HostAddress   On output, the shared VA pointer.
  @param[in]      Attributes    Allocation attributes (EDKII_IOMMU_ATTRIBUTE_*).

  @retval EFI_SUCCESS              Memory allocated and made host-visible.
  @retval EFI_INVALID_PARAMETER    This, HostAddress, or Pages is invalid.
  @retval EFI_OUT_OF_RESOURCES     Allocation or visibility call failed.
**/
STATIC
EFI_STATUS
EFIAPI
IoMmuAllocateBuffer (
    IN     EDKII_IOMMU_PROTOCOL    *This,
    IN     EFI_ALLOCATE_TYPE       Type,
    IN     EFI_MEMORY_TYPE         MemoryType,
    IN     UINTN                   Pages,
    IN OUT VOID                    **HostAddress,
    IN     UINT64                  Attributes
    )
{
    EFI_STATUS                    Status;
    IOMMU_HOST_VISIBILITY_CONTEXT VisibilityContext;
    PIOMMU_ALLOC_CONTEXT          AllocContext;
    EFI_PHYSICAL_ADDRESS          PhysicalAddress;

    if ((This == NULL) || (Pages == 0) || (HostAddress == NULL)) {
        DEBUG ((DEBUG_ERROR, "IoMmuAllocateBuffer: Invalid parameter\n"));
        Status = EFI_INVALID_PARAMETER;
        goto End;
    }

    PhysicalAddress = MAX_UINTN;

    if ((Attributes & EDKII_IOMMU_ATTRIBUTE_DUAL_ADDRESS_CYCLE) == 0) {
        //
        // Device cannot address above 4GB; constrain allocation below 4GB.
        //
        PhysicalAddress = SIZE_4GB - 1;
        Type            = AllocateMaxAddress;
    }

    Status = gBS->AllocatePages (
                     Type,
                     MemoryType,
                     Pages,
                     &PhysicalAddress
                     );
    if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "IoMmuAllocateBuffer: Failed to allocate pages: %r\n", Status));
        goto End;
    }

    if (!IoMmuIsBounceActive ()) {
        //
        // Non-isolated: just return the allocated buffer directly.
        //
        *HostAddress = (VOID *)(UINTN)PhysicalAddress;
        goto End;
    }

    //
    // Make the allocated range host-visible for DMA.
    //
    Status = IoMmuMakeAddressRangeShared (
                 (VOID *)(UINTN)PhysicalAddress,
                 (UINT32)Pages,
                 &VisibilityContext
                 );
    if (EFI_ERROR (Status)) {
        gBS->FreePages (PhysicalAddress, Pages);
        goto End;
    }

    //
    // Track this allocation so FreeBuffer can revoke visibility.
    //
    AllocContext = AllocateZeroPool (sizeof (IOMMU_ALLOC_CONTEXT));
    if (AllocContext == NULL) {
        IoMmuMakeAddressRangePrivate (&VisibilityContext);
        gBS->FreePages (PhysicalAddress, Pages);
        Status = EFI_OUT_OF_RESOURCES;
        goto End;
    }

    AllocContext->OriginalAddress   = (VOID *)(UINTN)PhysicalAddress;
    AllocContext->Pages             = Pages;
    AllocContext->VisibilityContext = VisibilityContext;
    InsertTailList (&mAllocContextListHead, &AllocContext->Link);

    //
    // Return the canonicalized shared VA.
    //
    *HostAddress = IoMmuGetSharedVa ((VOID *)(UINTN)PhysicalAddress);

End:
    ASSERT_EFI_ERROR (Status);
    return Status;
}


/**
  EDKII_IOMMU_PROTOCOL.FreeBuffer - Free DMA-capable memory.

  Revokes host visibility for the allocation and frees the pages.

  @param[in]  This          Protocol instance.
  @param[in]  Pages         Number of pages to free.
  @param[in]  HostAddress   The shared VA returned by AllocateBuffer.

  @retval EFI_SUCCESS       Visibility revoked and pages freed.
**/
STATIC
EFI_STATUS
EFIAPI
IoMmuFreeBuffer (
    IN EDKII_IOMMU_PROTOCOL    *This,
    IN UINTN                   Pages,
    IN VOID                    *HostAddress
    )
{
    LIST_ENTRY          *Entry;
    PIOMMU_ALLOC_CONTEXT AllocContext;

    if (!IoMmuIsBounceActive ()) {
        return gBS->FreePages ((EFI_PHYSICAL_ADDRESS)(UINTN)HostAddress, Pages);
    }

    //
    // Find the tracking entry and revoke host visibility.
    //
    for (Entry = GetFirstNode (&mAllocContextListHead);
         !IsNull (&mAllocContextListHead, Entry);
         Entry = GetNextNode (&mAllocContextListHead, Entry)) {

        AllocContext = BASE_CR (Entry, IOMMU_ALLOC_CONTEXT, Link);

        if (IoMmuGetSharedVa (AllocContext->OriginalAddress) == HostAddress) {
            RemoveEntryList (Entry);
            IoMmuMakeAddressRangePrivate (&AllocContext->VisibilityContext);
            FreePool (AllocContext);
            break;
        }
    }

    gBS->FreePages ((EFI_PHYSICAL_ADDRESS)(UINTN)HostAddress, Pages);

    return EFI_SUCCESS;
}


//
// Protocol instance.
//
STATIC EDKII_IOMMU_PROTOCOL  mIoMmuProtocol = {
    EDKII_IOMMU_PROTOCOL_REVISION,
    IoMmuSetAttribute,
    IoMmuMap,
    IoMmuUnmap,
    IoMmuAllocateBuffer,
    IoMmuFreeBuffer
};


/**
  IoMmuDxe driver entry point.

  Initializes the bounce buffer subsystem and installs the EDKII_IOMMU_PROTOCOL.
  In isolated VMs, the protocol provides bounce buffering for DMA. In non-isolated
  VMs, the protocol provides simple pass-through behavior (identity mapping,
  standard allocation). The protocol is always installed to satisfy the
  IoMmuLib AARCH64 DEPEX requirement.

  @param[in]  ImageHandle   Handle for this driver image.
  @param[in]  SystemTable   Pointer to the UEFI system table.

  @retval EFI_SUCCESS       Protocol installed successfully.
  @retval other             Initialization failed.
**/
EFI_STATUS
EFIAPI
IoMmuDxeEntryPoint (
    IN EFI_HANDLE        ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
    )
{
    EFI_STATUS  Status;

    //
    // Resolve the bounce dispatch mode up front so the
    // IoMmuIsBounceActive() / IoMmuRequiresHostVisibility() predicates
    // read meaningful state for the rest of init (and for any later Map()
    // calls that gate on them). Without this, the cached mode reads its
    // default of IOMMU_BOUNCE_MODE_NONE and the driver short-circuits.
    //
    mBounceMode = IoMmuComputeBounceMode ();

    if (IoMmuIsBounceActive ()) {
        PIOMMU_BOUNCE_BLOCK  InitialBlock;
        EFI_STATUS           BlockStatus;

        Status = IoMmuInitializeBounce ();
        if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "IoMmuDxe: Failed to initialize bounce buffers: %r\n", Status));
            return Status;
        }

        //
        // Pre-allocate one bounce block up front so the common-case Map()
        // path needs no AllocatePages/MakeAddressRangeHostVisible hypercall.
        // Failure here is non-fatal: subsequent Map() calls will lazily
        // allocate a block on demand.
        //
        BlockStatus = IoMmuPreAllocateBounceBlock (
                          IOMMU_BOUNCE_INITIAL_BLOCK_PAGES,
                          &InitialBlock
                          );
        if (EFI_ERROR (BlockStatus)) {
            DEBUG ((DEBUG_WARN,
                "IoMmuDxe: Failed to pre-allocate initial bounce block: %r\n",
                BlockStatus));
        } else {
            DEBUG ((DEBUG_INFO,
                "IoMmuDxe: Pre-allocated initial bounce block (%d pages)\n",
                IOMMU_BOUNCE_INITIAL_BLOCK_PAGES));
        }

        DEBUG ((DEBUG_INFO, "IoMmuDxe: Bounce buffers initialized\n"));
    } else {
        DEBUG ((DEBUG_INFO, "IoMmuDxe: Bounce buffering inactive, installing pass-through IOMMU protocol\n"));
    }

    //
    // Always install the IOMMU protocol. On AARCH64, the IoMmuLib has a DEPEX
    // on this protocol, so it must be available for VpcivscDxe (and any other
    // IoMmuLib consumer) to load.
    //
    Status = gBS->InstallProtocolInterface (
                     &ImageHandle,
                     &gEdkiiIoMmuProtocolGuid,
                     EFI_NATIVE_INTERFACE,
                     &mIoMmuProtocol
                     );
    if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "IoMmuDxe: Failed to install IOMMU protocol: %r\n", Status));
        return Status;
    }

    DEBUG ((DEBUG_INFO, "IoMmuDxe: IOMMU protocol installed\n"));

    return EFI_SUCCESS;
}
