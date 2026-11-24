#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <AcpiReplacementTable.h>
#include <BiosInterface.h>
#include "../SmbiosPlatformDxe/CacheInformation.h"
#undef NULL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

EFI_GUID                   gAcpiReplacementTableHobGuid;
STATIC UINT8               mTable[SIZE_1MB];
STATIC SMBIOS_TABLE_TYPE7  mRecords[16];
STATIC UINTN               mRecordCount;
STATIC UINTN               mCheckCount;
STATIC UINTN               mAllocationCount;
STATIC UINTN               mFailAllocation;
STATIC UINTN               mAddCount;
STATIC UINTN               mFailAdd;
STATIC BOOLEAN             mHaveTable;
STATIC struct {
  EFI_HOB_GUID_TYPE                  Hob;
  ACPI_REPLACEMENT_TABLE_HOB_DATA    Data;
} mReplacementHob;

VOID *EFIAPI
GetFirstGuidHob (
  CONST EFI_GUID  *Guid
  )
{
  return mHaveTable ? &mReplacementHob : NULL;
}

VOID *EFIAPI
GetNextGuidHob (
  CONST EFI_GUID  *Guid,
  CONST VOID      *HobStart
  )
{
  return NULL;
}

VOID *EFIAPI
AllocateZeroPool (
  UINTN  Size
  )
{
  return (++mAllocationCount == mFailAllocation) ? NULL : calloc (1, Size);
}

VOID EFIAPI
FreePool (
  VOID  *Buffer
  )
{
  free (Buffer);
}

VOID *EFIAPI
ZeroMem (
  VOID   *Buffer,
  UINTN  Size
  )
{
  return memset (Buffer, 0, Size);
}

VOID *EFIAPI
CopyMem (
  VOID        *Destination,
  CONST VOID  *Source,
  UINTN       Size
  )
{
  return memcpy (Destination, Source, Size);
}

UINT32 EFIAPI
ReadUnaligned32 (
  CONST UINT32  *Buffer
  )
{
  UINT32  value;

  memcpy (&value, Buffer, sizeof (value));
  return value;
}

UINT8 EFIAPI
CalculateCheckSum8 (
  CONST UINT8  *Buffer,
  UINTN        Length
  )
{
  UINT8  sum = 0;

  while (Length-- != 0) {
    sum = (UINT8)(sum + *Buffer++);
  }

  return (UINT8)(0 - sum);
}

STATIC
VOID
Check (
  BOOLEAN      Condition,
  CONST CHAR8  *Name
  )
{
  if (!Condition) {
    printf ("FAIL: %s\n", Name);
    exit (1);
  }

  mCheckCount++;
}

BOOLEAN
AddStructure (
  EFI_SMBIOS_PROTOCOL  *Smbios,
  VOID                 *Structure,
  CHAR8                **Strings,
  EFI_SMBIOS_HANDLE    *Handle
  )
{
  SMBIOS_TABLE_TYPE7  *cache   = Structure;
  CHAR8               *strings = (VOID *)((UINT8 *)cache + cache->Hdr.Length);

  if (++mAddCount == mFailAdd) {
    return FALSE;
  }

  Check (mRecordCount < ARRAY_SIZE (mRecords), "record capacity");
  Check ((cache->Hdr.Type == EFI_SMBIOS_TYPE_CACHE_INFORMATION) && (cache->Hdr.Length == 27), "SMBIOS 3.1 record layout");
  Check ((cache->Hdr.Handle == SMBIOS_HANDLE_PI_RESERVED) && (cache->SocketDesignation == 1), "header and designation");
  Check (
    (strings[0] == 'L') && (strings[1] == '1' + (cache->CacheConfiguration & 7)) &&
    (strcmp (strings + 2, " Cache") == 0) && (strings[9] == 0),
    "designation and double terminator"
    );
  Check (
    cache->SupportedSRAMType.Unknown && cache->CurrentSRAMType.Unknown &&
    (cache->ErrorCorrectionType == CacheErrorUnknown) && (cache->CacheSpeed == 0),
    "unsupported fields remain unknown"
    );
  memcpy (&mRecords[mRecordCount], Structure, sizeof (SMBIOS_TABLE_TYPE7));
  *Handle = (EFI_SMBIOS_HANDLE)(0x100 + mRecordCount++);
  return TRUE;
}

