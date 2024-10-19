#include "eggdev_internal.h"

/* --help=pack
 */
 
static void eggdev_print_help_pack() {
  fprintf(stderr,"\nUsage: %s pack -oROM DIRECTORY...\n\n",eggdev.exename);
  fprintf(stderr,
    "Generate a ROM file from loose inputs.\n"
    "IDs within each input must be unique.\n"
    "Across different inputs, the later one overrides the earlier.\n"
    "ROM files, executables, and HTML bundles are also accepted as input.\n"
    "So we also serve as the reverse of 'eggdev bundle'.\n"
    "\n"
  );
}

/* --help=unpack
 */
 
static void eggdev_print_help_unpack() {
  fprintf(stderr,"\nUsage: %s unpack -oDIRECTORY ROM|EXE|HTML [--raw]\n\n",eggdev.exename);
  fprintf(stderr,
    "Extract all resources into a new directory.\n"
    "By default, we will try to reformat standard types to the preferred input format.\n"
    "Use '--raw' to suppress that behavior, and output resources exactly as stored.\n"
    "\n"
  );
}

/* --help=bundle
 */
 
static void eggdev_print_help_bundle() {
  fprintf(stderr,"\nUsage: %s bundle -oEXE|HTML ROM [LIB|--recompile]\n\n",eggdev.exename);
  fprintf(stderr,
    "Generate a self-contained executable or web app from a ROM.\n"
    "We also accept loose directories, executables, and HTML files, but that's a little weird.\n"
    "\n"
    "Rules, in order:\n"
    "  - If the output file ends '.html' or '.htm', we're producing HTML.\n"
    "  - If LIB is provided, we strip code resources and produce a true-native executable.\n"
    "  - If --recompile is provided, we use WABT to decompile the ROM's code and recompile natively.\n"
    "  - Otherwise we produce a fake-native executable with the full WebAssembly runtime.\n"
    "\n"
  );
}

/* --help=list
 */
 
static void eggdev_print_help_list() {
  fprintf(stderr,"\nUsage: %s list ROM|EXE|HTML|DIRECTORY [-fFORMAT]\n\n",eggdev.exename);
  fprintf(stderr,
    "Show content of a ROM file (or the various other forms).\n"
    "\n"
    "FORMAT:\n"
    "  default: One-line header, then 'TYPE:RID SIZE [NAME]' for each resource.\n"
    "  toc: C header listing all custom types and named resources. Input is typically a directory.\n"
    "  summary: One-line header, then 'TYPE COUNT TOTAL_SIZE' for each type.\n"
    "\n"
  );
}

/* --help=validate
 */
 
static void eggdev_print_help_validate() {
  fprintf(stderr,"\nUsage: %s validate ROM|EXE|HTML\n\n",eggdev.exename);
  fprintf(stderr,
    "Look for format violations among the provided inputs.\n"
    "No output if valid.\n"
    "If you supply a directory, expect failures: Validator will not compile the resources.\n"
    "\n"
  );
}

/* --help=serve
 */
 
static void eggdev_print_help_serve() {
  fprintf(stderr,"\nUsage: %s serve [--htdocs=PATH...] [--write=PATH] [--port=INT] [--external] [--default-rom=REQPATH] [--audio=DRIVER...]\n\n",eggdev.exename);
  fprintf(stderr,
    "Run the dev server, mostly a generic HTTP server.\n"
    "BEWARE: This server is not hardened for use on untrusted networks.\n"
    "--external to serve on all interfaces. Localhost only by default.\n"
    "--htdocs arguments override previous ones, we serve the first file we find, searching backward.\n"
    "--write should match one of your --htdocs. The only directory we PUT or DELETE in.\n"
    "--default-rom gets inserted in the runtime's bootstrap.js.\n"
    "See etc/doc/eggdev-http.md for REST API and more details.\n"
    "--audio to enable audio output and synthesizer, for editing songs and sounds. Try `egg --help` for options.\n"
    "\n"
  );
}

