/** @file
  Implementation of EFI_ADAPTER_INFORMATION_PROTOCOL for NetvscDxe.

  This provides media state information so that upper layers (DxeNetLib) can
  quickly determine whether the network link is connected without falling back
  to the slower SNP GetStatus() polling path.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

// MS_HYP_CHANGE BEGIN

#include "Snp.h"

#include <Protocol/AdapterInformation.h>

/**
  Returns the current state information for the adapter.

  Only gEfiAdapterInfoMediaStateGuid is supported. Returns the current media
  state: EFI_SUCCESS if media is present, EFI_NO_MEDIA if not.

  @param[in]  This                   A pointer to the EFI_ADAPTER_INFORMATION_PROTOCOL instance.
  @param[in]  InformationType        A pointer to an EFI_GUID that defines the contents of InformationBlock.
  @param[out] InformationBlock       The service returns a pointer to the buffer with the InformationBlock
                                     structure which contains details about the data specific to InformationType.
  @param[out] InformationBlockSize   The driver returns the size of the InformationBlock in bytes.

  @retval EFI_SUCCESS                The InformationType information was retrieved.
  @retval EFI_UNSUPPORTED            The InformationType is not known.
  @retval EFI_INVALID_PARAMETER      A required parameter is NULL.
  @retval EFI_OUT_OF_RESOURCES       The request could not be completed due to a lack of resources.

**/
EFI_STATUS
EFIAPI
NetvscAipGetInformation (
  IN  EFI_ADAPTER_INFORMATION_PROTOCOL  *This,
  IN  EFI_GUID                          *InformationType,
  OUT VOID                              **InformationBlock,
  OUT UINTN                             *InformationBlockSize
  )
{
  EFI_ADAPTER_INFO_MEDIA_STATE  *MediaInfo;
  SNP_DRIVER                    *Snp;

  if ((This == NULL) || (InformationType == NULL) ||
      (InformationBlock == NULL) || (InformationBlockSize == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (!CompareGuid (InformationType, &gEfiAdapterInfoMediaStateGuid)) {
    return EFI_UNSUPPORTED;
  }

  Snp = SNP_DRIVER_FROM_AIP (This);

  MediaInfo = AllocateZeroPool (sizeof (EFI_ADAPTER_INFO_MEDIA_STATE));
  if (MediaInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  if (Snp->AdapterContext->NicInfo.MediaPresent) {
    MediaInfo->MediaState = EFI_SUCCESS;
  } else {
    MediaInfo->MediaState = EFI_NO_MEDIA;
  }

  *InformationBlock     = MediaInfo;
  *InformationBlockSize = sizeof (EFI_ADAPTER_INFO_MEDIA_STATE);

  return EFI_SUCCESS;
}

/**
  Sets state information for an adapter.

  No information types are writable, so this always returns EFI_UNSUPPORTED.

  @param[in]  This                   A pointer to the EFI_ADAPTER_INFORMATION_PROTOCOL instance.
  @param[in]  InformationType        A pointer to an EFI_GUID that defines the contents of InformationBlock.
  @param[in]  InformationBlock       A pointer to the InformationBlock structure.
  @param[in]  InformationBlockSize   The size of the InformationBlock in bytes.

  @retval EFI_UNSUPPORTED            Setting information is not supported.

**/
EFI_STATUS
EFIAPI
NetvscAipSetInformation (
  IN  EFI_ADAPTER_INFORMATION_PROTOCOL  *This,
  IN  EFI_GUID                          *InformationType,
  IN  VOID                              *InformationBlock,
  IN  UINTN                             InformationBlockSize
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Get a list of supported information types for this instance of the protocol.

  @param[in]  This                  A pointer to the EFI_ADAPTER_INFORMATION_PROTOCOL instance.
  @param[out] InfoTypesBuffer       A pointer to the array of InformationType GUIDs that are supported
                                    by This.
  @param[out] InfoTypesBufferCount  A pointer to the number of GUIDs present in InfoTypesBuffer.

  @retval EFI_SUCCESS               The list of information type GUIDs was returned.
  @retval EFI_INVALID_PARAMETER     A required parameter is NULL.
  @retval EFI_OUT_OF_RESOURCES      There is not enough pool memory to store the results.

**/
EFI_STATUS
EFIAPI
NetvscAipGetSupportedTypes (
  IN  EFI_ADAPTER_INFORMATION_PROTOCOL  *This,
  OUT EFI_GUID                          **InfoTypesBuffer,
  OUT UINTN                             *InfoTypesBufferCount
  )
{
  EFI_GUID  *GuidBuffer;

  if ((This == NULL) || (InfoTypesBuffer == NULL) || (InfoTypesBufferCount == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  GuidBuffer = AllocateCopyPool (sizeof (EFI_GUID), &gEfiAdapterInfoMediaStateGuid);
  if (GuidBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  *InfoTypesBuffer      = GuidBuffer;
  *InfoTypesBufferCount = 1;

  return EFI_SUCCESS;
}

// MS_HYP_CHANGE END
