#include "eggrt_internal.h"

#define NATIVE 1
#define WASM 2
#define RECOM 3

#if EXECFMT==NATIVE
  const int eggrt_is_native_client=1;
  
#elif EXECFMT==WASM
  const int eggrt_is_native_client=0;
  #include "wasm_c_api.h"
  #include "wasm_export.h"

  static struct eggrt_wasm {
    wasm_module_t mod;
    wasm_module_inst_t inst;
    wasm_exec_env_t ee;
  
    // Wamr evidently writes into the binary we give it. Why would it do that? Well, we have to copy it then.
    void *code;
    int codec;
  
    wasm_function_inst_t egg_client_quit;
    wasm_function_inst_t egg_client_init;
    wasm_function_inst_t egg_client_update;
    wasm_function_inst_t egg_client_render;
  } eggrt_wasm={0};

#elif EXECFMT==RECOM
  const int eggrt_is_native_client=1;

  #include "wasm-rt.h"

  struct w2c_env;

  typedef struct w2c_mm {
    struct w2c_env* w2c_env_instance;
    uint32_t w2c_0x5F_stack_pointer;
    wasm_rt_memory_t w2c_memory;
    wasm_rt_funcref_table_t w2c_0x5F_indirect_function_table;
  } w2c_mm;

  static struct eggrt_w2c {
    w2c_mm mod;
  } eggrt_w2c={0};

  void wasm2c_mm_instantiate(w2c_mm* instance);
  void wasm2c_mm_free(w2c_mm* instance);
  void w2c_mm_egg_client_quit(w2c_mm *mod,int termstatus);
  int w2c_mm_egg_client_init(w2c_mm *mod);
  void w2c_mm_egg_client_update(w2c_mm *mod,double elapsed);
  void w2c_mm_egg_client_render(w2c_mm *mod);
  
#else
  #error "Please build with -DEXECFMT=NATIVE or -DEXECFMT=WASM or -DEXECFMT=RECOM"
#endif

/* API wrapper for wasm-micro-runtime.
 * No business logic here. Just validate pointers and forward to the proper Egg API call.
 * Some trivial things like getters might bypass the proper call.
 */
