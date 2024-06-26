#include "egg/egg.h"
//#include <math.h>

#define M_PI 3.1415926

int some_other_function();

static double total_elapsed=0.0;
static int updatec=0;
static int texid;
static int imageid=0;
static double imageClock=0.0;
#define imageDisplayTime 2.0
static int screenw,screenh;
static int texid_glyphsheet;
static int texid_appicon;
static double iconrotation=0.0;
static double iconxs=1.0,icondxs=1.0;
static double iconys=2.0,icondys=1.0;

void egg_client_quit() {
  egg_log("%s elapsed=%f updatec=%d",__func__,total_elapsed,updatec);
}

static int cb_res(int tid,int qual,int rid,int len,void *userdata) {
  egg_log("  %d:%d:%d, %d bytes %d",tid,qual,rid,len);
  return 0;
}

static int cb_decode_image(int tid,int qual,int rid,int len,void *userdata) {
  if (tid>EGG_RESTYPE_image) return 1;
  if (tid<EGG_RESTYPE_image) return 0;
  egg_log("%s %d:%d:%d len=%d... -------------------------------",__func__,tid,qual,rid,len);
  int w=0,h=0,stride=0,fmt=0;
  egg_image_get_header(&w,&h,&stride,&fmt,qual,rid);
  int pixelslen=stride*h;
  egg_log("w=%d h=%d stride=%d fmt=%d pixelslen=%d",w,h,stride,fmt,pixelslen);
  uint8_t v[65536];
  int c=egg_image_decode(v,sizeof(v),qual,rid);
  if (c!=pixelslen) {
    egg_log("!!!!!!!!!!! actual pixels len %d",c);
    return 0;
  }
  if (c>sizeof(v)) {
    egg_log("...decode looks ok but larger than our buffer");
    return 0;
  }
  int yi=h; if (yi>40) yi=40;
  const uint8_t *row=v;
  for (;yi-->0;row+=stride) {
    char tmp[80];
    int tmpc=0;
    int xi=stride; if (xi>40) xi=40;
    const uint8_t *p=row;
    for (;xi-->0;p++) {
      tmp[tmpc++]=" 123456789abcdef"[(*p)>>4];
      tmp[tmpc++]=" 123456789abcdef"[(*p)&15];
    }
    egg_log(">> %.*s",tmpc,tmp);
  }
  return 0;
}

