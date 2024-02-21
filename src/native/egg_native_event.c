#include "egg_native_internal.h"

/* Public hooks for setting event mask.
 */

int egg_input_use_key(int usage) {
  switch (usage) {
    case EGG_INPUT_USAGE_QUERY: break;
    case EGG_INPUT_USAGE_DISABLE: egg.input_key=0; break;
    case EGG_INPUT_USAGE_ENABLE: egg.input_key=1; break;
    case EGG_INPUT_USAGE_REQUIRE: egg.input_key=1; break; //TODO Check (egg.hostio->video->type->provides_keyboard), emulate if needed
    default: return EGG_INPUT_USAGE_ERROR;
  }
  return egg.input_key;
}

int egg_input_use_text(int usage) {
  switch (usage) {
    case EGG_INPUT_USAGE_QUERY: break;
    case EGG_INPUT_USAGE_DISABLE: egg.input_text=0; break;
    case EGG_INPUT_USAGE_ENABLE: egg.input_text=1; break;
    case EGG_INPUT_USAGE_REQUIRE: egg.input_text=1; break; //TODO Check (egg.hostio->video->type->provides_keyboard), emulate if needed
    default: return EGG_INPUT_USAGE_ERROR;
  }
  return egg.input_text;
}

int egg_input_use_mmotion(int usage) {
  switch (usage) {
    case EGG_INPUT_USAGE_QUERY: break;
    case EGG_INPUT_USAGE_DISABLE: egg.input_mmotion=0; break;
    case EGG_INPUT_USAGE_ENABLE: egg.input_mmotion=1; break;
    case EGG_INPUT_USAGE_REQUIRE: egg.input_mmotion=1; break; //TODO Check (egg.hostio->video->type->provides_keyboard), emulate if needed
    default: return EGG_INPUT_USAGE_ERROR;
  }
  if (egg.hostio->video->type->show_cursor) {
    egg.hostio->video->type->show_cursor(egg.hostio->video,egg.input_mmotion||egg.input_mbutton);
  }
  return egg.input_mmotion;
}

int egg_input_use_mbutton(int usage) {
  switch (usage) {
    case EGG_INPUT_USAGE_QUERY: break;
    case EGG_INPUT_USAGE_DISABLE: egg.input_mbutton=0; break;
    case EGG_INPUT_USAGE_ENABLE: egg.input_mbutton=1; break;
    case EGG_INPUT_USAGE_REQUIRE: egg.input_mbutton=1; break; //TODO Check (egg.hostio->video->type->provides_keyboard), emulate if needed
    default: return EGG_INPUT_USAGE_ERROR;
  }
  if (egg.hostio->video->type->show_cursor) {
    egg.hostio->video->type->show_cursor(egg.hostio->video,egg.input_mmotion||egg.input_mbutton);
  }
  return egg.input_mbutton;
}

int egg_input_use_jbutton(int usage) {
  switch (usage) {
    case EGG_INPUT_USAGE_QUERY: break;
    case EGG_INPUT_USAGE_DISABLE: egg.input_jbutton=0; break;
    case EGG_INPUT_USAGE_ENABLE: egg.input_jbutton=1; break;
    case EGG_INPUT_USAGE_REQUIRE: egg.input_jbutton=1; break;
    default: return EGG_INPUT_USAGE_ERROR;
  }
  return egg.input_jbutton;
}

int egg_input_use_jbutton_all(int usage) {
  switch (usage) {
    case EGG_INPUT_USAGE_QUERY: break;
    case EGG_INPUT_USAGE_DISABLE: egg.input_jbutton_all=0; break;
    case EGG_INPUT_USAGE_ENABLE: egg.input_jbutton_all=1; break;
    case EGG_INPUT_USAGE_REQUIRE: egg.input_jbutton_all=1; break;
    default: return EGG_INPUT_USAGE_ERROR;
  }
  return egg.input_jbutton_all;
}

int egg_input_use_janalogue(int usage) {
  switch (usage) {
    case EGG_INPUT_USAGE_QUERY: break;
    case EGG_INPUT_USAGE_DISABLE: egg.input_janalogue=0; break;
    case EGG_INPUT_USAGE_ENABLE: egg.input_janalogue=1; break;
    case EGG_INPUT_USAGE_REQUIRE: egg.input_janalogue=1; break;
    default: return EGG_INPUT_USAGE_ERROR;
  }
  return egg.input_janalogue;
}

