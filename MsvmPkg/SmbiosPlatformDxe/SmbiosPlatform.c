/*++
    This module is responsible for creating the SMBIOS table.
    This driver will make a best effort to add all the SMBIOS v3.1 required
    structures. Failure is not fatal and may result in some of the required
    structures to not be installed.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/ConfigLib.h>
#include <Protocol/Smbios.h>
#include <IndustryStandard/SmBios.h>
#include <IndustryStandard/Acpi.h>

#define MAJOR_RELEASE_VERSION 4
#define MINOR_RELEASE_VERSION 1
#if defined(DEBUG_PLATFORM)
static CHAR8 RELEASE_VERSION_STRING[] = "Hyper-V UEFI DEBUG BUILD";
#else
static CHAR8 RELEASE_VERSION_STRING[] = "Hyper-V UEFI Release v4.1";
#endif
static CHAR8 RELEASE_DATE_STRING[] = "mm/dd/yyyy";

//
// Complying with SMBIOS v3.1 specification.
//
#define TARGETTED_SMBIOS_MAJOR_VERSION 3
#define TARGETTED_SMBIOS_MINOR_VERSION 1

//
// Implementation specific constant strings.
//
static CHAR8 MANUFACTURER_STRING[]    = "Microsoft Corporation";
static CHAR8 VIRTUAL_MACHINE_STRING[] = "Virtual Machine";
static CHAR8 NONE_STRING[]            = "None";

//
// Memory device location string size including null.
// Naming convention is "MXXXX" where XXXX are hex digits.
//
#define LOCATION_STRING_SIZE 6
static CHAR8 LOCATION_STRING_PRIMARY_MEMORY_DEVICE[] = "M0001";

//
// Maximum SMBIOS memory regions to create.
// 0xFFFF is more than enough for any anticipated memory scale.
// LOCATION_STRING_SIZE above is dependent on this max.
//
static const UINT64 gMaxMemoryRegions = 0xFFFFULL;

//
// Maximum memory size per SMBIOS v3.1 memory device.
// 30 bits in megabyte units, so max 2147 terabytes per device.
//
static const UINT64 gMaxSizePerMemoryDevice = (0x7FFFFFFFULL * SIZE_1MB);

//
// Helper macro to init SMBIOS structure header.
//
#define STANDARD_HEADER(type, typeId) { typeId, sizeof(type), SMBIOS_HANDLE_PI_RESERVED }

//
// Context structure for AddMemoryRegionsFromMemoryRange function.
//
typedef struct {
    UINT64                  CurrentRegion;
    EFI_SMBIOS_PROTOCOL*    Smbios;
    EFI_SMBIOS_HANDLE       PhysicalMemoryArrayHandle;
} ADD_MEMORY_REGIONS_CONTEXT;

//
// Callback definition for EnumerateMemoryRanges function.
//
typedef
VOID
(*ENUMERATE_MEMMAP_CALLBACK)(
    BOOLEAN LegacyMemoryMap,
    VOID *Range,
    VOID *Context
);

CHAR8*
LoadPcdSmbiosString(
    UINT64 StringAddress,
    UINT32 StringLength,
    UINT32 MaxLength
    )
/*++

Routine Description:

    Utility function to get and truncate a string from a PCD value.

Arguments:

    StringAddress - A pointer to the start of the string.

    StringLength - The length of the string.

    MaxLength - The maximum allowed length of the string, which will truncate
    the given string.

Return Value:

    Truncated string, or default None string if no string exists.

--*/
{
    CHAR8* String = (CHAR8*)(UINTN) StringAddress;

    if (StringLength == 0)
    {
        //
        // TLV struct for this string was not found, return the default None
        // string instead.
        //
        return NONE_STRING;
    }

    //
    // Truncate string by adding a null at the maximum allowed length.
    //
    if (MaxLength < StringLength)
    {
        String[MaxLength - 1] = 0;
    }

    return String;
}

VOID
NumberToMemoryLocationString(
        UINT16  Number,
    OUT CHAR8*  Buffer
)
/*++

Routine Description:

    Utility function to create a memory device location string.
    The string is of the form "Mxxxx" where xxxx is 0000 to FFFF.

Arguments:

    Number - A number value between 0 and FFFF (65535).

    String - A pointer to a pre-allocated string buffer of at least 6 bytes.

Return Value:

    n/a

--*/
{
    static const CHAR8* hexdigits = "0123456789ABCDEF";

    *Buffer++ = 'M';
    *Buffer++ = hexdigits[(Number >> 12) & 0xF];
    *Buffer++ = hexdigits[(Number >> 8) & 0xF];
    *Buffer++ = hexdigits[(Number >> 4) & 0xF];
    *Buffer++ = hexdigits[Number & 0xF];
    *Buffer = 0;
}


BOOLEAN
AddStructure(
    IN              EFI_SMBIOS_PROTOCOL*    Smbios,
    IN              VOID*                   Structure,
    IN OPTIONAL     CHAR8**                 Strings,
    OUT OPTIONAL    EFI_SMBIOS_HANDLE*      Handle
    )
/*++

Routine Description:

    Adds a structure to the global SMBIOS table.
    Optionally assists with appending the strings.
    Optionally returns the handle to the added structure for use in other structures.

Arguments:

    Smbios - The DXE Smbios protocol.

    Structure - The base or complete structure to add.

    Strings - Optional strings that will be copied at the end of the
              Structure before adding to the SMBIOS table.
              Structure must have sufficient space beyond its Length
              for the strings.

    Handle - Optionally returns the handle of the newly added structure.

Return Value:

    TRUE on success.

--*/
{
    EFI_SMBIOS_HANDLE handle = SMBIOS_HANDLE_PI_RESERVED;
    EFI_SMBIOS_TABLE_HEADER* header = (EFI_SMBIOS_TABLE_HEADER*) Structure;
    UINT32 index;
    CHAR8* destination = (CHAR8*)Structure + header->Length;

    //
    // Optionally copy the strings to end of table.
    // If no strings supplied then caller is expected to already have
    // appended strings and structure terminator.
    //
    if (Strings != NULL)
    {
        //
        // Append each string including its terminating null byte.
        //
        for (index = 0; Strings[index] != NULL; index++)
        {
            CHAR8* source = Strings[index];

            while((*destination++ = *source++) != '\0');
        }

        //
        // Finalize structure terminator.  The last string has a null byte so
        // this additional one results in two null bytes at the end of the structure.
        //
        *destination++ = '\0';
    }

    //
    // Add the structure to the table.
    //
    if (EFI_ERROR(Smbios->Add(Smbios, NULL, &handle, Structure)))
    {
        return FALSE;
    }

    //
    // Optionally output the structure's handle.
    //
    if (Handle != NULL)
    {
        *Handle = handle;
    }

    return TRUE;
}


void
DateToSmbiosDate(
    IN  char    *Source,
        size_t  SourceSizeInBytes,
    IN  char    *Dest,
        size_t  DestSizeInBytes
    )