int egg_client_init() {
  egg_log("%s",__func__);
  
  if (0) { // Dump all resources.
    egg_log("All resources:");
    int err=egg_res_for_each(cb_res,0);
    egg_log("egg_res_for_each => %d",err);
  }
  if (0) { // Dump content of a known custom resource.
    egg_log("Dumping privatetype:0:2, should be 'London':");
    char serial[32];
    int serialc=egg_res_get(serial,sizeof(serial),16,0,2);
    if ((serialc<1)||(serialc>sizeof(serial))) {
      egg_log("!!! Unexpected length %d",serialc);
    } else {
      egg_log("'%.*s'",serialc,serial);
    }
  }
  if (0) { // Read content of an image resource.
    int w=0,h=0,stride=0,fmt=0;
    egg_image_get_header(&w,&h,&stride,&fmt,0,2);
    if ((w<1)||(h<1)||(fmt!=EGG_TEX_FMT_A1)) {
      egg_log("image:0:2: Decode header failed or bad data: %d,%d,%d,%d",w,h,stride,fmt);
    } else {
      egg_log("image:0:2: w=%d h=%d stride=%d fmt=%d",w,h,stride,fmt);
      uint8_t v[1024]; // this image should be 594 (11*54)
      char rowbuf[128]; // 84 for this image
      int c=egg_image_decode(v,sizeof(v),0,2);
      if ((c!=h*stride)||(c>sizeof(v))||(w>sizeof(rowbuf))) {
        egg_log("image:0:2 decode failed. expected %d*%d==%d, got %d",h,stride,h*stride,c);
      } else {
        const uint8_t *row=v;
        int yi=h;
        for (;yi-->0;row+=stride) {
          const uint8_t *src=row;
          uint8_t srcmask=0x80;
          char *dst=rowbuf;
          int xi=w;
          for (;xi-->0;dst++) {
            if ((*src)&srcmask) *dst='X'; else *dst='.';
            if (srcmask==1) { srcmask=0x80; src++; }
            else srcmask>>=1;
          }
          egg_log(">> %.*s",w,rowbuf);
        }
      }
    }
  }
  if (0) { // Decode all images.
    egg_res_for_each(cb_decode_image,0);
  }
  if (0) { // System odds and ends.
    egg_log("real time: %f",egg_time_real());
    int v[7]={0};
    egg_time_local(v,7);
    egg_log("local time: %04d-%02d-%02dT%02d:%02d:%02d.%03d",v[0],v[1],v[2],v[3],v[4],v[5],v[6]);
    int langc=egg_get_user_languages(v,7);
    egg_log("%d user languages:",langc);
    int i=0; for (;(i<langc)&&(i<7);i++) {
      char repr[2]={
        "012345abcdefghijklmnopqrstuvwxyz"[(v[i]>>5)&31],
        "012345abcdefghijklmnopqrstuvwxyz"[v[i]&31],
      };
      egg_log("  %.2s %d",repr,v[i]);
    }
  }
  if (0) { // Storage.
    egg_log("Dumping storage:");
    int p=0; for (;;p++) {
      char k[256];
      int kc=egg_store_key_by_index(k,sizeof(k),p);
      if (kc<1) break;
      if (kc>sizeof(k)) {
        egg_log("!!! Unexpectedly long key, %d",kc);
      } else {
        char v[1024];
        int vc=egg_store_get(v,sizeof(v),k,kc);
        if ((vc<1)||(vc>sizeof(v))) {
          egg_log("!!! Unexpected length for key '%.*s': %d",kc,k,vc);
        } else {
          egg_log("'%.*s' = '%.*s'",kc,k,vc,v);
        }
      }
    }
    int time[6]={0};
    egg_time_local(time,6);
    char rtime[19]={
      '0'+(time[0]/1000)%10,
      '0'+(time[0]/ 100)%10,
      '0'+(time[0]/  10)%10,
      '0'+(time[0]     )%10,
      '-',
      '0'+(time[1]/  10)%10,
      '0'+(time[1]     )%10,
      '-',
      '0'+(time[2]/  10)%10,
      '0'+(time[2]     )%10,
      'T',
      '0'+(time[3]/  10)%10,
      '0'+(time[3]     )%10,
      ':',
      '0'+(time[4]/  10)%10,
      '0'+(time[4]     )%10,
      ':',
      '0'+(time[5]/  10)%10,
      '0'+(time[5]     )%10,
    };
    if (egg_store_set("lastPlayTime",12,rtime,19)<0) egg_log("!!! Failed to save lastPlayTime");
    else egg_log("Saved lastPlayTime = '%.19s'",rtime);
  }
  if (1) { // Enable every event and tell us how it goes.
    #define ENEV(tag) egg_log("Enable event %s: %s",#tag,egg_event_enable(EGG_EVENT_##tag,1)?"OK":"disabled");
    ENEV(JOY)
    ENEV(KEY)
    ENEV(TEXT)
    ENEV(MMOTION)
    ENEV(MBUTTON)
    ENEV(MWHEEL)
    ENEV(TOUCH)
    ENEV(ACCEL)
    #undef ENEV
  }
  if (1) { // Play a song.
    egg_audio_play_song(0, 4, 0, 1);
  }
  if (0) { // Dump strings.
    int en=EGG_QUAL("en");
    int fr=EGG_QUAL("fr");
    int it=EGG_QUAL("it");
    int rid=1;
    for (;;rid++) {
      int ok=0;
      char text[64];
      int textc;
      egg_log("string:%d:",rid);
      if (((textc=egg_res_get(text,sizeof(text),EGG_RESTYPE_string,0,rid))>0)&&(textc<=sizeof(text))) {
        egg_log("  00: %.*s",textc,text);
        ok=1;
      } else if (textc>0) egg_log("  00 too long"); else egg_log("  00 not found");
      if (((textc=egg_res_get(text,sizeof(text),EGG_RESTYPE_string,en,rid))>0)&&(textc<=sizeof(text))) {
        egg_log("  en: %.*s",textc,text);
        ok=1;
      } else if (textc>0) egg_log("  en too long"); else egg_log("  en not found");
      if (((textc=egg_res_get(text,sizeof(text),EGG_RESTYPE_string,fr,rid))>0)&&(textc<=sizeof(text))) {
        egg_log("  fr: %.*s",textc,text);
        ok=1;
      } else if (textc>0) egg_log("  fr too long"); else egg_log("  fr not found");
      if (((textc=egg_res_get(text,sizeof(text),EGG_RESTYPE_string,it,rid))>0)&&(textc<=sizeof(text))) {
        egg_log("  it: %.*s",textc,text);
        ok=1;
      } else if (textc>0) egg_log("  it too long"); else egg_log("  it not found");
      if (!ok) break;
    }
  }
  
  egg_texture_get_header(&screenw,&screenh,0,1);
  if ((texid=egg_texture_new())<1) return -1;
  
  if (egg_texture_load_image(texid_glyphsheet=egg_texture_new(),0,1)<0) return -1;
  if (egg_texture_load_image(texid_appicon=egg_texture_new(),0,8)<0) return -1;
  
  return 0;
}

