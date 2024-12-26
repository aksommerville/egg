#include "eggrt_internal.h"
#include "opt/serial/serial.h"

static int eggrt_input_path_with_suffix(char *dst,int dsta,const char *pfx,const char *sfx);

/* --version
 */
 
static void eggrt_print_version() {
  fprintf(stderr,"0.0.1\n");//TODO How to source this?
}

/* --help=drivers
 */
 
static void eggrt_help_drivers() {
  #define INTF(tag) { \
    fprintf(stderr,"\n%s drivers:\n",#tag); \
    int p=0; for (;;p++) { \
      const struct hostio_##tag##_type *type=hostio_##tag##_type_by_index(p); \
      if (!type) break; \
      fprintf(stderr,"%12s: %s\n",type->name,type->desc); \
    } \
  }
  INTF(video)
  INTF(audio)
  INTF(input)
  #undef INTF
  fprintf(stderr,"\n");
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
    "An environment variable EGG_CONFIG may name a file with KEY=VALUE lines, using the options listed here.\n"
    "If unset, we read from ~/.egg/egg.cfg.\n"
    "Options on the command-line override those in the config file.\n"
    "\n"
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
    "  --input-config=PATH           Where to load and save gamepad mappings.\n"
    "  --store=PATH                  Saved game. Blank for default, or \"none\" to disable.\n"
    "  --store:KEY=VALUE             Add or override a store field.\n"
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

/* Given a decoder containing a JSON object, and an incoming key and value string,
 * either replace that field or append it.
 * We guarantee to terminate (dst) on successful runs.
 */
 
static int eggrt_roll_in_json_string(struct sr_encoder *dst,struct sr_decoder *pv,const char *k,int kc,const char *v,int vc) {
  int got=0;
  int dstctx=sr_encode_json_object_start(dst,0,0);
  if (dstctx<0) return dstctx;
  if (sr_decode_json_object_start(pv)>=0) {
    const char *pvk;
    int pvkc;
    while ((pvkc=sr_decode_json_next(&pvk,pv))>0) {
      const char *pvv=0;
      int pvvc=sr_decode_json_expression(&pvv,pv);
      if ((pvkc==kc)&&!memcmp(pvk,k,kc)) {
        if (sr_encode_json_string(dst,k,kc,v,vc)<0) return -1;
        got=1;
      } else {
        if (sr_encode_json_preencoded(dst,pvk,pvkc,pvv,pvvc)<0) return -1;
      }
    }
  }
  if (!got) {
    if (sr_encode_json_string(dst,k,kc,v,vc)<0) return -1;
  }
  if (sr_encode_json_end(dst,dstctx)<0) return -1;
  if (sr_encoder_terminate(dst)<0) return -1;
  return 0;
}

/* Add fake store field.
 */
 
static int eggrt_configure_store_field(const char *k,int kc,const char *v,int vc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  struct sr_encoder dst={0};
  struct sr_decoder pv={.v=eggrt.store_extra,.c=0};
  if (pv.v) while (((char*)(pv.v))[pv.c]) pv.c++;
  int err=eggrt_roll_in_json_string(&dst,&pv,k,kc,v,vc);
  if (err<0) {
    sr_encoder_cleanup(&dst);
    return err;
  }
  if (eggrt.store_extra) free(eggrt.store_extra);
  eggrt.store_extra=dst.v; // HANDOFF
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
    int i=vc; while (i-->0) {
      if (v[i]=='x') {
        int w,h;
        if (
          (sr_int_eval(&w,v,i)>=2)&&
          (sr_int_eval(&h,v+i+1,vc-i-1)>=2)&&
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
  
  // --store:KEY=VALUE
  if ((kc>=6)&&!memcmp(k,"store:",6)) return eggrt_configure_store_field(k+6,kc-6,v,vc);
  
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
  STROPT("input-config",inmgr_path)
  STROPT("store",storepath)
  
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

/* Apply the text of a config file.
 */
 
static int eggrt_configure_text(const char *src,int srcc,const char *path) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=1;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    if (!linec||(line[0]=='#')) continue;
    
    const char *k=line;
    int kc=0;
    int linep=0;
    while ((linep<linec)&&(line[linep++]!='=')) kc++;
    while (kc&&((unsigned char)k[kc-1]<=0x20)) kc--;
    const char *v=line+linep;
    int vc=linec-linep;
    
    if (
      ((kc==4)&&!memcmp(k,"help",4))||
      ((kc==15)&&!memcmp(k,"configure-input",15))||
    0) {
      fprintf(stderr,"%s:%d:WARNING: Option '%.*s' is forbidden in config files. Ignoring.\n",path,lineno,kc,k);
      continue;
    }
    
    int err=eggrt_configure_kv(k,kc,v,vc);
    if (err<0) {
      fprintf(stderr,"%s:%d: Error applying config field '%.*s' = '%.*s'\n",path,lineno,kc,k,vc,v);
      return -2;
    }
  }
  return 0;
}

/* Read a text file and apply each line as a config field.
 */
 
static int eggrt_configure_file(const char *path) {
  if (!path||!path[0]) return -1;
  char *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file. Using default global configuration.\n",path);
    return 0;
  }
  fprintf(stderr,"%s: Applying global configuration.\n",path);
  int err=eggrt_configure_text(src,srcc,path);
  free(src);
  return err;
}

/* Configure from the file named in EGG_CONFIG, or a default.
 */
 
static int eggrt_configure_mainfile() {
  const char *path=getenv("EGG_CONFIG");
  if (path&&path[0]) return eggrt_configure_file(path);
  char tmp[1024];
  int tmpc=eggrt_input_path_with_suffix(tmp,sizeof(tmp),getenv("HOME"),"/.egg/egg.cfg");
  if (tmpc>0) return eggrt_configure_file(tmp);
  return 0;
}

/* Make up default value for storepath.
 */
 
static char *eggrt_configure_default_store() {

  /* If there is a ROM path, append ".save".
   * Our predecessors ensure that rompath is only set when the program is configured for it.
   */
  if (eggrt.rompath) {
    int romc=0;
    while (eggrt.rompath[romc]) romc++;
    int pathc=romc+5;
    char *path=malloc(pathc+1);
    if (!path) return 0;
    memcpy(path,eggrt.rompath,romc);
    memcpy(path+romc,".save",6); // sic 6, includes terminator
    return path;
  }
  
  /* If we have an embedded ROM, use a sibling of the executable.
   * Executable's basename plus ".save". Hmm. Actually the same thing as the external case.
   * No slash in the executable path means we end up with a wd-relative path, and that seems fair.
   */
  if (eggrt_has_embedded_rom) {
    const char *pfx=eggrt.exename;
    int pfxc=0;
    while (pfx[pfxc]) pfxc++;
    int pathc=pfxc+5;
    char *path=malloc(pathc+1);
    if (!path) return 0;
    memcpy(path,pfx,pfxc);
    memcpy(path+pfxc,".save",6); // sic 6, includes terminator
    return path;
  }
  
  return 0;
}

/* Compose path with a prefix and suffix.
 * No output if prefix is null or empty, and we always terminate and never report something longer than fits.
 */
 
static int eggrt_input_path_with_suffix(char *dst,int dsta,const char *pfx,const char *sfx) {
  if (!pfx||!pfx[0]) return -1;
  int pfxc=0; while (pfx[pfxc]) pfxc++;
  int sfxc=0; while (sfx[sfxc]) sfxc++;
  int dstc=pfxc+sfxc;
  if (dstc>=dsta) return -1;
  memcpy(dst,pfx,pfxc);
  memcpy(dst+pfxc,sfx,sfxc);
  dst[dstc]=0;
  return dstc;
}

/* Compose path to the default global input file: ~/.egg/input
 * If it doesn't fit, or we can't resolve '~', return <0.
 */
 
static int eggrt_input_path_home_egg(char *dst,int dsta) {
  int dstc;

  const char *HOME=getenv("HOME");
  if ((dstc=eggrt_input_path_with_suffix(dst,dsta,HOME,"/.egg/input"))>0) return dstc;
  
  const char *USER=getenv("USER");
  if (USER&&USER[0]) {
    int userc=0; while (USER[userc]) userc++;
    #if USE_macos
      const char prefix[]="/Users/";
    #elif USE_mswin
      what is the windows home directory these days? //TODO
    #else
      const char prefix[]="/home/";
    #endif
    int prefixc=sizeof(prefix)-1;
    dstc=prefixc+userc+11;
    if (dstc>=dsta) return -1;
    memcpy(dst,prefix,prefixc);
    memcpy(dst+prefixc,USER,userc);
    memcpy(dst+prefixc+userc,"/.egg/input",11);
    dst[dstc]=0;
    return dstc;
  }
  
  return -1;
}

/* Make up default value for inmgr_path.
 */
 
static char *eggrt_configure_default_input() {
  char path[1024];
  int pathc;
  #define RETURNIT { \
    char *dst=malloc(pathc+1); \
    if (!dst) return 0; \
    memcpy(dst,path,pathc); \
    dst[pathc]=0; \
    return dst; \
  }
  
  // "{ROM}.input" if the file exists.
  if ((pathc=eggrt_input_path_with_suffix(path,sizeof(path),eggrt.rompath,".input"))>0) {
    if (file_get_type(path)=='f') RETURNIT
  }
  
  // "{EXE}.input" if the file exists.
  if ((pathc=eggrt_input_path_with_suffix(path,sizeof(path),eggrt.exename,".input"))>0) {
    if (file_get_type(path)=='f') RETURNIT
  }
  
  // "~/.egg/input" if it exists.
  if ((pathc=eggrt_input_path_home_egg(path,sizeof(path)))>0) {
    if (file_get_type(path)=='f') RETURNIT
    // Create the directory if needed, and then "~/.egg/input" is still our answer.
    if (dir_mkdirp_parent(path)<0) return 0;
    RETURNIT
  }
  
  #undef RETURNIT
  return 0;
}

/* List languages supported by the user, in consultation with OS.
 * Never returns >dsta.
 */

#if USE_macos
  #include "opt/macos/macos.h"
#endif

static int eggrt_get_user_languages(int *dstv,int dsta) {
  if (dsta<1) return 0;
  int dstc=0;

  #if USE_macos
    if ((dstc=macos_get_preferred_languages(dstv,dsta))>0) return dstc;
  #endif
  //TODO Preferred language in Windows, however they do that.
  
  /* POSIX systems typically have LANG as the single preferred locale, which starts with a language code.
   * There can also be LANGUAGE, which is multiple language codes separated by colons.
   */
  const char *src;
  if (src=getenv("LANG")) {
    if ((src[0]>='a')&&(src[0]<='z')&&(src[1]>='a')&&(src[1]<='z')) {
      if (dstc<dsta) dstv[dstc++]=EGG_LANG_FROM_STRING(src);
    }
  }
  if (dstc>=dsta) return dstc;
  if (src=getenv("LANGUAGE")) {
    int srcp=0;
    while (src[srcp]&&(dstc<dsta)) {
      const char *token=src+srcp;
      int tokenc=0;
      while (src[srcp]&&(src[srcp++]!=':')) tokenc++;
      if ((tokenc>=2)&&(token[0]>='a')&&(token[0]<='z')&&(token[1]>='a')&&(token[1]<='z')) {
        int lang=EGG_LANG_FROM_STRING(token);
        int already=0,i=dstc;
        while (i-->0) if (dstv[i]==lang) { already=1; break; }
        if (!already) dstv[dstc++]=lang;
      }
    }
  }
  
  return dstc;
}

/* Parse the metadata language string into a list of integer language codes.
 * Never returns >dsta.
 */
 
static int eggrt_parse_languages(int *dstv,int dsta,const char *src,int srcc) {
  int srcp=0,dstc=0;
  while ((srcp<srcc)&&(dstc<dsta)) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (src[srcp]==',') { srcp++; continue; }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&(src[srcp++]!=',')) tokenc++;
    while (tokenc&&((unsigned char)token[tokenc-1]<=0x20)) tokenc--;
    if ((tokenc==2)&&(token[0]>='a')&&(token[0]<='z')&&(token[1]>='a')&&(token[1]<='z')) {
      dstv[dstc++]=EGG_LANG_FROM_STRING(token);
    }
  }
  return dstc;
}