/*++

Routine Description:

    Converts the compiler __DATE__ macro date format string to an SMBIOS date format string.

Arguments:

    Source - Pointer to the source date string.

    SourceSizeInBytes - Size of the source string buffer.

    Dest - Pointer to the destination date string.

    DestSizeInBytes - Size of the destination string buffer.

Return Value:

    none

Notes:

    If either pointer is null or sizes are not big enough no modification to the
    Dest buffer will be made.

--*/
{
    // Source format: "Mmm dd yyyy" e.g. "Jun  8 2017" or "Feb 23 1956"
    // Output format: "mm/dd/yyyy"  e.g. "06/08/2017"  or "05/17/1956"

    char smbiosDate[sizeof("mm/dd/yyyy")];

    if (SourceSizeInBytes < sizeof("Mmm dd yyyy") ||
        DestSizeInBytes < ARRAY_SIZE(smbiosDate))
    {
        return;
    }

    // Month
    smbiosDate[0] = '0';
    switch (Source[2])
    {
    case 'n':    // Jan Jun
        if (Source[1] == 'a')
        {
            smbiosDate[1] = '1';
        }
        else
        {
            smbiosDate[1] = '6';
        }
        break;
    case 'b':    // Feb
        smbiosDate[1] = '2';
        break;
    case 'r':    // Mar Apr
        if (Source[1] == 'a')
        {
            smbiosDate[1] = '3';
        }
        else
        {
            smbiosDate[1] = '4';
        }
        break;
    case 'y':    // May
        smbiosDate[1] = '5';
        break;
    case 'l':    // Jul
        smbiosDate[1] = '7';
        break;
    case 'g':    // Aug
        smbiosDate[1] = '8';
        break;
    case 'p':    // Sep
        smbiosDate[1] = '9';
        break;
    case 't':    // Oct
        smbiosDate[0] = '1';
        smbiosDate[1] = '0';
        break;
    case 'v':    // Nov
        smbiosDate[0] = '1';
        smbiosDate[1] = '1';
        break;
    default:    // Dec
        smbiosDate[0] = '1';
        smbiosDate[1] = '2';
        break;
    }

    // day
    smbiosDate[2] = '/';
    if (Source[4] == ' ')
    {
        smbiosDate[3] = '0';
    }
    else
    {
        smbiosDate[3] = Source[4];
    }
    smbiosDate[4] = Source[5];


    // year
    smbiosDate[5] = '/';
    smbiosDate[6] = Source[7];
    smbiosDate[7] = Source[8];
    smbiosDate[8] = Source[9];
    smbiosDate[9] = Source[10];

    smbiosDate[10] = 0;

    (void)AsciiStrCpyS(Dest, DestSizeInBytes, smbiosDate);
}


VOID
AddBiosInformation(
    IN  EFI_SMBIOS_PROTOCOL* Smbios
    )
/*++

Routine Description:

    Adds the BIOS Information structure (type 0) to the SMBIOS table.

Arguments:

    Smbios - The DXE Smbios protocol.

Return Value:

    n/a

--*/
{
    //
    // Initialized array of pointers to the strings for this structure.
    //
    static CHAR8* strings[] =
    {
        MANUFACTURER_STRING,
        RELEASE_VERSION_STRING,
        RELEASE_DATE_STRING,
        (CHAR8*)NULL
    };

    //
    // Initialized structure with string space appended and extra terminator byte.
    //
    static struct
    {
        SMBIOS_TABLE_TYPE0 Formatted;
        CHAR8 Unformed[sizeof(MANUFACTURER_STRING) +
                       sizeof(RELEASE_VERSION_STRING) +
                       sizeof(RELEASE_DATE_STRING) +
                       1];
    } biosInformation =

    {
        {
            STANDARD_HEADER(SMBIOS_TABLE_TYPE0, EFI_SMBIOS_TYPE_BIOS_INFORMATION),
            1,  // Vendor string index
            2,  // BIOS Version string index
            0,  // BIOS Starting Address Segment (meaningless for UEFI)
            3,  // BIOS Release Date string index
            0,  // BIOS ROM size (meaningless for UEFI)

            //
            // BiosCharacteristics
            //
            {
                0,  // Reserved (2 bits)
                0,  // Unknown
                1,  // BiosCharacteristicsNotSupported - yes
                0,  // IsaIsSupported - no
                0,  // McaIsSupported - no
                0,  // EisaIsSupported - no
                0,  // PciIsSupported - no
                0,  // PcmciaIsSupported - no
                1,  // PlugAndPlayIsSupported - yes
                0,  // ApmIsSupported - no
                0,  // BiosIsUpgradable - no
                0,  // BiosShadowingAllowed - no
                0,  // VlVesaIsSupported - no
                0,  // EscdSupportIsAvailable - no
                1,  // BootFromCdIsSupported - yes
                1,  // SelectableBootIsSupported - yes
                0,  // RomBiosIsSocketed - no
                0,  // BootFromPcmciaIsSupported - no
                1,  // EDDSpecificationIsSupported - no
                0,  // JapaneseNecFloppyIsSupported - no
                0,  // JapaneseToshibaFloppyIsSupported - no
                0,  // Floppy525_360IsSupported - no
                0,  // Floppy525_12IsSupported - no
                0,  // Floppy35_720IsSupported - no
                0,  // Floppy35_288IsSupported - no
                0,  // Floppy35_720IsSupported - no
                0,  // PrintScreenIsSupported - no
                1,  // SerialIsSupported - yes
                0,  // PrinterIsSupported - no
                0,  // CgaMonoIsSupported - no
                0,  // NecPc98 - no
                0,  // ReservedForVendor (32 bits)
            },

            //
            // BIOSCharacteristicsExtensionBytes
            //
            {
                1, // AcpiIsSupported
                0x1C // TargetContentDistributionEnabled, UefiSpecificationSupported, VirtualMachineSupported
            },

            MAJOR_RELEASE_VERSION,
            MINOR_RELEASE_VERSION,
            0xFF, // no field upgradeable embedded controller firmware
            0xFF, // no field upgradeable embedded controller firmware
            0, // Extended Bios ROM Size
        }
    };


    //
    // Fill in build date as release date.
    //
    DateToSmbiosDate(__DATE__, sizeof(__DATE__), strings[2], sizeof(RELEASE_DATE_STRING));

    //
    // Add the structure to the SMBIOS table. Error is not fatal and ignored.
    //
    (VOID)AddStructure(Smbios, &biosInformation, strings, NULL);
}


VOID
AddSystemInformation(
    IN  EFI_SMBIOS_PROTOCOL* Smbios
    )
