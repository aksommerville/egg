#ifndef MSWM_INTERNAL_H
#define MSWM_INTERNAL_H

#include "opt/hostio/hostio_video.h"
#include <stdint.h>
#include <stdio.h>
#include <windows.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>

#define MSWM_WINDOW_CLASS_NAME "com_aksommerville_fullmoon"

#ifndef WM_MOUSEHWHEEL
  #define WM_MOUSEHWHEEL 0x020e
#endif

#ifndef WM_INPUT_DEVICE_CHANGE
  #define WM_INPUT_DEVICE_CHANGE 0x00fe
#endif

#define MSWM_AUTORELEASE_LIMIT 4

struct hostio_video_mswm {
  struct hostio_video hdr;
  int window_setup_complete;
  int translate_events;
  HINSTANCE instance;
  WNDCLASSEX wndclass;
  ATOM wndclass_atom;
  HWND hwnd;
  HDC hdc;
  HGLRC hglrc;
  int winw,winh;
  int fullscreen;
  int showcursor;
  WINDOWPLACEMENT fsrestore;
  HICON appicon;
  GLuint texid;
  
  /* A stupid bug: When I switch fullscreen on or off, any held keys get forgotten or something.
   * I don't have a Windows installation, running this in Wine, and it's likely just a Wine problem.
   * But anyway, I'll fix by recording all held keys and auto-releasing them on fullscreen changes.
   */
  int autorelease[MSWM_AUTORELEASE_LIMIT];

  // Output area.
  GLfloat dstl,dstr,dstt,dstb;
  int dstdirty;
};

#define DRIVER ((struct hostio_video_mswm*)driver)

extern struct hostio_video *mswm_global_driver;

LRESULT mswm_cb_msg(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam);
int mswm_update(struct hostio_video *driver);
int mswm_setup_window(struct hostio_video *driver);

int mswm_usage_from_keysym(int keysym);

void mswm_autorelease_add(struct hostio_video *driver,int usage);
void mswm_autorelease_remove(struct hostio_video *driver,int usage);
void mswm_autorelease(struct hostio_video *driver);

/* Two semi-private functions from mshid.
 */
void mshid_event(int wparam,int lparam);
void mshid_poll_connections_later();

#endif