/* Guess language from system settings and ROM.
 */
 
int eggrt_configure_guess_language() {

  // Acquire preferences from the system and the game.
  int userv[16];
  int userc=eggrt_get_user_languages(userv,sizeof(userv)/sizeof(userv[0]));
  const char *romlang=0;
  int romlangc=rom_lookup_metadata(&romlang,eggrt.romserial,eggrt.romserialc,"lang",4,0);
  int romv[16];
  int romc=eggrt_parse_languages(romv,sizeof(romv)/sizeof(romv[0]),romlang,romlangc);
  
  // First user language supported at all by the game wins.
  int ai=0; for (;ai<userc;ai++) {
    int bi=romc; while (bi-->0) {
      if (romv[bi]==userv[ai]) return userv[ai];
    }
  }
  
  // If we wanted some fuzzy-matching logic, eg prefer Russian over English for speakers of Polish, we could do that here.
  // Sounds like a can of worms to me, I think it's not our problem.
  
  // If at least one language is declared by the game, go with its most preferred.
  if (romc>=1) return romv[0];
  
  // Game has no languages. Doesn't really matter what we pick now. Use the user's first language, or if none, English.
  if (userc>=1) return userv[0];
  return EGG_LANG_FROM_STRING("en");
}

/* Finalize configuration.
 */
 
