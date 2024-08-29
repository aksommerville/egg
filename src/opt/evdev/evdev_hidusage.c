#include "evdev_internal.h"
#include "evdev_internal.h"

/* KEY_* and BTN_*
 */
 
static int evdev_guess_hid_usage_KEY(int code) {
  switch (code) {
    case KEY_ESC: return 0x00070029;
    case KEY_1: return 0x0007001e;
    case KEY_2: return 0x0007001f;
    case KEY_3: return 0x00070020;
    case KEY_4: return 0x00070021;
    case KEY_5: return 0x00070022;
    case KEY_6: return 0x00070023;
    case KEY_7: return 0x00070024;
    case KEY_8: return 0x00070025;
    case KEY_9: return 0x00070026;
    case KEY_0: return 0x00070027;
    case KEY_MINUS: return 0x0007002d;
    case KEY_EQUAL: return 0x0007002e;
    case KEY_BACKSPACE: return 0x0007002a;
    case KEY_TAB: return 0x0007002b;
    case KEY_Q: return 0x00070014;
    case KEY_W: return 0x0007001a;
    case KEY_E: return 0x00070008;
    case KEY_R: return 0x00070015;
    case KEY_T: return 0x00070017;
    case KEY_Y: return 0x0007001c;
    case KEY_U: return 0x00070018;
    case KEY_I: return 0x0007000c;
    case KEY_O: return 0x00070012;
    case KEY_P: return 0x00070013;
    case KEY_LEFTBRACE: return 0x0007002f;
    case KEY_RIGHTBRACE: return 0x00070030;
    case KEY_ENTER: return 0x00070028;
    case KEY_LEFTCTRL: return 0x000700e0;
    case KEY_A: return 0x00070004;
    case KEY_S: return 0x00070016;
    case KEY_D: return 0x00070007;
    case KEY_F: return 0x00070009;
    case KEY_G: return 0x0007000a;
    case KEY_H: return 0x0007000b;
    case KEY_J: return 0x0007000d;
    case KEY_K: return 0x0007000e;
    case KEY_L: return 0x0007000f;
    case KEY_SEMICOLON: return 0x00070033;
    case KEY_APOSTROPHE: return 0x00070034;
    case KEY_GRAVE: return 0x00070035;
    case KEY_LEFTSHIFT: return 0x000700e1;
    case KEY_BACKSLASH: return 0x00070031;
    case KEY_Z: return 0x0007001d;
    case KEY_X: return 0x0007001b;
    case KEY_C: return 0x00070006;
    case KEY_V: return 0x00070019;
    case KEY_B: return 0x00070005;
    case KEY_N: return 0x00070011;
    case KEY_M: return 0x00070010;
    case KEY_COMMA: return 0x00070036;
    case KEY_DOT: return 0x00070037;
    case KEY_SLASH: return 0x00070038;
    case KEY_RIGHTSHIFT: return 0x000700e5;
    case KEY_KPASTERISK: return 0x00070055;
    case KEY_LEFTALT: return 0x000700e2;
    case KEY_SPACE: return 0x0007002c;
    case KEY_CAPSLOCK: return 0x00070039;
    case KEY_F1: return 0x0007003a;
    case KEY_F2: return 0x0007003b;
    case KEY_F3: return 0x0007003c;
    case KEY_F4: return 0x0007003d;
    case KEY_F5: return 0x0007003e;
    case KEY_F6: return 0x0007003f;
    case KEY_F7: return 0x00070040;
    case KEY_F8: return 0x00070041;
    case KEY_F9: return 0x00070042;
    case KEY_F10: return 0x00070043;
    case KEY_NUMLOCK: return 0x00070053;
    case KEY_SCROLLLOCK: return 0x00070047;
    case KEY_KP7: return 0x0007005f;
    case KEY_KP8: return 0x00070060;
    case KEY_KP9: return 0x00070061;
    case KEY_KPMINUS: return 0x00070056;
    case KEY_KP4: return 0x0007005c;
    case KEY_KP5: return 0x0007005d;
    case KEY_KP6: return 0x0007005e;
    case KEY_KPPLUS: return 0x00070057;
    case KEY_KP1: return 0x00070059;
    case KEY_KP2: return 0x0007005a;
    case KEY_KP3: return 0x0007005b;
    case KEY_KP0: return 0x00070062;
    case KEY_KPDOT: return 0x00070063;
    case KEY_F11: return 0x00070044;
    case KEY_F12: return 0x00070045;
    case KEY_KPENTER: return 0x00070058;
    case KEY_RIGHTCTRL: return 0x000700e4;
    case KEY_KPSLASH: return 0x00070054;
    case KEY_SYSRQ: return 0x0007009a;
    case KEY_RIGHTALT: return 0x000700e6;
    case KEY_HOME: return 0x0007004a;
    case KEY_UP: return 0x00070052;
    case KEY_PAGEUP: return 0x0007004b;
    case KEY_LEFT: return 0x00070050;
    case KEY_RIGHT: return 0x0007004f;
    case KEY_END: return 0x0007004d;
    case KEY_DOWN: return 0x00070051;
    case KEY_PAGEDOWN: return 0x0007004e;
    case KEY_INSERT: return 0x00070049;
    case KEY_DELETE: return 0x0007004c;
    case KEY_MUTE: return 0x0007007f;
    case KEY_VOLUMEDOWN: return 0x00070081;
    case KEY_VOLUMEUP: return 0x00070080;
    case KEY_POWER: return 0x00070066;
    case KEY_KPEQUAL: return 0x00070067;
    case KEY_LEFTMETA: return 0x000700e3;
    case KEY_RIGHTMETA: return 0x000700e7;
    case KEY_HELP: return 0x00070075;
    case KEY_MENU: return 0x00070076;
    case KEY_F13: return 0x00070068;
    case KEY_F14: return 0x00070069;
    case KEY_F15: return 0x0007006a;
    case KEY_F16: return 0x0007006b;
    case KEY_F17: return 0x0007006c;
    case KEY_F18: return 0x0007006d;
    case KEY_F19: return 0x0007006e;
    case KEY_F20: return 0x0007006f;
    case KEY_F21: return 0x00070070;
    case KEY_F22: return 0x00070071;
    case KEY_F23: return 0x00070072;
    case KEY_F24: return 0x00070073;
    case BTN_0: return 0x00090000;
    case BTN_1: return 0x00090001;
    case BTN_2: return 0x00090002;
    case BTN_3: return 0x00090003;
    case BTN_4: return 0x00090004;
    case BTN_5: return 0x00090005;
    case BTN_6: return 0x00090006;
    case BTN_7: return 0x00090007;
    case BTN_8: return 0x00090008;
    case BTN_9: return 0x00090009;
    case BTN_THUMB: return 0x00050037; // Gamepad Fire/Jump
    case BTN_THUMB2: return 0x00050037; // Gamepad Fire/Jump
    case BTN_TOP: return 0x00050039; // Gamepad Trigger
    case BTN_TOP2: return 0x00050039; // Gamepad Trigger
    case BTN_SOUTH: return 0x00050037; // Gamepad Fire/Jump
    case BTN_EAST: return 0x00050037; // Gamepad Fire/Jump
    case BTN_C: return 0x00050037; // Gamepad Fire/Jump
    case BTN_NORTH: return 0x00050037; // Gamepad Fire/Jump
    case BTN_WEST: return 0x00050037; // Gamepad Fire/Jump
    case BTN_Z: return 0x00050037; // Gamepad Fire/Jump
    case BTN_TL: return 0x00050039; // Gamepad Trigger
    case BTN_TR: return 0x00050039; // Gamepad Trigger
    case BTN_TL2: return 0x00050039; // Gamepad Trigger
    case BTN_TR2: return 0x00050039; // Gamepad Trigger
    case BTN_SELECT: return 0x0001003e; // Select
    case BTN_START: return 0x0001003d; // Start
    case BTN_MODE: return 0x00010085; // System Main Menu
  }
  return 0;
}