/* --help=config
 */
 
static void eggdev_print_help_config() {
  fprintf(stderr,"\nUsage: %s config [KEYS...]\n\n",eggdev.exename);
  fprintf(stderr,
    "Dump our build-time configuration.\n"
    "With no arguments, print every config field on its own line 'KEY = VALUE'.\n"
    "Otherwise, print only VALUE for each named key.\n"
    "\n"
    "KEYS:\n"
    "  CC              Native C compiler, with flags.\n"
    "  AS              Native assembler, with flags.\n"
    "  LD              Native linker, with flags.\n"
    "  LDPOST          Native libraries, to append to the LD command.\n"
    "  WEB_CC          Wasm C compiler, with flags.\n"
    "  WEB_LD          Wasm linker, with flags.\n"
    "  WEB_LDPOST      Wasm libraries, to append to the LD command.\n"
    "  EGG_SDK         Absolute path to the egg repo where this eggdev was built.\n"
    "  NATIVE_TARGET   Name of this build target (linux,macos,mswin,generic)\n"
    "  WABT_SDK        Absolute path to the wabt repo, if configured.\n"
    "\n"
  );
}

/* --help=dump
 */
 
static void eggdev_print_help_dump() {
  fprintf(stderr,"\nUsage: %s dump ROM TYPE:ID\n\n",eggdev.exename);
  fprintf(stderr,
    "Print a hex dump of one resource to stdout.\n"
    "If you need anything different format-wise, use 'unpack' instead. 'dump' is just a convenience.\n"
    "\n"
  );
}

/* --help=project
 */
 
static void eggdev_print_help_project() {
  fprintf(stderr,"\nUsage: %s project\n\n",eggdev.exename);
  fprintf(stderr,
    "Create a new empty project that can build an Egg ROM.\n"
    "Interactive.\n"
    "\n"
  );
}

/* --help=metadata
 */
 
static void eggdev_print_help_metadata() {
  fprintf(stderr,"\nUsage: %s metadata ROM [--lang=all|LANG] [--iconImage] [--posterImage]\n\n",eggdev.exename);
  fprintf(stderr,
    "Print metadata:1 as JSON.\n"
    "With no other arguments, we print exactly what's stored in the resource. All values are strings.\n"
    "'--lang=all' to print fields with language variants as objects: { default: string, [LANG]: string }\n"
    "'--lang=LANG' to print everything as strings, and select the given language if available.\n"
    "'--iconImage' and '--posterImage' look up image resources and print them as enormous base64 strings.\n"
    "In theory, you can give a directory as ROM, but we expect all resources to be in their compiled format.\n"
    "\n"
  );
}

/* --help=sound
 */
 
static void eggdev_print_help_sound() {
  fprintf(stderr,"\nUsage: %s sound [ROM TYPE:ID] [FILE] [-oPATH] [--audio=DRIVER] [--audio-rate=HZ] [--audio-chanc=1|2] [--audio-buffer=INT] [--audio-device=STRING]\n\n",eggdev.exename);
  fprintf(stderr,
    "Play a sound using the same audio driver and synthesizer available to the native runtime.\n"
    "We accept all ROM and source formats, ie: MIDI, WAV, EGS, PCM.\n"
    "With '-o', dump raw PCM to a file.\n"
    "One of [ROM TYPE:ID], [FILE] is required.\n"
    "[-oPATH], [--audio-DRIVER] typically you specify just one, but both is fine and neither is fine too, to only show the summary.\n"
    "After synth completes, we print runtime, peak, and RMS to stdout.\n"
    "Mind that the output we analyze will always include some amount of trailing silence.\n"
    "\n"
  );
}

/* --help default
 */
 
