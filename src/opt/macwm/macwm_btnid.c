#include "macwm.h"
#include <stdio.h>
#include <Carbon/Carbon.h>

/* Keycodes to USB-HID from MacOS.
 */

int macwm_hid_from_mac_keycode(int maccode) {
  switch (maccode) {
    case kVK_ANSI_A: return 0x00070004;
    case kVK_ANSI_S: return 0x00070016;
    case kVK_ANSI_D: return 0x00070007;
    case kVK_ANSI_F: return 0x00070009;
    case kVK_ANSI_H: return 0x0007000b;
    case kVK_ANSI_G: return 0x0007000a;
    case kVK_ANSI_Z: return 0x0007001d;
    case kVK_ANSI_X: return 0x0007001b;
    case kVK_ANSI_C: return 0x00070006;
    case kVK_ANSI_V: return 0x00070019;
    case kVK_ANSI_B: return 0x00070005;
    case kVK_ANSI_Q: return 0x00070014;
    case kVK_ANSI_W: return 0x0007001a;
    case kVK_ANSI_E: return 0x00070008;
    case kVK_ANSI_R: return 0x00070015;
    case kVK_ANSI_Y: return 0x0007001c;
    case kVK_ANSI_T: return 0x00070017;
    case kVK_ANSI_1: return 0x0007001e;
    case kVK_ANSI_2: return 0x0007001f;
    case kVK_ANSI_3: return 0x00070020;
    case kVK_ANSI_4: return 0x00070021;
    case kVK_ANSI_6: return 0x00070023;
    case kVK_ANSI_5: return 0x00070022;
    case kVK_ANSI_Equal: return 0x0007002e;
    case kVK_ANSI_9: return 0x00070026;
    case kVK_ANSI_7: return 0x00070024;
    case kVK_ANSI_Minus: return 0x0007002d;
    case kVK_ANSI_8: return 0x00070025;
    case kVK_ANSI_0: return 0x00070027;
    case kVK_ANSI_RightBracket: return 0x00070030;
    case kVK_ANSI_O: return 0x00070012;
    case kVK_ANSI_U: return 0x00070018;
    case kVK_ANSI_LeftBracket: return 0x0007002f;
    case kVK_ANSI_I: return 0x0007000c;
    case kVK_ANSI_P: return 0x00070013;
    case kVK_ANSI_L: return 0x0007000f;
    case kVK_ANSI_J: return 0x0007000d;
    case kVK_ANSI_Quote: return 0x00070034;
    case kVK_ANSI_K: return 0x0007000e;
    case kVK_ANSI_Semicolon: return 0x00070033;
    case kVK_ANSI_Backslash: return 0x00070031;
    case kVK_ANSI_Comma: return 0x00070036;
    case kVK_ANSI_Slash: return 0x00070038;
    case kVK_ANSI_N: return 0x00070011;
    case kVK_ANSI_M: return 0x00070010;
    case kVK_ANSI_Period: return 0x00070037;
    case kVK_ANSI_Grave: return 0x00070035;
    case kVK_ANSI_KeypadDecimal: return 0x00070063;
    case kVK_ANSI_KeypadMultiply: return 0x00070055;
    case kVK_ANSI_KeypadPlus: return 0x00070057;
    case kVK_ANSI_KeypadClear: return 0x00070053;
    case kVK_ANSI_KeypadDivide: return 0x00070054;
    case kVK_ANSI_KeypadEnter: return 0x00070058;
    case kVK_ANSI_KeypadMinus: return 0x00070056;
    case kVK_ANSI_KeypadEquals: return 0x00070067;
    case kVK_ANSI_Keypad0: return 0x00070062;
    case kVK_ANSI_Keypad1: return 0x00070059;
    case kVK_ANSI_Keypad2: return 0x0007005a;
    case kVK_ANSI_Keypad3: return 0x0007005b;
    case kVK_ANSI_Keypad4: return 0x0007005c;
    case kVK_ANSI_Keypad5: return 0x0007005d;
    case kVK_ANSI_Keypad6: return 0x0007005e;
    case kVK_ANSI_Keypad7: return 0x0007005f;
    case kVK_ANSI_Keypad8: return 0x00070060;
    case kVK_ANSI_Keypad9: return 0x00070061;
    case kVK_Return: return 0x00070028;
    case kVK_Tab: return 0x0007002b;
    case kVK_Space: return 0x0007002c;
    case kVK_Delete: return 0x0007002a;
    case kVK_Escape: return 0x00070029;
    case kVK_Command: return 0x000700e3;
    case kVK_Shift: return 0x000700e1;
    case kVK_CapsLock: return 0x00070039;
    case kVK_Option: return 0x000700e2;
    case kVK_Control: return 0x000700e0;
    case kVK_RightShift: return 0x000700e5;
    case kVK_RightOption: return 0x000700e6;
    case kVK_RightControl: return 0x000700e4;
    case kVK_Function: return 0x00070065; // "Application", kind of a stretch...
    case kVK_F17: return 0x0007006c;
    case kVK_VolumeUp: return 0x00070080;
    case kVK_VolumeDown: return 0x00070081;
    case kVK_Mute: return 0x0007007f;
    case kVK_F18: return 0x0007006d;
    case kVK_F19: return 0x0007006e;
    case kVK_F20: return 0x0007006f;
    case kVK_F5: return 0x0007003e;
    case kVK_F6: return 0x0007003f;
    case kVK_F7: return 0x00070040;
    case kVK_F3: return 0x0007003c;
    case kVK_F8: return 0x00070041;
    case kVK_F9: return 0x00070042;
    case kVK_F11: return 0x00070044;
    case kVK_F13: return 0x00070068;
    case kVK_F16: return 0x0007006b;
    case kVK_F14: return 0x00070069;
    case kVK_F10: return 0x00070043;
    case kVK_F12: return 0x00070045;
    case kVK_F15: return 0x0007006a;
    case kVK_Help: return 0x00070075;
    case kVK_Home: return 0x0007004a;
    case kVK_PageUp: return 0x0007004b;
    case kVK_ForwardDelete: return 0x0007004c;
    case kVK_F4: return 0x0007003d;
    case kVK_End: return 0x0007004d;
    case kVK_F2: return 0x0007003b;
    case kVK_PageDown: return 0x0007004e;
    case kVK_F1: return 0x0007003a;
    case kVK_LeftArrow: return 0x00070050;
    case kVK_RightArrow: return 0x0007004f;
    case kVK_DownArrow: return 0x00070051;
    case kVK_UpArrow: return 0x00070052;
  }
  return 0;
}
