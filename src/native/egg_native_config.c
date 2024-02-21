#include "egg_native_internal.h"
#include "opt/serial/serial.h"

/* Cleanup.
 */
 
void egg_native_config_cleanup() {
  if (egg.rompath) free(egg.rompath);
  if (egg.video_driver) free(egg.video_driver);
  if (egg.video_device) free(egg.video_device);
  if (egg.audio_driver) free(egg.audio_driver);
  if (egg.input_driver) free(egg.input_driver);
  if (egg.audio_device) free(egg.audio_device);
}

/* Set string.
 */
 
static int egg_native_set_string(char **dst,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (*dst) free(*dst);
  *dst=nv;
  return 0;
}

/* Evaluate boolean.
 */
 
static int egg_bool_eval(const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc==4)&&!memcmp(src,"true",4)) return 1;
  if ((srcc==5)&&!memcmp(src,"false",5)) return 0;
  if ((srcc==2)&&!memcmp(src,"on",2)) return 1;
  if ((srcc==3)&&!memcmp(src,"off",3)) return 0;
  int n;
  if (sr_int_eval(&n,src,srcc)>=2) return n?1:0;
  return -1;
}

/* --help
 */
 
static void egg_native_print_help(const char *topic,int topicc) {
  if (egg_native_rom_expects_file()) {
    fprintf(stderr,"\nUsage: %s [OPTIONS] ROMFILE\n\n",egg.exename);
  } else {
    fprintf(stderr,"\nUsage: %s [OPTIONS]\n\n",egg.exename);
  }
  fprintf(stderr,
    "OPTIONS:\n"
    "  --help                    Print this message and exit.\n"
    "  --video-driver=LIST       Comma-delimited list of drivers, in order of preference.\n"
    "  --video-size=WIDTHxHEIGHT Initial window size.\n"
    "  --fullscreen=BOOL   Start fullscreen.\n"
    "  --video-device=NAME       Depends on driver.\n"
    "  --audio-driver=LIST       Comma-delimited list of drivers, in order of preference.\n"
    "  --audio-rate=HZ           Default 44100.\n"
    "  --audio-chanc=1|2         Default 1.\n"
    "  --audio-buffersize=INT    Depends on driver.\n"
    "  --audio-device=NAME       Depends on driver.\n"
    "  --input-driver=LIST       Comma-delimited list of drivers, in order of preference.\n"
    "\n"
  );
  {
    fprintf(stderr,"Video drivers:\n");
    int p=0; for (;;p++) {
      const struct hostio_video_type *type=hostio_video_type_by_index(p);
      if (!type) break;
      fprintf(stderr,"  %s: %s%s\n",type->name,type->appointment_only?"[*] ":"",type->desc);
    }
    fprintf(stderr,"\n");
  }
  {
    fprintf(stderr,"Audio drivers:\n");
    int p=0; for (;;p++) {
      const struct hostio_audio_type *type=hostio_audio_type_by_index(p);
      if (!type) break;
      fprintf(stderr,"  %s: %s%s\n",type->name,type->appointment_only?"[*] ":"",type->desc);
    }
    fprintf(stderr,"\n");
  }
  {
    fprintf(stderr,"Input drivers:\n");
    int p=0; for (;;p++) {
      const struct hostio_input_type *type=hostio_input_type_by_index(p);
      if (!type) break;
      fprintf(stderr,"  %s: %s%s\n",type->name,type->appointment_only?"[*] ":"",type->desc);
    }
    fprintf(stderr,"\n");
  }
}

/* Key=value option.
 */
 
