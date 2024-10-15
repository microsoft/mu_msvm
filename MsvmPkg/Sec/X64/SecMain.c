/** @file
  Main SEC phase code.  Transitions to PEI.

  Copyright (c) 2008 - 2011, Intel Corporation. All rights reserved.
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>

#include <Hv/HvGuestCpuid.h>
#include <Library/PeimEntryPoint.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CpuLib.h>
#include <Library/DebugAgentLib.h>
#include <Library/PeCoffLib.h>
#include <Library/PeCoffGetEntryPointLib.h>
#include <Library/PeCoffExtraActionLib.h>
#include <Ppi/TemporaryRamSupport.h>
#include <BiosInterface.h>
#include <IsolationTypes.h>
#include "SecP.h"

#define SEC_IDT_ENTRY_COUNT 46

typedef struct _SEC_IDT_TABLE {
    EFI_PEI_SERVICES          *PeiService;
    IA32_IDT_GATE_DESCRIPTOR  IdtTable[SEC_IDT_ENTRY_COUNT];
} SEC_IDT_TABLE;


EFI_STATUS
EFIAPI
TemporaryRamMigration (
    IN  CONST EFI_PEI_SERVICES  **PeiServices,
        EFI_PHYSICAL_ADDRESS    TemporaryMemoryBase,
        EFI_PHYSICAL_ADDRESS    PermanentMemoryBase,
        UINTN                   CopySize
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

HV_HYPERVISOR_ISOLATION_CONFIGURATION mIsolationConfiguration;

UINT32
Expand3ByteSize (
  IN VOID* Size
  )
/*++

Routine Description:

    Expands the 3 byte size commonly used in Firmware Volume data structures

Arguments:

    Size - Address of the 3 byte array representing the size

Return Value

    The size as an unsigned 32bit integer.

--*/
{
  return (((UINT8*)Size)[2] << 16) +
         (((UINT8*)Size)[1] << 8) +
         ((UINT8*)Size)[0];
}

#if defined(SECMAIN_DEBUG_NOISY)
VOID
DebugPrintGuid(
    IN EFI_GUID *Guid
    )
/*++

Routine Description:

    Outputs a GUID value as a formatted string using DebugPrint().

Arguments:

    Guid - A pointer to a GUID value.

    Newline - if TRUE (non-zero) follow the output string with a newline.

Return Value:

    None.

--*/
{
    DebugPrint(DEBUG_VERBOSE, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
               Guid->Data1,
               Guid->Data2,
               Guid->Data3,
               Guid->Data4[0],
               Guid->Data4[1],
               Guid->Data4[2],
               Guid->Data4[3],
               Guid->Data4[4],
               Guid->Data4[5],
               Guid->Data4[6],
               Guid->Data4[7]);
}


CHAR16*
FileTypeToString(
    IN  EFI_FV_FILETYPE Type
    )
/*++

Routine Description:

    Converts a firmware volume filetype to a string for debugging purposes.

Arguments:

    Type - The firmware volume filetype value.

Return Value:

    A wide string representing the firmware volume filetype value
    or "*unknown*" if the value is out of range.

--*/
{
    switch (Type)
    {
        case EFI_FV_FILETYPE_ALL:                   return L"ALL";
        case EFI_FV_FILETYPE_RAW:                   return L"RAW";
        case EFI_FV_FILETYPE_FREEFORM:              return L"FREEFORM";
        case EFI_FV_FILETYPE_SECURITY_CORE:         return L"SECURITY_CORE";
        case EFI_FV_FILETYPE_PEI_CORE:              return L"PEI_CORE";
        case EFI_FV_FILETYPE_DXE_CORE:              return L"DXE_CORE";
        case EFI_FV_FILETYPE_PEIM:                  return L"PEIM";
        case EFI_FV_FILETYPE_DRIVER:                return L"DRIVER";
        case EFI_FV_FILETYPE_COMBINED_PEIM_DRIVER:  return L"COMBINED_PEIM_DRIVER";
        case EFI_FV_FILETYPE_APPLICATION:           return L"APPLICATION";
        case EFI_FV_FILETYPE_SMM:                   return L"SMM";
        case EFI_FV_FILETYPE_FIRMWARE_VOLUME_IMAGE: return L"FIRMWARE VOLUME IMAGE";
        case EFI_FV_FILETYPE_COMBINED_SMM_DXE:      return L"COMBINED_SMM_DXE";
        case EFI_FV_FILETYPE_SMM_CORE:              return L"SMM_CORE";
        default:                                    break;
    }
    if (Type >= EFI_FV_FILETYPE_OEM_MIN && Type <= EFI_FV_FILETYPE_OEM_MAX)
    {
        return L"OEM range";
    }
    if (Type >= EFI_FV_FILETYPE_DEBUG_MIN && Type <= EFI_FV_FILETYPE_DEBUG_MAX)
    {
        return L"DEBUG range";
    }
    if (Type >= EFI_FV_FILETYPE_FFS_MIN && Type <= EFI_FV_FILETYPE_FFS_MAX)
    {
        return L"FFS range";
    }
    return L"*unknown*";
}


