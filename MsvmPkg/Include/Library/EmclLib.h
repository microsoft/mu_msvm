/** @file
  Utility functions for EMCL.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
--*/

#pragma once


EFI_STATUS
EFIAPI
EmclInstallProtocol (
    IN  EFI_HANDLE ControllerHandle
    );

VOID
EFIAPI
EmclUninstallProtocol (
    IN  EFI_HANDLE ControllerHandle
    );

EFI_STATUS
EFIAPI
EmclSendPacketSync (
    IN  EFI_EMCL_PROTOCOL   *This,
    IN  VOID                *InlineBuffer,
    IN  UINT32              InlineBufferLength,
    IN  EFI_EXTERNAL_BUFFER *ExternalBuffers,
    IN  UINT32              ExternalBufferCount
    );

EFI_STATUS
EmclChannelTypeSupported (
    IN  EFI_HANDLE      ControllerHandle,
    IN  const EFI_GUID  *ChannelType,
    IN  EFI_HANDLE      AgentHandle
    );

EFI_STATUS
EmclChannelTypeAndInstanceSupported (
    IN  EFI_HANDLE ControllerHandle,
    IN  const EFI_GUID *ChannelType,
    IN  EFI_HANDLE AgentHandle,
    IN  const EFI_GUID *ChannelInstance OPTIONAL
    );