#if EXECFMT==WASM

  static void *eggrt_wasm_get_client_memory(uint32_t addr,int len) {
    if (!wasm_runtime_validate_app_addr(eggrt_wasm.inst,addr,len)) return 0;
    return wasm_runtime_addr_app_to_native(eggrt_wasm.inst,addr);
  }

  static void egg_wasm_log(wasm_exec_env_t ee,const char *msg) {
    egg_log(msg);
  }
  
  static void egg_wasm_terminate(wasm_exec_env_t ee,int status) {
    return egg_terminate(status);
  }
  
  static double egg_wasm_time_real(wasm_exec_env_t ee) {
    return egg_time_real();
  }
  
  static void egg_wasm_time_local(wasm_exec_env_t ee,int dstp,int dsta) {
    if (dsta<1) return;
    int *dst=eggrt_wasm_get_client_memory(dstp,dsta*sizeof(int));
    if (dst) egg_time_local(dst,dsta);
  }
  
  static int egg_wasm_get_language(wasm_exec_env_t ee) {
    return eggrt.lang;
  }
  
  static void egg_wasm_set_language(wasm_exec_env_t ee,int lang) {
    egg_set_language(lang);
  }
  
  static int egg_wasm_get_rom(wasm_exec_env_t ee,void *dst,int dsta) {
    return egg_get_rom(dst,dsta);
  }
  
  static int egg_wasm_store_get(wasm_exec_env_t ee,char *v,int va,const char *k,int kc) {
    return egg_store_get(v,va,k,kc);
  }
  
  static int egg_wasm_store_set(wasm_exec_env_t ee,const char *k,int kc,const char *v,int vc) {
    return egg_store_set(k,kc,v,vc);
  }
  
  static int egg_wasm_store_key_by_index(wasm_exec_env_t ee,char *k,int ka,int p) {
    return egg_store_key_by_index(k,ka,p);
  }
  
  static int egg_wasm_input_get_one(wasm_exec_env_t ee,int playerid) {
    return egg_input_get_one(playerid);
  }
  
  static int egg_wasm_input_get_all(wasm_exec_env_t ee,uint32_t dstp,int dsta) {
    int *dst=eggrt_wasm_get_client_memory(dstp,sizeof(int)*dsta);
    return egg_input_get_all(dst,dsta);
  }
  
  static int egg_wasm_input_configure(wasm_exec_env_t ee) {
    return egg_input_configure();
  }
  
  static void egg_wasm_play_sound(wasm_exec_env_t ee,int rid,int index) {
    egg_play_sound(rid,index);
  }
  
  static void egg_wasm_play_song(wasm_exec_env_t ee,int rid,int force,int repeat) {
    egg_play_song(rid,force,repeat);
  }
  
  static void egg_wasm_audio_event(wasm_exec_env_t ee,int chid,int opcode,int a,int b,int durms) {
    egg_audio_event(chid,opcode,a,b,durms);
  }
  
  static double egg_wasm_audio_get_playhead(wasm_exec_env_t ee) {
    return egg_audio_get_playhead();
  }
  
  static void egg_wasm_audio_set_playhead(wasm_exec_env_t ee,double s) {
    egg_audio_set_playhead(s);
  }
  
  static int egg_wasm_image_decode_header(wasm_exec_env_t ee,int wp,int hp,int pixelsizep,const void *src,int srcc) {
    int *w=eggrt_wasm_get_client_memory(wp,4);
    int *h=eggrt_wasm_get_client_memory(hp,4);
    int *pixelsize=eggrt_wasm_get_client_memory(pixelsizep,4);
    return egg_image_decode_header(w,h,pixelsize,src,srcc);
  }
  
  static int egg_wasm_image_decode(wasm_exec_env_t ee,void *dst,int dsta,const void *src,int srcc) {
    return egg_image_decode(dst,dsta,src,srcc);
  }
  
  static void egg_wasm_texture_del(wasm_exec_env_t ee,int texid) {
    egg_texture_del(texid);
  }
  
  static int egg_wasm_texture_new(wasm_exec_env_t ee) {
    return egg_texture_new();
  }
  
  static int egg_wasm_texture_get_status(wasm_exec_env_t ee,int wp,int hp,int texid) {
    int *w=eggrt_wasm_get_client_memory(wp,4);
    int *h=eggrt_wasm_get_client_memory(hp,4);
    return egg_texture_get_status(w,h,texid);
  }
  
  static int egg_wasm_texture_get_pixels(wasm_exec_env_t ee,void *dst,int dsta,int texid) {
    return egg_texture_get_pixels(dst,dsta,texid);
  }
  
  static int egg_wasm_texture_load_image(wasm_exec_env_t ee,int texid,int rid) {
    return egg_texture_load_image(texid,rid);
  }
  
  static int egg_wasm_texture_load_serial(wasm_exec_env_t ee,int texid,const void *src,int srcc) {
    return egg_texture_load_serial(texid,src,srcc);
  }
  
  static int egg_wasm_texture_load_raw(wasm_exec_env_t ee,int texid,int fmt,int w,int h,int stride,const void *src,int srcc) {
    return egg_texture_load_raw(texid,fmt,w,h,stride,src,srcc);
  }
  
  static void egg_wasm_draw_globals(wasm_exec_env_t ee,int tint,int alpha) {
    egg_draw_globals(tint,alpha);
  }
  
  static void egg_wasm_draw_clear(wasm_exec_env_t ee,int dsttexid,uint32_t rgba) {
    egg_draw_clear(dsttexid,rgba);
  }
  
  static void egg_wasm_draw_line(wasm_exec_env_t ee,int dsttexid,int vp,int c) {
    const void *v=eggrt_wasm_get_client_memory(vp,c*sizeof(struct egg_draw_line));
    egg_draw_line(dsttexid,v,c);
  }
  
  static void egg_wasm_draw_rect(wasm_exec_env_t ee,int dsttexid,int vp,int c) {
    const void *v=eggrt_wasm_get_client_memory(vp,c*sizeof(struct egg_draw_rect));
    egg_draw_rect(dsttexid,v,c);
  }
  
  static void egg_wasm_draw_trig(wasm_exec_env_t ee,int dsttexid,int vp,int c) {
    const void *v=eggrt_wasm_get_client_memory(vp,c*sizeof(struct egg_draw_trig));
    egg_draw_trig(dsttexid,v,c);
  }
  
  static void egg_wasm_draw_decal(wasm_exec_env_t ee,int dsttexid,int srctexid,int vp,int c) {
    const void *v=eggrt_wasm_get_client_memory(vp,c*sizeof(struct egg_draw_decal));
    egg_draw_decal(dsttexid,srctexid,v,c);
  }
  
  static void egg_wasm_draw_tile(wasm_exec_env_t ee,int dsttexid,int srctexid,int vp,int c) {
    const void *v=eggrt_wasm_get_client_memory(vp,c*sizeof(struct egg_draw_tile));
    egg_draw_tile(dsttexid,srctexid,v,c);
  }
  
  static void egg_wasm_draw_mode7(wasm_exec_env_t ee,int dsttexid,int srctexid,int vp,int c) {
    const void *v=eggrt_wasm_get_client_memory(vp,c*sizeof(struct egg_draw_mode7));
    egg_draw_mode7(dsttexid,srctexid,v,c);
  }

  static NativeSymbol eggrt_wasm_exports[]={
    {"egg_log",egg_wasm_log,"($)"},
    {"egg_terminate",egg_wasm_terminate,"(i)"},
    {"egg_time_real",egg_wasm_time_real,"()F"},
    {"egg_time_local",egg_wasm_time_local,"(ii)"},
    {"egg_get_language",egg_wasm_get_language,"()i"},
    {"egg_set_language",egg_wasm_set_language,"(i)"},
    {"egg_get_rom",egg_wasm_get_rom,"(*~)i"},
    {"egg_store_get",egg_wasm_store_get,"(*~*~)i"},
    {"egg_store_set",egg_wasm_store_set,"(*~*~)i"},
    {"egg_store_key_by_index",egg_wasm_store_key_by_index,"(*~i)i"},
    {"egg_input_get_one",egg_wasm_input_get_one,"(i)i"},
    {"egg_input_get_all",egg_wasm_input_get_all,"(ii)i"},
    {"egg_input_configure",egg_wasm_input_configure,"()i"},
    {"egg_play_sound",egg_wasm_play_sound,"(ii)"},
    {"egg_play_song",egg_wasm_play_song,"(iii)"},
    {"egg_audio_event",egg_wasm_audio_event,"(iiiii)"},
    {"egg_audio_get_playhead",egg_wasm_audio_get_playhead,"()F"},
    {"egg_audio_set_playhead",egg_wasm_audio_set_playhead,"(F)"},
    {"egg_image_decode_header",egg_wasm_image_decode_header,"(iii*~)i"},
    {"egg_image_decode",egg_wasm_image_decode,"(*~*~)i"},
    {"egg_texture_del",egg_wasm_texture_del,"(i)"},
    {"egg_texture_new",egg_wasm_texture_new,"()i"},
    {"egg_texture_get_status",egg_wasm_texture_get_status,"(iii)i"},
    {"egg_texture_get_pixels",egg_wasm_texture_get_pixels,"(*~i)i"},
    {"egg_texture_load_image",egg_wasm_texture_load_image,"(ii)i"},
    {"egg_texture_load_serial",egg_wasm_texture_load_serial,"(i*~)i"},
    {"egg_texture_load_raw",egg_wasm_texture_load_raw,"(iiiii*~)i"},
    {"egg_draw_globals",egg_wasm_draw_globals,"(ii)"},
    {"egg_draw_clear",egg_wasm_draw_clear,"(ii)"},
    {"egg_draw_line",egg_wasm_draw_line,"(iii)"},
    {"egg_draw_rect",egg_wasm_draw_rect,"(iii)"},
    {"egg_draw_trig",egg_wasm_draw_trig,"(iii)"},
    {"egg_draw_decal",egg_wasm_draw_decal,"(iiii)"},
    {"egg_draw_tile",egg_wasm_draw_tile,"(iiii)"},
    {"egg_draw_mode7",egg_wasm_draw_mode7,"(iiii)"},
  };