VOID
DebugFvhDump(
    IN  EFI_FIRMWARE_VOLUME_HEADER *FVH,
    IN  CHAR16 *Indent
    )
/*++

Routine Description:

    Prints detailed information about a Firmware Volume Header to the debugger.

Arguments:

    FVH - A pointer to a Firmware Volume Header structure.

    Indent - A prefix string to output on each line.  Typically spaces for indenting.

Return Value:

    None.

--*/
{
    DebugPrint(DEBUG_VERBOSE, "%sFileSystemGuid:  ", Indent); DebugPrintGuid(&FVH->FileSystemGuid);
    DebugPrint(DEBUG_VERBOSE, "%sFvLength:        0x%08X\n", Indent, FVH->FvLength);
    DebugPrint(DEBUG_VERBOSE, "%sSignature:       0x%08X\n", Indent, FVH->Signature);
    DebugPrint(DEBUG_VERBOSE, "%sAttributes:      0x%08X\n", Indent, FVH->Attributes);
    DebugPrint(DEBUG_VERBOSE, "%sHeaderLength:    0x%04X\n", Indent, FVH->HeaderLength);
    DebugPrint(DEBUG_VERBOSE, "%sChecksum:        0x%04X\n", Indent, FVH->Checksum);
    DebugPrint(DEBUG_VERBOSE, "%sExtHeaderOffset: 0x%04x\n", Indent, FVH->ExtHeaderOffset);
    DebugPrint(DEBUG_VERBOSE, "%sRevision:        0x%02x\n", Indent, FVH->Revision);
}


VOID
DebugFhDump(
    IN  EFI_FFS_FILE_HEADER *FH,
    IN  CHAR16 *Indent)
/*++

Routine Description:

    Prints detailed information about a Firmware File System File Header to the debugger.

Arguments:

    FH - A pointer to a Firmware File System File Header.

    Indent - A prefix string to output on each line.  Typically spaces for indenting.

Return Value:

    None.

--*/
{
    DebugPrint(DEBUG_VERBOSE, "%sName:           ", Indent); DebugPrintGuid(&FH->Name);
    DebugPrint(DEBUG_VERBOSE, "%sIntegrityCheck: 0x%04X\n", Indent, FH->IntegrityCheck.Checksum16);
    DebugPrint(DEBUG_VERBOSE, "%sType:           0x%02X - %s\n",
               Indent, FH->Type, FileTypeToString(FH->Type));
    DebugPrint(DEBUG_VERBOSE, "%sAttributes:     0x%08X\n", Indent, FH->Attributes);
    DebugPrint(DEBUG_VERBOSE, "%sSize:           0x%08X\n", Indent, Expand3ByteSize(FH->Size));
    DebugPrint(DEBUG_VERBOSE, "%sState:          0x%02X\n", Indent, FH->State);
}


VOID
DebugHexDump(
    IN  EFI_PHYSICAL_ADDRESS Base,
    IN  UINT32 Offset,
    IN  UINT32 Len,
    IN  CHAR16 *Indent)
/*++

Routine Description:

    Performs a traditional hex dump of memory to the debugger.

Arguments:

    Base - The base address of the memory buffer.

    Offset - The offset from the base address to begin the hex dump.

    Len - The number of bytes to output.

    Indent - A prefix string to output on each line. Typically spaces for indenting.

Return Value:

    None.

--*/
{
    UINT8 *buffer = (UINT8 *)(UINTN)(Base + Offset);
    UINT32 i, j;
    for (i = 0; i < Len; i += 16)
    {
        DebugPrint(DEBUG_VERBOSE, "%s%08x: ", Indent, Offset + i);
        for (j = 0; (i+j) < Len && j < 16; j++)
        {
            DebugPrint(DEBUG_VERBOSE, "%02X ", buffer[i+j]);
        }
        DebugPrint(DEBUG_VERBOSE, "\n");
    }
}


VOID
DebugVolDump(
    IN  EFI_PHYSICAL_ADDRESS Base,
    IN  UINT32 Len,
    IN  CHAR16 *Indent
    )