/*++

Routine Description:

    Adds the System Information structure (type 1) to the SMBIOS table.

Arguments:

    Smbios - The DXE Smbios protocol.

Return Value:

    n/a

--*/
{
    //
    // Initialized array of pointers to the strings for this structure.
    //
    static CHAR8* strings[] =
    {
        MANUFACTURER_STRING,
        VIRTUAL_MACHINE_STRING,
        RELEASE_VERSION_STRING,
        "",
        NONE_STRING,
        VIRTUAL_MACHINE_STRING,
        NULL
    };

    //
    // Initialized structure with string space appended and extra terminator byte.
    //
    static struct
    {
        SMBIOS_TABLE_TYPE1 Formatted;
        CHAR8 Unformed[BiosInterfaceSmbiosStringMax + 1 + // Manufacturer
                       BiosInterfaceSmbiosStringMax + 1 + // Product Name
                       BiosInterfaceSmbiosStringMax + 1 + // Version
                       BiosInterfaceSmbiosStringMax + 1 + // Serial Number
                       BiosInterfaceSmbiosStringMax + 1 + // SKU Number
                       BiosInterfaceSmbiosStringMax + 1 + // Family
                       1];
    } systemInformation =

    {
        {
            STANDARD_HEADER(SMBIOS_TABLE_TYPE1, EFI_SMBIOS_TYPE_SYSTEM_INFORMATION),
            1, // Manufacturer string index
            2, // Product Name string index
            3, // Version string index
            4, // Serial Number string index
            { 0 },
            SystemWakeupTypePowerSwitch,
            5, // SKU Number string index
            6  // Family string index
        }
    };

    //
    // Add the dynamic system information table fields to the structure.
    // If the user passed in field values manually, or simply wants the
    // host SMBIOS values mirrored, then update the corresponding strings.
    // If not, retain the default values.
    //

    UINT32 stringLength;
    //
    // Add the System Manufacturer string.
    //
    stringLength = PcdGet32(PcdSmbiosSystemManufacturerSize);

    if (stringLength)
    {
        strings[0] =
        LoadPcdSmbiosString(PcdGet64(PcdSmbiosSystemManufacturerStr),
                            stringLength,
                            BiosInterfaceSmbiosStringMax + 1);
    }

    //
    // Add the System Product Number string.
    //
    stringLength = PcdGet32(PcdSmbiosSystemProductNameSize);

    if (stringLength)
    {
        strings[1] =
        LoadPcdSmbiosString(PcdGet64(PcdSmbiosSystemProductNameStr),
                            stringLength,
                            BiosInterfaceSmbiosStringMax + 1);
    }

    //
    // Add the System Version string.
    //
    stringLength = PcdGet32(PcdSmbiosSystemVersionSize);

    if (stringLength)
    {
        strings[2] =
        LoadPcdSmbiosString(PcdGet64(PcdSmbiosSystemVersionStr),
                            stringLength,
                            BiosInterfaceSmbiosStringMax + 1);
    }

    //
    // Add the System Serial Number string.
    // If it wasn't passed in, then we set the System Serial Number to the default - "None".
    //
    strings[3] =
        LoadPcdSmbiosString(PcdGet64(PcdSmbiosSystemSerialNumberStr),
                            PcdGet32(PcdSmbiosSystemSerialNumberSize),
                            BiosInterfaceSmbiosStringMax + 1);

    //
    // Add the System SKU Number string.
    //
    stringLength = PcdGet32(PcdSmbiosSystemSKUNumberSize);

    if (stringLength)
    {
        strings[4] =
        LoadPcdSmbiosString(PcdGet64(PcdSmbiosSystemSKUNumberStr),
                            stringLength,
                            BiosInterfaceSmbiosStringMax + 1);
    }

    //
    // Add the System Family string.
    //
    stringLength = PcdGet32(PcdSmbiosSystemFamilySize);

    if (stringLength)
    {
        strings[5] =
        LoadPcdSmbiosString(PcdGet64(PcdSmbiosSystemFamilyStr),
                            stringLength,
                            BiosInterfaceSmbiosStringMax + 1);
    }

    CopyMem(&systemInformation.Formatted.Uuid, (VOID*)(UINTN) PcdGet64(PcdBiosGuidPtr), sizeof(EFI_GUID));

    //
    // Add the structure to the SMBIOS table. Error is not fatal and ignored.
    //
    (VOID)AddStructure(Smbios, &systemInformation, strings, NULL);
}


BOOLEAN
AddSystemEnclosure(
    IN  EFI_SMBIOS_PROTOCOL* Smbios,
    OUT EFI_SMBIOS_HANDLE*   ChassisHandle
    )
/*++

Routine Description:

    Adds the System Enclosure structure (type 3) to the SMBIOS table.

Arguments:

    Smbios - The DXE Smbios protocol.

    ChassisHandle - Returns the handle of the newly added structure.

Return Value:

    TRUE on success.

--*/
{
    //
    // Initialized array of pointers to the strings for this structure.
    //
    static CHAR8* strings[] =
    {
        MANUFACTURER_STRING,
        RELEASE_VERSION_STRING,
        "",
        "",
        VIRTUAL_MACHINE_STRING,
        NULL
    };

    //
    // Initialized structure with string space appended and extra terminator byte.
    //
    static struct
    {
        SMBIOS_TABLE_TYPE3 Formatted;
        CHAR8 Unformed[sizeof(MANUFACTURER_STRING) +
                       sizeof(RELEASE_VERSION_STRING) +
                       BiosInterfaceSmbiosStringMax + 1 +
                       BiosInterfaceSmbiosStringMax + 1 +
                       sizeof(VIRTUAL_MACHINE_STRING) +
                       2];
    } systemEnclosure =

    {
        {
            STANDARD_HEADER(SMBIOS_TABLE_TYPE3, EFI_SMBIOS_TYPE_SYSTEM_ENCLOSURE),
            1, // Manufacturer string index
            MiscChassisTypeDeskTop,
            2, // Version string index
            3, // Serial Number string index
            4, // Asset Tag Number string index
            ChassisStateSafe, // Boot-up State
            ChassisStateSafe, // Power Supply State
            ChassisStateSafe, // Thermal State
            ChassisSecurityStatusUnknown, // Security Status
            { 0 }, // OEM-defined
            0, // Height
            0, // Number of Power Cords
            0, // Contained Element Count
            0, // Contained Element Record Count
            // NOTE: Our System Enclosure structure has no contained elements,
            // so the contained elements value in this structure is actually the
            // SKU Number string index, as access to the SKU Number string
            // index is based on the above two values.
            { 5 }, // SKU Number string index
        }
    };

    //
    // Add the dynamic information to the structure.
    //
    strings[2] =
        LoadPcdSmbiosString(PcdGet64(PcdSmbiosChassisSerialNumberStr),
                            PcdGet32(PcdSmbiosChassisSerialNumberSize),
                            BiosInterfaceSmbiosStringMax + 1);

    strings[3] =
        LoadPcdSmbiosString(PcdGet64(PcdSmbiosChassisAssetTagStr),
                            PcdGet32(PcdSmbiosChassisAssetTagSize),
                            BiosInterfaceSmbiosStringMax + 1);

    //
    // Add the structure to the SMBIOS table.
    //
    return AddStructure(Smbios, &systemEnclosure, strings, ChassisHandle);
}


VOID
AddBaseboardInformation(
    IN  EFI_SMBIOS_PROTOCOL* Smbios,
    IN  EFI_SMBIOS_HANDLE    ChassisHandle
    )
/*++

Routine Description:

    Adds the Baseboard Information structure (type 2) to the SMBIOS table.

Arguments:

    Smbios - The DXE Smbios protocol.

    ChassisHandle - The handle of SystemEnclosure structure in which this
        baseboard resides.

Return Value:

    n/a

--*/
{
    //
    // Initialized array of pointers to the strings for this structure.
    //
    static CHAR8* strings[] =
    {
        MANUFACTURER_STRING,
        VIRTUAL_MACHINE_STRING,
        RELEASE_VERSION_STRING,
        "",
        NONE_STRING,
        VIRTUAL_MACHINE_STRING,
        NULL
    };

    //
    // Initialized structure with string space appended and extra terminator byte.
    //
    static struct
    {
        SMBIOS_TABLE_TYPE2 Formatted;
        CHAR8 Unformed[sizeof(MANUFACTURER_STRING) +
                       sizeof(VIRTUAL_MACHINE_STRING) +
                       sizeof(RELEASE_VERSION_STRING) +
                       BiosInterfaceSmbiosStringMax + 1 +
                       sizeof(NONE_STRING) +
                       sizeof(VIRTUAL_MACHINE_STRING) +
                       2];
    } baseboardInformation =

    {
        {
            STANDARD_HEADER(SMBIOS_TABLE_TYPE2, EFI_SMBIOS_TYPE_BASEBOARD_INFORMATION),
            1, // Manufacturer string index
            2, // Product string index
            3, // Version string index
            4, // Serial Number string index
            5, // Asset Tag string index
            {
                1   // Feature Flags - Motherboard
            },
            6, // Location in Chassis string index
            SMBIOS_HANDLE_PI_RESERVED, // Chassis Handle
            BaseBoardTypeMotherBoard, // Board Type
            0, // Number of Contained Object Handles
            SMBIOS_HANDLE_PI_RESERVED, // Contained Object Handles
        }
    };

    //
    // Add the dynamic information to the structure.
    //
    baseboardInformation.Formatted.ChassisHandle = ChassisHandle;

    strings[3] =
        LoadPcdSmbiosString(PcdGet64(PcdSmbiosBaseSerialNumberStr),
                            PcdGet32(PcdSmbiosBaseSerialNumberSize),
                            BiosInterfaceSmbiosStringMax + 1);

    //
    // Add the structure to the SMBIOS table. Error is not fatal and ignored.
    //
    (VOID)AddStructure(Smbios, &baseboardInformation, strings, NULL);
}