static void eggdev_print_help_default() {
  fprintf(stderr,"\nUsage: %s COMMAND -oOUTPUT [INPUT...] [OPTIONS]\n\n",eggdev.exename);
  fprintf(stderr,
    "Try --help=COMMAND for more detail:\n"
    "      pack -oROM DIRECTORY...\n"
    "    unpack -oDIRECTORY ROM|EXE|HTML [--raw]\n"
    "    bundle -oEXE|HTML ROM [LIB|--recompile]\n"
    "      list ROM|EXE|HTML|DIRECTORY [-fFORMAT]\n"
    "  validate ROM|EXE|HTML|DIRECTORY\n"
    "     serve [--htdocs=PATH...] [--write=PATH] [--port=INT] [--external] [--default-rom=REQPATH] [--audio=DRIVER...]\n"
    "    config [KEYS...]\n"
    "      dump ROM TYPE:ID\n"
    "   project\n"
    "  metadata ROM [--lang=all|LANG] [--iconImage] [--posterImage]\n"
    "     sound [ROM TYPE:ID] [FILE] [-oPATH] [--audio=DRIVER] [--audio-rate=HZ] [--audio-chanc=1|2] [--audio-buffer=INT] [--audio-device=STRING]\n"
    "\n"
  );
}

/* --help, dispatch.
 */
 
void eggdev_print_help(const char *topic) {
  if (!topic||!topic[0]) eggdev_print_help_default();
  #define _(tag) else if (!strcmp(topic,#tag)) eggdev_print_help_##tag();
  _(pack)
  _(unpack)
  _(bundle)
  _(list)
  _(validate)
  _(serve)
  _(config)
  _(dump)
  _(project)
  _(metadata)
  _(sound)
  #undef _
  else eggdev_print_help_default();
}

/* Key=value.
 * NB (v) strings may be retained as is and must be nul-terminated.
 * Might need to change some things if invoke this from a config file.
 */
 
static int eggdev_configure_kv(const char *k,int kc,const char *v,int vc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  if (!v) {
    if ((kc>=3)&&!memcmp(k,"no-",3)) {
      k+=3;
      kc-=3;
      v="0";
      vc=1;
    } else {
      v="1";
      vc=1;
    }
  }
  int vn=0;
  sr_int_eval(&vn,v,vc);
  
  if ((kc==4)&&!memcmp(k,"help",4)) {
    eggdev.command="help";
    eggdev.helptopic=v;
    return 0;
  }
  
  if ((kc==1)&&!memcmp(k,"o",1)) {
    if (eggdev.dstpath) {
      fprintf(stderr,"%s: Multiple output paths.\n",eggdev.exename);
      return -2;
    }
    eggdev.dstpath=v;
    return 0;
  }
  
  if ((kc==3)&&!memcmp(k,"raw",3)) {
    eggdev.raw=vn;
    return 0;
  }
  
  if ((kc==9)&&!memcmp(k,"recompile",9)) {
    eggdev.recompile=vn;
    return 0;
  }
  
  if ((kc==6)&&!memcmp(k,"format",6)) {
    eggdev.format=v;
    return 0;
  }
  if ((kc==1)&&!memcmp(k,"f",1)) {
    eggdev.format=v;
    return 0;
  }
  
  if ((kc==6)&&!memcmp(k,"htdocs",6)) {
    if (eggdev.htdocsc>=eggdev.htdocsa) {
      int na=eggdev.htdocsa+4;
      if (na>INT_MAX/sizeof(void*)) return -1;
      void *nv=realloc(eggdev.htdocsv,sizeof(void*)*na);
      if (!nv) return -1;
      eggdev.htdocsv=nv;
      eggdev.htdocsa=na;
    }
    eggdev.htdocsv[eggdev.htdocsc++]=v;
    return 0;
  }
  
  if ((kc==5)&&!memcmp(k,"write",5)) {
    if (eggdev.writepath) {
      fprintf(stderr,"%s: Multiple --write directories.\n",eggdev.exename);
      return -2;
    }
    eggdev.writepath=v;
    return 0;
  }
  
  if ((kc==4)&&!memcmp(k,"port",4)) {
    eggdev.port=vn;
    return 0;
  }
  
  if ((kc==8)&&!memcmp(k,"external",8)) {
    eggdev.external=vn;
    return 0;
  }
  
  if ((kc==4)&&!memcmp(k,"lang",4)) {
    eggdev.lang=v;
    return 0;
  }
  
  if ((kc==9)&&!memcmp(k,"iconImage",9)) {
    eggdev.iconImage=vn;
    return 0;
  }
  
  if ((kc==11)&&!memcmp(k,"posterImage",11)) {
    eggdev.posterImage=vn;
    return 0;
  }
  
  if ((kc==11)&&!memcmp(k,"default-rom",11)) {
    eggdev.default_rom_path=v;
    return 0;
  }
  
  if ((kc==5)&&!memcmp(k,"audio",5)) {
    eggdev.audio_drivers=v;
    return 0;
  }
  
  if ((kc==10)&&!memcmp(k,"audio-rate",10)) {
    eggdev.audio_rate=vn;
    return 0;
  }
  
  if ((kc==11)&&!memcmp(k,"audio-chanc",11)) {
    eggdev.audio_chanc=vn;
    return 0;
  }
  
  if ((kc==12)&&!memcmp(k,"audio-buffer",12)) {
    eggdev.audio_buffer=vn;
    return 0;
  }
  
  if ((kc==12)&&!memcmp(k,"audio-device",12)) {
    eggdev.audio_device=v;
    return 0;
  }
  
  fprintf(stderr,"%s: Unexpected option '%.*s' = '%.*s'\n",eggdev.exename,kc,k,vc,v);
  return -2;
}