/*++

Routine Description:

    Prints detailed information about a Firmware Volume to the debugger.

Arguments:

    Base - The base address of the Firmware Volume.

    Len - The length of the volume in bytes.

    Indent - A prefix string to output on each line. Typically spaces for indenting.

Return Value:

    None.

--*/
{
    UINT32 imageOffset, volOffset, size;
    EFI_FIRMWARE_VOLUME_HEADER *fvh;
    EFI_FFS_FILE_HEADER *fh;

    //
    // loop through the volumes in the image
    //
    for (imageOffset = 0; imageOffset < Len; /*empty*/)
    {
        fvh = (EFI_FIRMWARE_VOLUME_HEADER *)(UINTN)(Base + imageOffset);

        DebugPrint(DEBUG_VERBOSE, "Firmware Volume Header\n\n");
        DebugHexDump(Base, imageOffset, sizeof(EFI_FIRMWARE_VOLUME_HEADER), L"    ");
        DebugPrint(DEBUG_VERBOSE, "\n");
        DebugFvhDump(fvh, L"    ");
        DebugPrint(DEBUG_VERBOSE, "\n");

        //
        // loop through the files in the volume
        //
        for (volOffset = fvh->HeaderLength; volOffset < fvh->FvLength; /*empty*/)
        {
            //
            // round up the offset to 8 byte boundary
            //
            volOffset = (volOffset + 7) & 0xfffffff8ULL;

            fh = (EFI_FFS_FILE_HEADER *)(UINTN)(Base + imageOffset + volOffset);
            size = Expand3ByteSize(fh->Size);

            DebugPrint(DEBUG_VERBOSE, "\n    FFS File Header\n\n");
            DebugHexDump(Base, imageOffset + volOffset, sizeof(EFI_FFS_FILE_HEADER), L"        ");
            DebugFhDump(fh, L"        ");
            DebugPrint(DEBUG_VERBOSE, "\n");

            volOffset += size;
        }

        imageOffset += (UINT32)fvh->FvLength;
    }

}
#endif


EFI_STATUS
FindMainFv(
    IN  EFI_FIRMWARE_VOLUME_HEADER  *SecFv,
    OUT EFI_FIRMWARE_VOLUME_HEADER  **MainFv
    )
/*++

Routine Description:

    Finds the MAIN firmware volume.

    TODO: This could be further optimized to avoid searching
    downward in memory by adding support for the fixed-at-build
    PCDs in SEC and simply using the PcdMsvmFvBase value as the
    location of the MAIN FV.

Arguments:

    SecFv - Pointer to the SEC firmware volume header.

    MainFv - Returns a pointer to the MAIN firmware volume header.

Return Value:

    EFI_SUCCESS     - if volume is found.
    EFI_NOT_FOUND   - if volume is not found.

--*/
{
    EFI_FIRMWARE_VOLUME_HEADER  *fv;
    UINTN                       distance;
    EFI_STATUS                  status;

    DEBUG((DEBUG_VERBOSE,
           ">>> FindMainFv(0x%x, 0x%x)\n",
           (UINT32)(UINTN)SecFv,
           (UINT32)(UINTN)*MainFv
           ));

    ASSERT(((UINTN)SecFv & EFI_PAGE_MASK) == 0);

    //
    // Start cursor at beginning of SEC CORE volume header.
    //
    fv = SecFv;

    //
    // Include SEC CORE volume size in total distance searched.
    //
    distance = (UINTN)SecFv->FvLength;

    for (;;)
    {
        //
        // Move down one page.
        //
        fv = (EFI_FIRMWARE_VOLUME_HEADER*)((UINT8*)fv - EFI_PAGE_SIZE);

        //
        // Accumulate distance searched.
        //
        distance += EFI_PAGE_SIZE;

        //
        // Stop beyond 8MB
        //
        if (distance > SIZE_8MB)
        {
            DEBUG((DEBUG_ERROR, "--- exceeded 8MB search limit looking for MAIN FV\n"));
            status = EFI_NOT_FOUND;
            break;
        }

        //
        // Continue searching if not possibily pointing at FV header.
        //
        if (fv->Signature != EFI_FVH_SIGNATURE)
        {
            continue;
        }

        //
        // Continue searching is size is not sensible (coincidental signature).
        //
        if ((UINTN)fv->FvLength > distance)
        {
            continue;
        }

        //
        // Output the found volume header and stop searching.
        //
        *MainFv = fv;
        status = EFI_SUCCESS;
        break;
    }

    DEBUG((DEBUG_VERBOSE,
           "<<< FindMainFv(0x%x, 0x%x) result 0x%x\n",
           (UINT32)(UINTN)SecFv,
           (UINT32)(UINTN)*MainFv,
           status
           ));

    return status;
}


