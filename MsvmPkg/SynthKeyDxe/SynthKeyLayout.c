/** @file
  VMBUS Keyboard Layout. Handles the translation of key press messages from the
  synthetic keyboard vdev to EFI_KEYs based on the UEFI keyboard layout.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "SynthKeyDxe.h"
#include <Protocol/SynthKeyProtocol.h>
#include "SynthKeyLayout.h"


//
// TODO: Use UEFI HII Keyboard layouts to do the translation instead of our own table.
//
// Layout for standard en-us keyboards.
//
SYNTHKEY_KEY_MAP_ENTRY
ScanCodeToEfiKey_En_Us[] =
{

    {   0x01,   SCAN_ESC,   0x0000, 0x0000  },     //   Escape
    {   0x02,   SCAN_NULL,  L'1',   L'!'    },
    {
    0x03,
    SCAN_NULL,
    L'2',
    L'@'
    },
    {
    0x04,
    SCAN_NULL,
    L'3',
    L'#'
    },
    {
    0x05,
    SCAN_NULL,
    L'4',
    L'$'
    },
    {
    0x06,
    SCAN_NULL,
    L'5',
    L'%'
    },
    {
    0x07,
    SCAN_NULL,
    L'6',
    L'^'
    },
    {
    0x08,
    SCAN_NULL,
    L'7',
    L'&'
    },
    {
    0x09,
    SCAN_NULL,
    L'8',
    L'*'
    },
    {
    0x0A,
    SCAN_NULL,
    L'9',
    L'('
    },
    {
    0x0B,
    SCAN_NULL,
    L'0',
    L')'
    },
    {
    0x0C,
    SCAN_NULL,
    L'-',
    L'_'
    },
    {
    0x0D,
    SCAN_NULL,
    L'=',
    L'+'
    },
    {
    0x0E, //  BackSpace
    SCAN_NULL,
    0x0008,
    0x0008
    },
    {
    0x0F, //  Tab
    SCAN_NULL,
    0x0009,
    0x0009
    },
    {
    0x10,
    SCAN_NULL,
    L'q',
    L'Q'
    },
    {
    0x11,
    SCAN_NULL,
    L'w',
    L'W'
    },
    {
    0x12,
    SCAN_NULL,
    L'e',
    L'E'
    },
    {
    0x13,
    SCAN_NULL,
    L'r',
    L'R'
    },
    {
    0x14,
    SCAN_NULL,
    L't',
    L'T'
    },
    {
    0x15,
    SCAN_NULL,
    L'y',
    L'Y'
    },
    {
    0x16,
    SCAN_NULL,
    L'u',
    L'U'
    },
    {
    0x17,
    SCAN_NULL,
    L'i',
    L'I'
    },
    {
    0x18,
    SCAN_NULL,
    L'o',
    L'O'
    },
    {
    0x19,
    SCAN_NULL,
    L'p',
    L'P'
    },
    {
    0x1a,
    SCAN_NULL,
    L'[',
    L'{'
    },
    {
    0x1b,
    SCAN_NULL,
    L']',
    L'}'
    },
    {
    0x1c, //   Enter
    SCAN_NULL,
    0x000d,
    0x000d
    },
    {
    0x1d,
    SCAN_NULL,
    0x0000,
    0x0000
    },
    {
    0x1e,
    SCAN_NULL,
    L'a',
    L'A'
    },
    {
    0x1f,
    SCAN_NULL,
    L's',
    L'S'
    },
    {
    0x20,
    SCAN_NULL,
    L'd',
    L'D'
    },
    {
    0x21,
    SCAN_NULL,
    L'f',
    L'F'
    },
    {
    0x22,
    SCAN_NULL,
    L'g',
    L'G'
    },
    {
    0x23,
    SCAN_NULL,
    L'h',
    L'H'
    },
    {
    0x24,
    SCAN_NULL,
    L'j',
    L'J'
    },
    {
    0x25,
    SCAN_NULL,
    L'k',
    L'K'
    },
    {
    0x26,
    SCAN_NULL,
    L'l',
    L'L'
    },
    {
    0x27,
    SCAN_NULL,
    L';',
    L':'
    },
    {
    0x28,
    SCAN_NULL,
    L'\'',
    L'"'
    },
    {
    0x29,
    SCAN_NULL,
    L'`',
    L'~'
    },
    {
    0x2a, //   Left Shift
    SCAN_NULL,
    0x0000,
    0x0000
    },
    {
    0x2b,
    SCAN_NULL,
    L'\\',
    L'|'
    },
    {
    0x2c,
    SCAN_NULL,
    L'z',
    L'Z'
    },
    {
    0x2d,
    SCAN_NULL,
    L'x',
    L'X'
    },
    {
    0x2e,
    SCAN_NULL,
    L'c',
    L'C'
    },
    {
    0x2f,
    SCAN_NULL,
    L'v',
    L'V'
    },
    {
    0x30,
    SCAN_NULL,
    L'b',
    L'B'
    },
    {
    0x31,
    SCAN_NULL,
    L'n',
    L'N'
    },
    {
    0x32,
    SCAN_NULL,
    L'm',
    L'M'
    },
    {
    0x33,
    SCAN_NULL,
    L',',
    L'<'
    },
    {
    0x34,
    SCAN_NULL,
    L'.',
    L'>'
    },
    {
    0x35,
    SCAN_NULL,
    L'/',
    L'?'
    },
    {
    0x36, //Right Shift
    SCAN_NULL,
    0x0000,
    0x0000
    },
    {
    0x37, // Numeric Keypad *
    SCAN_NULL,
    L'*',
    L'*'
    },
    {
    0x38,  //Left Alt/Extended Right Alt
    SCAN_NULL,
    0x0000,
    0x0000
    },
    {
    0x39,
    SCAN_NULL,
    L' ',
    L' '
    },
    {
    0x3A, //CapsLock
    SCAN_NULL,
    0x0000,
    0x0000
    },
    {
    0x3B,
    SCAN_F1,
    0x0000,
    0x0000
    },
    {
    0x3C,
    SCAN_F2,
    0x0000,
    0x0000
    },
    {
    0x3D,
    SCAN_F3,
    0x0000,
    0x0000
    },
    {
    0x3E,
    SCAN_F4,
    0x0000,
    0x0000
    },
    {
    0x3F,
    SCAN_F5,
    0x0000,
    0x0000
    },
    {
    0x40,
    SCAN_F6,
    0x0000,
    0x0000
    },
    {
    0x41,
    SCAN_F7,
    0x0000,
    0x0000
    },
    {
    0x42,
    SCAN_F8,
    0x0000,
    0x0000
    },
    {
    0x43,
    SCAN_F9,
    0x0000,
    0x0000
    },
    {
    0x44,
    SCAN_F10,
    0x0000,
    0x0000
    },
    {
    0x45, // NumLock
    SCAN_NULL,
    0x0000,
    0x0000
    },
    {
    0x46, //  ScrollLock
    SCAN_NULL,
    0x0000,
    0x0000
    },
    {
    0x47,
    SCAN_HOME,
    L'7',
    L'7'
    },
    {
    0x48,
    SCAN_UP,
    L'8',
    L'8'
    },
    {
    0x49,
    SCAN_PAGE_UP,
    L'9',
    L'9'
    },
    {
    0x4a,
    SCAN_NULL,
    L'-',
    L'-'
    },
    {
    0x4b,
    SCAN_LEFT,
    L'4',
    L'4'
    },
    {
    0x4c, //  Numeric Keypad 5
    SCAN_NULL,
    L'5',
    L'5'
    },
    {
    0x4d,
    SCAN_RIGHT,
    L'6',
    L'6'
    },
    {
    0x4e,
    SCAN_NULL,
    L'+',
    L'+'
    },
    {
    0x4f,
    SCAN_END,
    L'1',
    L'1'
    },
    {
    0x50,
    SCAN_DOWN,
    L'2',
    L'2'
    },
    {
    0x51,
    SCAN_PAGE_DOWN,
    L'3',
    L'3'
    },
    {
    0x52,
    SCAN_INSERT,
    L'0',
    L'0'
    },
    {
    0x53,
    SCAN_DELETE,
    L'.',
    L'.'
    },
    {
    0x57,
    SCAN_F11,
    0x0000,
    0x0000
    },
    {
    0x58,
    SCAN_F12,
    0x0000,
    0x0000
    },
    {
    0x5B,  //Left LOGO (Windows Key)
    SCAN_NULL,
    0x0000,
    0x0000
    },
    {
    0x5C,  //Right LOGO (Windows Key)
    SCAN_NULL,
    0x0000,
    0x0000
    },
    {
    0x5D,  //Menu key
    SCAN_NULL,
    0x0000,
    0x0000
    },
    {
    TABLE_END,
    TABLE_END,
    SCAN_NULL,
    SCAN_NULL
    }

};


SynthKeyStateChangeType
SynthKeyLayoutUpdateKeyState(
    IN          PHK_MESSAGE_KEYSTROKE       RawKey,
    IN          PSYNTH_KEYBOARD_STATE       KeyState
    )
/*++

Routine Description:

    Processes the keystroke and updates the KeyState flags if needed.

Arguments:

    RawKey      The raw, untranslated keystroke from the vdev.
    KeyState    EFI_KEY_STATE to update if applicable.

Return Value:

    Indicates what state if any was updated.
    KeyChangeNone   - No state was updated.
    KeyChangeShift  - Shift state was updated.
    KeyChangeToggle - Toggle state (caps lock, etc.) was updated.

--*/
{
    SynthKeyStateChangeType status = KeyChangeNone;
    UINT32                  newShiftState = 0;
    EFI_KEY_TOGGLE_STATE    newToggleState = 0;

    //
    // Shift state and toggle keys are not present for Unicode key messages
    //
    if (RawKey->IsUnicode)
    {
        return status;
    }

    //
    // We will only get make codes, plus a flag indicating break.
    // Break codes will never be directly seen.
    //
    // First figure out which flag is applicable based on the scancode
    // then set or clear it based on whether or not it was a make or break code
    //
    switch (RawKey->MakeCode)
    {
    case SCANCODE_CTRL_MAKE:
        if (RawKey->IsE0)
        {
            newShiftState = EFI_RIGHT_CONTROL_PRESSED;
        }
        else if (RawKey->IsE1)
        {

            //
            // Pause Key sequence starts with E1 1D,
            // which is shared with CTRL.
            //
            KeyState->PauseSequence = TRUE;
        }
        else
        {
            newShiftState = EFI_LEFT_CONTROL_PRESSED;
        }

        break;

    case SCANCODE_ALT_MAKE:
        if (RawKey->IsE0)
        {
            newShiftState = EFI_RIGHT_ALT_PRESSED;
        }
        else
        {
            newShiftState = EFI_LEFT_ALT_PRESSED;
        }
        break;

    case SCANCODE_LEFT_SHIFT_MAKE:

        //
        // PRNT_SCRN and Left Shift share a scan code,
        // except Left Shift does not have the E0 prefix.
        //
        if (!RawKey->IsE0)
        {
            newShiftState = EFI_LEFT_SHIFT_PRESSED;
        }
        break;

    case SCANCODE_RIGHT_SHIFT_MAKE:
        newShiftState = EFI_RIGHT_SHIFT_PRESSED;
        break;

    case SCANCODE_LEFT_LOGO_MAKE:
        newShiftState = EFI_LEFT_LOGO_PRESSED;
        break;

    case SCANCODE_RIGHT_LOGO_MAKE:
        newShiftState = EFI_RIGHT_LOGO_PRESSED;
        break;

    case SCANCODE_MENU_MAKE:
        newShiftState = EFI_MENU_KEY_PRESSED;
        break;

    case SCANCODE_SYS_REQ_MAKE:

        //
        // SysReq is shared with Keypad-*
        //
        if (RawKey->IsE0)
        {
            newShiftState = EFI_SYS_REQ_PRESSED;
        }
        break;

    case SCANCODE_SYS_REQ_MAKE_WITH_ALT:

        //
        // Treat SysReq and Alt-SysReq as the same
        //
        newShiftState = EFI_SYS_REQ_PRESSED;
        break;

    case SCANCODE_CAPS_LOCK_MAKE:
        newToggleState = EFI_CAPS_LOCK_ACTIVE;
        break;

    case SCANCODE_NUM_LOCK_MAKE:

        //
        // Num lock is shared with the pause sequence
        //
        if (!KeyState->PauseSequence)
        {
            newToggleState = EFI_NUM_LOCK_ACTIVE;
        }
        break;

    case SCANCODE_SCROLL_LOCK_MAKE:

        //
        // Scroll lock scan code is shared with CTRL-BREAK
        // (Ctrl-break uses the E0 prefix).
        //
        if (!RawKey->IsE0)
        {
            newToggleState = EFI_SCROLL_LOCK_ACTIVE;
        }
        break;

    }

    //
    // If there are any state changes, actually record them now
    // for shift states, look at the make/break flag and
    // set/clear the state accordingly.
    //
    if (newShiftState)
    {

        if (RawKey->IsBreak)
        {

            //
            // Make sure the shift state is set before processing a break key.
            // This serves to handle the RDP client which will send various key breaks
            // when the RDP control receives focus.  When the VM first starts, no shift
            // states are recorded.  If these unneeded break keys are not filtered out,
            // they will cause issues with the windows boot loader's
            // "Press a key to boot...." handling. Since the boot loader sets EFI_KEY_STATE_EXPOSED,
            // these key breaks will be queued and then ignored.
            // Because the boot loader only processes a small number of keystrokes
            // before giving up, a legitimate key press gets stuck behind these unneeded breaks
            // and the user misses the chance to boot from CD.
            //
            if (KeyState->KeyState.KeyShiftState & newShiftState)
            {
                KeyState->KeyState.KeyShiftState &= (~newShiftState);
                status = KeyChangeShift;
            }
        }
        else
        {
            KeyState->KeyState.KeyShiftState |= newShiftState;
            status = KeyChangeShift;
        }

    }

    //
    // For toggle states, just toggle it.
    // Note that break keys are ignored and the state
    // is only toggled on key make.
    //
    else if ((newToggleState) &&
             (!RawKey->IsBreak))
    {

        KeyState->KeyState.KeyToggleState ^= newToggleState;
        status = KeyChangeToggle;
    }

    return status;
}


