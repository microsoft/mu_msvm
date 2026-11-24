#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <AcpiReplacementTable.h>
#include "CacheInformation.h"

#define SMBIOS_PROCESSOR_CACHE_LEVELS  3
#define PPTT_PROCESSOR_NODE            1
#define PPTT_CACHE_NODE                2
#define PPTT_OTHER_NODE                3

typedef struct {
  UINT8      Type;
  UINT8      Level;
  BOOLEAN    Counted;
  BOOLEAN    HasChildren;
} PPTT_NODE_INFO;

typedef struct {
  UINT64     Size;
  BOOLEAN    Present;
  BOOLEAN    SizeValid;
  UINT8      CacheType;
  UINT8      Associativity;
  UINT16     Mode;
} SMBIOS_CACHE_SUMMARY;

STATIC
UINT8
CacheAssociativity (
  IN EFI_ACPI_6_5_PPTT_STRUCTURE_CACHE  *Cache
  )
{
  if (!Cache->Flags.AssociativityValid || (Cache->Associativity == 0)) {
    return CacheAssociativityUnknown;
  }

  switch (Cache->Associativity) {
    case 1:   return CacheAssociativityDirectMapped;
    case 2:   return CacheAssociativity2Way;
    case 4:   return CacheAssociativity4Way;
    case 8:   return CacheAssociativity8Way;
    case 12:  return CacheAssociativity12Way;
    case 16:  return CacheAssociativity16Way;
    case 20:  return CacheAssociativity20Way;
    case 24:  return CacheAssociativity24Way;
    case 32:  return CacheAssociativity32Way;
    case 48:  return CacheAssociativity48Way;
    case 64:  return CacheAssociativity64Way;
    case 255: return CacheAssociativityFully;
    default:  return CacheAssociativityOther;
  }
}

STATIC
BOOLEAN
IndexPptt (
  IN     EFI_ACPI_DESCRIPTION_HEADER  *Table,
  IN OUT PPTT_NODE_INFO               *Nodes
  )
{
  UINT32                                 offset;
  UINT32                                 resource;
  UINT32                                 reference;
  EFI_ACPI_6_5_PPTT_STRUCTURE_HEADER     *header;
  EFI_ACPI_6_5_PPTT_STRUCTURE_PROCESSOR  *processor;
  EFI_ACPI_6_5_PPTT_STRUCTURE_CACHE      *cache;

  for (offset = sizeof (*Table); offset < Table->Length; offset += header->Length) {
    if (Table->Length - offset < sizeof (*header)) {
      return FALSE;
    }

    header = (VOID *)((UINT8 *)Table + offset);
    if ((header->Length < sizeof (*header)) || (header->Length > Table->Length - offset)) {
      return FALSE;
    }

    Nodes[offset].Type = PPTT_OTHER_NODE;
    if (header->Type == EFI_ACPI_6_5_PPTT_TYPE_PROCESSOR) {
      processor = (VOID *)header;
      if ((header->Length < sizeof (*processor)) ||
          (processor->NumberOfPrivateResources != (header->Length - sizeof (*processor)) / sizeof (UINT32)) ||
          ((header->Length - sizeof (*processor)) % sizeof (UINT32) != 0))
      {
        return FALSE;
      }

      Nodes[offset].Type = PPTT_PROCESSOR_NODE;
    } else if (header->Type == EFI_ACPI_6_5_PPTT_TYPE_CACHE) {
      if (header->Length < OFFSET_OF (EFI_ACPI_6_5_PPTT_STRUCTURE_CACHE, CacheId)) {
        return FALSE;
      }

      Nodes[offset].Type = PPTT_CACHE_NODE;
    }
  }

  for (offset = sizeof (*Table); offset < Table->Length; offset += header->Length) {
    header = (VOID *)((UINT8 *)Table + offset);
    if (Nodes[offset].Type == PPTT_PROCESSOR_NODE) {
      processor = (VOID *)header;
      reference = processor->Parent;
      if (reference != 0) {
        if ((reference >= Table->Length) || (Nodes[reference].Type != PPTT_PROCESSOR_NODE)) {
          return FALSE;
        }

        Nodes[reference].HasChildren = TRUE;
      }

      for (resource = 0; resource < processor->NumberOfPrivateResources; resource++) {
        reference = ReadUnaligned32 ((VOID *)((UINT8 *)processor + sizeof (*processor) + resource * sizeof (UINT32)));
        if ((reference >= Table->Length) ||
            ((Nodes[reference].Type != PPTT_CACHE_NODE) && (Nodes[reference].Type != PPTT_OTHER_NODE)))
        {
          return FALSE;
        }
      }
    } else if (Nodes[offset].Type == PPTT_CACHE_NODE) {
      cache     = (VOID *)header;
      reference = cache->NextLevelOfCache;
      if ((reference != 0) && ((reference >= Table->Length) || (Nodes[reference].Type != PPTT_CACHE_NODE))) {
        return FALSE;
      }
    }
  }

  return TRUE;
}