EFI_STATUS
EFIAPI
FindFfsFile(
    IN  EFI_FIRMWARE_VOLUME_HEADER  *Fv,
        EFI_FV_FILETYPE             FileType,
    OUT EFI_FFS_FILE_HEADER         **FoundFile
    )
/*++

Routine Description:

    Finds a file of the specified type in a firmware volume.

Arguments:

    Fv - The firmware folume header.

    FileType - The type of file to find.

    FoundFile - Returns a pointer to the found file header.

Return Value:

    EFI_SUCCESS             - The file type was found.
    EFI_NOT_FOUND           - The file type was not found.
    EFI_VOLUME_CORRUPTED    - The volume structure is not valid.

--*/
{
    EFI_STATUS                  status;
    EFI_PHYSICAL_ADDRESS        currentAddress;
    EFI_PHYSICAL_ADDRESS        endOfVolume;
    EFI_FFS_FILE_HEADER         *file;
    UINT32                      size;

   DEBUG((DEBUG_VERBOSE,
          ">>> FindFfsFile(0x%x, 0x%x)\n",
          (UINT32)(UINTN)Fv,
          (UINT32)(UINTN)FileType
          ));

    //
    // Validate FV signature.
    //
    if (Fv->Signature != EFI_FVH_SIGNATURE)
    {
       DEBUG((EFI_D_ERROR, "--- Invalid FVH signature\n"));
       status = EFI_VOLUME_CORRUPTED;
       goto Cleanup;
    }

    //
    // Calculate end of volume and point cursor at first file header.
    //
    endOfVolume = ((EFI_PHYSICAL_ADDRESS)(UINTN) Fv) + Fv->FvLength;
    currentAddress = ((EFI_PHYSICAL_ADDRESS)(UINTN) Fv) + Fv->HeaderLength;

    //
    // Loop through the files in the volume.
    //
    for (;;)
    {
        //
        // File headers are 8 byte aligned.
        //
        currentAddress = (currentAddress + 7) & ~(7ULL);
        if (currentAddress > endOfVolume)
        {
           DEBUG((EFI_D_ERROR, "--- Aligned file header address past volume end\n"));
           status = EFI_VOLUME_CORRUPTED;
           goto Cleanup;
        }

        file = (EFI_FFS_FILE_HEADER*)(UINTN) currentAddress;
        size = Expand3ByteSize(file->Size);

#if defined(SECMAIN_DEBUG_NOISY)
        DEBUG((DEBUG_VERBOSE, "--- File:       0x%x\n", file));
        DEBUG((DEBUG_VERBOSE, "--- File->Type: 0x%x\n", file->Type));
        DEBUG((DEBUG_VERBOSE, "--- File->Size: 0x%x\n", size));
#endif

        //
        // File type match?
        //
        if (file->Type == FileType)
        {
            DEBUG((DEBUG_VERBOSE, "--- Found Type 0x%x\n", FileType));
            *FoundFile = file;
            status = EFI_SUCCESS;
            break;
        }

        //
        // Move to end of current file.
        //
        currentAddress += size;
        if (currentAddress >= endOfVolume)
        {
            DEBUG((EFI_D_ERROR, "--- End of Volume hit before file found\n"));
            status = EFI_NOT_FOUND;
            goto Cleanup;
        }
    }

Cleanup:

    DEBUG((DEBUG_VERBOSE,
           "<<< FindFfsFile(0x%x, 0x%x, 0x%x) result 0x%x\n",
           (UINT32)(UINTN)Fv,
           (UINT32)(UINTN)FileType,
           (UINT32)(UINTN)*FoundFile,
           status
           ));

    return status;
}


EFI_STATUS
FindFfsFileSection(
        EFI_PHYSICAL_ADDRESS        StartOfFile,
        EFI_PHYSICAL_ADDRESS        EndOfFile,
        EFI_SECTION_TYPE            SectionType,
    OUT EFI_COMMON_SECTION_HEADER   **FoundSection
    )
