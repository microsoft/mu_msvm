/** @file
  Shared definitions for Hyper-V FrontPage VFR and C code.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

//
// THIS FILE MUST ONLY CONTAIN DEFINTIONS THAT CAN BE INTERPRETED
// BY BOTH THE VFR AND C COMPILERS.
//

#ifndef FRONT_PAGE_VFR_H
#define FRONT_PAGE_VFR_H


#define FRONT_PAGE_CLASS                               0x0000
#define FRONT_PAGE_SUBCLASS                            0x0002
///
/// The size of a 3 character ISO639 language code.
///
#define ISO_639_2_ENTRY_SIZE   3

//
// This is the VFR compiler generated header file which defines the
// string identifiers.
//
#define PRINTABLE_LANGUAGE_NAME_STRING_ID               0x0001

//
// Hyper-V Front Page exposes the following forms
//
// NOTE: Form ID order and values must align with VFR code.
//
#define FRONT_PAGE_FORM_ID_NONE                         0x0000
#define FRONT_PAGE_FORM_ID_SECURITY                     0x0001
#define FRONT_PAGE_FORM_ID_DEVICES                      0x0002
#define FRONT_PAGE_FORM_ID_PCINFO                       0x0003
#define FRONT_PAGE_FORM_ID_BOOTMENU                     0x0004
#define FRONT_PAGE_FORM_ID_ABOUT                        0x0005
#define FRONT_PAGE_FORM_ID_EXIT                         0x0006
#define FRONT_PAGE_FORM_ID_BOOT_SUMMARY                 0x0007

//
// Hyper-V Front Page triggers the following actions
//
// NOTE: Form ID order and values must align with VFR code.
//
#define FRONT_PAGE_ACTION_CONTINUE                        0x1000
#define FRONT_PAGE_ACTION_DEFAULTS                        0x1001
#define FRONT_PAGE_ACTION_EXIT_FRONTPAGE                  0x1014
#define FRONT_PAGE_ACTION_REBOOT_TO_FRONTPAGE             0x1015

#define LABEL_ABOUT_COMPLIANCE_LABLEL_TAG_START           0x2004
#define LABEL_ABOUT_COMPLIANCE_LABLEL_TAG_END             0x2005

//
// Grid class Start delimeter (GUID opcode).
//
#define SURFACE_GRID_START_OPCODE_GUID                                             \
  {                                                                                \
    0xc0b6e247, 0xe140, 0x4b4d, { 0xa6, 0x4, 0xc3, 0xae, 0x1f, 0xa6, 0xcc, 0x12 }  \
  }

//
// Grid class End delimeter (GUID opcode).
//
#define SURFACE_GRID_END_OPCODE_GUID                                               \
  {                                                                                \
    0x30879de9, 0x7e69, 0x4f1b, { 0xb5, 0xa5, 0xda, 0x15, 0xbf, 0x6, 0x25, 0xce }  \
  }

//
// Grid class select cell location (GUID opcode).
//
#define SURFACE_GRID_SELECT_CELL_OPCODE_GUID                                       \
  {                                                                                \
    0x3147b040, 0xeac3, 0x4b9f, { 0xb5, 0xec, 0xc2, 0xe2, 0x88, 0x45, 0x17, 0x4e } \
  }

//
// Bitmap class definition (GUID opcode).
//
#define SURFACE_BITMAP_OPCODE_GUID                                                 \
  {                                                                                \
    0xefbdb196, 0x91d7, 0x4e04, { 0xb7, 0xef, 0xa4, 0x4c, 0x5f, 0xba, 0x2e, 0xb5 } \
  }

//
// Compliance Label Bitmap File GUID - Expanded.
// NOTE: This is unfortunate but it has to be done until VFR recognizes GUID as a data type for guided opcodes.
//
typedef struct {
  UINT32  Data1;
  UINT16  Data2;
  UINT16  Data3;
  UINT64  Data4;
} VFR_EFI_GUID;

typedef struct{
    UINT64  PlatformDeviceDisableSupportedMask;  // This allows the platform to control UI elements for what device support they disable
    BOOLEAN PostReadyToBoot;
}FRONT_PAGE_HACK;

typedef struct{
    UINT8   TPMMode;          // TPM:             1 = Enabled,  0 = Disabled
    UINT8   SecureBootMode;   // SecureBoot:      1 = Enabled,  0 = Disabled
}FRONT_PAGE_SECURITY_CONFIGURATION;

typedef struct{
    UINT8   DockingPortMode;  // Docking Port:    1 = Enabled,  0 = Disabled
    UINT8   FCameraMode;      // Front Camera:    1 = Enabled,  0 = Disabled
    UINT8   RCameraMode;      // Rear Camera:     1 = Enabled,  0 = Disabled
    UINT8   IRCameraMode;     // IR Camera:       1 = Enabled,  0 = Disabled
    UINT8   ACameraMode;      // Any Cameras:     1 = Enabled,  0 = Disabled
    UINT8   OnBoardAudioMode; // On-board Audio:  1 = Enabled,  0 = Disabled
    UINT8   MicroSDMode;      // microSD:         1 = Enabled,  0 = Disabled
    UINT8   WiFiMode;         // Wi-Fi:           1 = Enabled,  0 = Disabled
    UINT8   BluetoothMode;    // Bluetooth:       1 = Enabled,  0 = Disabled
    UINT8   LanMode;          // Wired Lan        1 = Enabled,  0 = Disabled
    UINT8   BladePort;        // Blade Port       1 = Enabled,  0 = Disabled
    UINT8   AccessoryRadioMode;// Accessory Radio 1 = Enabled,  0 = Disabled
    UINT8   LteModemMode;     // LTE Modem        1 = Enabled,  0 = Disabled
    UINT8   WFOVCameraMode;   // WFOV Cameras:    1 = Enabled,  0 = Disabled
}FRONT_PAGE_DEVICE_CONFIGURATION;

typedef struct{
    UINT8   DisplayRestrictedStringSecurity; // Flag to Display String header "Some settings are controlled by your organization" in the security page.
    UINT8   DisplayRestrictedStringDevices;
    UINT8   TPMMode;          // TPM:             1 = Enabled,  0 = GrayedOut
    UINT8   SecureBootMode;   // SecureBoot:      1 = Enabled,  0 = GrayedOut
    UINT8   DockingPortMode;  // Docking Port:    1 = Enabled,  0 = GrayedOut
    UINT8   FCameraMode;      // Front Camera:    1 = Enabled,  0 = GrayedOut
    UINT8   RCameraMode;      // Rear Camera:     1 = Enabled,  0 = GrayedOut
    UINT8   IRCameraMode;     // IR Camera:       1 = Enabled,  0 = GrayedOut
    UINT8   ACameraMode;      // Any Cameras:     1 = Enabled,  0 = GrayedOut
    UINT8   OnBoardAudioMode; // On-board Audio:  1 = Enabled,  0 = GrayedOut
    UINT8   MicroSDMode;      // microSD:         1 = Enabled,  0 = GrayedOut
    UINT8   WiFiMode;         // Wi-Fi:           1 = Enabled,  0 = GrayedOut
    UINT8   BluetoothMode;    // Bluetooth:       1 = Enabled,  0 = GrayedOut
    UINT8   LanMode;          // Wired Lan        1 = Enabled,  0 = GrayedOut
    UINT8   BladePort;        // Blade Port       1 = Enabled,  0 = GrayedOut
    UINT8   SystemPassword;   // System Password  1 = Enabled,  0 = GrayedOut
    UINT8   AccessoryRadio;   // Accessory Radio  1 = Enabled,  0 = GrayedOut
    UINT8   LteModem;         // LTE Modem        1 = Enabled,  0 = GreyedOut
    UINT8   WFOVCameraMode;   // WFOV Cameras:    1 = Enabled,  0 = Disabled
}FRONT_PAGE_GRAYOUT_CONFIGURATION;

#define FRONT_PAGE_VARID            0x0071
#define FRONT_PAGE_HACK_VARID       0x0072
#define FRONT_PAGE_SECURITY_VARID   0x0074
#define FRONT_PAGE_GRAYOUT_VARID    0x0075
#define FRONT_PAGE_DEVICE_VARID     0x0076

#endif  // FRONT_PAGE_VFR_H