STATIC
UINT32
PpttPackage (
  IN EFI_ACPI_DESCRIPTION_HEADER  *Table,
  IN UINT32                       Offset
  )
{
  UINT32                                 remaining;
  UINT32                                 package;
  EFI_ACPI_6_5_PPTT_STRUCTURE_PROCESSOR  *processor;

  package = 0;
  for (remaining = Table->Length / sizeof (*processor); (Offset != 0) && (remaining != 0); remaining--) {
    processor = (VOID *)((UINT8 *)Table + Offset);
    if (processor->Flags.PhysicalPackage) {
      if (package != 0) {
        return 0;
      }

      package = Offset;
    }

    Offset = processor->Parent;
  }

  return (Offset == 0) ? package : 0;
}

STATIC
BOOLEAN
SummarizePpttCaches (
  IN     EFI_ACPI_DESCRIPTION_HEADER  *Table,
  IN OUT PPTT_NODE_INFO               *Nodes,
  IN     UINT32                       Offset,
  IN OUT SMBIOS_CACHE_SUMMARY         *Summary
  )
{
  EFI_ACPI_6_5_PPTT_STRUCTURE_PROCESSOR  *processor;
  EFI_ACPI_6_5_PPTT_STRUCTURE_CACHE      *cache;
  SMBIOS_CACHE_SUMMARY                   *levelSummary;
  UINT32                                 resource;
  UINT32                                 reference;
  UINT8                                  startingLevel;
  UINT8                                  maximumLevel;
  UINT8                                  level;
  UINT8                                  cacheType;
  UINT8                                  associativity;
  UINT16                                 mode;

  startingLevel = 0;
  while (Offset != 0) {
    processor    = (VOID *)((UINT8 *)Table + Offset);
    maximumLevel = startingLevel;
    for (resource = 0; resource < processor->NumberOfPrivateResources; resource++) {
      reference = ReadUnaligned32 ((VOID *)((UINT8 *)processor + sizeof (*processor) + resource * sizeof (UINT32)));
      if (Nodes[reference].Type != PPTT_CACHE_NODE) {
        continue;
      }

      level = startingLevel;
      while (reference != 0) {
        if (++level > 8) {
          return FALSE;
        }

        if ((Nodes[reference].Level != 0) && (Nodes[reference].Level != level)) {
          return FALSE;
        }

        Nodes[reference].Level = level;
        maximumLevel           = MAX (maximumLevel, level);
        cache                  = (VOID *)((UINT8 *)Table + reference);
        if ((level <= SMBIOS_PROCESSOR_CACHE_LEVELS) && !Nodes[reference].Counted) {
          Nodes[reference].Counted = TRUE;
          levelSummary             = &Summary[level - 1];
          cacheType                = CacheTypeUnknown;
          if (cache->Flags.CacheTypeValid) {
            switch (cache->Attributes.CacheType) {
              case EFI_ACPI_6_5_CACHE_ATTRIBUTES_CACHE_TYPE_DATA:
                cacheType = CacheTypeData;
                break;
              case EFI_ACPI_6_5_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION:
                cacheType = CacheTypeInstruction;
                break;
              default:
                cacheType = CacheTypeUnified;
                break;
            }
          }

          associativity = CacheAssociativity (cache);
          mode          = BIT8 | BIT9;
          if (cache->Flags.WritePolicyValid) {
            mode = cache->Attributes.WritePolicy ? 0 : BIT8;
          }

          if (!levelSummary->Present) {
            levelSummary->SizeValid     = TRUE;
            levelSummary->CacheType     = cacheType;
            levelSummary->Associativity = associativity;
            levelSummary->Mode          = mode;
          } else {
            if ((levelSummary->CacheType == CacheTypeUnknown) || (cacheType == CacheTypeUnknown)) {
              levelSummary->CacheType = CacheTypeUnknown;
            } else if (levelSummary->CacheType != cacheType) {
              levelSummary->CacheType = CacheTypeUnified;
            }

            if (levelSummary->Associativity != associativity) {
              levelSummary->Associativity = CacheAssociativityUnknown;
            }

            if (levelSummary->Mode != mode) {
              levelSummary->Mode = BIT8 | BIT9;
            }
          }

          levelSummary->Present = TRUE;
          if (!cache->Flags.SizePropertyValid || (cache->Size == 0)) {
            levelSummary->SizeValid = FALSE;
          }

          levelSummary->Size += cache->Size;
        }

        reference = cache->NextLevelOfCache;
      }
    }

    startingLevel = maximumLevel;
    Offset        = processor->Parent;
  }

  return TRUE;
}

