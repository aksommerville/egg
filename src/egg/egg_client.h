/* egg_client.h
 * These functions must be implemented by the client; they are not part of Egg's Platform API.
 */
 
#ifndef EGG_CLIENT_H
#define EGG_CLIENT_H

/* Called during load.
 * Must return:
 *   <0: Error, abort hard.
 *    0: Decline to start up. Platform will abort quietly.
 *   >0: Normal startup.
 * If you return <=0, no other client function will be called.
 */
int egg_client_init();

/* Called during cleanup, if the client initialized properly.
 * There's usually not much need for this.
 * Maybe you need to finalize a saved game, or send a Farewell packet over the network or something.
 * All platform services remain available during this call, but beware they will not update after.
 */
void egg_client_quit();

/* Called repeatedly, if the client initialized properly.
 * (elapsed) is the time in seconds since the previous update, or zero the very first time.
 * Platform manages timing. (elapsed) is not necessarily real time; it's what the game should use.
 */
void egg_client_update(double elapsed);

/* Typically called after each update.
 * The GX context is in scope at this time, not necessarily at other times.
 */
void egg_client_render();

/* Called between updates, as needed, if the client initialized properly.
 * This is not used for ordinary input state: You should poll for that.
 * We never use callback functions. Any deferred operation, you get the result as an event.
 */
struct egg_event {
  int type;
};
void egg_client_event(const struct egg_event *event);

#define EGG_EVENT_TYPE_HTTP_RESPONSE 1
struct egg_event_http_response {
  struct egg_event hdr;
  const void *userdata;
  int status_code;
  const char *status_message;
  const char **headers;
  const void *body;
  int bodyc;
};

#define EGG_EVENT_TYPE_WEBSOCKET_ERROR 2
struct egg_event_websocket_error {
  struct egg_event hdr;
  int websocketid;
  const void *userdata;
  //TODO error detail?
};

#define EGG_EVENT_TYPE_WEBSOCKET_CLOSE 3
struct egg_event_websocket_close {
  struct egg_event hdr;
  int websocketid;
  const void *userdata;
};

#define EGG_EVENT_TYPE_WEBSOCKET_CONNECT 4
struct egg_event_websocket_connect {
  struct egg_event hdr;
  int websocketid;
  const void *userdata;
};

#define EGG_EVENT_TYPE_WEBSOCKET_MESSAGE 5
struct egg_event_websocket_message {
  struct egg_event hdr;
  int websocketid;
  const void *userdata;
  int opcode;
  const void *body;
  int bodyc;
};

#define EGG_EVENT_TYPE_KEY 6
struct egg_event_key {
  struct egg_event hdr;
  int keycode; // USB-HID page 7, or 0xffxxxxxx if HID code unknown.
  int value; // 0=off, 1=on, 2=repeat
};

#define EGG_EVENT_TYPE_TEXT 7
struct egg_event_text {
  struct egg_event hdr;
  int codepoint; // Unicode.
};

#define EGG_EVENT_TYPE_MMOTION 8
struct egg_event_mmotion {
  struct egg_event hdr;
  int x,y;
};

#define EGG_EVENT_TYPE_MBUTTON 9
struct egg_event_mbutton {
  struct egg_event hdr;
  int x,y;
  int btnid; // 1=left, 2=right, others undefined (TODO?)
  int value; // 0=off, 1=on
};

#define EGG_EVENT_TYPE_MWHEEL 10
struct egg_event_mwheel {
  struct egg_event hdr;
  int x,y;
  int dx,dy;
};

#define EGG_EVENT_TYPE_JCONNECT 11
struct egg_event_jconnect {
  struct egg_event hdr;
  int devid;
  const char *name;
  int namec;
  int vid,pid,version;
};

#define EGG_EVENT_TYPE_JDISCONNECT 12
struct egg_event_jdisconnect {
  struct egg_event hdr;
  int devid;
};

#define EGG_EVENT_TYPE_JBUTTON 13
struct egg_event_jbutton {
  struct egg_event hdr;
  int devid;
  int btnid;
  int value; // Normally 0 or 1, but devices may report any integer.
};

#define EGG_EVENT_TYPE_JANALOGUE 14
struct egg_event_janalogue {
  struct egg_event hdr;
  int devid;
  int btnid;
  float value; // -1..1 or 0..1. TODO: How does platform decide whether signed?
};

#endif
