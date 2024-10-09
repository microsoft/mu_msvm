/** @file

    This file contains types and constants shared between
    the virtual device and the UEFI firmware.

    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once

//
// BIOS Interface constants
//
enum
{
    BiosInterfaceMaximumProcessorNumber    = 64,
    BiosInterfaceEntropyTableSize          = 64,
    BiosInterfaceGenerationIdSize          = 16,
    BiosInterfaceSmbiosStringMax           = 32
};


//
// BIOS configuration ports.
//
// See PCD MsvmPkgTokenSpaceGuid.PcdBiosBaseAddress
//

//
// Values/Selectors for the BIOS configuration ports.
//
// Existing values can not change after Hyper-V is released.
// Only new values can be added if they were previously unused.
//
enum
{
    BiosConfigFirstMemoryBlockSize      = 0x00,
    BiosConfigNumLockEnabled            = 0x01,
    BiosConfigBiosGuid                  = 0x02,
    BiosConfigBiosSystemSerialNumber    = 0x03,
    BiosConfigBiosBaseSerialNumber      = 0x04,
    BiosConfigBiosChassisSerialNumber   = 0x05,
    BiosConfigBiosChassisAssetTag       = 0x06,
    BiosConfigBootDeviceOrder           = 0x07,
    BiosConfigBiosProcessorCount        = 0x08,
    BiosConfigProcessorLocalApicId      = 0x09,
    BiosConfigSratSize                  = 0x0A,
    BiosConfigSratOffset                = 0x0B,
    BiosConfigSratData                  = 0x0C,
    BiosConfigMemoryAmountAbove4Gb      = 0x0D,
    BiosConfigGenerationIdPtrLow        = 0x0E,
    BiosConfigGenerationIdPtrHigh       = 0x0F,
    //
    // intentional gap here - obsolete values
    //
    BiosConfigPciIoGapStart             = 0x12,
    BiosConfigProcessorReplyStatusIndex = 0x13,
    BiosConfigProcessorReplyStatus      = 0x14,
    BiosConfigProcessorMatEnable        = 0x15,
    BiosConfigProcessorStaEnable        = 0x16,
    BiosConfigWaitNano100               = 0x17,
    BiosConfigWait1Millisecond          = 0x18,
    BiosConfigWait10Milliseconds        = 0x19,
    BiosConfigBootFinalize              = 0x1A,
    BiosConfigWait2Millisecond          = 0x1B,
    BiosConfigBiosLockString            = 0x1C,
    BiosConfigProcessorDMTFTable        = 0x1D,
    BiosConfigEntropyTable              = 0x1E,
    BiosConfigMemoryAboveHighMmio       = 0x1F,
    BiosConfigHighMmioGapBaseInMb       = 0x20,
    BiosConfigHighMmioGapLengthInMb     = 0x21,
    BiosConfigE820Entry                 = 0x22,
    BiosConfigInitialMegabytesBelowGap  = 0x23,
    //
    // Values added in Windows Blue
    //
    BiosConfigNvramCommand              = 0x24,
    BiosConfigWriteConfigPage           = 0x25,
    BiosConfigCryptoCommand             = 0x26,
    //
    // Watchdog device (Windows 8.1 MQ)
    //
    BiosConfigWatchdogConfig            = 0x27,
    BiosConfigWatchdogResolution        = 0x28,
    BiosConfigWatchdogCount             = 0x29,
    //
    // Memory Map Size
    //
    BiosConfigMemoryMapSize             = 0x2A,
    //
    // Event Logging (Windows 8.1 MQ/M0)
    //
    BiosConfigEventLogFlush             = 0x30,
    //
    // Set MOR bit variable. Triggered by TPM _DSM Memory Clear Interface.
    // In real hardware, _DSM triggers CPU SMM. UEFI SMM driver sets the
    // MOR state via variable service. Hypervisor does not support virtual SMM,
    // so _DSM is not able to trigger SMI in Hyper-V virtualization. The
    // alternative is to send an IO port command to BIOS device and persist the
    // MOR state in UEFI NVRAM via variable service on host.
    //
    BiosConfigMorSetVariable            = 0x31,
    //
    // VDev version. (Windows Threshold)
    //
    BiosConfigVdevVersion               = 0x32,
    //
    // Memory Map. (Windows Threshold)
    //
    BiosConfigMemoryMap                 = 0x33,
    //
    // ARM64 RTC GetTime SetTime (RS2)
    //
    BiosConfigGetTime                   = 0x34,
    BiosConfigSetTime                   = 0x35,
    //
    // Debugger output
    //
    BiosDebugOutputString               = 0x36,
    //
    // vPMem NFIT (RS3)
    //
    BiosConfigNfitSize                  = 0x37,
    BiosConfigNfitPopulate              = 0x38,
    BiosConfigVpmemSetACPIBuffer        = 0x39,
    //
    // This value should be the maximum possible value for the
    // address register with the exception of BiosConfigDebug.
    //
    BiosConfigMaxValue                  = BiosConfigVpmemSetACPIBuffer,
};


//
// Common SMBIOS structure header.
//
#pragma pack(push, 1)

typedef struct _SMBIOS_HEADER
{
    UINT8  Type;
    UINT8  Length;
    UINT16 Handle;
} SMBIOS_HEADER;

#pragma pack(pop)


//
// Other string to use as default string.
//
#define SMBIOS_NONE_STRING "None"
#define SMBIOS_NONE_STRING_SIZE sizeof(SMBIOS_NONE_STRING)


//
// Maximum length of a string in v2.4 SMBIOS structure.
//
#define MAX_SMBIOS_STRING_LENGTH 64


//
// SMBIOS v2.4 CPU Information structure.
//
#pragma pack(push, 1)

typedef struct _SMBIOS_CPU_INFO_FORMATTED
{
    SMBIOS_HEADER     Header;
    UINT8             SocketDesignation;
    UINT8             ProcessorType;
    UINT8             ProcessorFamily;
    UINT8             ProcessorManufacturer;
    UINT64            ProcessorId;
    UINT8             ProcessorVersion;
    UINT8             Voltage;
    UINT16            ExternalClock;
    UINT16            MaxSpeed;
    UINT16            CurrentSpeed;
    UINT8             Status;
    UINT8             Upgrade;
    UINT16            L1Handle;
    UINT16            L2Handle;
    UINT16            L3Handle;
    UINT8             SerialNumber;
    UINT8             AssetTag;
    UINT8             PartNumber;
 } SMBIOS_CPU_INFO_FORMATTED;


typedef struct _SMBIOS_CPU_INFO_STRINGS
{
    //
    // CPU Information structure string table.
    //
    // Sized for:
    //  4 "None" strings.
    //  2 strings obtained from host that are max 64 chars each.
    //  1 empty string to terminate the table.
    //
    char              StringTable[(4 * SMBIOS_NONE_STRING_SIZE) +
                                  ((MAX_SMBIOS_STRING_LENGTH + 1) * 2) +
                                  1];
} SMBIOS_CPU_INFO_STRINGS;


typedef struct _SMBIOS_CPU_INFO_STRINGS_LEGACY
{
    //
    // CPU Information structure string table for legacy BIOS.
    //
    // Sized for:
    //  1 "None" strings.
    //  2 strings obtained from host that are max 64 chars each.
    //  1 empty string to terminate the table.
    //
    char              StringTable[SMBIOS_NONE_STRING_SIZE +
                                  ((MAX_SMBIOS_STRING_LENGTH + 1) * 2)+
                                  1];
} SMBIOS_CPU_INFO_STRINGS_LEGACY;


typedef struct _SMBIOS_CPU_INFORMATION
{
    SMBIOS_CPU_INFO_FORMATTED Formatted;
    SMBIOS_CPU_INFO_STRINGS   Unformatted;
} SMBIOS_CPU_INFORMATION;


typedef struct _SMBIOS_CPU_INFORMATION_LEGACY
{
    SMBIOS_CPU_INFO_FORMATTED      Formatted;
    SMBIOS_CPU_INFO_STRINGS_LEGACY Unformatted;
} SMBIOS_CPU_INFORMATION_LEGACY;

#pragma pack(pop)

//
// Memory map for VDev versions 2-4
//
typedef struct _VM_MEMORY_RANGE
{
    UINT64  BaseAddress;
    UINT64  Length;
} VM_MEMORY_RANGE, *PVM_MEMORY_RANGE;


//
// Memory map beginning with VDev version 5
//
#define VM_MEMORY_RANGE_FLAG_PLATFORM_RESERVED  0x1
#define VM_MEMORY_RANGE_FLAG_PERSISTENT_MEMORY  0x2
#define VM_MEMORY_RANGE_FLAG_SPECIFIC_PURPOSE   0x4

typedef struct _VM_MEMORY_RANGE_V5
{
    UINT64  BaseAddress;
    UINT64  Length;
    UINT32  Flags;
    UINT32  Reserved;
} VM_MEMORY_RANGE_V5, *PVM_MEMORY_RANGE_V5;


//
// Command types for NVRAM_COMMAND_DESCRIPTOR.
//
// These correlate with the semantics of the UEFI runtime variable services.
//
typedef enum _NVRAM_COMMAND
{
    NvramGetVariableCommand,
    NvramSetVariableCommand,
    NvramGetFirstVariableNameCommand,
    NvramGetNextVariableNameCommand,
    NvramQueryInfoCommand,
    NvramSignalRuntimeCommand,
    NvramDebugStringCommand
} NVRAM_COMMAND;


//
// The maximum sizes in bytes for EFI Variable Name and Data.
//
// The Data size must be minimum 32K for secure boot databases.
//
#define EFI_MAX_VARIABLE_NAME_SIZE  (2 * 1024)
#define EFI_MAX_VARIABLE_DATA_SIZE  (32 * 1024)

//
// In-memory descriptor used to pass NVRAM variable requests
// from the UEFI firmware to the BIOS VDev.
//
#pragma pack(push, 1)

typedef struct _NVRAM_COMMAND_DESCRIPTOR
{
    NVRAM_COMMAND   Command;

    //
    // Status of the processed command.
    //
    UINT64          Status;

    union
    {
        struct
        {
            //
            // UEFI variable attributes associated with
            // the variable: access rights (RT/BS).
            // Used as input for the SetVariable command.
            // Used as output for the GetVariable command.
            //
            UINT32 VariableAttributes;

            //
            // GPA of the buffer containing a 16-bit
            // unicode variable name.
            // Memory at this location is read for the
            // GetVariable, SetVariable, GetNextVariable command.
            // Memory at this location is written to for
            // the GetNextVariable command.
            //
            UINT64 VariableNameAddress;

            //
            // Size in bytes of the buffer at
            // VariableNameAddress.
            // Used as input for GetVariable, SetVariable,
            // and GetNextVariable commands.
            // Used as output for the GetNextVariable command.
            //
            UINT32 VariableNameBytes;

            //
            // A GUID comprising the other half of the variable name.
            // Used as input for GetVariable, SetVariable, and
            // GetNextVariable commands.
            // Used as output for the GetNextVariable command.
            //
            GUID VariableVendorGuid;

            //
            // GPA of the buffer containing variable data.
            // Memory at this location is written to for the
            // GetVariable command.
            // Memory at this location is read for the SetVariable
            // command.
            //
            UINT64 VariableDataAddress;

            //
            // Size of the buffer at VariableDataAddress.
            // Used as input for the GetVariable command.
            // Used as output for the GetVariable and
            // SetVariable commands.
            //
            UINT32 VariableDataBytes;

        } VariableCommand;

        struct
        {
            //
            // Attribute mask, controls variable type for which
            // the information is returned.
            // Used as an input for the QueryInfo command.
            //
            UINT32 Attributes;

            //
            // These are outputs for the QueryInfo command.
            //
            UINT64 MaximumVariableStorage;
            UINT64 RemainingVariableStorage;
            UINT64 MaximumVariableSize;
        } QueryInfo;

        union
        {
            struct
            {
                UINT64 VsmAware : 1;
                UINT64 Unused   : 63;
            } S;
            UINT64 AsUINT64;
        }SignalRuntimeCommand;
    } U;
} NVRAM_COMMAND_DESCRIPTOR, *PNVRAM_COMMAND_DESCRIPTOR;

#pragma pack(pop)


#define CRYPT_HASH_CONTEXT_SIZE (2 * sizeof(UINT64))


typedef enum _HASH_ALG_ID
{
     HashAlgSha1,
     HashAlgSha256
} HASH_ALG_ID;


typedef enum _CRYPTO_COMMAND
{
    CryptoComputeHash,
    CryptoVerifyRsaPkcs1,
    CryptoVerifyPkcs7,
    CryptoVerifyAuthenticode,
    CryptoLogEvent,
    CryptoGetRandomNumber
} CRYPTO_COMMAND;


typedef enum _SECUREBOOT_EVENT_INFO
{
    ImageFailedVerification,
    ImageFailedVerificationUnsignedAndHashNotInDb,
    ImageFailedVerificationHashInDbx,
    ImageFailedVerificationNeitherCertNorHashInDb,
    ImageFailedVerificationCertInDbx,
    ImageFailedVerificationNotValidPeOrCoff
} SECUREBOOT_EVENT_INFO;


#pragma pack(push, 1)

typedef struct _CRYPTO_COMMAND_DESCRIPTOR
{
    CRYPTO_COMMAND    Command;
    UINT64            Status;
    union
    {
        struct
        {
            HASH_ALG_ID HashAlgorithm;
            UINT64      DataAddress;
            UINT32      DataLength;
            UINT64      ValueAddress;
            UINT32      ValueLength;
        }
        ComputeHashParams;
        struct
        {
            UINT64      RsaContextAddress;
            UINT32      RsaContextLength;
            UINT64      MessageHashAddress;
            UINT32      MessageHashLength;
            UINT64      SignatureAddress;
            UINT32      SignatureLength;
        }
        RsaPkcs1Params;
        struct
        {
            UINT64      AuthDataAddress;
            UINT32      AuthDataSize;
            UINT64      TrustedCertAddress;
            UINT32      TrustedCertSize;
            UINT64      HashOrPkcsDataAddress;
            UINT32      HashOrPkcsDataSize;
        }AuthenticodeOrPkcs7Params;
        struct
        {
            SECUREBOOT_EVENT_INFO      EventInfo;
        }
        LogEventParams;
        struct
        {
            UINT64      BufferAddress;
            UINT32      BufferSize;
        }
        GetRandomNumberParams;
    } U;
} CRYPTO_COMMAND_DESCRIPTOR, *PCRYPTO_COMMAND_DESCRIPTOR;

#pragma pack(pop)

//
// Value returned to for any watchdog register reads if the
// BIOS watchdog timer is disabled.
//
#define BIOS_WATCHDOG_NOT_ENABLED   ((UINT32)0xFFFFFFFF)

//
// Values for the BiosConfigWatchdogConfig DWORD.
// Update BIOS_WATCHDOG_VALID_CONFIG_BITS as new values are added
//
#define BIOS_WATCHDOG_CONFIGURED    0x00000001
#define BIOS_WATCHDOG_ENABLED       0x00000002
#define BIOS_WATCHDOG_ONE_SHOT      0x00000010
#define BIOS_WATCHDOG_BOOT_STATUS   0x00000100
#define BIOS_WATCHDOG_FOR_GUEST     0x00001000

#define BIOS_WATCHDOG_RUNNING       (BIOS_WATCHDOG_CONFIGURED | BIOS_WATCHDOG_ENABLED)

#pragma pack(push, 1)

//
// Common config header.
//
// NOTE: Length is the length of the overall structure in bytes, including the
// header.
//
typedef struct _UEFI_CONFIG_HEADER
{
    UINT32  Type;
    UINT32  Length;
} UEFI_CONFIG_HEADER;

//
// Config structure types.
//
enum UefiStructureType
{
    UefiConfigStructureCount                 = 0x00,
    UefiConfigBiosInformation                = 0x01,
    UefiConfigSrat                           = 0x02,
    UefiConfigMemoryMap                      = 0x03,
    UefiConfigEntropy                        = 0x04,
    UefiConfigBiosGuid                       = 0x05,
    UefiConfigSmbiosSystemSerialNumber       = 0x06,
    UefiConfigSmbiosBaseSerialNumber         = 0x07,
    UefiConfigSmbiosChassisSerialNumber      = 0x08,
    UefiConfigSmbiosChassisAssetTag          = 0x09,
    UefiConfigSmbiosBiosLockString           = 0x0A,
    UefiConfigSmbios31ProcessorInformation   = 0x0B,
    UefiConfigSmbiosSocketDesignation        = 0x0C,
    UefiConfigSmbiosProcessorManufacturer    = 0x0D,
    UefiConfigSmbiosProcessorVersion         = 0x0E,
    UefiConfigSmbiosProcessorSerialNumber    = 0x0F,
    UefiConfigSmbiosProcessorAssetTag        = 0x10,
    UefiConfigSmbiosProcessorPartNumber      = 0x11,
    UefiConfigFlags                          = 0x12,
    UefiConfigProcessorInformation           = 0x13,
    UefiConfigMmioRanges                     = 0x14,
    UefiConfigAARCH64MPIDR                   = 0x15, // to remove
    UefiConfigAcpiTable                      = 0x16,
    UefiConfigNvdimmCount                    = 0x17,
    UefiConfigMadt                           = 0x18,
    UefiConfigVpciInstanceFilter             = 0x19,
    UefiConfigSmbiosSystemManufacturer       = 0x1A,
    UefiConfigSmbiosSystemProductName        = 0x1B,
    UefiConfigSmbiosSystemVersion            = 0x1C,
    UefiConfigSmbiosSystemSKUNumber          = 0x1D,
    UefiConfigSmbiosSystemFamily             = 0x1E,
    UefiConfigSmbiosMemoryDeviceSerialNumber = 0x1F,
    UefiConfigSlit                           = 0x20,
    UefiConfigAspt                           = 0x21,
    UefiConfigPptt                           = 0x22,
    UefiConfigGic                            = 0x23,
    UefiConfigMcfg                           = 0x24,
    UefiConfigSsdt                           = 0x25,
    UefiConfigHmat                           = 0x26,
};

//
// Config Structures.
//
// NOTE: All config structures _must_ be aligned to 8 bytes, as AARCH64 does not
// support unaligned accesses. For variable length structures, they must be
// padded appropriately to 8 byte boundaries.
//

//
// NOTE: TotalStructureCount is the count of all structures in the config blob,
// including this structure.
//
// NOTE: TotalConfigBlobSize is in bytes.
//
typedef struct _UEFI_CONFIG_STRUCTURE_COUNT
{
    UEFI_CONFIG_HEADER Header;
    UINT32 TotalStructureCount;
    UINT32 TotalConfigBlobSize;
} UEFI_CONFIG_STRUCTURE_COUNT;

typedef struct _UEFI_CONFIG_BIOS_INFORMATION
{
    UEFI_CONFIG_HEADER Header;
    UINT32 BiosSizePages;
    struct {
        UINT32 LegacyMemoryMap : 1;
        UINT32 Reserved : 31;
    } Flags;
} UEFI_CONFIG_BIOS_INFORMATION;

typedef struct _UEFI_CONFIG_MADT
{
    UEFI_CONFIG_HEADER Header;
    UINT8 Madt[];
} UEFI_CONFIG_MADT;

typedef struct _UEFI_CONFIG_SRAT
{
    UEFI_CONFIG_HEADER Header;
    UINT8 Srat[];
} UEFI_CONFIG_SRAT;

typedef struct _UEFI_CONFIG_SLIT
{
    UEFI_CONFIG_HEADER Header;
    UINT8 Slit[];
} UEFI_CONFIG_SLIT;

typedef struct _UEFI_CONFIG_PPTT
{
    UEFI_CONFIG_HEADER Header;
    UINT8 Pptt[];
} UEFI_CONFIG_PPTT;

typedef struct _UEFI_CONFIG_HMAT
{
    UEFI_CONFIG_HEADER Header;
    UINT8 Hmat[];
} UEFI_CONFIG_HMAT;

typedef struct _UEFI_CONFIG_MEMORY_MAP
{
    UEFI_CONFIG_HEADER Header;
    UINT8 MemoryMap[];
} UEFI_CONFIG_MEMORY_MAP;

typedef struct _UEFI_CONFIG_ENTROPY
{
    UEFI_CONFIG_HEADER Header;
    UINT8 Entropy[BiosInterfaceEntropyTableSize];
} UEFI_CONFIG_ENTROPY;

typedef struct _UEFI_CONFIG_BIOS_GUID
{
    UEFI_CONFIG_HEADER Header;
    UINT8 BiosGuid[sizeof(GUID)];
} UEFI_CONFIG_BIOS_GUID;

typedef struct _UEFI_CONFIG_SMBIOS_SYSTEM_MANUFACTURER
{
    UEFI_CONFIG_HEADER Header;
    UINT8 SystemManufacturer[];
} UEFI_CONFIG_SMBIOS_SYSTEM_MANUFACTURER;

typedef struct _UEFI_CONFIG_SMBIOS_SYSTEM_PRODUCT_NAME
{
    UEFI_CONFIG_HEADER Header;
    UINT8 SystemProductName[];
} UEFI_CONFIG_SMBIOS_SYSTEM_PRODUCT_NAME;

typedef struct _UEFI_CONFIG_SMBIOS_SYSTEM_VERSION
{
    UEFI_CONFIG_HEADER Header;
    UINT8 SystemVersion[];
} UEFI_CONFIG_SMBIOS_SYSTEM_VERSION;

typedef struct _UEFI_CONFIG_SMBIOS_SYSTEM_SERIAL_NUMBER
{
    UEFI_CONFIG_HEADER Header;
    UINT8 SystemSerialNumber[];
} UEFI_CONFIG_SMBIOS_SYSTEM_SERIAL_NUMBER;

typedef struct _UEFI_CONFIG_SMBIOS_SYSTEM_SKU_NUMBER
{
    UEFI_CONFIG_HEADER Header;
    UINT8 SystemSKUNumber[];
} UEFI_CONFIG_SMBIOS_SYSTEM_SKU_NUMBER;

typedef struct _UEFI_CONFIG_SMBIOS_SYSTEM_FAMILY
{
    UEFI_CONFIG_HEADER Header;
    UINT8 SystemFamily[];
} UEFI_CONFIG_SMBIOS_SYSTEM_FAMILY;

typedef struct _UEFI_CONFIG_SMBIOS_BASE_SERIAL_NUMBER
{
    UEFI_CONFIG_HEADER Header;
    UINT8 BaseSerialNumber[];
} UEFI_CONFIG_SMBIOS_BASE_SERIAL_NUMBER;

typedef struct _UEFI_CONFIG_SMBIOS_CHASSIS_SERIAL_NUMBER
{
    UEFI_CONFIG_HEADER Header;
    UINT8 ChassisSerialNumber[];
} UEFI_CONFIG_SMBIOS_CHASSIS_SERIAL_NUMBER;

typedef struct _UEFI_CONFIG_SMBIOS_CHASSIS_ASSET_TAG
{
    UEFI_CONFIG_HEADER Header;
    UINT8 ChassisAssetTag[];
} UEFI_CONFIG_SMBIOS_CHASSIS_ASSET_TAG;

typedef struct _UEFI_CONFIG_SMBIOS_BIOS_LOCK_STRING
{
    UEFI_CONFIG_HEADER Header;
    UINT8 BiosLockString[];
} UEFI_CONFIG_SMBIOS_BIOS_LOCK_STRING;

typedef struct _UEFI_CONFIG_SMBIOS_MEMORY_DEVICE_SERIAL_NUMBER
{
    UEFI_CONFIG_HEADER Header;
    UINT8 MemoryDeviceSerialNumber[];
} UEFI_CONFIG_SMBIOS_MEMORY_DEVICE_SERIAL_NUMBER;

typedef struct _UEFI_CONFIG_SMBIOS_3_1_PROCESSOR_INFORMATION
{
    UEFI_CONFIG_HEADER Header;
    UINT64 ProcessorID;
    UINT16 ExternalClock;
    UINT16 MaxSpeed;
    UINT16 CurrentSpeed;
    UINT16 ProcessorCharacteristics;
    UINT16 ProcessorFamily2;
    UINT8 ProcessorType;
    UINT8 Voltage;
    UINT8 Status;
    UINT8 ProcessorUpgrade;
    UINT16 Reserved;
} UEFI_CONFIG_SMBIOS_3_1_PROCESSOR_INFORMATION;

typedef struct _UEFI_CONFIG_SMBIOS_SOCKET_DESIGNATION
{
    UEFI_CONFIG_HEADER Header;
    UINT8 SocketDesignation[];
} UEFI_CONFIG_SMBIOS_SOCKET_DESIGNATION;

typedef struct _UEFI_CONFIG_SMBIOS_PROCESSOR_MANUFACTURER
{
    UEFI_CONFIG_HEADER Header;
    UINT8 ProcessorManufacturer[];
} UEFI_CONFIG_SMBIOS_PROCESSOR_MANUFACTURER;

typedef struct _UEFI_CONFIG_SMBIOS_PROCESSOR_VERSION
{
    UEFI_CONFIG_HEADER Header;
    UINT8 ProcessorVersion[];
} UEFI_CONFIG_SMBIOS_PROCESSOR_VERSION;

typedef struct _UEFI_CONFIG_SMBIOS_PROCESSOR_SERIAL_NUMBER
{
    UEFI_CONFIG_HEADER Header;
    UINT8 ProcessorSerialNumber[];
} UEFI_CONFIG_SMBIOS_PROCESSOR_SERIAL_NUMBER;

typedef struct _UEFI_CONFIG_SMBIOS_PROCESSOR_ASSET_TAG
{
    UEFI_CONFIG_HEADER Header;
    UINT8 ProcessorAssetTag[];
} UEFI_CONFIG_SMBIOS_PROCESSOR_ASSET_TAG;

typedef struct _UEFI_CONFIG_SMBIOS_PROCESSOR_PART_NUMBER
{
    UEFI_CONFIG_HEADER Header;
    UINT8 ProcessorPartNumber[];
} UEFI_CONFIG_SMBIOS_PROCESSOR_PART_NUMBER;

typedef struct _UEFI_CONFIG_FLAGS
{
    UEFI_CONFIG_HEADER Header;
    struct {
        UINT64 SerialControllersEnabled:1;
        UINT64 PauseAfterBootFailure:1;
        UINT64 PxeIpV6:1;
        UINT64 DebuggerEnabled:1;
        UINT64 LoadOempTable:1;
        UINT64 TpmEnabled:1;
        UINT64 HibernateEnabled:1;
        UINT64 ConsoleMode:2;
        UINT64 MemoryAttributesTableEnabled:1;
        UINT64 VirtualBatteryEnabled:1;
        UINT64 SgxMemoryEnabled:1;
        UINT64 IsVmbfsBoot:1;
        UINT64 MeasureAdditionalPcrs:1;
        UINT64 DisableFrontpage:1;
        UINT64 DefaultBootAlwaysAttempt:1;
        UINT64 LowPowerS0IdleEnabled : 1;
        UINT64 VpciBootEnabled:1;
        UINT64 ProcIdleEnabled : 1;
        UINT64 DisableSha384Pcr : 1;
        UINT64 MediaPresentEnabledByDefault : 1;
        UINT64 MemoryProtectionMode: 2;
        UINT64 EnableIMCWhenIsolated: 1;
        UINT64 WatchdogEnabled : 1;
        UINT64 TpmLocalityRegsEnabled : 1;
        UINT64 Dhcp6DuidTypeLlt : 1;
        UINT64 Reserved:37;
    } Flags;
} UEFI_CONFIG_FLAGS;

typedef struct _UEFI_CONFIG_PROCESSOR_INFORMATION
{
    UEFI_CONFIG_HEADER Header;
    UINT32 MaxProcessorCount;
    UINT32 ProcessorCount;
    UINT32 ProcessorsPerVirtualSocket;
    UINT32 ThreadsPerProcessor;
} UEFI_CONFIG_PROCESSOR_INFORMATION;

typedef struct _UEFI_CONFIG_MMIO
{
    UINT64 MmioPageNumberStart;
    UINT64 MmioSizeInPages;
} UEFI_CONFIG_MMIO;

typedef struct _UEFI_CONFIG_MMIO_RANGES
{
    UEFI_CONFIG_HEADER Header;
    UEFI_CONFIG_MMIO Ranges[];
} UEFI_CONFIG_MMIO_RANGES;

// Dynamically sized structure for MPIDR values for AARCH64
typedef struct _UEFI_CONFIG_AARCH64_MPIDR
{
    UEFI_CONFIG_HEADER Header;
    UINT64 ProcessorMPIDRValues[];
} UEFI_CONFIG_AARCH64_MPIDR;

// Dynamically sized structure for binary blob that is an ACPI table.
// Only used internally for testing, gated behind velocity.
typedef struct _UEFI_CONFIG_ACPI_TABLE
{
    UEFI_CONFIG_HEADER Header;
    UINT8 AcpiTableData[];
} UEFI_CONFIG_ACPI_TABLE;

typedef struct _UEFI_CONFIG_NVDIMM_COUNT
{
    UEFI_CONFIG_HEADER Header;
#pragma warning(disable : 4201)
    union
    {
        UINT64 Padding;
        UINT16 Count;
    };
#pragma warning(default : 4201)
} UEFI_CONFIG_NVDIMM_COUNT;

typedef struct _UEFI_CONFIG_VPCI_INSTANCE_FILTER
{
    UEFI_CONFIG_HEADER Header;
    UINT8 InstanceGuid[sizeof(GUID)];
} UEFI_CONFIG_VPCI_INSTANCE_FILTER;

typedef struct _UEFI_CONFIG_AMD_ASPT
{
    UEFI_CONFIG_HEADER Header;
    UINT8 Aspt[];
} UEFI_CONFIG_AMD_ASPT;

typedef struct _UEFI_CONFIG_GIC
{
    UEFI_CONFIG_HEADER Header;
    UINT64 GicDistributorBase;    // GICD
    UINT64 GicRedistributorsBase; // Redistributor block containing the BSP's GICR
} UEFI_CONFIG_GIC;

typedef struct _UEFI_CONFIG_MCFG
{
    UEFI_CONFIG_HEADER Header;
    UINT8 Mcfg[];
} UEFI_CONFIG_MCFG;

typedef struct _UEFI_CONFIG_SSDT
{
    UEFI_CONFIG_HEADER Header;
    UINT8 Ssdt[];
} UEFI_CONFIG_SSDT;

//
// UEFI configuration information for direct parsing of IGVM parameters.
//

typedef struct _UEFI_IGVM_PARAMETER_INFO
{
    UINT32 ParameterPageCount;
    UINT32 CpuidPagesOffset;
    UINT64 VpContextPageNumber;
    UINT32 LoaderBlockOffset;
    UINT32 CommandLineOffset;
    UINT32 CommandLinePageCount;
    UINT32 MemoryMapOffset;
    UINT32 MemoryMapPageCount;
    UINT32 MadtOffset;
    UINT32 MadtPageCount;
    UINT32 SratOffset;
    UINT32 SratPageCount;
    UINT32 MaximumProcessorCount;
    UINT32 UefiMemoryMapOffset;
    UINT32 UefiMemoryMapPageCount;
    UINT32 UefiIgvmConfigurationFlags;
    UINT32 SecretsPageOffset;
} UEFI_IGVM_PARAMETER_INFO;

//
// Various flags for UefiIgvmConfigurationFlags
//
#define UEFI_IGVM_CONFIGURATION_ENABLE_HOST_EMULATORS 0x00000001

typedef struct _UEFI_IGVM_LOADER_BLOCK
{
    UINT32 NumberOfProcessors;
} UEFI_IGVM_LOADER_BLOCK;

#pragma pack(pop)