VOID
AddProcessorInformation(
    IN  EFI_SMBIOS_PROTOCOL* Smbios
    )
/*++

Routine Description:

    Adds the ProcessorInformation structures (type 4) to the SMBIOS table.
    One per virtual processor.

Arguments:

    Smbios - The DXE Smbios protocol.

Return Value:

   n/a

--*/
{
    // The PCDs below are unfortunately named because 'processor' doesn't always mean the same thing.
    //  Each PCD is guaranteed non-zero in Config.c
    // For these, 'processor' means logical processor
    UINT16 lpCount = (UINT16) PcdGet32(PcdProcessorCount);
    UINT16 lpsPerVirtualSocket = (UINT16) PcdGet32(PcdProcessorsPerVirtualSocket);
    // This means threads per core (physical processor)
    UINT16 hwThreadsPerCore = (UINT16) PcdGet32(PcdThreadsPerProcessor);

    // Divide the processors equally between the sockets
    UINT16 totalSocketCount = (lpCount + lpsPerVirtualSocket - 1) / lpsPerVirtualSocket;
    UINT16 lpsPerSocketQuotient = lpCount / totalSocketCount;
    UINT16 lpsPerSocketRemainder = lpCount % totalSocketCount;

    //
    // Initialized array of pointers to the strings for this structure.
    //
    static CHAR8* strings[] =
    {
        "",
        "",
        "",
        "",
        "",
        "",
        NULL
    };

    static struct
    {
        SMBIOS_TABLE_TYPE4 Formatted;
        CHAR8 Unformed[((MAX_SMBIOS_STRING_LENGTH + 1) * 6) + 1];
    } cpuInfo =
    {
        {
            STANDARD_HEADER(SMBIOS_TABLE_TYPE4, EFI_SMBIOS_TYPE_PROCESSOR_INFORMATION),
            1, // SocketDesignation string index
            0, // ProcessorType
            0, // ProcessorFamily
            2, // ProcessorManufacturer string index
            {0, 0, 0, 0, 0, 0, 0, 0}, // ProcessorId
            3, // ProcessorVersion string index
            {0}, // Voltage
            0, // ExternalClock
            0, // MaxSpeed
            0, // CurrentSpeed
            0, // Status
            0, // Upgrade
            0xFFFF, // L1 Cache Handle - Unknown
            0xFFFF, // L2 Cache Handle - Unknown
            0xFFFF, // L3 Cache Handle - Unknown
            4, // SerialNumber string index
            5, // AssetTag string index
            6, // PartNumber string index
            0, // CoreCount
            0, // CoreEnabled
            0, // ThreadCount
            0, // ProcessorCharacteristics
            0, // ProcessorFamily2
            0, // CoreCount2
            0, // CoreEnabled2
            0, // ThreadCount2
        }
    };

    // Set values and strings read in PEI via PCDs.
    cpuInfo.Formatted.ProcessorType            = PcdGet8(PcdSmbiosProcessorType);
    cpuInfo.Formatted.ExternalClock            = PcdGet16(PcdSmbiosProcessorExternalClock);
    cpuInfo.Formatted.MaxSpeed                 = PcdGet16(PcdSmbiosProcessorMaxSpeed);
    cpuInfo.Formatted.CurrentSpeed             = PcdGet16(PcdSmbiosProcessorCurrentSpeed);
    cpuInfo.Formatted.Status                   = PcdGet8(PcdSmbiosProcessorStatus);
    cpuInfo.Formatted.ProcessorUpgrade         = PcdGet8(PcdSmbiosProcessorUpgrade);
    cpuInfo.Formatted.ProcessorCharacteristics = PcdGet16(PcdSmbiosProcessorCharacteristics);
    cpuInfo.Formatted.ProcessorFamily2         = PcdGet16(PcdSmbiosProcessorFamily2);

    // Copy ProcessorId and Voltage using casts because they have explicit
    // structure types with no unions to access all the data.
    *((UINT64*)(&cpuInfo.Formatted.ProcessorId)) = PcdGet64(PcdSmbiosProcessorID);
    *((UINT8*)(&cpuInfo.Formatted.Voltage))      = PcdGet8(PcdSmbiosProcessorVoltage);

    // Set processor family, anything greater than 0xFE means to check ProcessorFamily2
    if (cpuInfo.Formatted.ProcessorFamily2 > 0xFE)
    {
        cpuInfo.Formatted.ProcessorFamily = 0xFE;
    }
    else
    {
        cpuInfo.Formatted.ProcessorFamily = (UINT8) cpuInfo.Formatted.ProcessorFamily2;
    }

    strings[0] =
        LoadPcdSmbiosString(PcdGet64(PcdSmbiosProcessorSocketDesignationStr),
                            PcdGet32(PcdSmbiosProcessorSocketDesignationSize),
                            MAX_SMBIOS_STRING_LENGTH + 1);

    strings[1] =
        LoadPcdSmbiosString(PcdGet64(PcdSmbiosProcessorManufacturerStr),
                            PcdGet32(PcdSmbiosProcessorManufacturerSize),
                            MAX_SMBIOS_STRING_LENGTH + 1);

    strings[2] =
        LoadPcdSmbiosString(PcdGet64(PcdSmbiosProcessorVersionStr),
                            PcdGet32(PcdSmbiosProcessorVersionSize),
                            MAX_SMBIOS_STRING_LENGTH + 1);

    strings[3] =
        LoadPcdSmbiosString(PcdGet64(PcdSmbiosProcessorSerialNumberStr),
                            PcdGet32(PcdSmbiosProcessorSerialNumberSize),
                            MAX_SMBIOS_STRING_LENGTH + 1);

    strings[4] =
        LoadPcdSmbiosString(PcdGet64(PcdSmbiosProcessorAssetTagStr),
                            PcdGet32(PcdSmbiosProcessorAssetTagSize),
                            MAX_SMBIOS_STRING_LENGTH + 1);

    strings[5] =
        LoadPcdSmbiosString(PcdGet64(PcdSmbiosProcessorPartNumberStr),
                            PcdGet32(PcdSmbiosProcessorPartNumberSize),
                            MAX_SMBIOS_STRING_LENGTH + 1);

    //
    // Add one CPU structure per socket.
    // The number of VPs (logical processors) is represented as the ThreadCount
    // We never expose disabled cores to the guest
    //
    UINT16 processorsAdded = 0;
    UINT16 socketCounter = 0;

    while (processorsAdded < lpCount)
    {
        UINT16 hwThreadCountInThisSocket = lpsPerSocketQuotient;

        if (lpsPerSocketRemainder > socketCounter)
        {
            // This socket gets an extra logical processor
            hwThreadCountInThisSocket++;
        }

        UINT16 enabledCoresInThisSocket = (hwThreadCountInThisSocket + hwThreadsPerCore - 1) / hwThreadsPerCore;

        cpuInfo.Formatted.CoreCount2 = enabledCoresInThisSocket;
        cpuInfo.Formatted.CoreCount = (UINT8) MIN(cpuInfo.Formatted.CoreCount2, 0xFF);

        cpuInfo.Formatted.EnabledCoreCount2 = enabledCoresInThisSocket;
        cpuInfo.Formatted.EnabledCoreCount = (UINT8) MIN(cpuInfo.Formatted.EnabledCoreCount2, 0xFF);

        cpuInfo.Formatted.ThreadCount2 = hwThreadCountInThisSocket;
        cpuInfo.Formatted.ThreadCount = (UINT8) MIN(cpuInfo.Formatted.ThreadCount2, 0xFF);

        //
        // Add the structure to the SMBIOS table. Error is not fatal and ignored.
        // Only copy the string table on adding the first structure.
        //
        (VOID)AddStructure(Smbios, &cpuInfo, (socketCounter == 0) ? strings : NULL, NULL);

        processorsAdded += hwThreadCountInThisSocket;
        socketCounter++;
    }

    ASSERT(processorsAdded == lpCount);
}