// Press space bar to list all joysticks. This is the only test of egg_joystick_devid_by_index.
static void dump_joysticks() {
  egg_log("Currently connected devices:");
  int p=0; for (;;p++) {
    int devid=egg_joystick_devid_by_index(p);
    if (devid<1) return;
    char name[256];
    int namec=egg_joystick_get_name(name,sizeof(name),devid);
    if ((namec<0)||(namec>sizeof(name))) namec=0;
    int vid=0,pid=0,version=0;
    egg_joystick_get_ids(&vid,&pid,&version,devid);
    egg_log("  %04x:%04x:%04x %.*s",vid,pid,version,namec,name);
  }
}

static int cb_joybtn(int btnid,int hidusage,int lo,int hi,int value,void *userdata) {
  egg_log("  %08x %08x %d..%d =%d",btnid,hidusage,lo,hi,value);
  return 0;
}

static void on_joy(const struct egg_event_joy *event) {
  if (!event->btnid) {
    if (event->value) { // Connected.
      char name[256];
      int namec=egg_joystick_get_name(name,sizeof(name),event->devid);
      if ((namec<0)||(namec>sizeof(name))) namec=0;
      int vid=0,pid=0,version=0;
      egg_joystick_get_ids(&vid,&pid,&version,event->devid);
      egg_log("Connected joystick %d: %04x:%04x:%04x %.*s",event->devid,vid,pid,version,namec,name);
      egg_joystick_for_each_button(event->devid,cb_joybtn,0);
    } else { // Disconnected.
      egg_log("Lost joystick %d",event->devid);
    }
  } else { // State change.
    switch (event->btnid) {
      case EGG_JOYBTN_LX:    egg_log("JOY %d.LX=%d",event->devid,event->value); break;
      case EGG_JOYBTN_LY:    egg_log("JOY %d.LY=%d",event->devid,event->value); break;
      case EGG_JOYBTN_RX:    egg_log("JOY %d.RX=%d",event->devid,event->value); break;
      case EGG_JOYBTN_RY:    egg_log("JOY %d.RY=%d",event->devid,event->value); break;
      case EGG_JOYBTN_LEFT:  egg_log("JOY %d.LEFT=%d",event->devid,event->value); break;
      case EGG_JOYBTN_RIGHT: egg_log("JOY %d.RIGHT=%d",event->devid,event->value); break;
      case EGG_JOYBTN_UP:    egg_log("JOY %d.UP=%d",event->devid,event->value); break;
      case EGG_JOYBTN_DOWN:  egg_log("JOY %d.DOWN=%d",event->devid,event->value); break;
      case EGG_JOYBTN_SOUTH: egg_log("JOY %d.SOUTH=%d",event->devid,event->value); break;
      case EGG_JOYBTN_WEST:  egg_log("JOY %d.WEST=%d",event->devid,event->value); break;
      case EGG_JOYBTN_EAST:  egg_log("JOY %d.EAST=%d",event->devid,event->value); break;
      case EGG_JOYBTN_NORTH: egg_log("JOY %d.NORTH=%d",event->devid,event->value); break;
      case EGG_JOYBTN_L1:    egg_log("JOY %d.L1=%d",event->devid,event->value); break;
      case EGG_JOYBTN_R1:    egg_log("JOY %d.R1=%d",event->devid,event->value); break;
      case EGG_JOYBTN_L2:    egg_log("JOY %d.L2=%d",event->devid,event->value); break;
      case EGG_JOYBTN_R2:    egg_log("JOY %d.R2=%d",event->devid,event->value); break;
      case EGG_JOYBTN_LP:    egg_log("JOY %d.LP=%d",event->devid,event->value); break;
      case EGG_JOYBTN_RP:    egg_log("JOY %d.RP=%d",event->devid,event->value); break;
      case EGG_JOYBTN_AUX1:  egg_log("JOY %d.AUX1=%d",event->devid,event->value); break;
      case EGG_JOYBTN_AUX2:  egg_log("JOY %d.AUX2=%d",event->devid,event->value); break;
      case EGG_JOYBTN_AUX3:  egg_log("JOY %d.AUX3=%d",event->devid,event->value); break;
      default: egg_log("JOY %d.0x%x=%d",event->devid,event->btnid,event->value);
    }
  }
}

