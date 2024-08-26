#include "xegl_internal.h"
#include <X11/keysym.h>

/* Codepoint from keysym.
 */
 
int xegl_codepoint_from_keysym(int keysym) {
  if (keysym<1) return 0;
  
  /* keysymdef.h:538 */
  if ((keysym>=0x20)&&(keysym<=0x7e)) return keysym;
  
  /* keysymdef.h:642 */
  if ((keysym>=0xa1)&&(keysym<=0xff)) return keysym;
  
  /* If the high byte is 0x01, the other 3 are Unicode.
   * I can't seem to find that documentation, but I'm pretty sure that's true.
   */
  if ((keysym&0xff000000)==0x01000000) return keysym&0x00ffffff;
  
  /* keysymdef.h:359 XK_ISO_Left_Tab, ie Shift+Tab, ie just call it Tab please.
   * Andy: Just give me a tab.
   * GLX: You want a tab, kid, you got to order something.
   */
  if (keysym==0xfe20) return 0x09;
  
  /* keysymdef.h:124 */
  if (keysym==0xff0d) return 0x0a;
  if (keysym==0xffff) return 0x7f;
  if ((keysym>=0xff08)&&(keysym<=0xff1b)) return keysym-0xff00;
  
  /* keysymdef.h:202 */
  if ((keysym>=0xffb0)&&(keysym<=0xffb9)) return '0'+keysym-0xffb0;
  if ((keysym>=0xff80)&&(keysym<=0xffaf)) switch (keysym) {
    case XK_KP_Space: return 0x20;
    case XK_KP_Tab: return 0x09;
    case XK_KP_Enter: return 0x0a;
    case XK_KP_Delete: return 0x7f;
    case XK_KP_Equal: return '=';
    case XK_KP_Multiply: return '*';
    case XK_KP_Add: return '+';
    case XK_KP_Separator: return ',';
    case XK_KP_Subtract: return '-';
    case XK_KP_Decimal: return '.';
    case XK_KP_Divide: return '/';
  }
  
  return 0;
}

/* USB usage from keysym.
 */
 