/*++

Routine Description:

    Finds a section of the desired type within a firmware file.

Arguments:

    StartOfFile - The beginning of the file data to search.

    EndOfFile - The end of the file data to search.

    SectionType - The section type desired.

    FoundSection - Returns a pointer to the found section header.

Return Value:

    EFI_SUCCESS             - The desired section was found.
    EFI_NOT_FOUND           - The desired section was not found.
    EFI_VOLUME_CORRUPTED    - The file structure was not valid.

--*/
{
    EFI_STATUS                   status;
    EFI_PHYSICAL_ADDRESS         currentAddress;
    EFI_COMMON_SECTION_HEADER   *section;
    UINT32                       size;

    DEBUG((DEBUG_VERBOSE,
           ">>> FindFfsFileSection(0x%lx, 0x%lx, 0x%lx)\n",
           StartOfFile,
           EndOfFile,
           SectionType
           ));

    //
    // Point cursor at start of file.
    //
    currentAddress = StartOfFile;

    //
    // Loop through the sections in the file
    //
    for (;;)
    {
        //
        // Section headers are 4 byte aligned.
        //
        currentAddress = (currentAddress + 3) & ~(3ULL);
        if (currentAddress >= EndOfFile)
        {
            DEBUG((EFI_D_ERROR, "--- Aligned section address past file end\n"));
            status = EFI_VOLUME_CORRUPTED;
            goto Cleanup;
        }

        //
        // Validate section header.
        //
        section = (EFI_COMMON_SECTION_HEADER*)(UINTN)currentAddress;
        size = Expand3ByteSize(section->Size);

#if defined(SECMAIN_DEBUG_NOISY)
        DEBUG((DEBUG_VERBOSE, "--- Section: 0x%x\n", section));
        DEBUG((DEBUG_VERBOSE, "--- Section->Type: 0x%x\n", section->Type));
        DEBUG((DEBUG_VERBOSE, "--- Section->Size: 0x%x\n", size));
#endif

        if (size < sizeof(EFI_COMMON_SECTION_HEADER))
        {
            DEBUG((EFI_D_ERROR, "--- Section size too small\n"));
            status = EFI_VOLUME_CORRUPTED;
            goto Cleanup;
        }
        if ((currentAddress + size) > EndOfFile)
        {
            DEBUG((EFI_D_ERROR, "--- Section size exceeds end of file\n"));
            status = EFI_VOLUME_CORRUPTED;
            goto Cleanup;
        }

        //
        // Section type match?
        //
        if (section->Type == SectionType)
        {
            DEBUG((DEBUG_VERBOSE, "--- Found Type 0x%x\n", SectionType));
            *FoundSection = section;
            status = EFI_SUCCESS;
            break;
        }

        //
        // Move cursor to end of this section.
        //
        currentAddress += size;
        if (currentAddress >= EndOfFile)
        {
            DEBUG((EFI_D_ERROR, "--- End of File before section found\n"));
            status = EFI_NOT_FOUND;
            goto Cleanup;
        }
    }

Cleanup:

    DEBUG((DEBUG_VERBOSE,
           "<<< FindFfsFileSection(0x%x, 0x%x, 0x%x, 0x%x) result 0x%x\n",
           (UINT32)(UINTN)StartOfFile,
           (UINT32)(UINTN)EndOfFile,
           (UINT32)(UINTN)SectionType,
           (UINT32)(UINTN)*FoundSection,
           status
           ));

    return status;
}


EFI_STATUS
EFIAPI
FindImageBaseInFv(
    IN  EFI_FIRMWARE_VOLUME_HEADER  *Fv,
        EFI_FV_FILETYPE             FileType,
    OUT EFI_PHYSICAL_ADDRESS        *ImageBase
    )
/*++

Routine Description:

    Finds the image base (entrypoint) in a particular file type in
    a firmware volume.

Arguments:

    Fv - A pointer to the firmware volume header.

    FileType - The file type containing the desired image.

    ImageBase - Returns the address of the entrypoint of the image.

Return Value:

    EFI_SUCCESS             - the file and entrypoint were found.
    EFI_NOT_FOUND           - the file and/or entrypoint were not found.
    EFI_VOLUME_CORRUPTED    - The volume, file or section was not valid.

--*/
{
    EFI_STATUS                  status;
    EFI_FFS_FILE_HEADER         *file;
    EFI_COMMON_SECTION_HEADER   *section;

    DEBUG((DEBUG_VERBOSE,
           ">>> FindImageBaseInFv(0x%lx, 0x%lx, 0x%lx)\n",
           (EFI_PHYSICAL_ADDRESS)Fv,
           FileType,
           *ImageBase
           ));

#if defined(SECMAIN_DEBUG_NOISY)
    DebugVolDump((EFI_PHYSICAL_ADDRESS)Fv, (UINT32)Fv->FvLength, L"");
#endif

    //
    // Find the file type specified.
    //
    status = FindFfsFile(Fv,
                         FileType,
                         &file);

    if (status != EFI_SUCCESS)
    {
        goto Cleanup;
    }

    //
    // First look for a PE32 section.
    //
    status = FindFfsFileSection((EFI_PHYSICAL_ADDRESS)(UINTN)(file + 1),
                                (EFI_PHYSICAL_ADDRESS)(UINTN)file + Fv->FvLength,
                                EFI_SECTION_PE32,
                                &section);

    if (status == EFI_NOT_FOUND)
    {
        //
        // Alternative is a TE section.
        //
        status = FindFfsFileSection((EFI_PHYSICAL_ADDRESS)(UINTN)(file + 1),
                                    (EFI_PHYSICAL_ADDRESS)(UINTN)file + Fv->FvLength,
                                    EFI_SECTION_TE,
                                    &section);
        if (status != EFI_SUCCESS)
        {
            goto Cleanup;
        }
    }

    //
    // Image base is immediately following the section header.
    //
    *ImageBase = (EFI_PHYSICAL_ADDRESS)(UINTN)(section + 1);

Cleanup:

    DEBUG((DEBUG_VERBOSE,
           "<<< FindImageBaseInFv(0x%x, 0x%x, 0x%x) result 0x%x\n",
           (UINT32)(UINTN)Fv,
           FileType,
           (UINT32)(UINTN)*ImageBase,
           status
           ));

    return status;
}