/* Positional arguments.
 */
 
static int eggdev_configure_positional(const char *v) {
  if (!eggdev.command) {
    eggdev.command=v;
    return 0;
  }
  if (eggdev.srcpathc>=eggdev.srcpatha) {
    int na=eggdev.srcpatha+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(eggdev.srcpathv,sizeof(void*)*na);
    if (!nv) return -1;
    eggdev.srcpathv=nv;
    eggdev.srcpatha=na;
  }
  eggdev.srcpathv[eggdev.srcpathc++]=v;
  return 0;
}

/* Argv.
 */
 
static int eggdev_configure_argv(int argc,char **argv) {
  int argi=1,err;
  while (argi<argc) {
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    
    // No dash: Positional args.
    if (arg[0]!='-') {
      if ((err=eggdev_configure_positional(arg))<0) return err;
      continue;
    }
    
    // Single dash alone: Reserved.
    if (!arg[1]) {
      fprintf(stderr,"%s: Unexpected argument '%s'\n",eggdev.exename,arg);
      return -2;
    }
    
    // Single dash: '-kVV' or '-k VV'.
    if (arg[1]!='-') {
      char k=arg[1];
      const char *v=0;
      if (arg[2]) v=arg+2;
      else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
      if ((err=eggdev_configure_kv(&k,1,v,-1))<0) return err;
      continue;
    }
    
    // Double dash alone: Reserved.
    if (!arg[2]) {
      fprintf(stderr,"%s: Unexpected argument '%s'\n",eggdev.exename,arg);
      return -2;
    }
    
    // Double dash: '--kk=vv' or '--kk vv'.
    const char *k=arg+2;
    int kc=0;
    while (k[kc]&&(k[kc]!='=')) kc++;
    const char *v=0;
    if (k[kc]=='=') v=k+kc+1;
    else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
    if ((err=eggdev_configure_kv(k,kc,v,-1))<0) return err;
  }
  return 0;
}

/* Configure.
 */
 
int eggdev_configure(int argc,char **argv) {
  int err;
  
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) eggdev.exename=argv[0];
  else eggdev.exename="eggdev";
  
  if ((err=eggdev_configure_argv(argc,argv))<0) return err;
  
  return 0;
}