static void on_key(const struct egg_event_key *event) {
  egg_log("KEY 0x%x=%d",event->keycode,event->value);
  if (event->value) switch (event->keycode) {
    case 0x00070029: egg_request_termination(); break; // Esc
    case 0x0007002c: dump_joysticks(); break; // Space
  }
}

static void on_text(const struct egg_event_text *event) {
  egg_log("TEXT U+%x",event->codepoint);
}

static void on_mmotion(const struct egg_event_mmotion *event) {
  egg_log("MMOTION %d,%d",event->x,event->y);
}

static void on_mbutton(const struct egg_event_mbutton *event) {
  egg_log("MBUTTON %d=%d @%d,%d",event->btnid,event->value,event->x,event->y);
  if (event->value&&(event->btnid==1)) {
    // I'd do this during egg_client_init(), but it's like Audio and Fullscreen -- a user gesture is required first.
    egg_lock_cursor(1);
  }
}

static void on_mwheel(const struct egg_event_mwheel *event) {
  egg_log("MWHEEL %d,%d @%d,%d",event->dx,event->dy,event->x,event->y);
}

static void on_touch(const struct egg_event_touch *event) {
  const char *statestr="!!!INVALID!!!";
  switch (event->state) {
    case 0: statestr="release"; break;
    case 1: statestr="press"; break;
    case 2: statestr="move"; break;
  }
  egg_log("TOUCH #%d %d(%s) @%d,%d",event->touchid,event->state,statestr,event->x,event->y);
}

static void on_accel(const struct egg_event_accel *event) {
  egg_log("ACCEL %+d,%+d,%+d",event->x,event->y,event->z);
}

