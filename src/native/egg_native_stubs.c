/* egg_native_stubs.c
 * Placeholders for the platform API, just to get things running.
 * This file should eventually become empty.
 */

#include "egg_native_internal.h"

int egg_get_languages(char *dst,int dsta) {
  if (dst&&(dsta>0)) dst[0]=0;
  return 0;
}

int egg_network_probe() {
  return 0;
}

int egg_http(
  const char *host,
  int port,
  const char *path,
  const char **headers,
  const void *body,int bodyc,
  const void *userdata
) {
  return -1;
}

int egg_websocket_connect(
  const char *host,
  int port,
  const char *path,
  const char *protocol,
  const void *userdata
) {
  return -1;
}

void egg_websocket_disconnect(int websocketid) {
}

int egg_websocket_send(int websocketid,int opcode,const void *body,int bodyc) {
  return -1;
}

int egg_audio_status() {
  return EGG_AUDIO_STATUS_IMPOSSIBLE;
}

void egg_audio_play_song(const char *path,int force,int repeat) {
  fprintf(stderr,"%s '%s' force=%d repeat=%d\n",__func__,path,force,repeat);
}

void egg_audio_play_sound(const char *path,double trim,double pan) {
  fprintf(stderr,"%s '%s' trim=%.03f pan=%+.03f\n",__func__,path,trim,pan);
}

void egg_audio_event(int opcode,int chid,int a,int b) {
  fprintf(stderr,"%s %02x %02x %02x %02x\n",__func__,opcode,chid,a,b);
}

int egg_audio_get_playhead() {
  return 0;
}