EFI_STATUS
SynthKeyLayoutTranslateKey(
    IN          PHK_MESSAGE_KEYSTROKE       RawKey,
    IN          PSYNTH_KEYBOARD_STATE       KeyState,
    OUT         EFI_KEY_DATA               *TranslatedKey
    )
/*++

Routine Description:

    Translates the given keystroke message to an EFI_KEY_DATA structure based
    on the current keyboard layout.

Arguments:

    RawKey          The raw, untranslated keystroke from the vdev.
    KeyState        Keyboard state to use when translating the key.
    TranslatedKey   On output, will contain the translated UEFI key data and associated
                    key state.

Return Value:

    EFI_SUCCESS on success.
    EFI_NO_MAPPING if no translation can be performed.

--*/
{
    EFI_STATUS  status = EFI_SUCCESS;

    ASSERT(KeyState);
    ASSERT(TranslatedKey);

    ZeroMem(TranslatedKey, sizeof(*TranslatedKey));


    //
    // EFI is not concerned with break keys (i.e. there is no translation to be done.)
    // There are also no relevent keys with a E1 prefix
    //
    if ((RawKey->IsBreak) ||
        (RawKey->IsE1))
    {
        return EFI_NO_MAPPING;
    }

    //
    // Unicode keys need no further translation.
    //
    if (RawKey->IsUnicode)
    {
        TranslatedKey->Key.ScanCode    = SCAN_NULL;
        TranslatedKey->Key.UnicodeChar = RawKey->MakeCode;

        //
        // Leave key states zero for Unicode input
        // as the vdev doesn't provide this information
        //
        return status;
    }

    //
    // Special case handling.
    // These cases handle keys that share scan codes but have different prefixes
    // to indicate different keys.
    //

    //
    // Pause/Break is fun because it generates a series of scan codes,
    //      e1 1d 45 e1 9d c5.
    //
    // The vdev will combine E1 with the first scan code in the sequence
    // (via the IsE1 flag) so we'll see four messages total with IsBreak
    // set on the last two.
    //
    //      E1+1D, 45, E1+1D (break), 45 (break)
    //
    // we need to be careful to handle this otherwise 0x45 will be interpreted as Numlock.
    // We'll carry the E1 state which is done in SynthKeyLayoutUpdateKeyState
    //
    if ((KeyState->PauseSequence) && (RawKey->MakeCode == SCANCODE_NUM_LOCK_MAKE))
    {
        TranslatedKey->Key.UnicodeChar = CHAR_NULL;
        TranslatedKey->Key.ScanCode    = SCAN_PAUSE;
    }

    //
    // PAUSE shares the same scancode as Scroll Lock except PAUSE (CTRL pressed) has E0 prefix
    //
    else if ((RawKey->IsE0) && (RawKey->MakeCode == SCANCODE_SCROLL_LOCK_MAKE))
    {
        TranslatedKey->Key.UnicodeChar = CHAR_NULL;
        TranslatedKey->Key.ScanCode    = SCAN_PAUSE;
    }

    //
    // PRNT_SCRN shares the same scancode as that of Key Pad "*" except PRNT_SCRN has E0 prefix
    //
    else if ((RawKey->IsE0) && (RawKey->MakeCode == SCANCODE_SYS_REQ_MAKE))
    {
        TranslatedKey->Key.UnicodeChar = CHAR_NULL;
        TranslatedKey->Key.ScanCode    = SCAN_NULL;
    }
    else
    {
        int index;

        //
        // Conversion table can handle the rest.
        //
        for (index = 0; ScanCodeToEfiKey_En_Us[index].ScanCode != TABLE_END; index++)
        {
            if (RawKey->MakeCode == ScanCodeToEfiKey_En_Us[index].ScanCode)
            {
                TranslatedKey->Key.ScanCode    = ScanCodeToEfiKey_En_Us[index].EfiScanCode;
                TranslatedKey->Key.UnicodeChar = ScanCodeToEfiKey_En_Us[index].UnicodeChar;

                //
                // If a shift key is active and the translation table has a different shift state,
                // apply it now.
                //
                if ((EFI_KEY_SHIFT_ACTIVE(KeyState->KeyState.KeyShiftState)) &&
                    (ScanCodeToEfiKey_En_Us[index].UnicodeChar != ScanCodeToEfiKey_En_Us[index].ShiftUnicodeChar))
                {
                    TranslatedKey->Key.UnicodeChar = ScanCodeToEfiKey_En_Us[index].ShiftUnicodeChar;

                    //
                    // Clear the shift states for this key since the shift modification was just applied.
                    //
                    TranslatedKey->KeyState.KeyShiftState &= ~(EFI_LEFT_SHIFT_PRESSED | EFI_RIGHT_SHIFT_PRESSED);
                }

                //
                // Alphabetic key is affected by CapsLock State.
                //
                // TODO: try to combine with the shift check.
                // Shift toggles the Caps Lock state (shiftState = CapsState ^ ShiftState)
                //
                if (KeyState->KeyState.KeyToggleState & EFI_CAPS_LOCK_ACTIVE)
                {
                    if (TranslatedKey->Key.UnicodeChar >= L'a' && TranslatedKey->Key.UnicodeChar <= L'z')
                    {
                        TranslatedKey->Key.UnicodeChar = (UINT16) (TranslatedKey->Key.UnicodeChar - L'a' + L'A');
                    }
                    else if (TranslatedKey->Key.UnicodeChar >= L'A' && TranslatedKey->Key.UnicodeChar <= L'Z')
                    {
                        TranslatedKey->Key.UnicodeChar = (UINT16) (TranslatedKey->Key.UnicodeChar - L'A' + L'a');
                    }
                }

                break;

            }

        }

        //
        // Translate the CTRL-Alpha characters to their corresponding control value
        // (ctrl-A = 0x0001 through ctrl-Z = 0x001A)
        // TODO: this won't work with key layouts that don't translate to English characters, is that OK?
        //
        if (EFI_KEY_CTRL_ACTIVE(KeyState->KeyState.KeyShiftState))
        {
            if (TranslatedKey->Key.UnicodeChar >= L'a' && TranslatedKey->Key.UnicodeChar <= L'z')
            {
                TranslatedKey->Key.UnicodeChar = (CHAR16) (TranslatedKey->Key.UnicodeChar - L'a' + 1);
            }
            else if (TranslatedKey->Key.UnicodeChar >= L'A' && TranslatedKey->Key.UnicodeChar <= L'Z')
            {
                TranslatedKey->Key.UnicodeChar = (CHAR16) (TranslatedKey->Key.UnicodeChar - L'A' + 1);
            }
        }

    }

    //
    // Numeric Keypad handling.
    // These are either control codes or numeric characters depending on
    // the NUM lock and shift state.
    //
    // 0x47 => keypad '7' / Home        0x53 => keypad '.' / Del
    //
    if ((RawKey->MakeCode >= 0x47) && (RawKey->MakeCode <= 0x53))
    {

        //
        // If numlock is active, we want to use the number values.
        //
        // Special cases:
        //  E0 prefixed keys share the same scan codes but are not from the number pad.
        //  Shift overrides the numlock state.
        //
        // We'll signify this by clearing the scan code, leaving only the unicode value.
        //
        if ((KeyState->KeyState.KeyToggleState & EFI_NUM_LOCK_ACTIVE) &&
            !(EFI_KEY_SHIFT_ACTIVE(KeyState->KeyState.KeyShiftState)) &&
            !(RawKey->IsE0))
        {
            TranslatedKey->Key.ScanCode = SCAN_NULL;
        }

        //
        // Otherwise, we want to use the control key (up arrow, etc.)
        // We'll signify this by clearing the Unicode value leaving the scan code.
        //
        // Special Cases:
        //      These keys are exempt from num lock and shift modification
        //      0x4a => '-'      0x4e => '+'
        //
        else if (RawKey->MakeCode != 0x4a && RawKey->MakeCode != 0x4e)
        {
            TranslatedKey->Key.UnicodeChar = CHAR_NULL;
        }

    }

    return status;
}
