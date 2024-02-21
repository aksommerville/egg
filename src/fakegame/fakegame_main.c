#include "egg/egg.h"
#include <GLES2/gl2.h>

int mygl_compile_shader(const char *name,const char *vsrc,const char *fsrc);

/* Game globals.
 */
 
static int glprogram=0;
static int ap_pos,ap_color;

/* Init.
 */

int egg_client_init() {
  egg_log("%s",__func__);
  
  {
    char src[256];
    int srcc;
    #define PRINTSTR(path) { \
      srcc=egg_res_copy(src,sizeof(src),path); \
      if ((srcc>=0)&&(srcc<=sizeof(src))) { \
        egg_log("%s: '%.*s'",path,srcc,src); \
      } else { \
        egg_log("%s:ERROR: srcc=%d",path,srcc); \
      } \
    }
    PRINTSTR("s/en/1")
    PRINTSTR("s/fr/1")
    #undef PRINTSTR
  }
  {
    uint8_t src[2048];
    int srcc=egg_res_copy(src,sizeof(src),"i/1");
    if ((srcc>0)&&(srcc<=sizeof(src))) {
      egg_log("got i/1 (%d): %02x %02x %02x %02x",srcc,src[0],src[1],src[2],src[3]);
    } else {
      egg_log("i/1 failed: %d",srcc);
    }
  }
  
  egg_input_use_key(EGG_INPUT_USAGE_ENABLE);
  egg_input_use_text(EGG_INPUT_USAGE_ENABLE);
  egg_input_use_mmotion(EGG_INPUT_USAGE_ENABLE);
  egg_input_use_mbutton(EGG_INPUT_USAGE_ENABLE);
  egg_input_use_jbutton(EGG_INPUT_USAGE_ENABLE);
  egg_input_use_jbutton_all(EGG_INPUT_USAGE_ENABLE);
  egg_input_use_janalogue(EGG_INPUT_USAGE_ENABLE);
  egg_input_use_accelerometer(EGG_INPUT_USAGE_ENABLE);
  
  if ((glprogram=mygl_compile_shader(
    "raw"
    ,
    "attribute vec2 apos;\n"
    "attribute vec4 acolor;\n"
    "varying vec4 vcolor;\n"
    "void main() {\n"
    "  gl_Position=vec4(apos,0.0,1.0);\n"
    "  vcolor=acolor;\n"
    "}\n"
    ,
    "varying vec4 vcolor;\n"
    "void main() {\n"
    "  gl_FragColor=vcolor;\n"
    "}\n"
  ))<=0) {
    egg_log("FAILED TO COMPILE SHADER");
    return -1;
  }
  ap_pos=glGetAttribLocation(glprogram,"apos");
  ap_color=glGetAttribLocation(glprogram,"acolor");
  
  egg_audio_play_song("t/1",0,1);
  
  return 1;
}

/* Clean up.
 */

void egg_client_quit() {
  egg_log("%s",__func__);
}

/* Update.
 */

void egg_client_update(double elapsed) {
  //egg_log("%s %f",__func__,elapsed);
}

/* Render.
 */