void egg_client_update(double elapsed) {
  updatec++;
  total_elapsed+=elapsed;
  
  union egg_event eventv[16];
  int eventc;
  while ((eventc=egg_event_get(eventv,16))>0) {
    const union egg_event *event=eventv;
    int i=eventc;
    for (;i-->0;event++) {
      switch (event->type) {
        case EGG_EVENT_JOY: on_joy((void*)event); break;
        case EGG_EVENT_KEY: on_key((void*)event); break;
        case EGG_EVENT_TEXT: on_text((void*)event); break;
        case EGG_EVENT_MMOTION: on_mmotion((void*)event); break;
        case EGG_EVENT_MBUTTON: on_mbutton((void*)event); break;
        case EGG_EVENT_MWHEEL: on_mwheel((void*)event); break;
        case EGG_EVENT_TOUCH: on_touch((void*)event); break;
        case EGG_EVENT_ACCEL: on_accel((void*)event); break;
        default: { const int *e=(void*)event; egg_log("UNKNOWN EVENT [%d,%d,%d,%d,%d]",e[0],e[1],e[2],e[3],e[4]); } break;
      }
    }
    if (eventc<16) break;
  }
  
  if ((imageClock-=elapsed)<=0.0) {
    imageClock+=imageDisplayTime;
    imageid++;
    if (egg_texture_load_image(texid,0,imageid)<0) {
      egg_log("%s: Failed to load image:0:%d, terminating",__FILE__,imageid);
      egg_request_termination();
    }
  }
  
  if ((iconrotation+=elapsed*1.5)>M_PI) iconrotation-=M_PI*2.0;
  iconxs+=elapsed*icondxs;
  if ((iconxs<0.25)&&(icondxs<0.0)) icondxs=-icondxs;
  else if ((iconxs>3.0)&&(icondxs>0.0)) icondxs=-icondxs;
  iconys+=elapsed*icondys;
  if ((iconys<0.30)&&(icondys<0.0)) icondys=-icondys;
  else if ((iconys>2.5)&&(icondys>0.0)) icondys=-icondys;
}

void egg_client_render() {
  int w=0,h=0;
  egg_texture_get_header(&w,&h,0,texid);
  egg_draw_rect(1,0,0,screenw,screenh,0x4080c0ff);
  egg_draw_decal(1,texid,(screenw>>1)-(w>>1),(screenh>>1)-(h>>1),0,0,w,h,0);
  
  {
    static const struct egg_draw_tile vtxv[]={
      {100,200,'H',0},
      {108,200,'e',0},
      {116,200,'l',0},
      {124,200,'l',0},
      {132,200,'o',0},
      {148,200,'w',0},
      {156,200,'o',0},
      {164,200,'r',0},
      {172,200,'l',0},
      {180,200,'d',0},
      {188,200,'!',0},
    };
    egg_render_tint(0xffff00ff);
    egg_draw_tile(1,texid_glyphsheet,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
    egg_render_tint(0);
  }
  {
    static const struct egg_draw_line vtxv[]={
      { 10, 10,0xff,0x00,0x00,0xff},
      {100, 20,0x00,0xff,0x00,0xff},
      {100,100,0x00,0x00,0xff,0xff},
      { 10, 50,0xff,0xff,0xff,0xff},
    };
    egg_draw_line(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  
  // Draw a shape with egg_draw_trig that wouldn't be possible otherwise.
  // Say a trapezoid filled with gradient?
  {
    static const struct egg_draw_line vtxv[]={
      {200, 50,0xff,0xff,0xff,0xff},
      {250,100,0xff,0x00,0x00,0xff},
      {250, 50,0x00,0xff,0x00,0xff},
      {300,100,0x00,0x00,0xff,0xff},
      {350, 50,0x00,0x00,0x00,0xff},
    };
    egg_draw_trig(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  
  // Draw our app icon scaled up and spinning.
  egg_draw_decal_mode7(1,texid_appicon,400,80,0,0,16,16,(int)(iconrotation*65536.0),(int)(iconxs*65536.0),(int)(iconys*65536.0));
}