EFI_STATUS
EFIAPI
FindPeiCoreImageBase (
    IN  EFI_FIRMWARE_VOLUME_HEADER  *SecCoreFv,
    OUT EFI_FIRMWARE_VOLUME_HEADER  **PeiCoreFv,
    OUT EFI_PHYSICAL_ADDRESS        *PeiCoreImageBase
  )
/*++

Routine Description:

    Finds and output the firmware volume containing the PEI CORE file and the
    PEI image base (entrypoint).

Arguments:

    SecCoreFv - The SEC firmware volume (start point for the search).

    PeiCoreFv - Returns a pointer to the MAIN firmware volume containing PEI.

    PeiCoreImageBase - Returns an address to the PEI image base (entrypoint).

Return Value:

    EFI_SUCCESS             - The PEI image base was found.
    EFI_NOT_FOUND           - The PEI image base was not found.
    EFI_VOLUME_CORRUPTED    - The volume, file, or sections were not valid.

--*/
{
    EFI_STATUS                  status;
    EFI_FIRMWARE_VOLUME_HEADER  *mainFv;
    EFI_PHYSICAL_ADDRESS        peiCoreImageBase;

    DEBUG((DEBUG_VERBOSE,
           ">>> FindPeiCoreImageBase(0x%x, 0x%x, 0x%x)\n",
           (UINT32)(UINTN)SecCoreFv,
           (UINT32)(UINTN)*PeiCoreFv,
           (UINT32)(UINTN)*PeiCoreImageBase
           ));

    //
    // Find the MAIN volume.
    //
    status = FindMainFv(SecCoreFv, &mainFv);
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR,
               "--- FindPeiCoreImageBase failed to find main volume\n"));
        goto Cleanup;
    }

    DEBUG((DEBUG_VERBOSE,
           "--- FindPeiCoreImageBase found main FV @ 0x%x\n",
           (UINT32)(UINTN)mainFv
           ));

    //
    // Find the PEI image base.
    //
    status = FindImageBaseInFv(mainFv, EFI_FV_FILETYPE_PEI_CORE, &peiCoreImageBase);
    if (EFI_ERROR(status))
    {
        DEBUG((EFI_D_ERROR,
            "--- FindPeiCoreImageBase failed to find PEI CORE image base\n"));
        goto Cleanup;
    }

    DEBUG((DEBUG_VERBOSE,
           "--- FindPeiCoreImageBase PEI CORE image base 0x%x\n",
           (UINT32)(UINTN)peiCoreImageBase
           ));

    //
    // Output the volume and image base location.
    //
    *PeiCoreFv = mainFv;
    *PeiCoreImageBase = peiCoreImageBase;

Cleanup:

    DEBUG((DEBUG_VERBOSE,
           "<<< FindPeiCoreImageBase(0x%x, 0x%x, 0x%x) result 0x%x\n",
           (UINT32)(UINTN)SecCoreFv,
           (UINT32)(UINTN)*PeiCoreFv,
           (UINT32)(UINTN)*PeiCoreImageBase,
           status
           ));

    return status;
}


VOID
EFIAPI
FindAndReportEntryPoints (
    IN  EFI_FIRMWARE_VOLUME_HEADER  *SecCoreFv,
    OUT EFI_FIRMWARE_VOLUME_HEADER  **PeiCoreFv,
    OUT EFI_PEI_CORE_ENTRY_POINT    *PeiCoreEntryPoint
    )