VOID
AddOEMStrings(
    IN  EFI_SMBIOS_PROTOCOL* Smbios
    )
/*++

Routine Description:

    Adds the OEM Strings structure (type 11) to the SMBIOS table.

Arguments:

    Smbios - The DXE Smbios protocol.

Return Value:

    n/a

--*/
{
    //
    // Prefix and Postfix strings for the BIOS lock string.
    //
    static CHAR8 OEM_STRING_1[] = "[MS_VM_CERT/SHA1/9b80ca0d5dd061ec9da4e494f4c3fd1196270c22]";
    static CHAR8 OEM_STRING_3[] = "To be filled by OEM";

    //
    // Initialized array of pointers to the strings for this structure.
    //
    static CHAR8* strings[] =
    {
        OEM_STRING_1,
        "",
        OEM_STRING_3,
        NULL
    };

    //
    // Initialized structure with string space appended and extra terminator byte.
    //
    static struct
    {
        SMBIOS_TABLE_TYPE11 Formatted;
        CHAR8 Unformed[sizeof(OEM_STRING_1) +
                       BiosInterfaceSmbiosStringMax + 1 +
                       sizeof(OEM_STRING_3) +
                       1];
    } oemStrings =

    {
        {
            STANDARD_HEADER(SMBIOS_TABLE_TYPE11, EFI_SMBIOS_TYPE_OEM_STRINGS),
            3 // string count
        }
    };

    //
    // Add the dynamic information to the structure.
    //
    strings[1] =
        LoadPcdSmbiosString(PcdGet64(PcdSmbiosBiosLockStringStr),
                            PcdGet32(PcdSmbiosBiosLockStringSize),
                            BiosInterfaceSmbiosStringMax + 1);

    //
    // Add the structure to the SMBIOS table. Error is not fatal and ignored.
    //
    (VOID)AddStructure(Smbios, &oemStrings, strings, NULL);
}


BOOLEAN
AddPhysicalMemoryArray(
    IN  EFI_SMBIOS_PROTOCOL*    Smbios,
    IN  EFI_SMBIOS_HANDLE       MemoryErrorHandle,
    OUT EFI_SMBIOS_HANDLE*      PhysicalMemoryArrayHandle,
    IN  UINT16                  PhysicalMemoryArraySize
    )
/*++

Routine Description:

    Adds a Physical Memory Array structure (type 16) to the SMBIOS table.

Arguments:

    Smbios - The DXE Smbios protocol.

    MemoryErrorHandle - The handle of the error information structure for this
        array.

    PhysicalMemoryArrayHandle - Returns the handle of the newly added structure.

    PhysicalMemoryArraySize - The size of the Physical Memory Array.

Return Value:

    TRUE on success.

--*/
{
    //
    // Initialized structure with extra terminator bytes (no strings).
    //
    static struct
    {
        SMBIOS_TABLE_TYPE16 Formatted;
        CHAR8 Unformed[2];
    } physicalMemoryArray =

    {
        {
            STANDARD_HEADER(SMBIOS_TABLE_TYPE16, EFI_SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY),
            MemoryArrayLocationSystemBoard, // Location
            MemoryArrayUseSystemMemory, // Use
            MemoryErrorCorrectionNone, // Memory Error Correction
            0x80000000, // Maximum Capacity - unknown
            SMBIOS_HANDLE_PI_RESERVED, // Memory Error Information Handle
            0, // Number of Memory Devices
            0  // Extended Maximum Capacity
        },
        {
            0, // terminator bytes
            0
        }
    };

    //
    // Add the dynamic information to the structure.
    //
    physicalMemoryArray.Formatted.MemoryErrorInformationHandle = MemoryErrorHandle;
    physicalMemoryArray.Formatted.NumberOfMemoryDevices = PhysicalMemoryArraySize;

    //
    // Add the structure to the SMBIOS table.
    //
    return AddStructure(Smbios, &physicalMemoryArray, NULL, PhysicalMemoryArrayHandle);
}


BOOLEAN
AddMemoryArrayMappedAddress(
    IN  EFI_SMBIOS_PROTOCOL*    Smbios,
    IN  UINT64                  BaseAddress,
    IN  UINT64                  Size,
    IN  EFI_SMBIOS_HANDLE       PhysicalMemoryArrayHandle,
    OUT EFI_SMBIOS_HANDLE*      MemoryArrayMappedAddressHandle
    )
/*++

Routine Description:

    Adds a Memory Array Mapped Address structure (type 19) to the SMBIOS table.

Arguments:

    Smbios - The DXE Smbios protocol.

    BaseAddress - The address where this memory array is mapped.

    Size - The amount of memory mapped (in bytes).

    PhysicalMemoryArrayHandle - The handle of the PhysicalMemoryArray structure for
        this mapping.

    MemoryArrayMappedAddressHandle - Returns the handle of the newly added structure.

Return Value:

    TRUE on success.

--*/
{
    //
    // Initialized structure with extra terminator bytes (no strings).
    //
    static struct
    {
        SMBIOS_TABLE_TYPE19 Formatted;
        CHAR8 Unformed[2];
    } memoryArrayMappedAddress =

    {
        {
            STANDARD_HEADER(SMBIOS_TABLE_TYPE19, EFI_SMBIOS_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS),
            0, // Starting Address
            0, // Ending Address
            SMBIOS_HANDLE_PI_RESERVED, // Memory Array Handle
            0, // Partition Width
            0, // Extended Starting Address
            0  // Extended Ending Address
        },
        {
            0, // terminator bytes
            0
        }
    };

    UINT64 endAddress = BaseAddress + Size;
    UINT64 endAddressInKb = (endAddress / 1024) + ((endAddress % 1024) > 0);
    UINT64 baseAddressInKb = BaseAddress / 1024;
    BOOLEAN result = FALSE;

    //
    // The non-extended addresses for the type 19 structure only support 32 bit
    // addresses specified in kilobyte units.  This means we can declare memory up to
    // 1K below 4 terabytes using the non-extended, and for anything larger we
    // need to use the extended fields.
    //
    // Extended Addresses were added in SMBIOS v2.7
    //
    if ((BaseAddress > (BASE_4TB - SIZE_1KB)) || endAddress > (BASE_4TB - SIZE_1KB))
    {
        //
        // Use the extended addresses, which are in byte units, not kilobyte like
        // the non-extended.
        //
        memoryArrayMappedAddress.Formatted.StartingAddress         = 0xFFFFFFFF;
        memoryArrayMappedAddress.Formatted.EndingAddress           = 0xFFFFFFFF;
        memoryArrayMappedAddress.Formatted.ExtendedStartingAddress = BaseAddress;
        memoryArrayMappedAddress.Formatted.ExtendedEndingAddress   = endAddress;
    }
    else
    {
        //
        // Size is small enough to be represented in the non-extended addresses.
        //
        memoryArrayMappedAddress.Formatted.StartingAddress = (UINT32) baseAddressInKb;
        memoryArrayMappedAddress.Formatted.EndingAddress   = (UINT32) endAddressInKb;
    }

    memoryArrayMappedAddress.Formatted.MemoryArrayHandle = PhysicalMemoryArrayHandle;

    //
    // Add the structure to the SMBIOS table.
    //
    result = AddStructure(Smbios, &memoryArrayMappedAddress, NULL, MemoryArrayMappedAddressHandle);

    return result;
}