void egg_client_render() {

  glClearColor(0.5f,0.25f,0.0f,1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  
  glUseProgram(glprogram);
  struct vertex {
    GLfloat x,y;
    GLubyte r,g,b,a;
  } vtxv[]={
    { -0.5f,-0.5f, 0xff,0x00,0x00,0xff},
    {  0.5f,-0.5f, 0x00,0xff,0x00,0xff},
    {  0.0f, 0.5f, 0x00,0x00,0xff,0xff},
  };
  glEnableVertexAttribArray(ap_pos);
  glEnableVertexAttribArray(ap_color);
  glVertexAttribPointer(ap_pos,2,GL_FLOAT,0,sizeof(struct vertex),&vtxv[0].x);
  glVertexAttribPointer(ap_color,4,GL_UNSIGNED_BYTE,1,sizeof(struct vertex),&vtxv[0].r);
  glDrawArrays(GL_TRIANGLE_STRIP,0,sizeof(vtxv)/sizeof(vtxv[0]));
}

/* Trivial testing of persistence.
 */
 
static void test_save() {
  char msg[32];
  int msgc=egg_get_local_time_string(msg,sizeof(msg));
  if ((msgc<1)||(msgc>sizeof(msg))) {
    msg[0]='?';
    msg[1]=0;
    msgc=1;
  }
  egg_log("Saving myMessage: '%.*s'",msgc,msg);
  if (egg_store_set("myMessage",9,msg,msgc)<0) {
    egg_log("save failed!");
  } else {
    egg_log("save ok");
  }
}

static void test_load() {
  char msg[256];
  int msgc=egg_store_get(msg,sizeof(msg),"myMessage",9);
  if ((msgc>0)&&(msgc<sizeof(msg))) {
    egg_log("Loaded myMessage ok: '%.*s'",msgc,msg);
  } else {
    egg_log("!! Failed to load myMessage: %d",msgc);
  }
}

/* Receive event.
 */

void egg_client_event(const struct egg_event *event) {
  switch (event->type) {
    case EGG_EVENT_TYPE_HTTP_RESPONSE: {
        struct egg_event_http_response *EVENT=(void*)event;
        egg_log("HTTP_RESPONSE");
      } break;
    case EGG_EVENT_TYPE_WEBSOCKET_ERROR: {
        struct egg_event_websocket_error *EVENT=(void*)event;
        egg_log("WEBSOCKET_ERROR");
      } break;
    case EGG_EVENT_TYPE_WEBSOCKET_CLOSE: {
        struct egg_event_websocket_close *EVENT=(void*)event;
        egg_log("WEBSOCKET_CLOSE");
      } break;
    case EGG_EVENT_TYPE_WEBSOCKET_CONNECT: {
        struct egg_event_websocket_connect *EVENT=(void*)event;
        egg_log("WEBSOCKET_CONNECT");
      } break;
    case EGG_EVENT_TYPE_WEBSOCKET_MESSAGE: {
        struct egg_event_websocket_message *EVENT=(void*)event;
        egg_log("WEBSOCKET_MESSAGE");
      } break;
    case EGG_EVENT_TYPE_KEY: {
        struct egg_event_key *EVENT=(void*)event;
        egg_log("KEY 0x%08x=%d",EVENT->keycode,EVENT->value);
      } break;
    case EGG_EVENT_TYPE_TEXT: {
        struct egg_event_text *EVENT=(void*)event;
        egg_log("TEXT U+%x",EVENT->codepoint);
        switch (EVENT->codepoint) {
          case 's': test_save(); break;
          case 'l': test_load(); break;
        }
      } break;
    case EGG_EVENT_TYPE_MMOTION: {
        struct egg_event_mmotion *EVENT=(void*)event;
        egg_log("MMOTION %d,%d",EVENT->x,EVENT->y);
      } break;
    case EGG_EVENT_TYPE_MBUTTON: {
        struct egg_event_mbutton *EVENT=(void*)event;
        egg_log("MBUTTON %d=%d @%d,%d",EVENT->btnid,EVENT->value,EVENT->x,EVENT->y);
      } break;
    case EGG_EVENT_TYPE_MWHEEL: {
        struct egg_event_mwheel *EVENT=(void*)event;
        egg_log("MWHEEL %+d,%+d @%d,%d",EVENT->dx,EVENT->dy,EVENT->x,EVENT->y);
      } break;
    case EGG_EVENT_TYPE_JCONNECT: {
        struct egg_event_jconnect *EVENT=(void*)event;
        egg_log("JCONNECT %d %04x:%04x:%04x '%s'",EVENT->devid,EVENT->vid,EVENT->pid,EVENT->version,EVENT->name);
      } break;
    case EGG_EVENT_TYPE_JDISCONNECT: {
        struct egg_event_jdisconnect *EVENT=(void*)event;
        egg_log("JDISCONNECT %d",EVENT->devid);
      } break;
    case EGG_EVENT_TYPE_JBUTTON: {
        struct egg_event_jbutton *EVENT=(void*)event;
        egg_log("JBUTTON %d.0x%08x=%d",EVENT->devid,EVENT->btnid,EVENT->value);
      } break;
    case EGG_EVENT_TYPE_JANALOGUE: {
        struct egg_event_janalogue *EVENT=(void*)event;
        egg_log("JANALOGUE %d.0x%08x=%.03f",EVENT->devid,EVENT->btnid,EVENT->value);
      } break;
    default: egg_log("%s unknown event type %d",__func__,event->type);
  }
}