STATIC
UINT32
AppendProcessor (
  UINT32  Parent,
  UINT32  Uid,
  UINT32  Flags,
  UINT32  FirstResource,
  UINT32  SecondResource
  )
{
  EFI_ACPI_DESCRIPTION_HEADER            *table     = (VOID *)mTable;
  UINT32                                 offset     = table->Length;
  UINT32                                 count      = (FirstResource != 0) + (SecondResource != 0);
  EFI_ACPI_6_5_PPTT_STRUCTURE_PROCESSOR  *processor = (VOID *)(mTable + offset);

  processor->Type   = EFI_ACPI_6_5_PPTT_TYPE_PROCESSOR;
  processor->Length = (UINT8)(sizeof (*processor) + count * sizeof (UINT32));
  memcpy (&processor->Flags, &Flags, sizeof (Flags));
  processor->Parent                   = Parent;
  processor->AcpiProcessorId          = Uid;
  processor->NumberOfPrivateResources = count;
  if (count != 0) {
    memcpy (processor + 1, &FirstResource, sizeof (FirstResource));
  }

  if (count == 2) {
    memcpy ((UINT8 *)(processor + 1) + sizeof (UINT32), &SecondResource, sizeof (SecondResource));
  }

  table->Length += processor->Length;
  return offset;
}

STATIC
UINT32
AppendCache (
  UINT32  Next,
  UINT32  Size,
  UINT8   Type,
  UINT8   Ways
  )
{
  EFI_ACPI_DESCRIPTION_HEADER        *table = (VOID *)mTable;
  UINT32                             offset = table->Length;
  EFI_ACPI_6_5_PPTT_STRUCTURE_CACHE  *cache = (VOID *)(mTable + offset);

  cache->Type                     = EFI_ACPI_6_5_PPTT_TYPE_CACHE;
  cache->Length                   = sizeof (*cache);
  cache->Flags.SizePropertyValid  = TRUE;
  cache->Flags.CacheTypeValid     = TRUE;
  cache->Flags.AssociativityValid = TRUE;
  cache->Associativity            = Ways;
  cache->Attributes.CacheType     = Type;
  cache->NextLevelOfCache         = Next;
  cache->Size                     = Size;
  table->Length                  += cache->Length;
  return offset;
}

STATIC
VOID
Reset (
  VOID
  )
{
  EFI_ACPI_DESCRIPTION_HEADER  *table = (VOID *)mTable;

  memset (mTable, 0, sizeof (mTable));
  mRecordCount               = 0;
  mAllocationCount           = 0;
  mFailAllocation            = 0;
  mAddCount                  = 0;
  mFailAdd                   = 0;
  mHaveTable                 = TRUE;
  table->Signature           = EFI_ACPI_6_5_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_STRUCTURE_SIGNATURE;
  table->Length              = sizeof (*table);
  table->Revision            = 1;
  mReplacementHob.Data.Table = table;
}

STATIC
SMBIOS_TABLE_TYPE4
Run (
  UINT32  FirstUid,
  UINT32  Count
  )
{
  EFI_ACPI_DESCRIPTION_HEADER  *table = (VOID *)mTable;
  SMBIOS_TABLE_TYPE4           processor;

  memset (&processor, 0, sizeof (processor));
  table->Checksum         = 0;
  table->Checksum         = CalculateCheckSum8 (mTable, table->Length);
  processor.L1CacheHandle = processor.L2CacheHandle = processor.L3CacheHandle = 0xFFFF;
  AddProcessorCacheInformation (NULL, FirstUid, Count, &processor);
  return processor;
}

