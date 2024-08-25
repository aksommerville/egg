#include "eggrt_internal.h"
#include "opt/serial/serial.h"

/* --version
 */
 
static void eggrt_print_version() {
  fprintf(stderr,"0.0.1\n");//TODO How to source this?
}

/* --help=drivers
 */
 
static void eggrt_help_drivers() {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

/* --help, default.
 */
 
static void eggrt_help_default() {
  if (eggrt_has_embedded_rom) {
    fprintf(stderr,"\nUsage: %s [OPTIONS]\n\n",eggrt.exename);
  } else {
    fprintf(stderr,"\nUsage: %s [OPTIONS] ROM\n\n",eggrt.exename);
  }
  fprintf(stderr,
    "OPTIONS:\n"
    "  --help[=TOPIC]                Print canned documentation, see below.\n"
    "  --lang=ISO639                 Set language. Overrides environment.\n"
    "  --video=DRIVERS               Comma-delimited list of video drivers in preference order.\n"
    "  --audio=DRIVERS               '' audio.\n"
    "  --input=DRIVERS               '' input.\n"
    "  --fullscreen=0|1              Start in fullscreen.\n"
    "  --window=WxH                  Initial window size.\n"
    "  --video-device=STRING         Usage depends on driver.\n"
    "  --audio-rate=HZ               Preferred audio output rate.\n"
    "  --audio-chanc=1..8            Preferred audio channel count.\n"
    "  --mono                        --audio-chanc=1\n"
    "  --stereo                      --audio-chanc=2\n"
    "  --audio-buffer=FRAMES         Recommend audio buffer size.\n"
    "  --audio-device=STRING         Usage depends on driver.\n"
    "  --configure-input             Enter a special interactive mode to set up a gamepad.\n"
    "\n"
  );
  fprintf(stderr,
    "Help topics:\n"
    "  --help                        What you're reading right now.\n"
    "  --help=drivers                List available I/O drivers.\n"
    "\n"
  );
}

/* --help, dispatch.
 */
 
static void eggrt_help(const char *topic,int topicc) {
  if (!topic) topicc=0; else if (topicc<0) { topicc=0; while (topic[topicc]) topicc++; }
  if (!topicc||((topicc==1)&&(topic[0]=='1'))) eggrt_help_default();
  else if ((topicc==7)&&!memcmp(topic,"drivers",7)) eggrt_help_drivers();
  //TODO Help topics.
  else {
    fprintf(stderr,"%s: Unknown help topic '%.*s'.\n",eggrt.exename,topicc,topic);
    eggrt_help_default();
  }
}

/* Set string argument.
 */
 
static int eggrt_config_set_string(char **dstpp,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (*dstpp) free(*dstpp);
  *dstpp=nv;
  return 0;
}

/* Key=value argument from command line or config file.
 */
 
static int eggrt_configure_kv(const char *k,int kc,const char *v,int vc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  // If (v) is null (not just empty), substitute "1" or "0".
  if (!v) {
    if ((kc>=3)&&!memcmp(k,"no-",3)) v="0";
    else v="1";
    vc=1;
  }
  int vn,vnok=0;
  if (sr_int_eval(&vn,v,vc)>=1) vnok=1;
  
  // --help
  if ((kc==4)&&!memcmp(k,"help",4)) {
    eggrt_help(v,vc);
    eggrt.terminate=1;
    return 0;
  }
  
  // --version
  if ((kc==7)&&!memcmp(k,"version",7)) {
    eggrt_print_version();
    eggrt.terminate=1;
    return 0;
  }
  
  // --lang
  if ((kc==4)&&!memcmp(k,"lang",4)) {
    if ((vc!=2)||(v[0]<'a')||(v[0]>'z')||(v[1]<'a')||(v[1]>'z')) {
      fprintf(stderr,"%s: Expected 2 lowercase letters for 'lang', found '%.*s'\n",eggrt.exename,vc,v);
      return -2;
    }
    eggrt.lang=((v[0]-'a')<<5)|(v[1]-'a');
    return 0;
  }
  
  // --window
  if ((kc==6)&&!memcmp(k,"window",6)) {
    int i=kc; while (i-->0) {
      if (k[i]=='x') {
        int w,h;
        if (
          (sr_int_eval(&w,k,i)>=2)&&
          (sr_int_eval(&h,k+i+1,kc-i-1)>=2)&&
          (w>=1)&&(w<=4096)&&(h>=1)&&(h<=4096)
        ) {
          eggrt.window_w=w;
          eggrt.window_h=h;
          return 0;
        }
      }
    }
    fprintf(stderr,"%s: Expected 'WxH' in 1..4096 for '%.*s', found '%.*s'\n",eggrt.exename,kc,k,vc,v);
    return -2;
  }
  
  // --mono,--stereo
  if ((kc==4)&&!memcmp(k,"mono",4)) { eggrt.audio_chanc=1; return 0; }
  if ((kc==6)&&!memcmp(k,"stereo",6)) { eggrt.audio_chanc=2; return 0; }
  
  #define STROPT(opt,fld) if ((kc==sizeof(opt)-1)&&!memcmp(k,opt,kc)) return eggrt_config_set_string(&eggrt.fld,v,vc);
  #define INTOPT(opt,fld,lo,hi) if ((kc==sizeof(opt)-1)&&!memcmp(k,opt,kc)) { \
    if (!vnok||(vn<lo)||(vn>hi)) { \
      fprintf(stderr,"%s: Expected integer in %d..%d for '%.*s', found '%.*s'\n",eggrt.exename,lo,hi,kc,k,vc,v); \
      return -2; \
    } \
    eggrt.fld=vn; \
    return 0; \
  }
  
  STROPT("video",video_drivers)
  STROPT("audio",audio_drivers)
  STROPT("input",input_drivers)
  STROPT("video-device",video_device)
  STROPT("audio-device",audio_device)
  INTOPT("fullscreen",fullscreen,0,1)
  INTOPT("audio-rate",audio_rate,0,200000)
  INTOPT("audio-chanc",audio_chanc,0,8)
  INTOPT("audio-buffer",audio_buffer,0,100000)
  INTOPT("configure-input",configure_input,0,1)
  
  #undef STROPT
  #undef INTOPT
  
  fprintf(stderr,"%s: Unexpected option '%.*s' = '%.*s'\n",eggrt.exename,kc,k,vc,v);
  return -2;
}

