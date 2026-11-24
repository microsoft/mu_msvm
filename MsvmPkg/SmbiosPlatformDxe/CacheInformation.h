#pragma once

#include <Uefi.h>
#include <Protocol/Smbios.h>
#include <IndustryStandard/SmBios.h>

BOOLEAN
AddStructure (
  IN              EFI_SMBIOS_PROTOCOL  *Smbios,
  IN              VOID                 *Structure,
  IN OPTIONAL     CHAR8                **Strings,
  OUT OPTIONAL    EFI_SMBIOS_HANDLE    *Handle
  );

VOID
AddProcessorCacheInformation (
  IN     EFI_SMBIOS_PROTOCOL  *Smbios,
  IN     UINT32               FirstProcessorUid,
  IN     UINT32               ProcessorCount,
  IN OUT SMBIOS_TABLE_TYPE4   *Processor
  );