STATIC
BOOLEAN
CollectSocketCaches (
  IN     EFI_ACPI_DESCRIPTION_HEADER  *Table,
  IN OUT PPTT_NODE_INFO               *Nodes,
  IN     UINT32                       FirstProcessorUid,
  IN     UINT32                       ProcessorCount,
  IN OUT SMBIOS_CACHE_SUMMARY         *Summary
  )
{
  EFI_ACPI_6_5_PPTT_STRUCTURE_HEADER     *header;
  EFI_ACPI_6_5_PPTT_STRUCTURE_PROCESSOR  *processor;
  UINT32                                 offset;
  UINT32                                 package;
  UINT32                                 socketPackage;
  UINT32                                 matched;
  UINT8                                  *processorSeen;
  BOOLEAN                                result;

  processorSeen = AllocateZeroPool (ProcessorCount);
  if (processorSeen == NULL) {
    return FALSE;
  }

  result        = FALSE;
  socketPackage = 0;
  matched       = 0;
  for (offset = sizeof (*Table); offset < Table->Length; offset += header->Length) {
    header    = (VOID *)((UINT8 *)Table + offset);
    processor = (VOID *)header;
    if ((Nodes[offset].Type != PPTT_PROCESSOR_NODE) || Nodes[offset].HasChildren || !processor->Flags.AcpiProcessorIdValid) {
      continue;
    }

    if ((processor->AcpiProcessorId < FirstProcessorUid) || (processor->AcpiProcessorId - FirstProcessorUid >= ProcessorCount)) {
      continue;
    }

    package = PpttPackage (Table, offset);
    if ((package == 0) || ((socketPackage != 0) && (socketPackage != package)) ||
        processorSeen[processor->AcpiProcessorId - FirstProcessorUid])
    {
      goto Exit;
    }

    processorSeen[processor->AcpiProcessorId - FirstProcessorUid] = TRUE;
    socketPackage                                                 = package;
    matched++;
    if (!SummarizePpttCaches (Table, Nodes, offset, Summary)) {
      goto Exit;
    }
  }

  if (matched != ProcessorCount) {
    goto Exit;
  }

  for (offset = sizeof (*Table); offset < Table->Length; offset += header->Length) {
    header    = (VOID *)((UINT8 *)Table + offset);
    processor = (VOID *)header;
    if ((Nodes[offset].Type == PPTT_PROCESSOR_NODE) && !Nodes[offset].HasChildren &&
        (!processor->Flags.AcpiProcessorIdValid || (processor->AcpiProcessorId < FirstProcessorUid) ||
         (processor->AcpiProcessorId - FirstProcessorUid >= ProcessorCount)) &&
        (PpttPackage (Table, offset) == socketPackage))
    {
      goto Exit;
    }
  }

  result = TRUE;

Exit:
  FreePool (processorSeen);
  return result;
}