/* Positional arguments.
 */
 
static int eggrt_configure_positional(const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  // ROM file, if we're built to expect one and don't have it yet.
  if (!eggrt_has_embedded_rom&&!eggrt.rompath) {
    return eggrt_config_set_string(&eggrt.rompath,src,srcc);
  }

  // And that's probably all we'll ever do positionally.
  return -1;
}

/* Read argv.
 */
 
static int eggrt_configure_argv(int argc,char **argv) {
  int argi=1,err;
  while (argi<argc) {
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    
    // No dash: Positional arguments.
    if (arg[0]!='-') {
      if ((err=eggrt_configure_positional(arg,-1))<0) {
        if (err!=-2) fprintf(stderr,"%s: Unexpected argument '%s'\n",eggrt.exename,arg);
        return -2;
      }
      continue;
    }
    
    // Single dash alone: Reserved.
    if (!arg[1]) {
      fprintf(stderr,"%s: Unexpected argument '%s'\n",eggrt.exename,arg);
      return -2;
    }
    
    // Single dash: '-kVV' or '-k VV'
    if (arg[1]!='-') {
      char k=arg[1];
      const char *v=0;
      if (arg[2]) v=arg+2;
      else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
      if ((err=eggrt_configure_kv(&k,1,v,-1))<0) {
        if (err!=-2) fprintf(stderr,"%s: Unexpected argument '%s'\n",eggrt.exename,arg);
        return -2;
      }
      continue;
    }
    
    // Double dash alone, or more than two dashes: Reserved.
    if (!arg[2]||(arg[2]=='-')) {
      fprintf(stderr,"%s: Unexpected argument '%s'\n",eggrt.exename,arg);
      return -2;
    }
    
    // Double dash: '--kk=vv' or '--kk vv'
    const char *k=arg+2;
    int kc=0;
    while (k[kc]&&(k[kc]!='=')) kc++;
    const char *v=0;
    if (k[kc]=='=') v=k+kc+1;
    else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
    if ((err=eggrt_configure_kv(k,kc,v,-1))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unexpected argument '%s'\n",eggrt.exename,arg);
      return -2;
    }
  }
  return 0;
}

/* Finalize configuration.
 */
 
static int eggrt_configure_final() {
  if (eggrt.terminate) return 0;
  
  if (eggrt.rompath) eggrt.rptname=eggrt.rompath;
  else eggrt.rptname=eggrt.exename;
  
  return 0;
}

/* Configure, main entry point.
 */
 
int eggrt_configure(int argc,char **argv) {
  int err;

  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) eggrt.exename=argv[0];
  else eggrt.exename="egg";
  
  //TODO Config files.
  //TODO Environment?
  if ((err=eggrt_configure_argv(argc,argv))<0) return err;
  
  return eggrt_configure_final();
}