static int eggrt_configure_final() {
  if (eggrt.terminate) return 0;
  
  // Set (rptname) for game-related logging, where we don't necessarily want to speak as Egg.
  if (eggrt.rompath) eggrt.rptname=eggrt.rompath;
  else eggrt.rptname=eggrt.exename;
  
  // Pick a default (storepath) in the typical case where it wasn't provided.
  if (!eggrt.storepath) eggrt.storepath=eggrt_configure_default_store();
  else if (!strcmp(eggrt.storepath,"none")) {
    free(eggrt.storepath);
    eggrt.storepath=0;
  }
  
  // Default (inmgr_path) if not provided, best effort.
  if (!eggrt.inmgr_path) {
    eggrt.inmgr_path=eggrt_configure_default_input();
  }
  
  // If (inmgr_path) unset, asking for (configure_input) is a hard error.
  // (we could gather the input, sure, but there's nowhere to write it to).
  if (!eggrt.inmgr_path) {
    if (eggrt.configure_input) {
      fprintf(stderr,"%s: Must provide path to input config as '--input-config=PATH' to use '--configure-input'\n",eggrt.exename);
      return -2;
    }
  }
  
  // You'd expect to see defaulting of (lang) here too, but we want to wait until the ROM is loaded. See eggrt_main.c:eggrt_init().
  
  return 0;
}

/* Configure, main entry point.
 */
 
int eggrt_configure(int argc,char **argv) {
  int err;

  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) eggrt.exename=argv[0];
  else eggrt.exename="egg";
  
  if ((err=eggrt_configure_mainfile())<0) return err;
  if ((err=eggrt_configure_argv(argc,argv))<0) return err;
  
  return eggrt_configure_final();
}