/*++

Routine Description:

    Finds and outputs the PEI firmware volume and the PEI entrypoint.

    This function also reports the SEC and PEI image locations to the debugger.

Arguments:

    SecCoreFv - A pointer to the SEC firmware volume.

    PeiCoreFv - Returns a pointer to the PEI firmware volume.

    PeiCoreEntryPoint - Returns a pointer to the PEI entry point.

Return Value:

    EFI_SUCCESS             - the PEI entry point was found.
    EFI_NOT_FOUND           - the PEI entry point was not found.
    EFI_VOLUME_CORRUPTED    - the volume, file, or section was not valid.


--*/
{
    EFI_STATUS                      status;
    EFI_PHYSICAL_ADDRESS            secCoreImageBase;
    EFI_PHYSICAL_ADDRESS            peiCoreImageBase;
    PE_COFF_LOADER_IMAGE_CONTEXT    imageContext;

    DEBUG((DEBUG_VERBOSE,
           ">>> FindAndReportEntryPoints(0x%x, 0x%x, 0x%x)\n",
           (UINT32)(UINTN)SecCoreFv,
           (UINT32)(UINTN)*PeiCoreFv,
           (UINT32)(UINTN)*PeiCoreEntryPoint
           ));

    //
    // Find SEC Core image base just so it can be passed to debugger.
    //
    status = FindImageBaseInFv(SecCoreFv, EFI_FV_FILETYPE_SECURITY_CORE,  &secCoreImageBase);
    ASSERT_EFI_ERROR(status);
    if (EFI_ERROR(status)) CpuDeadLoop();

    //
    // Find PEI Core image base.
    //
    status = FindPeiCoreImageBase(SecCoreFv, PeiCoreFv, &peiCoreImageBase);
    ASSERT_EFI_ERROR(status);
    if (EFI_ERROR(status)) CpuDeadLoop();

    //
    // Report SEC Core debug information.
    //
    ZeroMem((VOID *)&imageContext, sizeof(PE_COFF_LOADER_IMAGE_CONTEXT));
    imageContext.ImageAddress = secCoreImageBase;
    imageContext.PdbPointer = PeCoffLoaderGetPdbPointer((VOID*)(UINTN)imageContext.ImageAddress);
    PeCoffLoaderRelocateImageExtraAction(&imageContext);

    //
    // Report PEI Core debug information.
    //
    imageContext.ImageAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)peiCoreImageBase;
    imageContext.PdbPointer = PeCoffLoaderGetPdbPointer((VOID*)(UINTN)imageContext.ImageAddress);
    PeCoffLoaderRelocateImageExtraAction(&imageContext);

    //
    // Find PEI Core entry point in the image.
    //
    status = PeCoffLoaderGetEntryPoint((VOID *)(UINTN)peiCoreImageBase, (VOID**)PeiCoreEntryPoint);
    if (EFI_ERROR(status))
    {
        *PeiCoreEntryPoint = 0;
    }

    DEBUG((DEBUG_VERBOSE,
           "<<< FindAndReportEntryPoints(0x%x, 0x%x, 0x%x)\n",
           (UINT32)(UINTN)SecCoreFv,
           (UINT32)(UINTN)*PeiCoreFv,
           (UINT32)(UINTN)*PeiCoreEntryPoint
           ));

    return;
}


