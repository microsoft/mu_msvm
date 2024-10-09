/** @file
Header file to support the Hyper-V Setup Front Page

Copyright (c) 2015, Microsoft Corporation. All rights reserved.<BR>

**/

#ifndef _FRONT_PAGE_EVENT_DATA_STRUCT_H_
#define _FRONT_PAGE_EVENT_DATA_STRUCT_H_

//==========================================
// Data Structure GUID and Definitions
//==========================================

//
// Establish the structure version that this was written for.
#define FRONT_PAGE_STRUCTURE_VERSION      8

//
// Setup variables - stored in the varstore, configured via setup UI
#pragma pack(1)
typedef struct {
  UINT8   StructVersion;  // Indicates the version of the structure being used.

  //-------------------- Structure Version 1 >>>
  UINT8   TPMMode;          // TPM:             1 = Enabled,  0 = Disabled
  UINT8   SecureBootMode;   // SecureBoot:      1 = Enabled,  0 = Disabled
  UINT8   DockingPortMode;  // Docking Port:    1 = Enabled,  0 = Disabled
  UINT8   FCameraMode;      // Front Camera:    1 = Enabled,  0 = Disabled
  UINT8   RCameraMode;      // Rear Camera:     1 = Enabled,  0 = Disabled
  UINT8   IRCameraMode;     // IR Camera:       1 = Enabled,  0 = Disabled
  UINT8   ACameraMode;      // Any Cameras:     1 = Enabled,  0 = Disabled
  UINT8   OnBoardAudioMode; // On-board Audio:  1 = Enabled,  0 = Disabled
  UINT8   MicroSDMode;      // microSD:         1 = Enabled,  0 = Disabled
  UINT8   WiFiMode;         // Wi-Fi:           1 = Enabled,  0 = Disabled
  UINT8   BluetoothMode;    // Bluetooth:       1 = Enabled,  0 = Disabled
  //-------------------- <<< Structure Version 1

  //-------------------- Structure Version 2 >>>
  UINT8   LanMode;          // Wired Lan        1 = Enabled, 0 = Disabled

  //  see FrontPageDeviceDisablePlatformSupport.h for details  //nv value will be ignored //here only for transport into vfr
  UINT64  PlatformDeviceDisableSupportedMask;  // This allows platform to control UI elements for what device disable they support 

  //-------------------- <<< Structure Version 2

  //-------------------- Structure Version 3 >>>
  BOOLEAN   PostReadyToBoot;
  //-------------------- <<< Structure Version 3

  //-------------------- Structure Version 4 >>>
  UINT32    Usb2PortDisableMask;  //NOT USED See USER USB PORT Disable
  UINT32    Usb3PortDisableMask;  //NOT USED See USER USB PORT Disable
  //-------------------- <<< Structure Version 4

  //-------------------- Structure Version 5 >>>
  UINT8     BladePortMode;      //1 = Enabled, 0 = Disabled
  // ------------------- <<< Structure Version 5

  //-------------------- Structure Version 6 >>>
  UINT8     AccessoryRadioMode; //1 = Enabled, 0 = Disabled
  // ------------------- <<< Structure Version 6

  //-------------------- Structure Version 7 >>>
  UINT8     LteModemMode; //1 = Enabled, 0 = Disabled
  // ------------------- <<< Structure Version 7
  
  //-------------------- Structure Version 8 >>>
  UINT8     WFOVCameraMode; //1 = Enabled, 0 = Disabled
  // ------------------- <<< Structure Version 8
  
  //-------------------- Add any future fields below this line...
  //-------------------- IMPORTANT!! Make sure to update the structure version.
} FRONT_PAGE_CONFIGURATION;
#pragma pack()

// {7F98EFE9-50AA-4598-B7C1-CB72E1CC5224}
#define FRONT_PAGE_CONFIG_FORMSET_GUID \
  { \
    0x7f98efe9, 0x50aa, 0x4598, { 0xb7, 0xc1, 0xcb, 0x72, 0xe1, 0xcc, 0x52, 0x24 } \
  }

extern EFI_GUID gFrontPageNVVarGuid;


//==========================================
// Event GUIDs and Definitions
//==========================================

//
// Create definitions for the Hyper-V FrontPage variables.
#define SFP_NV_SETTINGS_VAR_NAME          L"FPConfigNVData"
#define SFP_NV_ATTRIBUTES                 (EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS)
#define SFP_SB_VIOLATION_SIGNAL_VAR_NAME  L"SecureBootAlert"

#define MSP_REBOOT_REASON_VAR_NAME        L"RebootReason"
#define MSP_REBOOT_REASON_LENGTH          8

// Reboot Reasons used for setting the front page icon
#define MSP_REBOOT_REASON_SETUP_KEY       "VOL+    " // Display VOL+ Icon
#define MSP_REBOOT_REASON_SETUP_BOOTFAIL  "BOOTFAIL" // Display Disk Icon
#define MSP_REBOOT_REASON_SETUP_SEC_FAIL  "BSecFail"
#define MSP_REBOOT_REASON_SETUP_OS        "OS      "
#define MSP_REBOOT_REASON_SETUP_NONE      "        " // Not a fail

#endif  // _FRONT_PAGE_EVENT_DATA_STRUCT_H_