STATIC
VOID
BuildSocket (
  UINT32  FirstUid,
  UINT32  CoreCount,
  UINT32  ThreadsPerCore,
  UINT32  DieCount
  )
{
  UINT32  socket;
  UINT32  parent;
  UINT32  l3;
  UINT32  l2;
  UINT32  l1i;
  UINT32  l1d;
  UINT32  core;
  UINT32  thread;
  UINT32  coreNode;
  UINT32  uid = FirstUid;

  l3     = DieCount == 0 ? AppendCache (0, 8 * SIZE_1MB, 2, 16) : 0;
  socket = AppendProcessor (0, 0, BIT0, l3, 0);
  parent = socket;
  for (core = 0; core < CoreCount; core++) {
    if ((DieCount != 0) && (core % (CoreCount / DieCount) == 0)) {
      l3     = AppendCache (0, 8 * SIZE_1MB, 3, 16);
      parent = AppendProcessor (socket, 0, 0, l3, 0);
    }

    l2       = AppendCache (0, 256 * SIZE_1KB, 2, 8);
    l1i      = AppendCache (l2, 32 * SIZE_1KB, 1, 4);
    l1d      = AppendCache (l2, 64 * SIZE_1KB, 0, 8);
    coreNode = AppendProcessor (parent, ThreadsPerCore == 1 ? uid++ : 0, ThreadsPerCore == 1 ? BIT1 : 0, l1i, l1d);
    if (ThreadsPerCore > 1) {
      for (thread = 0; thread < ThreadsPerCore; thread++) {
        AppendProcessor (coreNode, uid++, BIT1, 0, 0);
      }
    }
  }
}

STATIC
VOID
CheckNoRecords (
  UINT32       FirstUid,
  UINT32       Count,
  CONST CHAR8  *Name
  )
{
  SMBIOS_TABLE_TYPE4  processor = Run (FirstUid, Count);

  Check (
    (mRecordCount == 0) && (processor.L1CacheHandle == 0xFFFF) &&
    (processor.L2CacheHandle == 0xFFFF) && (processor.L3CacheHandle == 0xFFFF),
    Name
    );
}

