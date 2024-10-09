/**

    \copyright Copyright (c) Microsoft Corporation

    \file BiosDeviceLib.h

    \brief Library functions to access BIOS VDev config registers

**/

#pragma once

VOID WriteBiosDevice(UINT32 AddressRegisterValue, UINT32 DataRegisterValue);

UINT32 ReadBiosDevice(UINT32 AddressRegisterValue);