static int egg_native_config_kv(const char *k,int kc,const char *v,int vc,const char *path,int lineno) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  // When (v) is null -- not just empty -- we substitute "0" or "1" as the value and strip "no-" from the key.
  if (!v) {
    if ((kc>3)&&!memcmp(k,"no-",3)) {
      k+=3;
      kc-=3;
      v="0";
      vc=1;
    } else {
      v="1";
      vc=1;
    }
  }
  
  #define STROPT(arg,fld) if ((kc==sizeof(arg)-1)&&!memcmp(k,arg,kc)) { \
    return egg_native_set_string(&egg.fld,v,vc); \
  }
  #define INTOPT(arg,fld,lo,hi) if ((kc==sizeof(arg)-1)&&!memcmp(k,arg,kc)) { \
    int n; \
    if ((sr_int_eval(&n,v,vc)<2)||(n<lo)||(n>hi)) { \
      fprintf(stderr,"%s:%d: Expected integer in %d..%d for '%.*s', found '%.*s'\n",path,lineno,lo,hi,kc,k,vc,v); \
      return -2; \
    } \
    egg.fld=n; \
    return 0; \
  }
  #define BOOLOPT(arg,fld) if ((kc==sizeof(arg)-1)&&!memcmp(k,arg,kc)) { \
    int n=egg_bool_eval(v,vc); \
    if (n<0) { \
      fprintf(stderr,"%s:%d: Expected boolean for '%.*s', found '%.*s'\n",path,lineno,kc,k,vc,v); \
      return -2; \
    } \
    egg.fld=n?1:0; \
    return 0; \
  }
  
  if ((kc==4)&&!memcmp(k,"help",4)) {
    egg_native_print_help(v,vc);
    egg.terminate=1;
    return 0;
  }
  
  STROPT("video-driver",video_driver)
  INTOPT("video-rate",video_rate,0,1000)
  BOOLOPT("fullscreen",video_fullscreen)
  STROPT("video-device",video_device)
  STROPT("audio-driver",audio_driver)
  INTOPT("audio-rate",audio_rate,200,200000)
  INTOPT("audio-chanc",audio_chanc,1,8)
  INTOPT("audio-buffersize",audio_buffersize,0,INT_MAX)
  STROPT("audio-device",audio_device)
  STROPT("input-driver",input_driver)
  
  #undef STROPT
  #undef INTOPT
  #undef BOOLOPT
  
  fprintf(stderr,"%s:%d: Unexpected option '%.*s' = '%.*s'\n",path,lineno,kc,k,vc,v);
  return -2;
}

/* Argv positional argument.
 */
 
static int egg_native_config_positional(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return 0;
  
  if (!egg.rompath&&egg_native_rom_expects_file()) {
    return egg_native_set_string(&egg.rompath,src,srcc);
  }
  
  return -1;
}

/* From argv.
 */
 
static int egg_native_config_argv(int argc,char **argv) {
  int argi=1,err;
  while (argi<argc) {
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    
    // No dashes?
    if (arg[0]!='-') {
      if ((err=egg_native_config_positional(arg,-1))<0) {
        if (err!=-2) fprintf(stderr,"%s: Unexpected argument '%s'\n",egg.exename,arg);
        return -2;
      }
      continue;
    }
    
    // Single dash alone?
    if (!arg[1]) {
      fprintf(stderr,"%s: Unexpected argument '%s'\n",egg.exename,arg);
      return -2;
    }
    
    // Short option?
    if (arg[1]!='-') {
      char k=arg[1];
      const char *v=0;
      if (arg[2]) v=arg+2;
      else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
      if ((err=egg_native_config_kv(&k,1,v,-1,"<cmdline>",0))<0) {
        if (err!=-2) fprintf(stderr,"%s: Unexpected argument '%s'\n",egg.exename,arg);
        return -2;
      }
      continue;
    }
    
    // Long option or multiple dashes alone.
    const char *k=arg+2;
    while (k[0]=='-') k++;
    if (!*k) {
      fprintf(stderr,"%s: Unexpected argument '%s'\n",egg.exename,arg);
      return -2;
    }
    int kc=0;
    while (k[kc]&&(k[kc]!='=')) kc++;
    const char *v=0;
    if (k[kc]=='=') v=k+kc+1;
    else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
    if ((err=egg_native_config_kv(k,kc,v,-1,"<cmdline>",0))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unexpected argument '%s'\n",egg.exename,arg);
      return -2;
    }
  }
  
  return 0;
}

/* Configure, main entry point.
 */
 
int egg_native_config(int argc,char **argv) {
  int err;
  
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) {
    egg.exename=argv[0];
  } else {
    egg.exename="egg";
  }
  
  if ((err=egg_native_config_argv(argc,argv))<0) return err;
  //TODO config files
  //TODO defaults, validation
  
  return 0;
}