int
main (
  void
  )
{
  UEFI_CONFIG_FLAGS                      configFlags = { 0 };
  UINT64                                 rawFlags;
  SMBIOS_TABLE_TYPE4                     processor;
  EFI_ACPI_DESCRIPTION_HEADER            *table = (VOID *)mTable;
  EFI_ACPI_6_5_PPTT_STRUCTURE_PROCESSOR  *node;
  EFI_ACPI_6_5_PPTT_STRUCTURE_CACHE      *cache;
  UINT32                                 socket;
  UINT32                                 leaf;
  UINT32                                 other;
  UINT32                                 l1;
  UINT32                                 l2;
  UINTN                                  failure;

  Check (sizeof (configFlags.Flags) == sizeof (UINT64), "configuration flags remain 64 bits");
  Check (sizeof (configFlags) == sizeof (UEFI_CONFIG_HEADER) + sizeof (UINT64), "configuration ABI size unchanged");
  Check (!configFlags.Flags.SmbiosCacheInformation, "zero flags preserve disabled cache reporting");
  configFlags.Flags.SmbiosCacheInformation = TRUE;
  memcpy (&rawFlags, &configFlags.Flags, sizeof (rawFlags));
  Check (rawFlags == BIT34, "cache reporting uses reserved wire bit 34");
  configFlags.Flags.SmbiosCacheInformation = FALSE;
  configFlags.Flags.ForceDmaBounceEnabled  = TRUE;
  configFlags.Flags.IpmiEnabled            = TRUE;
  memcpy (&rawFlags, &configFlags.Flags, sizeof (rawFlags));
  Check (rawFlags == (BIT32 | BIT33), "existing neighboring wire bits unchanged");

  Reset ();
  mHaveTable = FALSE;
  CheckNoRecords (1, 1, "absent PPTT");

  Reset ();
  BuildSocket (1, 2, 2, 0);
  processor = Run (1, 4);
  Check (
    (mRecordCount == 3) && (processor.L1CacheHandle == 0x100) &&
    (processor.L2CacheHandle == 0x101) && (processor.L3CacheHandle == 0x102),
    "Type 4 links"
    );
  Check (
    (mRecords[0].InstalledSize.Size == 192) && (mRecords[1].InstalledSize.Size == 512) &&
    (mRecords[2].InstalledSize.Size == 8192),
    "socket aggregation, split L1 and shared L2/L3 deduplication"
    );
  Check (
    (mRecords[0].SystemCacheType == CacheTypeUnified) &&
    (mRecords[0].Associativity == CacheAssociativityUnknown) &&
    (mRecords[1].Associativity == CacheAssociativity8Way),
    "mixed cache types and associativity"
    );
  Check (
    (mRecords[0].CacheConfiguration == (BIT7 | BIT8 | BIT9)) &&
    (mRecords[1].CacheConfiguration == (BIT7 | BIT8 | BIT9 | 1)),
    "level and unknown write policy"
    );

  BuildSocket (5, 1, 1, 0);
  processor = Run (5, 1);
  Check (
    (mRecordCount == 6) && (mRecords[3].InstalledSize.Size == 96) &&
    (processor.L1CacheHandle == 0x103),
    "independent second socket"
    );

  Reset ();
  BuildSocket (1, 4, 1, 2);
  Run (1, 4);
  Check ((mRecordCount == 3) && (mRecords[2].InstalledSize.Size == 16384), "per-die L3 aggregation");

  Reset ();
  BuildSocket (1, 2, 2, 0);
  CheckNoRecords (1, 2, "partial package rejected");
  CheckNoRecords (1, 5, "missing processor rejected");
  BuildSocket (5, 1, 1, 0);
  CheckNoRecords (4, 2, "mixed packages rejected");

  for (failure = 1; failure <= 2; failure++) {
    Reset ();
    BuildSocket (1, 1, 1, 0);
    mFailAllocation = failure;
    CheckNoRecords (1, 1, "allocation failure");
  }

  Reset ();
  BuildSocket (1, 1, 1, 0);
  mFailAdd  = 2;
  processor = Run (1, 1);
  Check (
    (mRecordCount == 2) && (processor.L1CacheHandle == 0x100) &&
    (processor.L2CacheHandle == 0xFFFF) && (processor.L3CacheHandle == 0x101),
    "add failure cannot create dangling handle"
    );

  Reset ();
  l2        = AppendCache (0, 256 * SIZE_1KB, 2, 8);
  l1        = AppendCache (l2, 32 * SIZE_1KB, 0, 255);
  socket    = AppendProcessor (0, 0, BIT0, 0, 0);
  leaf      = AppendProcessor (socket, 1, BIT1, l1, 0);
  processor = Run (1, 1);
  Check (
    (mRecordCount == 2) && (processor.L3CacheHandle == 0xFFFF) &&
    (mRecords[0].Associativity == CacheAssociativityFully),
    "no L3 and fully associative L1"
    );

  mRecordCount                   = 0;
  cache                          = (VOID *)(mTable + l1);
  cache->Flags.SizePropertyValid = FALSE;
  processor                      = Run (1, 1);
  Check (
    (mRecordCount == 1) && (processor.L1CacheHandle == 0xFFFF) &&
    (mRecords[0].CacheConfiguration % 8 == 1),
    "invalid size suppresses only that level"
    );

  mRecordCount                    = 0;
  cache->Flags.SizePropertyValid  = TRUE;
  cache->Flags.AssociativityValid = FALSE;
  cache->Flags.CacheTypeValid     = FALSE;
  cache->Flags.WritePolicyValid   = TRUE;
  cache->Attributes.WritePolicy   = EFI_ACPI_6_5_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK;
  Run (1, 1);
  Check (
    (mRecords[0].Associativity == CacheAssociativityUnknown) &&
    (mRecords[0].SystemCacheType == CacheTypeUnknown) &&
    (mRecords[0].CacheConfiguration == (BIT7 | BIT8)),
    "validity flags and known write policy"
    );

  for (failure = 0; failure < 12; failure++) {
    Reset ();
    l1     = AppendCache (0, 32 * SIZE_1KB, 0, 1);
    socket = AppendProcessor (0, 0, BIT0, 0, 0);
    leaf   = AppendProcessor (socket, 1, BIT1, l1, 0);
    node   = (VOID *)(mTable + leaf);
    cache  = (VOID *)(mTable + l1);
    switch (failure) {
      case 0: node->Length = 0;
        break;
      case 1: node->NumberOfPrivateResources = MAX_UINT32;
        break;
      case 2: node->Parent = sizeof (*table) + 1;
        break;
      case 3: other = l1 + 1;
        memcpy (node + 1, &other, sizeof (other));
        break;
      case 4: memcpy (node + 1, &socket, sizeof (socket));
        break;
      case 5: cache->NextLevelOfCache = l1;
        break;
      case 6: node->Parent = leaf;
        break;
      case 7: AppendProcessor (socket, 1, BIT1, l1, 0);
        break;
      case 8: table->Revision = 4;
        break;
      case 9: cache->NextLevelOfCache = table->Length;
        break;
      case 10: node->Length += sizeof (UINT32);
        table->Length       += sizeof (UINT32);
        break;
      case 11: cache->Length = 4;
        break;
    }

    CheckNoRecords (1, 1, "malformed or unsupported PPTT rejected");
  }

  Reset ();
  l1     = AppendCache (0, 32 * SIZE_1KB, 0, 1);
  socket = AppendProcessor (0, 0, BIT0, 0, 0);
  AppendProcessor (socket, 1, BIT1, l1, 0);
  table->Checksum = CalculateCheckSum8 (mTable, table->Length);
  table->Checksum++;
  memset (&processor, 0, sizeof (processor));
  AddProcessorCacheInformation (NULL, 1, 1, &processor);
  Check (mRecordCount == 0, "bad checksum rejected");

  Reset ();
  l1     = AppendCache (0, 0x80000000, 2, 16);
  socket = AppendProcessor (0, 0, BIT0, 0, 0);
  AppendProcessor (socket, 1, BIT1, l1, 0);
  Run (1, 1);
  Check (
    (mRecordCount == 1) && (mRecords[0].InstalledSize.Size == 0x7FFF) &&
    mRecords[0].InstalledSize.Granularity64K &&
    (mRecords[0].InstalledSize2.Size == 32768) && mRecords[0].InstalledSize2.Granularity64K,
    "extended cache size over 2047 MiB"
    );

  Reset ();
  l1     = AppendCache (0, 32 * SIZE_1MB, 2, 16);
  socket = AppendProcessor (0, 0, BIT0, 0, 0);
  AppendProcessor (socket, 1, BIT1, l1, 0);
  Run (1, 1);
  Check (
    (mRecords[0].InstalledSize.Size == 512) && mRecords[0].InstalledSize.Granularity64K &&
    (mRecords[0].InstalledSize2.Size == 512) && mRecords[0].InstalledSize2.Granularity64K,
    "legacy and extended size granularities match"
    );

  Reset ();
  BuildSocket (1, 1024, 2, 0);
  Run (1, 2048);
  Check (
    (mRecordCount == 3) && (mRecords[0].InstalledSize2.Size == 1536) &&
    mRecords[0].InstalledSize2.Granularity64K,
    "2048 logical processors without SMT double-counting"
    );

  printf ("PASS: %llu cache-reporting assertions\n", (unsigned long long)mCheckCount);
  return 0;
}
