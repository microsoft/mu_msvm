/** @file
  Defines the firmware version record that is embedded as a RAW file in the
  firmware volume so the host/VMM can identify the firmware image mechanically
  (i.e. by parsing the image bytes) at load time, without executing it.

  The record is generated at build time by MsvmPkg/PlatformBuild.py and placed
  into the DXE firmware volume (see MsvmPkg/MsvmPkgX64.fdf /
  MsvmPkg/MsvmPkgAARCH64.fdf) as a leaf file named by
  gMsvmFirmwareVersionFileGuid.

  Host-side location strategy:
    - Scan the loaded image for the 4-byte Signature ('MVFW'), OR
    - Scan for the 16-byte file GUID (gMsvmFirmwareVersionFileGuid) that appears
      in the FFS file header, then read the record that follows.

  All multi-byte integer fields are little-endian. String fields are ASCII and
  NUL-terminated. The layout is append-only: new fields may be added at the end
  guarded by StructVersion / HeaderSize, but existing fields and the Signature
  must never change meaning so the host parser stays stable.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

//
// Signature 'MVFW' stored little-endian ('M','V','F','W').
//
#define MSVM_FIRMWARE_VERSION_SIGNATURE  SIGNATURE_32 ('M', 'V', 'F', 'W')

//
// Current StructVersion value. Bump when the layout is extended.
//
#define MSVM_FIRMWARE_VERSION_STRUCT_VERSION  1

//
// Firmware/VMM interface (compatibility) version. This is a human-curated
// semantic contract version that is deliberately independent of the release
// (BaseVersion) and of git state.
//
// The value is NOT defined here. It lives in MsvmPkg/FirmwareVersion.toml (the
// single source of truth) and is embedded into this record and into the
// PcdMsvmFirmwareInterfaceVersionMajor/Minor FixedAtBuild PCDs at build time.
// Bump it there, by hand, in code review, whenever the contract between this
// firmware and the host/VMM changes:
//
//   - Bump MAJOR for any breaking change: one where an existing VMM built
//     against an older major would malfunction. This includes the firmware
//     hard-requiring new VMM-provided behavior it cannot boot without. Reset
//     MINOR to 0 on a major bump.
//   - Bump MINOR for a backward-compatible addition: an optional capability the
//     firmware can degrade gracefully without.
//
// The VMM supports a range of majors and refuses anything outside it (firmware
// too new or too old to talk to). A higher-than-known minor is always safe to
// proceed on; the VMM may additionally require a minimum minor for features it
// depends on.
//

//
// Bit definitions for the Flags field.
//
// MSVM_FIRMWARE_VERSION_FLAG_DIRTY: the build was produced from a tree with
// uncommitted changes (modified tracked files or untracked files present), so
// GitCommit identifies the base commit but not the exact source that was built.
// When clear, GitCommit identifies the exact committed source state.
//
// MSVM_FIRMWARE_VERSION_FLAG_OFFICIAL: the build was produced by the official
// CI pipeline (e.g. building main), as opposed to a developer or pull-request
// build. Set from the OFFICIAL_BUILD build environment; clear otherwise.
//
#define MSVM_FIRMWARE_VERSION_FLAG_DIRTY     BIT0
#define MSVM_FIRMWARE_VERSION_FLAG_OFFICIAL  BIT1

//
// Maximum sizes (including the terminating NUL) for the string fields.
//
#define MSVM_FIRMWARE_BASE_VERSION_SIZE  16
#define MSVM_FIRMWARE_GIT_COMMIT_SIZE    48

#pragma pack(1)

typedef struct {
  //
  // MSVM_FIRMWARE_VERSION_SIGNATURE. Lets the host find/validate the record by
  // a flat byte scan regardless of FFS/section wrapping.
  //
  UINT32    Signature;

  //
  // MSVM_FIRMWARE_VERSION_STRUCT_VERSION. Incremented when fields are added.
  //
  UINT16    StructVersion;

  //
  // Size of this structure in bytes. Lets the host skip unknown trailing
  // fields from a newer firmware build.
  //
  UINT16    HeaderSize;

  //
  // Bitmask of MSVM_FIRMWARE_VERSION_FLAG_* values describing the build.
  //
  UINT32    Flags;

  //
  // MSVM_FIRMWARE_INTERFACE_VERSION_MAJOR / _MINOR: the human-curated
  // firmware/VMM compatibility contract version. Unlike BaseVersion this is a
  // machine-comparable gate: the VMM checks these to decide whether it can talk
  // to this firmware and should warn/bail early if not. The values live in
  // MsvmPkg/FirmwareVersion.toml; see the interface version comment above for
  // the bump rules.
  //
  UINT16    InterfaceVersionMajor;
  UINT16    InterfaceVersionMinor;

  //
  // Static "major.minor" release prefix (e.g. "26.0"). The release patch
  // number is assigned later by the GitHub release workflow and is therefore
  // not present here; resolve the full version by finding the release whose
  // tag targets GitCommit.
  //
  CHAR8     BaseVersion[MSVM_FIRMWARE_BASE_VERSION_SIZE];

  //
  // Full 40-character git commit hash. Set to "unknown" when git information is
  // unavailable at build time. Whether the tree was dirty at build time is
  // reported separately via MSVM_FIRMWARE_VERSION_FLAG_DIRTY in Flags.
  //
  CHAR8     GitCommit[MSVM_FIRMWARE_GIT_COMMIT_SIZE];
} MSVM_FIRMWARE_VERSION_INFO;

#pragma pack()