/* ABS_*
 */
 
static int evdev_guess_hid_usage_ABS(int code) {
  //if ((code>=ABS_MISC)&&(code<ABS_RESERVED)) {
    // MISC+n (0..5)
    // HID doesn't have a "generic absolute axis" like it does for buttons. Whatever.
    //return 0;
  //}
  switch (code) {
    case ABS_X: return 0x00010030; // X
    case ABS_Y: return 0x00010031; // Y
    case ABS_Z: return 0x00010032; // Z
    case ABS_RX: return 0x00010033; // Rx
    case ABS_RY: return 0x00010034; // Ry
    case ABS_RZ: return 0x00010035; // Rz
    case ABS_THROTTLE: return 0x000200bb; // Throttle
    case ABS_RUDDER: return 0x000200ba; // Rudder
    case ABS_WHEEL: return 0x000200c8; // Steering
    case ABS_GAS: return 0x000200c4; // Accelerator
    case ABS_BRAKE: return 0x000200c5; // Brake
    // HID does have "Hat switch" 0x00010039, but that's for 0..7 rotational hats.
    case ABS_HAT0X: return 0x00010030; // X
    case ABS_HAT0Y: return 0x00010031; // Y
    case ABS_HAT1X: return 0x00010030; // X
    case ABS_HAT1Y: return 0x00010031; // Y
    case ABS_HAT2X: return 0x00010030; // X
    case ABS_HAT2Y: return 0x00010031; // Y
    case ABS_HAT3X: return 0x00010030; // X
    case ABS_HAT3Y: return 0x00010031; // Y
  }
  return 0;
}

/* Guess HID usage.
 */
 
int evdev_guess_hidusage(int type,int code) {
  switch (type) {
    case EV_KEY: return evdev_guess_hid_usage_KEY(code);
    case EV_ABS: return evdev_guess_hid_usage_ABS(code);
  }
  return 0;
}