BOOLEAN
AddMemoryDevice(
    IN  EFI_SMBIOS_PROTOCOL*    Smbios,
    IN  UINT64                  Size,
    IN  UINT32                  MemoryFlags,
    IN  EFI_SMBIOS_HANDLE       PhysicalMemoryArrayHandle,
    IN  EFI_SMBIOS_HANDLE       MemoryErrorHandle,
    IN  CHAR8*                  LocationString,
    OUT EFI_SMBIOS_HANDLE*      MemoryDeviceHandle
    )
/*++

Routine Description:

    Adds a MemoryDevice structure (type 17) to the SMBIOS table.

Arguments:

    Smbios - The DXE Smbios protocol.

    Size - The amount of memory in the device (in bytes).

    MemoryFlags - Flags indicating special properties of the memory region.

    PhysicalMemoryArrayHandle - The handle of the PhysicalMemoryArray structure
        for this device.

    MemoryErrorHandle - The handle of the error information structure for this
        device.

    LocationString - String which describes the "slot" where this memory device
        is located.

    MemoryDeviceHandle - Returns the handle of the newly added structure.

Return Value:

    TRUE on success.

--*/
{
    //
    // Initialized array of pointers to the strings for this structure.
    //
    static CHAR8* strings[] =
    {
        "",
        NONE_STRING,
        MANUFACTURER_STRING,
        NONE_STRING,
        NONE_STRING,
        NONE_STRING,
        NULL
    };

    //
    // Initialized structure with string space appended and extra terminator byte.
    //
    static struct
    {
        SMBIOS_TABLE_TYPE17 Formatted;
        CHAR8 Unformed[LOCATION_STRING_SIZE +             // Device Locator
                       sizeof(NONE_STRING) +              // Bank Locator
                       sizeof(MANUFACTURER_STRING) +      // Manufacturer
                       BiosInterfaceSmbiosStringMax + 1 + // Serial Number
                       sizeof(NONE_STRING) +              // Asset Tag
                       sizeof(NONE_STRING) +              // Part Number
                       1];
    } memoryDevice =

    {
        {
            STANDARD_HEADER(SMBIOS_TABLE_TYPE17, EFI_SMBIOS_TYPE_MEMORY_DEVICE),
            SMBIOS_HANDLE_PI_RESERVED, // Physical Memory Array Handle
            SMBIOS_HANDLE_PI_RESERVED, // Memory Error Information Handle
            0xffff, // Total Width - unknown
            0xffff, // Data Width - unknown
            0xffff, // Size - unknown
            MemoryFormFactorUnknown, // Form Factor - unknown
            0, // Device Set - not part of a set
            1, // Device Locator string index
            2, // Bank Locator string index
            MemoryTypeUnknown, // Memory Type - unknown
            { // Type Detail
                0,  // Reserved
                0,  // Other
                1,  // Unknown
            },
            0, // Speed - unknown
            3, // Manufacturer string index
            4, // Serial Number string index
            5, // Asset Tag string index
            6, // Part Number string index
            0, // Attributes - Unknown
            0, // Extended Size
            0, // Configured Memory Clock Speed - Unknown
            0, // Minimum Voltage - Unknown
            0, // Maximum Voltage - Unknown
            0, // Configured Voltage - Unknown
        }
    };

    BOOLEAN result = FALSE;

    UINT64 sizeInKB = (Size / 1024) + ((Size % 1024) > 0);

    if (sizeInKB <= 0x7fff)
    {
        memoryDevice.Formatted.Size = (UINT16) sizeInKB | 0x8000;
    }
    else
    {
        UINT64 sizeInMB = (sizeInKB / 1024) + ((sizeInKB % 1024) > 0);

        if (sizeInMB < 0x7fff)
        {
            memoryDevice.Formatted.Size = (UINT16) sizeInMB;
        }
        else if (sizeInMB < 0x7fffffff)
        {
            //
            // Use the extended size field to store the size.
            // A Size of 0x7fff means look at the Extended Size field for
            // SMBIOS v2.7+.
            //
            memoryDevice.Formatted.Size         = (UINT16) 0x7fff;
            memoryDevice.Formatted.ExtendedSize = (UINT32) sizeInMB;
        }
        else
        {
            //
            // Size is too big to be represented, report as unknown.
            //
            memoryDevice.Formatted.Size = 0xffff;
        }
    }

    //
    // If this is a persistent memory range, mark it as nonvolatile as well.
    //
    if ((MemoryFlags & VM_MEMORY_RANGE_FLAG_PERSISTENT_MEMORY) != 0)
    {
        memoryDevice.Formatted.TypeDetail.Nonvolatile = 1;
    }

    //
    // Add the dynamic information to the structure.
    //
    memoryDevice.Formatted.MemoryArrayHandle = PhysicalMemoryArrayHandle;
    memoryDevice.Formatted.MemoryErrorInformationHandle = MemoryErrorHandle;
    strings[0] = LocationString;

    //
    // Add the Memory Device Serial Number to the Bank 0 device.
    //
    if (AsciiStrCmp(LocationString, LOCATION_STRING_PRIMARY_MEMORY_DEVICE) == 0)
    {
        UINT32 stringLength = PcdGet32(PcdSmbiosMemoryDeviceSerialNumberSize);

        if (stringLength)
        {
            strings[3] =
            LoadPcdSmbiosString(PcdGet64(PcdSmbiosMemoryDeviceSerialNumberStr),
                                stringLength,
                                BiosInterfaceSmbiosStringMax + 1);
        }
    }
    else
    {
        strings[3] = NONE_STRING;
    }

    //
    // Add the structure to the SMBIOS table.
    //
    result = AddStructure(Smbios, &memoryDevice, strings, MemoryDeviceHandle);

    return result;
}


VOID
AddMemoryDeviceMappedAddress(
    IN  EFI_SMBIOS_PROTOCOL* Smbios,
    IN  UINT64               BaseAddress,
    IN  UINT64               Size,
    IN  EFI_SMBIOS_HANDLE    MemoryDeviceHandle,
    IN  EFI_SMBIOS_HANDLE    MemoryArrayMappedAddressHandle
    )