#endif

/* API wrapper for WABT wasm2c.
 */
#if EXECFMT==RECOM

  #define HOSTADDR(wasmaddr,len) (void*)(((wasmaddr)<=eggrt_w2c.mod.w2c_memory.size-(len))?(eggrt_w2c.mod.w2c_memory.data+(wasmaddr)):0)

  void w2c_env_egg_log(struct w2c_env *env,uint32_t msgp) {
    const char *msg=HOSTADDR(msgp,0);
    egg_log(msg);
  }
  
  void w2c_env_egg_terminate(struct w2c_env *env,int status) {
    egg_terminate(status);
  }
  
  double w2c_env_egg_time_real(struct w2c_env *env) {
    return egg_time_real();
  }
  
  void w2c_env_egg_time_local(struct w2c_env *env,uint32_t dstp,int dsta) {
    int *dst=HOSTADDR(dstp,sizeof(int)*dsta);
    egg_time_local(dst,dsta);
  }
  
  int w2c_env_egg_get_language(struct w2c_env *env) {
    return egg_get_language();
  }
  
  void w2c_env_egg_set_language(struct w2c_env *env,int lang) {
    egg_set_language(lang);
  }
  
  int w2c_env_egg_get_rom(struct w2c_env *env,uint32_t dstp,int dsta) {
    void *dst=HOSTADDR(dstp,dsta);
    return egg_get_rom(dst,dsta);
  }
  
  int w2c_env_egg_store_get(struct w2c_env *env,uint32_t vp,int va,uint32_t kp,int kc) {
    char *v=HOSTADDR(vp,va);
    const char *k=HOSTADDR(kp,kc);
    return egg_store_get(v,va,k,kc);
  }
  
  int w2c_env_egg_store_set(struct w2c_env *env,uint32_t kp,int kc,uint32_t vp,int vc) {
    const char *k=HOSTADDR(kp,kc);
    const char *v=HOSTADDR(vp,vc);
    return egg_store_set(k,kc,v,vc);
  }
  
  int w2c_env_egg_store_key_by_index(struct w2c_env *env,uint32_t kp,int ka,int p) {
    char *k=HOSTADDR(kp,ka);
    return egg_store_key_by_index(k,ka,p);
  }
  
  int w2c_env_egg_input_get_one(struct w2c_env *env,int playerid) {
    return egg_input_get_one(playerid);
  }
  
  int w2c_env_egg_input_get_all(struct w2c_env *env,uint32_t dstp,int dsta) {
    int *dst=HOSTADDR(dstp,sizeof(int)*dsta);
    return egg_input_get_all(dst,dsta);
  }
  
  int w2c_env_egg_input_configure(struct w2c_env *env) {
    return egg_input_configure();
  }
  
  void w2c_env_egg_play_sound(struct w2c_env *env,int rid,int index) {
    egg_play_sound(rid,index);
  }
  
  void w2c_env_egg_play_song(struct w2c_env *env,int rid,int force,int repeat) {
    egg_play_song(rid,force,repeat);
  }
  
  void w2c_env_egg_audio_event(struct w2c_env *env,int chid,int opcode,int a,int b,int durms) {
    egg_audio_event(chid,opcode,a,b,durms);
  }
  
  double w2c_env_egg_audio_get_playhead(struct w2c_env *env) {
    return egg_audio_get_playhead();
  }
  
  void w2c_env_egg_audio_set_playhead(struct w2c_env *env,double s) {
    egg_audio_set_playhead(s);
  }
  
  int w2c_env_egg_image_decode_header(struct w2c_env *env,uint32_t wp,uint32_t hp,uint32_t pixelsizep,uint32_t srcp,int srcc) {
    int *w=HOSTADDR(wp,4);
    int *h=HOSTADDR(hp,4);
    int *pixelsize=HOSTADDR(pixelsizep,4);
    void *src=HOSTADDR(srcp,srcc);
    return egg_image_decode_header(w,h,pixelsize,src,srcc);
  }
  
  int w2c_env_egg_image_decode(struct w2c_env *env,uint32_t dstp,int dsta,uint32_t srcp,int srcc) {
    void *dst=HOSTADDR(dstp,dsta);
    void *src=HOSTADDR(srcp,srcc);
    return egg_image_decode(dst,dsta,src,srcc);
  }
  
  void w2c_env_egg_texture_del(struct w2c_env *env,int texid) {
    egg_texture_del(texid);
  }
  
  int w2c_env_egg_texture_new(struct w2c_env *env) {
    return egg_texture_new();
  }
  
  int w2c_env_egg_texture_get_status(struct w2c_env *env,uint32_t wp,uint32_t hp,int texid) {
    int *w=HOSTADDR(wp,4);
    int *h=HOSTADDR(hp,4);
    return egg_texture_get_status(w,h,texid);
  }
  
  int w2c_env_egg_texture_get_pixels(struct w2c_env *env,uint32_t dstp,int dsta,int texid) {
    void *dst=HOSTADDR(dstp,dsta);
    return egg_texture_get_pixels(dst,dsta,texid);
  }
  
  int w2c_env_egg_texture_load_image(struct w2c_env *env,int texid,int rid) {
    return egg_texture_load_image(texid,rid);
  }
  
  int w2c_env_egg_texture_load_serial(struct w2c_env *env,int texid,uint32_t srcp,int srcc) {
    const void *src=HOSTADDR(srcp,srcc);
    return egg_texture_load_serial(texid,src,srcc);
  }
  
  int w2c_env_egg_texture_load_raw(struct w2c_env *env,int texid,int fmt,int w,int h,int stride,uint32_t srcp,int srcc) {
    const void *src=HOSTADDR(srcp,srcc);
    return egg_texture_load_raw(texid,fmt,w,h,stride,src,srcc);
  }
  
  void w2c_env_egg_draw_globals(struct w2c_env *env,int tint,int alpha) {
    egg_draw_globals(tint,alpha);
  }
  
  void w2c_env_egg_draw_clear(struct w2c_env *env,int dsttexid,uint32_t rgba) {
    egg_draw_clear(dsttexid,rgba);
  }
  
  void w2c_env_egg_draw_line(struct w2c_env *env,int dsttexid,uint32_t vp,int c) {
    void *v=HOSTADDR(vp,sizeof(struct egg_draw_line)*c);
    egg_draw_line(dsttexid,v,c);
  }
  
  void w2c_env_egg_draw_rect(struct w2c_env *env,int dsttexid,uint32_t vp,int c) {
    void *v=HOSTADDR(vp,sizeof(struct egg_draw_rect)*c);
    egg_draw_rect(dsttexid,v,c);
  }
  
  void w2c_env_egg_draw_trig(struct w2c_env *env,int dsttexid,uint32_t vp,int c) {
    void *v=HOSTADDR(vp,sizeof(struct egg_draw_trig)*c);
    egg_draw_trig(dsttexid,v,c);
  }
  
  void w2c_env_egg_draw_decal(struct w2c_env *env,int dsttexid,int srctexid,uint32_t vp,int c) {
    void *v=HOSTADDR(vp,sizeof(struct egg_draw_decal)*c);
    egg_draw_decal(dsttexid,srctexid,v,c);
  }
  
  void w2c_env_egg_draw_tile(struct w2c_env *env,int dsttexid,int srctexid,uint32_t vp,int c) {
    void *v=HOSTADDR(vp,sizeof(struct egg_draw_tile)*c);
    egg_draw_tile(dsttexid,srctexid,v,c);
  }
  
  void w2c_env_egg_draw_mode7(struct w2c_env *env,int dsttexid,int srctexid,uint32_t vp,int c) {
    void *v=HOSTADDR(vp,sizeof(struct egg_draw_mode7)*c);
    egg_draw_mode7(dsttexid,srctexid,v,c);
  }

