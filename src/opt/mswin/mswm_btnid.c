#include "mswm_internal.h"

/* Report keyboard buttons.
 */
#if 0
int mswm_report_buttons_keyboard(
  struct ps_input_device *dev,
  void *userdata,
  int (*cb)(struct ps_input_device *dev,const struct ps_input_btncfg *btncfg,void *userdata)
) {
  /* We could do a static list here, but I think this is easier...
   * Consider the range 1..255, anything that reports a USB-HID usage is a real button.
   */
  int keysym; for (keysym=1;keysym<256;keysym++) {
    int usage=ps_mswm_usage_from_keysym(keysym);
    if (!usage) continue;
    struct ps_input_btncfg btncfg={
      .srcbtnid=keysym,
      .lo=0,
      .hi=2,
      .value=0,
      .default_usage=usage,
    };
    int err=cb(dev,&btncfg,userdata);
    if (err) return err;
  }
  return 0;
}
#endif

/* USB-HID usage from keysym.
 */
 
int mswm_usage_from_keysym(int keysym) {

  /* All VK codes are in 1..255 
   */
  if ((keysym<1)||(keysym>255)) return 0;

  /* ASCII digits and letters don't have VK symbols.
   * Digits and upper-case letters are themselves.
   * BEWARE: Lowercase ASCII are used for other purposes in the VK gamut.
   */
  if (keysym==0x30) return 0x00070027;
  if ((keysym>=0x31)&&(keysym<=0x39)) return 0x0007001e +keysym-0x31;
  if ((keysym>=0x41)&&(keysym<=0x5a)) return 0x00070004+keysym-0x41;

  /* Keypad 1..9 are sequential.
   */
  if ((keysym>=VK_NUMPAD1)&&(keysym<=VK_NUMPAD9)) return 0x00070059+keysym-VK_NUMPAD1;

  /* F1..F24 are sequential (what a concept!)
   * Unfortunately, USB-HID are not. They are split into two ranges.
   */
  if ((keysym>=VK_F1)&&(keysym<=VK_F12)) return 0x0007003a+keysym-VK_F1;
  if ((keysym>=VK_F13)&&(keysym<=VK_F24)) return 0x00070068+keysym-VK_F13;

  /* etc...
   */
  switch (keysym) {
    case VK_BACK: return 0x0007002a;
    case VK_TAB: return 0x0007002b;
    case VK_CLEAR: return 0x0007009c;
    case VK_RETURN: return 0x00070028;
    case VK_SHIFT: return 0x000700e1; // presume left
    case VK_CONTROL: return 0x000700e0; // presume left
    case VK_MENU: return 0x000700e2; // ALT, presume left
    case VK_PAUSE: return 0x0007000a; // PAUSE, using SysRq instead
    case VK_CAPITAL: return 0x00070039;
    case VK_ESCAPE: return 0x00070029;
    case VK_SPACE: return 0x0007002c;
    case VK_PRIOR: return 0x0007004b;
    case VK_NEXT: return 0x0007004e;
    case VK_END: return 0x0007004d;
    case VK_HOME: return 0x0007004a;
    case VK_LEFT: return 0x00070050;
    case VK_UP: return 0x00070052;
    case VK_RIGHT: return 0x0007004f;
    case VK_DOWN: return 0x00070051;
    case VK_SELECT: return 0x00070077;
    case VK_PRINT: return 0x00070046; // Can't find "Print" in USB-HID, calling it "Print Screen"
    case VK_EXECUTE: return 0x00070074;
    case VK_SNAPSHOT: return 0x00070046; // print screen
    case VK_INSERT: return 0x00070049;
    case VK_DELETE: return 0x0007004c;
    case VK_HELP: return 0x00070075;
    case VK_LWIN: return 0x000700e3;
    case VK_RWIN: return 0x000700e7;
    case VK_APPS: return 0x00070065;
    case VK_SLEEP: return 0x00070066; // "Power"
    case VK_NUMPAD0: return 0x00070062;
    case VK_MULTIPLY: return 0x00070055;
    case VK_ADD: return 0x00070057;
    case VK_SEPARATOR: return 0x00070085;
    case VK_SUBTRACT: return 0x00070056;
    case VK_DECIMAL: return 0x00070063;
    case VK_DIVIDE: return 0x00070054;
    case VK_NUMLOCK: return 0x00070053;
    case VK_SCROLL: return 0x00070047;
    case VK_LSHIFT: return 0x000700e1;
    case VK_RSHIFT: return 0x000700e5;
    case VK_LCONTROL: return 0x000700e0;
    case VK_RCONTROL: return 0x000700e4;
    case VK_LMENU: return 0x000700e2;
    case VK_RMENU: return 0x000700e6;
    /* I don't see much value in the rest...
    case VK_LBUTTON: return 0x000700
    case VK_RBUTTON: return 0x000700
    case VK_CANCEL: return 0x000700 // "control-break processing", whatever that is
    case VK_MBUTTON: return 0x000700
    case VK_XBUTTON1: return 0x000700
    case VK_XBUTTON2: return 0x000700
    case VK_KANA: return 0x000700
    case VK_HANGUL: return 0x000700
    case VK_JUNJA: return 0x000700
    case VK_FINAL: return 0x000700
    case VK_HANJA: return 0x000700
    case VK_KANJI: return 0x000700
    case VK_CONVERT: return 0x000700
    case VK_NONCONVERT: return 0x000700
    case VK_ACCEPT: return 0x000700
    case VK_MODECHANGE: return 0x000700
    case VK_BROWSER_BACK: return 0x000700
    case VK_BROWSER_FORWARD: return 0x000700
    case VK_BROWSER_REFRESH: return 0x000700
    case VK_BROWSER_STOP: return 0x000700
    case VK_BROWSER_SEARCH: return 0x000700
    case VK_BROWSER_FAVORITES: return 0x000700
    case VK_BROWSER_HOME: return 0x000700
    case VK_VOLUME_MUTE: return 0x000700
    case VK_VOLUME_DOWN: return 0x000700
    case VK_VOLUME_UP: return 0x000700
    case VK_MEDIA_NEXT_TRACK: return 0x000700
    case VK_MEDIA_PREV_TRACK: return 0x000700
    case VK_MEDIA_STOP: return 0x000700
    case VK_MEDIA_PLAY_PAUSE: return 0x000700
    case VK_LAUNCH_MAIL: return 0x000700
    case VK_LAUNCH_MEDIA_SELECT: return 0x000700
    case VK_LAUNCH_APP1: return 0x000700
    case VK_LAUNCH_APP2: return 0x000700
    case VK_OEM_1: return 0x000700
    case VK_OEM_PLUS: return 0x000700
    case VK_OEM_COMMA: return 0x000700
    case VK_OEM_MINUS: return 0x000700
    case VK_OEM_PERIOD: return 0x000700
    case VK_OEM_2: return 0x000700
    case VK_OEM_3: return 0x000700
    case VK_OEM_4: return 0x000700
    case VK_OEM_5: return 0x000700
    case VK_OEM_6: return 0x000700
    case VK_OEM_7: return 0x000700
    case VK_OEM_8: return 0x000700
    case VK_OEM_102: return 0x000700
    case VK_PROCESSKEY: return 0x000700
    case VK_ATTN: return 0x000700
    case VK_CRSEL: return 0x000700
    case VK_EXSEL: return 0x000700
    case VK_EREOF: return 0x000700
    case VK_PLAY: return 0x000700
    case VK_ZOOM: return 0x000700
    case VK_NONAME: return 0x000700
    case VK_PA1: return 0x000700
    case VK_OEM_CLEAR: return 0x000700
    */
  }

  return 0;
}