VOID
AddProcessorCacheInformation (
  IN     EFI_SMBIOS_PROTOCOL  *Smbios,
  IN     UINT32               FirstProcessorUid,
  IN     UINT32               ProcessorCount,
  IN OUT SMBIOS_TABLE_TYPE4   *Processor
  )
{
  EFI_ACPI_DESCRIPTION_HEADER  *table;
  PPTT_NODE_INFO               *nodes;
  SMBIOS_CACHE_SUMMARY         summary[SMBIOS_PROCESSOR_CACHE_LEVELS];
  EFI_SMBIOS_HANDLE            handle;
  UINTN                        level;
  UINT64                       size;
  BOOLEAN                      granularity64K;

  struct {
    SMBIOS_TABLE_TYPE7    Formatted;
    CHAR8                 Strings[11];
  } cacheInfo;

  table = FindAcpiReplacementTable (EFI_ACPI_6_5_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_STRUCTURE_SIGNATURE);
  if ((table == NULL) || (table->Length < sizeof (*table)) || (table->Revision < 1) || (table->Revision > 3) ||
      (ProcessorCount == 0) || (CalculateCheckSum8 ((VOID *)table, table->Length) != 0))
  {
    return;
  }

  nodes = AllocateZeroPool ((UINTN)table->Length * sizeof (*nodes));
  if (nodes == NULL) {
    return;
  }

  ZeroMem (summary, sizeof (summary));
  if (IndexPptt (table, nodes) && CollectSocketCaches (table, nodes, FirstProcessorUid, ProcessorCount, summary)) {
    for (level = 0; level < ARRAY_SIZE (summary); level++) {
      if (!summary[level].Present || !summary[level].SizeValid || (summary[level].Size % SIZE_1KB != 0)) {
        continue;
      }

      size           = summary[level].Size / SIZE_1KB;
      granularity64K = size > SMBIOS_CACHE_SIZE_MAX_SIZE_1K_GRANULARITY;
      if (granularity64K) {
        if (size % 64 != 0) {
          continue;
        }

        size /= 64;
      }

      if (size > MAX_INT32) {
        continue;
      }

      ZeroMem (&cacheInfo, sizeof (cacheInfo));
      cacheInfo.Formatted.Hdr.Type                         = EFI_SMBIOS_TYPE_CACHE_INFORMATION;
      cacheInfo.Formatted.Hdr.Length                       = sizeof (SMBIOS_TABLE_TYPE7);
      cacheInfo.Formatted.Hdr.Handle                       = SMBIOS_HANDLE_PI_RESERVED;
      cacheInfo.Formatted.SocketDesignation                = 1;
      cacheInfo.Formatted.CacheConfiguration               = (UINT16)(BIT7 | summary[level].Mode | level);
      cacheInfo.Formatted.MaximumCacheSize.Size            = (UINT16)MIN (size, MAX_INT16);
      cacheInfo.Formatted.MaximumCacheSize.Granularity64K  = granularity64K;
      cacheInfo.Formatted.InstalledSize                    = cacheInfo.Formatted.MaximumCacheSize;
      cacheInfo.Formatted.MaximumCacheSize2.Size           = (UINT32)size;
      cacheInfo.Formatted.MaximumCacheSize2.Granularity64K = granularity64K;
      cacheInfo.Formatted.InstalledSize2                   = cacheInfo.Formatted.MaximumCacheSize2;
      cacheInfo.Formatted.SupportedSRAMType.Unknown        = 1;
      cacheInfo.Formatted.CurrentSRAMType.Unknown          = 1;
      cacheInfo.Formatted.ErrorCorrectionType              = CacheErrorUnknown;
      cacheInfo.Formatted.SystemCacheType                  = summary[level].CacheType;
      cacheInfo.Formatted.Associativity                    = summary[level].Associativity;
      CopyMem (cacheInfo.Strings, "L1 Cache", sizeof ("L1 Cache"));
      cacheInfo.Strings[1] = (CHAR8)('1' + level);
      if (AddStructure (Smbios, &cacheInfo, NULL, &handle)) {
        switch (level) {
          case 0: Processor->L1CacheHandle = handle;
            break;
          case 1: Processor->L2CacheHandle = handle;
            break;
          case 2: Processor->L3CacheHandle = handle;
            break;
        }
      }
    }
  }

  FreePool (nodes);
}