/*++

Routine Description:

    Adds a MemoryDeviceMappedAddress structure (type 20) to the SMBIOS table.

Arguments:

    Smbios - The DXE Smbios protocol.

    BaseAddress - The address where this memory array is mapped.

    Size - The amount of memory mapped (in bytes).

    MemoryDeviceHandle - The handle of the MemoryDevice structure to which this
        mapping applies.

    MemoryArrayMappedAddressHandle - The handle of the address mapping
        structure for the array in which this mapping's device resides.

Return Value:

    n/a

--*/
{
    //
    // Initialized structure with terminator bytes.
    //
    static struct
    {
        SMBIOS_TABLE_TYPE20 Formatted;
        CHAR8 Unformed[2];
    } memoryDeviceMappedAddress =

    {
        {
            STANDARD_HEADER(SMBIOS_TABLE_TYPE20, EFI_SMBIOS_TYPE_MEMORY_DEVICE_MAPPED_ADDRESS),
            0, // Starting Address
            0, // Ending Address
            SMBIOS_HANDLE_PI_RESERVED, // Memory Device Handle
            SMBIOS_HANDLE_PI_RESERVED, // Memory Array Mapped Address Handle
            0xff, // Partition Row Position - unknown
            0, // Interleave Position - not interleaved
            0, // Interleaved Data Depth - not part of interleave
            0, // Extended Starting Address
            0  // Extended Ending Address
        },
        {
            0, // terminator bytes
            0
        }
    };

    UINT64 endAddress = BaseAddress + Size;
    UINT64 endAddressInKb = (endAddress / 1024) + ((endAddress % 1024) > 0);
    UINT64 baseAddressInKb = BaseAddress / 1024;

    //
    // The non-extended addresses for the type 20 structure only support 32 bit
    // addresses specified in kilobyte units.  This means we can declare memory up to
    // 1K below 4 terabytes using the non-extended, and for anything larger we
    // need to use the extended fields.
    //
    // Extended Addresses were added in SMBIOS v2.7
    //
    if ((BaseAddress > (BASE_4TB - SIZE_1KB)) || endAddress > (BASE_4TB - SIZE_1KB))
    {
        //
        // Use the extended addresses, which are in byte units, not kilobyte like
        // the non-extended.
        //
        memoryDeviceMappedAddress.Formatted.StartingAddress         = 0xFFFFFFFF;
        memoryDeviceMappedAddress.Formatted.EndingAddress           = 0xFFFFFFFF;
        memoryDeviceMappedAddress.Formatted.ExtendedStartingAddress = BaseAddress;
        memoryDeviceMappedAddress.Formatted.ExtendedEndingAddress   = endAddress;
    }
    else
    {
        //
        // Size is small enough to use the non-extended addresses.
        //
        memoryDeviceMappedAddress.Formatted.StartingAddress = (UINT32) baseAddressInKb;
        memoryDeviceMappedAddress.Formatted.EndingAddress   = (UINT32) endAddressInKb;
    }

    memoryDeviceMappedAddress.Formatted.MemoryDeviceHandle = MemoryDeviceHandle;
    memoryDeviceMappedAddress.Formatted.MemoryArrayMappedAddressHandle =
        MemoryArrayMappedAddressHandle;

    //
    // Add the structure to the SMBIOS table. Error not fatal and ignored.
    //
    (VOID)AddStructure(Smbios, &memoryDeviceMappedAddress, NULL, NULL);
}


VOID
AddSystemBootInformation(
    IN  EFI_SMBIOS_PROTOCOL* Smbios
    )
/*++

Routine Description:

    Adds the SystemBootInformation structure (type 32) to the SMBIOS table.

Arguments:

    Smbios - The DXE Smbios protocol.

Return Value:

    n/a

--*/
{
    //
    // Initialized structure with terminator bytes.
    //
    static struct
    {
        SMBIOS_TABLE_TYPE32 Formatted;
        CHAR8 Unformed[2];
    } systemBootInformation =

    {
        {
            STANDARD_HEADER(SMBIOS_TABLE_TYPE32, EFI_SMBIOS_TYPE_SYSTEM_BOOT_INFORMATION),
            { 0 }, // Reserved
            { BootInformationStatusNoError } // Boot Status
        },
        {
            0, // terminator bytes
            0
        }
    };

    //
    // Add the structure to the SMBIOS table. Error not fatal and ignored.
    //
    (VOID)AddStructure(Smbios, &systemBootInformation, NULL, NULL);
}


VOID
AddMemoryRegion(
    IN  EFI_SMBIOS_PROTOCOL* Smbios,
    IN  UINT64 BaseAddress,
    IN  UINT64 Length,
    IN  UINT32 MemoryFlags,
    IN  CHAR8* LocationString,
    IN  EFI_SMBIOS_HANDLE PhysicalMemoryArrayHandle,
    IN  EFI_SMBIOS_HANDLE MemoryErrorHandle
    )
/*++

Routine Description:

    Adds three memory device/region related structures to the SMBIOS table for a memory region.
    Only adds a zero length Memory Device structure if Length specified as zero.

    Memory Device                (type 17)
    Memory Array Mapped Address  (type 19)
    Memory Device Mapped Address (type 20)

Arguments:

    Smbios - The DXE Smbios protocol.

    BaseAddress - The base physical address where the memory device is mapped.

    Length - The length of the memory device.

    MemoryFlags - Flags indicating special properties of the memory region.

    LocationString - The identifying string for the device.

    PhysicalMemoryArrayHandle - The handle of the existing Physical Memory Array structure.

    MemoryErrorHandle

Return Value:

    n/a

--*/
{
    EFI_SMBIOS_HANDLE memoryDeviceHandle;
    EFI_SMBIOS_HANDLE memoryArrayMappedAddressHandle;

    //
    // Add Memory Device structure.
    //
    if (AddMemoryDevice(Smbios,
                        Length,
                        MemoryFlags,
                        PhysicalMemoryArrayHandle,
                        MemoryErrorHandle,
                        LocationString,
                        &memoryDeviceHandle))
    {
        //
        // If length of memory device is zero then done.
        //
        if (Length > 0)
        {
            //
            // Add additional structures for memory.
            //
            if (AddMemoryArrayMappedAddress(Smbios,
                                            BaseAddress,
                                            Length,
                                            PhysicalMemoryArrayHandle,
                                            &memoryArrayMappedAddressHandle))
            {
                AddMemoryDeviceMappedAddress(Smbios,
                                             BaseAddress,
                                             Length,
                                             memoryDeviceHandle,
                                             memoryArrayMappedAddressHandle);
            }
        }
    }
}


VOID
AccumulateMemoryRegionsFromMemoryRange(
    IN  BOOLEAN LegacyMemoryMap,
    IN  VOID    *Range,
    OUT VOID    *Context
    )
/*++

Routine Description:

    Callback function for EnumerateMemoryRanges that counts the number
    of SMBIOS Memory regions required to represent a memory range.

Arguments:

    Range - A pointer to an memory range.

    Context - The context pointer.  Expected to be a UINT64*
              pointer in which to accumulate the regions.

Return Value:

    n/a

--*/
{
    UINT64 *numMemoryRegions = (UINT64 *)Context;
    UINT64 size;

    if (LegacyMemoryMap)
    {
        size = ((PVM_MEMORY_RANGE)Range)->Length;
    }
    else
    {
        size = ((PVM_MEMORY_RANGE_V5)Range)->Length;
    }

    //
    // Compute the number of SMBIOS Memory regions that will represent
    // the size expressed by the memory map range structure.
    //
    *numMemoryRegions += ((size + gMaxSizePerMemoryDevice - 1) / gMaxSizePerMemoryDevice);
}


VOID
AddMemoryRegionsFromMemoryRange(
    IN  BOOLEAN LegacyMemoryMap,
    IN  VOID    *Range,
    OUT VOID    *Context
    )