#endif

/* Quit.
 */
 
void eggrt_exec_quit() {
  #if EXECFMT==WASM
    if (eggrt_wasm.ee) wasm_runtime_destroy_exec_env(eggrt_wasm.ee);
    if (eggrt_wasm.inst) wasm_runtime_deinstantiate(eggrt_wasm.inst);
    if (eggrt_wasm.mod) wasm_module_delete(&eggrt_wasm.mod);
    if (eggrt_wasm.code) free(eggrt_wasm.code);
  #elif EXECFMT==RECOM
    wasm2c_mm_free(&eggrt_w2c.mod);
    wasm_rt_free();
  #endif
}

/* Init.
 */
 
int eggrt_exec_init() {

  #if EXECFMT==WASM
    {
      void *src=0;
      int srcc=eggrt_rom_get(&src,EGG_TID_code,1);
      if (srcc<1) {
        fprintf(stderr,"%s: code:1 not found\n",eggrt.rptname);
        return -2;
      }
      if (!(eggrt_wasm.code=malloc(srcc))) return -1;
      memcpy(eggrt_wasm.code,src,srcc);
      eggrt_wasm.codec=srcc;
    }
  
    if (!wasm_runtime_init()) return -1;
    if (!wasm_runtime_register_natives("env",eggrt_wasm_exports,sizeof(eggrt_wasm_exports)/sizeof(NativeSymbol))) return -1;
  
    int stack_size=0x01000000;//TODO
    int heap_size=0x01000000;//TODO
    char msg[1024]={0};
    if (!(eggrt_wasm.mod=wasm_runtime_load(eggrt_wasm.code,eggrt_wasm.codec,msg,sizeof(msg)))) {
      fprintf(stderr,"%s: wasm_runtime_load failed: %s\n",eggrt.rptname,msg);
      return -2;
    }
    if (!(eggrt_wasm.inst=wasm_runtime_instantiate(eggrt_wasm.mod,stack_size,heap_size,msg,sizeof(msg)))) {
      fprintf(stderr,"%s: wasm_runtime_instantiate failed: %s\n",eggrt.rptname,msg);
      return -2;
    }
    if (!(eggrt_wasm.ee=wasm_runtime_create_exec_env(eggrt_wasm.inst,stack_size))) {
      fprintf(stderr,"%s: wasm_runtime_create_exec_env failed\n",eggrt.rptname);
      return -2;
    }
  
    #define LOADFN(name) { \
      if (!(eggrt_wasm.name=wasm_runtime_lookup_function(eggrt_wasm.inst,#name))) { \
        fprintf(stderr,"%s: ROM does not export required function '%s'\n",eggrt.rptname,#name); \
        return -2; \
      } \
    }
    LOADFN(egg_client_quit)
    LOADFN(egg_client_init)
    LOADFN(egg_client_update)
    LOADFN(egg_client_render)
    #undef LOADFN
    
  #elif EXECFMT==RECOM
    wasm_rt_init();
    wasm2c_mm_instantiate(&eggrt_w2c.mod);
    
  #endif
  return 0;
}

/* Call client hooks.
 */
 
#if EXECFMT==NATIVE

void eggrt_exec_client_quit(int status) {
  if (eggrt.exec_callstate!=1) return;
  eggrt.exec_callstate=2;
  egg_client_quit(status);
}

int eggrt_exec_client_init() {
  if (eggrt.exec_callstate!=0) return -1;
  eggrt.exec_callstate=1;
  return egg_client_init();
}

int eggrt_exec_client_update(double elapsed) {
  if (eggrt.exec_callstate!=1) return -1;
  egg_client_update(elapsed);
  return 0;
}

int eggrt_exec_client_render() {
  if (eggrt.exec_callstate!=1) return -1;
  egg_client_render();
  return 0;
}

#elif EXECFMT==WASM

void eggrt_exec_client_quit(int status) {
  if (eggrt.exec_callstate!=1) return;
  eggrt.exec_callstate=2;
  uint32_t argv[1]={eggrt.exitstatus};
  if (!wasm_runtime_call_wasm(eggrt_wasm.ee,eggrt_wasm.egg_client_quit,1,argv)) {
    const char *msg=wasm_runtime_get_exception(eggrt_wasm.inst);
    fprintf(stderr,"%s: egg_client_quit failed hard: %s\n",eggrt.rptname,msg);
  }
}

int eggrt_exec_client_init() {
  if (eggrt.exec_callstate!=0) return -1;
  eggrt.exec_callstate=1;
  uint32_t argv[1]={0};
  if (wasm_runtime_call_wasm(eggrt_wasm.ee,eggrt_wasm.egg_client_init,0,argv)) {
    int result=argv[0];
    if (result<0) {
      fprintf(stderr,"%s: Error %d from egg_client_init\n",eggrt.rptname,result);
      return -2;
    }
  } else {
    const char *msg=wasm_runtime_get_exception(eggrt_wasm.inst);
    fprintf(stderr,"%s: egg_client_init failed hard: %s\n",eggrt.rptname,msg);
    return -2;
  }
  return 0;
}

int eggrt_exec_client_update(double elapsed) {
  if (eggrt.exec_callstate!=1) return -1;
  uint32_t argv[2]={0,0};
  memcpy(argv,&elapsed,sizeof(double));
  if (wasm_runtime_call_wasm(eggrt_wasm.ee,eggrt_wasm.egg_client_update,2,argv)) return 0;
  const char *msg=wasm_runtime_get_exception(eggrt_wasm.inst);
  fprintf(stderr,"%s: egg_client_update failed hard: %s\n",eggrt.rptname,msg);
  return -2;
}

int eggrt_exec_client_render() {
  if (eggrt.exec_callstate!=1) return -1;
  uint32_t argv[1]={0};
  if (!wasm_runtime_call_wasm(eggrt_wasm.ee,eggrt_wasm.egg_client_render,0,argv)) {
    const char *msg=wasm_runtime_get_exception(eggrt_wasm.inst);
    fprintf(stderr,"%s: egg_client_render failed hard: %s\n",eggrt.rptname,msg);
    return -2;
  }
  return 0;
}

#elif EXECFMT==RECOM

void eggrt_exec_client_quit(int status) {
  if (eggrt.exec_callstate!=1) return;
  eggrt.exec_callstate=2;
  w2c_mm_egg_client_quit(&eggrt_w2c.mod,status);
}

int eggrt_exec_client_init() {
  if (eggrt.exec_callstate!=0) return -1;
  eggrt.exec_callstate=1;
  int err=w2c_mm_egg_client_init(&eggrt_w2c.mod);
  if (err<0) {
    fprintf(stderr,"%s: Error %d from egg_client_init\n",eggrt.rptname,err);
    return -2;
  }
  return 0;
}

int eggrt_exec_client_update(double elapsed) {
  if (eggrt.exec_callstate!=1) return -1;
  w2c_mm_egg_client_update(&eggrt_w2c.mod,elapsed);
  return 0;
}

int eggrt_exec_client_render() {
  if (eggrt.exec_callstate!=1) return -1;
  w2c_mm_egg_client_render(&eggrt_w2c.mod);
  return 0;
}

#endif