VOID
EFIAPI
SecStartupPhase2(
    IN VOID *Context
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
    EFI_SEC_PEI_HAND_OFF        *SecCoreData;
    EFI_FIRMWARE_VOLUME_HEADER  *PeiCoreFv;
    EFI_PEI_CORE_ENTRY_POINT    PeiCoreEntryPoint;

    SecCoreData = (EFI_SEC_PEI_HAND_OFF *)Context;

    DEBUG((DEBUG_VERBOSE,
           ">>> SecStartupPhase2 @ %p (%p)\n",
           SecStartupPhase2,
           Context));

    //
    // Find PEI Core entry point.
    // Also reports SEC and Pei Core debug information if remote debug is enabled.
    //
    FindAndReportEntryPoints((EFI_FIRMWARE_VOLUME_HEADER *)SecCoreData->BootFirmwareVolumeBase,
                             &PeiCoreFv,
                             &PeiCoreEntryPoint);

    //
    // Pass on PEI volume info to PEI.
    //
    SecCoreData->BootFirmwareVolumeBase = PeiCoreFv;
    SecCoreData->BootFirmwareVolumeSize = (UINTN) PeiCoreFv->FvLength;

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
SecCoreStartupWithStack (
    IN          EFI_FIRMWARE_VOLUME_HEADER              *BootFv,
    IN          VOID                                    *TopOfCurrentStack,
    IN          PHV_HYPERVISOR_ISOLATION_CONFIGURATION  IsolationConfiguration,
    IN OPTIONAL VOID                                    *UefiIgvmConfigHeader
    )
/*++

Routine Description:

    'C' Entrypoint in the SEC phase code.

Arguments:

    BootFv - A pointer to the boot firmware volume.

    TopOfCurrentStack - The top of the current stack.

    IsolationConfiguration - Supplies the isolation configuration of the
        current partition.

Return Value:

    None.

--*/
{
    EFI_SEC_PEI_HAND_OFF    SecCoreData;
    UINT64                  Handler;
    SEC_IDT_TABLE           IdtTableInStack;
    IA32_DESCRIPTOR         IdtDescriptor;
    UINT32                  Index;
    UINT32                  Vector;

    DEBUG((DEBUG_VERBOSE, "\x1b")); // clear screen and scrollback
    DEBUG((DEBUG_VERBOSE, "c\x1b"));
    DEBUG((DEBUG_VERBOSE, "[3J"));
    DEBUG((DEBUG_VERBOSE,
           ">>> SecCoreStartupWithStack @ %p (%p, %p)\n",
           SecCoreStartupWithStack,
           BootFv,
           TopOfCurrentStack
           ));

    //
    // Initialize floating point operating environment
    // to be compliant with UEFI spec.
    //
    InitializeFloatingPointUnits ();

    //
    // Initialize IDT.  No valid vectors exist, so mark every entry as invalid.
    //
    IdtTableInStack.PeiService = NULL;
    for (Index = 0; Index < SEC_IDT_ENTRY_COUNT; Index ++)
    {
        IdtTableInStack.IdtTable[Index].Bits.GateType = 0;
    }

    //
    // If this is a hardware-isolated VM with no paravisor, then install a
    // minimal isolation exception handler to enable PEI core services to
    // function.
    //

    mIsolationConfiguration = *IsolationConfiguration;

    Handler = 0;
    Vector = 0;

    if (mIsolationConfiguration.ParavisorPresent == 0)
    {
        if (mIsolationConfiguration.IsolationType == HV_PARTITION_ISOLATION_TYPE_SNP)
        {
            //
            // #VC is exception vector 29.
            //

            Handler = (UINTN)SecVirtualCommunicationExceptionHandler;
            Vector = 29;

            if (!SecInitializeHardwareIsolation(UefiIsolationTypeSnp, UefiIgvmConfigHeader))
            {
                return;
            }
        }
        else if (mIsolationConfiguration.IsolationType == HV_PARTITION_ISOLATION_TYPE_TDX)
        {
            //
            // #VE is exception vector 20.
            //

            Handler = (UINTN)SecVirtualizationExceptionHandler;
            Vector = 20;

            if (!SecInitializeHardwareIsolation(UefiIsolationTypeTdx, UefiIgvmConfigHeader))
            {
                return;
            }
        }
    }

    if (Handler != 0)
    {
        IdtTableInStack.IdtTable[Vector].Uint128.Uint64 = 0;
        IdtTableInStack.IdtTable[Vector].Uint128.Uint64_1 = 0;
        IdtTableInStack.IdtTable[Vector].Bits.OffsetLow = (UINT16)Handler;
        IdtTableInStack.IdtTable[Vector].Bits.OffsetHigh = (UINT16)(Handler >> 16);
        IdtTableInStack.IdtTable[Vector].Bits.OffsetUpper = Handler >> 32;
        IdtTableInStack.IdtTable[Vector].Bits.Selector = (UINT16)AsmReadCs();
        IdtTableInStack.IdtTable[Vector].Bits.GateType = IA32_IDT_GATE_TYPE_INTERRUPT_32;
    }

    IdtDescriptor.Base  = (UINTN)&IdtTableInStack.IdtTable;
    IdtDescriptor.Limit = (UINT16)(sizeof (IdtTableInStack.IdtTable) - 1);

    AsmWriteIdtr (&IdtDescriptor);

    //
    // |-------------|       <-- TopOfCurrentStack
    // |   Stack     | 32k
    // |-------------|
    // |    Heap     | 32k
    // |-------------|       <-- SecCoreData.TemporaryRamBase
    //

    //
    // Initialize SEC hand-off state
    //
    SecCoreData.DataSize = sizeof(EFI_SEC_PEI_HAND_OFF);

    SecCoreData.TemporaryRamSize       = SIZE_64KB;
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
    // Initialize Debug Agent to support source level debug in SEC/PEI phases before memory ready.
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
    IA32_DESCRIPTOR                  IdtDescriptor;
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
    // Rebase IDT table in permanent memory
    //
    AsmReadIdtr(&IdtDescriptor);
    IdtDescriptor.Base = IdtDescriptor.Base - (UINTN)OldStack + (UINTN)NewStack;

    AsmWriteIdtr(&IdtDescriptor);

    //
    // Use SetJump()/LongJump() to switch to a new stack.
    //
    if (SetJump (&JumpBuffer) == 0)
    {
        JumpBuffer.Rsp = JumpBuffer.Rsp + DebugAgentContext.StackMigrateOffset;
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