/*++

Routine Description:

    Callback function for EnumerateMemoryRanges to add one or more
    SMBIOS Memory Regions to represent a memory range.

Arguments:

    Range - A pointer to a memory range.

    Context - The context pointer.

Return Value:

    n/a

--*/
{
    ADD_MEMORY_REGIONS_CONTEXT* context = (ADD_MEMORY_REGIONS_CONTEXT*)Context;
    CHAR8 location[LOCATION_STRING_SIZE];
    UINT64 base;
    UINT64 size;
    UINT32 flags = 0;

    if (LegacyMemoryMap)
    {
        base = ((PVM_MEMORY_RANGE)Range)->BaseAddress;
        size = ((PVM_MEMORY_RANGE)Range)->Length;
    }
    else
    {
        base = ((PVM_MEMORY_RANGE_V5)Range)->BaseAddress;
        size = ((PVM_MEMORY_RANGE_V5)Range)->Length;
        flags = ((PVM_MEMORY_RANGE_V5)Range)->Flags;
    }

    //
    // Add memory regions until this memory map entry (range) is consumed or
    // the maximum number of SMBIOS memory regions is reached.
    //
    while ((context->CurrentRegion < gMaxMemoryRegions) && (size > 0))
    {
        context->CurrentRegion++;
        NumberToMemoryLocationString((UINT16)context->CurrentRegion, location);
        AddMemoryRegion(
            context->Smbios,
            base,
            MIN(size, gMaxSizePerMemoryDevice),
            flags,
            location,
            context->PhysicalMemoryArrayHandle,
            SMBIOS_HANDLE_PI_RESERVED);
        size -= MIN(size, gMaxSizePerMemoryDevice);
        base += MIN(size, gMaxSizePerMemoryDevice);
    }
}


VOID
EnumerateMemoryRanges(
    BOOLEAN                     LegacyMemoryMap,
    VOID                        *Memmap,
    UINT32                      MemmapLength,
    ENUMERATE_MEMMAP_CALLBACK   Callback,
    VOID *                      Context
    )
/*++

Routine Description:

    Utility function to enumerate all the memory ranges in the memory map.
    Calls the passed in callback function for each range.

Arguments:

    Memmap - A pointer to the memory map.

    MemmapLength - The numer of entries (ranges) in the memory map.

    Callback - The function to call with each enumerated entry (range).

    Context - The context pointer to pass to the Callback function.

Return Value:

    n/a

--*/
{
    if (LegacyMemoryMap)
    {
        VM_MEMORY_RANGE *cursor;

        for (cursor = Memmap; cursor < ((PVM_MEMORY_RANGE)Memmap + MemmapLength); cursor++)
        {
            Callback(LegacyMemoryMap, cursor, Context);
        }
    }
    else
    {
        VM_MEMORY_RANGE_V5 *cursor;

        for (cursor = Memmap; cursor < ((PVM_MEMORY_RANGE_V5)Memmap + MemmapLength); cursor++)
        {
            Callback(LegacyMemoryMap, cursor, Context);
        }
    }
}


VOID
AddMemoryStructures(
    IN  EFI_SMBIOS_PROTOCOL* Smbios
)
/*++

Routine Description:

    Adds all the memory related structures to the SMBIOS table.

    Physical Memory Array        (type 16)
    Memory Device                (type 17)
    Memory Array Mapped Address  (type 19)
    Memory Device Mapped Address (type 20)

    The memory structures on a physical machine typically represent the
    physical memory devices/modules installed.  In a virtual machine this can
    only be simulated.  The most accurate simulation is to create a memory
    device for each non-hot-add region expressed in the SRAT.

Arguments:

    Smbios - The DXE Smbios protocol.

Return Value:

    n/a

--*/
{
    ADD_MEMORY_REGIONS_CONTEXT context;
    UINT32 memmapSize;
    VOID* memmap;
    UINT32 memmapLength;
    UINT32 memRangeSize;
    UINT64 regions;

    BOOLEAN legacyMemoryMap = PcdGetBool(PcdLegacyMemoryMap);

    //
    // Get Memory Map from Config blob via PCDs.
    //
    memmapSize = PcdGet32(PcdMemoryMapSize);
    memmap = (VOID*)(UINTN) PcdGet64(PcdMemoryMapPtr);
    memRangeSize = legacyMemoryMap ? sizeof(VM_MEMORY_RANGE) : sizeof(VM_MEMORY_RANGE_V5);
    memmapLength = memmapSize / memRangeSize;

    //
    // Calculate the number of SMBIOS memory regions required to represent
    // starting RAM in the machine. This requires a first pass through the
    // memmap entries.
    //
    regions = 0;
    EnumerateMemoryRanges(
        legacyMemoryMap,
        memmap,
        memmapLength,
        AccumulateMemoryRegionsFromMemoryRange,
        (VOID *)&regions);

    //
    // Limit the SMBIOS memory regions to this implementation's maximum.
    //
    if (regions > gMaxMemoryRegions)
    {
        regions = gMaxMemoryRegions;
    }

    //
    // Add the single SMBIOS Physical Memory Array structure (type 16)
    // using the count of require regions from above.
    //
    if (AddPhysicalMemoryArray(
            Smbios,
            SMBIOS_HANDLE_PI_RESERVED,
            &context.PhysicalMemoryArrayHandle,
            (UINT16)regions))
    {
        //
        // Enumerate the memory regions again and add one or more
        // SMBIOS memory regions represent each entry.
        //
        context.Smbios = Smbios;
        context.CurrentRegion = 0;
        EnumerateMemoryRanges(
            legacyMemoryMap,
            memmap,
            memmapLength,
            AddMemoryRegionsFromMemoryRange,
            &context);
    }
}


VOID
AddAllStructures(
    IN  EFI_SMBIOS_PROTOCOL* Smbios
    )
/*++

Routine Description:

    Adds all the SMBIOS structures to the SMBIOS table.

Arguments:

    Smbios - The DXE Smbios protocol.

Return Value:

    n/a

--*/
{
    EFI_SMBIOS_HANDLE chassisHandle;

    AddBiosInformation(Smbios);
    AddSystemInformation(Smbios);
    if (AddSystemEnclosure(Smbios, &chassisHandle))
    {
       AddBaseboardInformation(Smbios, chassisHandle);
    }
    AddProcessorInformation(Smbios);
    AddOEMStrings(Smbios);
    AddMemoryStructures(Smbios);
    AddSystemBootInformation(Smbios);
}


EFI_STATUS
EFIAPI
SmbiosPlatformEntryPoint(
    IN  EFI_HANDLE        ImageHandle,
    IN  EFI_SYSTEM_TABLE* SystemTable
    )
/*++

Routine Description:

    Entrypoint of platform SMBIOS driver.

    This entry point adds the SMBIOS structures.

Arguments:

    ImageHandle - Driver Image Handle.

    SystemTable - EFI System Table.

Return Value:

    An appropriate EFI_STATUS value.

--*/
{
    EFI_SMBIOS_PROTOCOL* smbios;
    //
    // Get the DXE Smbios protocol to use for adding structures.
    //
    if (EFI_ERROR(gBS->LocateProtocol(&gEfiSmbiosProtocolGuid, NULL, (VOID**) &smbios)))
    {
        return EFI_PROTOCOL_ERROR;
    }

    //
    // Check if version matches.
    //
    if ((smbios->MajorVersion != TARGETTED_SMBIOS_MAJOR_VERSION) ||
        (smbios->MinorVersion != TARGETTED_SMBIOS_MINOR_VERSION))
    {
        return EFI_INCOMPATIBLE_VERSION;
    }

    //
    // Add all the structures.
    //
    AddAllStructures(smbios);

    return EFI_SUCCESS;
}