int egg_input_use_accelerometer(int usage) {
  switch (usage) {
    case EGG_INPUT_USAGE_QUERY: break;
    case EGG_INPUT_USAGE_DISABLE: egg.input_accelerometer=0; break;
    case EGG_INPUT_USAGE_ENABLE: egg.input_accelerometer=0; break;
    case EGG_INPUT_USAGE_REQUIRE: return EGG_INPUT_USAGE_ERROR;
    default: return EGG_INPUT_USAGE_ERROR;
  }
  return egg.input_accelerometer;
}

/* Misc window manager.
 */

void egg_native_cb_close(void *userdata) {
  egg.terminate=1;
}

void egg_native_cb_resize(void *userdata,int w,int h) {
  fprintf(stderr,"%s\n",__func__);
  //TODO notify GL adapter
}

void egg_native_cb_focus(void *userdata,int focus) {
  //TODO hard pause?
}

/* Keyboard and mouse.
 */

int egg_native_cb_key(void *userdata,int keycode,int value) {
  if (egg.input_key) {
    struct egg_event_key event={
      .hdr={.type=EGG_EVENT_TYPE_KEY},
      .keycode=keycode,
      .value=value,
    };
    egg_native_rom_event(&event);
  }
  if (egg.input_text) return 0;
  return 1;
}

void egg_native_cb_char(void *userdata,int codepoint) {
  if (egg.input_text) {
    struct egg_event_text event={
      .hdr={.type=EGG_EVENT_TYPE_TEXT},
      .codepoint=codepoint,
    };
    egg_native_rom_event(&event);
  }
}

void egg_native_cb_mmotion(void *userdata,int x,int y) {
  egg.mousex=x;
  egg.mousey=y;
  if (egg.input_mmotion) {
    struct egg_event_mmotion event={
      .hdr={.type=EGG_EVENT_TYPE_MMOTION},
      .x=x,
      .y=y,
    };
    egg_native_rom_event(&event);
  }
}

void egg_native_cb_mbutton(void *userdata,int btnid,int value) {
  if (egg.input_mbutton) {
    struct egg_event_mbutton event={
      .hdr={.type=EGG_EVENT_TYPE_MBUTTON},
      .x=egg.mousex,
      .y=egg.mousey,
      .btnid=btnid,
      .value=value,
    };
    egg_native_rom_event(&event);
  }
}

void egg_native_cb_mwheel(void *userdata,int dx,int dy) {
  if (egg.input_mbutton) {
    struct egg_event_mwheel event={
      .hdr={.type=EGG_EVENT_TYPE_MWHEEL},
      .x=egg.mousex,
      .y=egg.mousey,
      .dx=dx,
      .dy=dy,
    };
    egg_native_rom_event(&event);
  }
}

/* Audio.
 */

void egg_native_cb_pcm_out(int16_t *v,int c,void *userdata) {
  memset(v,0,c<<1);//TODO synthesizer
}

/* Joystick.
 */

void egg_native_cb_connect(void *userdata,int devid) {
  struct egg_event_jconnect event={
    .hdr={.type=EGG_EVENT_TYPE_JCONNECT},
    .devid=devid,
    .name="?",
    .vid=0,
    .pid=0,
    .version=0,
  };
  if (egg.hostio->input&&egg.hostio->input->type->get_ids) {
    if (!(event.name=egg.hostio->input->type->get_ids(&event.vid,&event.pid,&event.version,egg.hostio->input,devid))) {
      event.name="?";
    }
  }
  egg_native_rom_event(&event);
}

void egg_native_cb_disconnect(void *userdata,int devid) {
  struct egg_event_jdisconnect event={
    .hdr={.type=EGG_EVENT_TYPE_JDISCONNECT},
    .devid=devid,
  };
  egg_native_rom_event(&event);
}

void egg_native_cb_button(void *userdata,int devid,int btnid,int value) {
  if (egg.hostio->video->type->suppress_screensaver) {
    egg.hostio->video->type->suppress_screensaver(egg.hostio->video);
  }
  if (egg.input_jbutton) {//TODO distinguish jbutton, jbutton_all, janalogue, accelerometer
    struct egg_event_jbutton event={
      .hdr={.type=EGG_EVENT_TYPE_JBUTTON},
      .devid=devid,
      .btnid=btnid,
      .value=value,
    };
    egg_native_rom_event(&event);
  }
}