int xegl_usb_usage_from_keysym(int keysym) {

  /* Letters are in order, keysyms match ASCII. Beware that GLX might report them upper or lower case. */
  if ((keysym>=0x41)&&(keysym<=0x5a)) return 0x00070004+keysym-0x41;
  if ((keysym>=0x61)&&(keysym<=0x7a)) return 0x00070004+keysym-0x61;
  
  /* 1..9 are ordered in USB; for GLX these are ASCII. Ditto for keypad. */
  if ((keysym>=0x31)&&(keysym<=0x39)) return 0x0007001e +keysym-0x31;
  if ((keysym>=0xffb1)&&(keysym<=0xffb9)) return 0x00070059+keysym-0xffb1;

  switch (keysym) {
  
    case 0x0030: return 0x00070027; // 0
    case 0xfe34: return 0x00070028; // ISO Enter (?)
    case 0xfd1e: return 0x00070028; // 3270 Enter (?)
    case 0xff0d: return 0x00070028; // Enter -- the real thing
    case 0xff1b: return 0x00070029; // Escape
    case 0xff08: return 0x0007002a; // Backspace
    case 0xff09: return 0x0007002b; // Tab
    case 0x0020: return 0x0007002c; // Space
    case 0x002d: return 0x0007002d; // Dash
    case 0x003d: return 0x0007002e; // Equal
    case 0x005b: return 0x0007002f; // Left Bracket
    case 0x005d: return 0x00070030; // Right Bracket
    case 0x005c: return 0x00070031; // Backslash
    case 0x003b: return 0x00070033; // Semicolon
    case 0x0027: return 0x00070034; // Apostrophe
    case 0x0060: return 0x00070035; // Grave
    case 0x002c: return 0x00070036; // Comma
    case 0x002e: return 0x00070037; // Dot
    case 0x002f: return 0x00070038; // Slash
    
    case 0xffaf: return 0x00070054; // KP Slash
    case 0xffaa: return 0x00070055; // KP Star
    case 0xffad: return 0x00070056; // KP Dash
    case 0xffab: return 0x00070057; // KP Plus
    case 0xff8d: return 0x00070058; // KP Enter
    case 0xffb0: return 0x00070062; // KP 0
    case 0xffae: return 0x00070063; // KP Dot
    case 0xffbd: return 0x00070067; // KP Equal

    // GLX "helpfully" turns the keypad into special-function keys...
    case 0xff9c: return 0x00070059; // KP 1
    case 0xff99: return 0x0007005a; // KP 2
    case 0xff9b: return 0x0007005b; // KP 3
    case 0xff96: return 0x0007005c; // KP 4
    case 0xff9d: return 0x0007005d; // KP 5
    case 0xff98: return 0x0007005e; // KP 6
    case 0xff95: return 0x0007005f; // KP 7
    case 0xff97: return 0x00070060; // KP 8
    case 0xff9a: return 0x00070061; // KP 9
    case 0xff9e: return 0x00070062; // KP 0
    case 0xff9f: return 0x00070063; // KP Dot (aka Delete)
  
    case 0xffbe: return 0x0007003a; // F1
    case 0xffbf: return 0x0007003b; // F2
    case 0xffc0: return 0x0007003c; // F3
    case 0xffc1: return 0x0007003d; // F4
    case 0xffc2: return 0x0007003e; // F5
    case 0xffc3: return 0x0007003f; // F6
    case 0xffc4: return 0x00070040; // F7
    case 0xffc5: return 0x00070041; // F8
    case 0xffc6: return 0x00070042; // F9
    case 0xffc7: return 0x00070043; // F10
    case 0xffc8: return 0x00070044; // F11
    case 0xffc9: return 0x00070045; // F12
    case 0xffca: return 0x00070068; // F13
    case 0xffcb: return 0x00070069; // F14
    case 0xffcc: return 0x0007006a; // F15
    case 0xffcd: return 0x0007006b; // F16
    case 0xffce: return 0x0007006c; // F17
    case 0xffcf: return 0x0007006d; // F18
    case 0xffd0: return 0x0007006e; // F19
    case 0xffd1: return 0x0007006f; // F20
    case 0xffd2: return 0x00070070; // F21
    case 0xffd3: return 0x00070071; // F22
    case 0xffd4: return 0x00070072; // F23
    case 0xffd5: return 0x00070073; // F24
    
    case 0xff61: return 0x00070046; // print screen
    case 0xff14: return 0x00070047; // scroll lock
    case 0xff13: return 0x00070048; // pause
    case 0xff63: return 0x00070049; // insert
    case 0xff50: return 0x0007004a; // home
    case 0xff55: return 0x0007004b; // page up
    case 0xff57: return 0x0007004d; // end
    case 0xff56: return 0x0007004e; // page down
    case 0xff53: return 0x0007004f; // right
    case 0xff51: return 0x00070050; // left
    case 0xff54: return 0x00070051; // down
    case 0xff52: return 0x00070052; // up
    case 0xff7f: return 0x00070053; // num lock
    case 0xffff: return 0x0007004c; // delete
    case 0xff67: return 0x00070076; // menu
  
    case 0xffe1: return 0x000700e1; // shift l
    case 0xffe2: return 0x000700e5; // shift r
    case 0xffe3: return 0x000700e0; // control l
    case 0xffe4: return 0x000700e4; // control r
    case 0xffe5: return 0x00070039; // caps lock
    case 0xffe7: return 0x000700e3; // l meta
    case 0xffe8: return 0x000700e7; // r meta
    case 0xffe9: return 0x000700e2; // l alt
    case 0xffea: return 0x000700e6; // r alt
    case 0xffeb: return 0x000700e3; // l super
    case 0xffec: return 0x000700e7; // r super
    
  }
  return 0;
}
