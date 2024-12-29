#include "eggdev_internal.h"
#include <stdarg.h>
#include <unistd.h>

/* Helper, wrapper around system().
 */
 
static int eggdev_run_shell(const char *fmt,...) {
  va_list vargs;
  va_start(vargs,fmt);
  char cmd[4096];
  int cmdc=vsnprintf(cmd,sizeof(cmd),fmt,vargs);
  if ((cmdc<1)||(cmdc>=sizeof(cmd))) return -1;
  int err=system(cmd);
  if (err) {
    fprintf(stderr,"%s: Shell command failed:\n  %s\n",eggdev.exename,cmd);
    return -1;
  }
  return 0;
}

/* Context for holding paths and such.
 */
 
struct eggdev_bundle_context {
  const char *dstpath;
  const char *rompath;
  const char *clientlibpath; // true
  char *modrompath; // If we're rewriting the ROM. true,recom
  char *asmpath; // Assembly bootstrap to incbin it. all
  char *objpath; // Output of that assembly. all
  char *cpath; // Text from wasm2c. recom
  char *cobjpath; // Compiled object from wasm2c. recom
  struct eggdev_rom rom;
  struct sr_encoder scratch;
};

// Our compilers usually use -MMD and produce a '.d' file adjacent to the '.o'.
// Instead of fixing it right, by filtering out the -MMD option, just try deleting '.d' always.
// Also trying '.h', wasm2c creates that unbidden.
// This modifies (path) in place.
static void unlink_d(char *path) {
  if (!path) return;
  int c=0; while (path[c]) c++;
  if (c<2) return;
  if (memcmp(path+c-2,".o",2)) return;
  path[c-1]='d';
  unlink(path);
  path[c-1]='h';
  unlink(path);
}

static void eggdev_bundle_context_cleanup(struct eggdev_bundle_context *ctx) {
  if (ctx->modrompath) {
    unlink(ctx->modrompath);
    free(ctx->modrompath);
  }
  if (ctx->asmpath) {
    unlink(ctx->asmpath);
    free(ctx->asmpath);
  }
  if (ctx->objpath) {
    unlink(ctx->objpath);
    unlink_d(ctx->objpath);
    free(ctx->objpath);
  }
  if (ctx->cpath) {
    unlink(ctx->cpath);
    free(ctx->cpath);
  }
  if (ctx->cobjpath) {
    unlink(ctx->cobjpath);
    unlink_d(ctx->cobjpath);
    free(ctx->cobjpath);
  }
  eggdev_rom_cleanup(&ctx->rom);
  sr_encoder_cleanup(&ctx->scratch);
}

/* Generate temporary paths.
 */
 
static char *eggdev_path_append(const char *a,const char *b) {
  int ac=0; while (a[ac]) ac++;
  int bc=0; while (b[bc]) bc++;
  int cc=ac+bc;
  char *c=malloc(cc+1);
  if (!c) return 0;
  memcpy(c,a,ac);
  memcpy(c+ac,b,bc);
  c[cc]=0;
  return c;
}

static int eggdev_bundle_add_assembly_paths(struct eggdev_bundle_context *ctx) {
  if (!(ctx->asmpath=eggdev_path_append(ctx->rompath,".s"))) return -1;
  if (!(ctx->objpath=eggdev_path_append(ctx->rompath,".o"))) return -1;
  return 0;
}

static int eggdev_bundle_add_modrompath(struct eggdev_bundle_context *ctx) {
  if (!(ctx->modrompath=eggdev_path_append(ctx->rompath,".mod"))) return -1;
  return 0;
}

static int eggdev_bundle_add_wasm2c_paths(struct eggdev_bundle_context *ctx) {
  if (!(ctx->cpath=eggdev_path_append(ctx->rompath,".w2c.c"))) return -1;
  if (!(ctx->cobjpath=eggdev_path_append(ctx->rompath,".w2c.o"))) return -1;
  return 0;
}

/* Read and decode the ROM file into context.
 */
 
static int eggdev_bundle_acquire_rom(struct eggdev_bundle_context *ctx) {
  return eggdev_rom_add_path(&ctx->rom,ctx->rompath);
}

/* Write the assembly file.
 */
 
static int eggdev_bundle_generate_asm(struct eggdev_bundle_context *ctx) {
  const char *rompath=ctx->modrompath?ctx->modrompath:ctx->rompath;
  ctx->scratch.c=0;
  if (sr_encode_fmt(&ctx->scratch,
    #if USE_macos || USE_mswin
      // Apparently MacOS ld needs a leading underscore to find these.
      ".globl _egg_embedded_rom,_egg_embedded_rom_size\n"
      "_egg_embedded_rom:\n"
      ".incbin \"%s\"\n"
      "_egg_embedded_rom_size:\n"
      ".int (_egg_embedded_rom_size-_egg_embedded_rom)\n"
    #else
      ".globl egg_embedded_rom,egg_embedded_rom_size\n"
      "egg_embedded_rom:\n"
      ".incbin \"%s\"\n"
      "egg_embedded_rom_size:\n"
      ".int (egg_embedded_rom_size-egg_embedded_rom)\n"
    #endif
  ,rompath)<0) return -1;
  if (file_write(ctx->asmpath,ctx->scratch.v,ctx->scratch.c)<0) {
    fprintf(stderr,"%s: Failed to write intermediate assembly file, %d bytes\n",ctx->asmpath,ctx->scratch.c);
    return -2;
  }
  return 0;
}

/* Assemble the linkable ROM object.
 * It's just an incbin and some exports, the actual work here is trivial.
 * But we're pretty far abstracted out, lots of configuration type stuff can fail around here.
 */
 
static int eggdev_bundle_assemble(struct eggdev_bundle_context *ctx) {
  return eggdev_run_shell(
    "%s -o%s %s",
    eggdev_buildcfg.AS,ctx->objpath,ctx->asmpath
  );
}

/* Drop code:1, reencode the ROM, and stash it in (modrompath).
 * We're allowed to damage (rom) along the way.
 */
 
static int eggdev_bundle_rewrite_rom_without_code(struct eggdev_bundle_context *ctx) {
  if (!ctx->modrompath) return -1;
  // It's supposed to be the second entry. But search generically anyway.
  int p=eggdev_rom_search(&ctx->rom,EGG_TID_code,1);
  if (p>=0) {
    ctx->rom.resv[p].serialc=0;
  }
  int err=eggdev_rom_validate(&ctx->rom);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Rewritten ROM failed validation.\n",ctx->modrompath);
    return -2;
  }
  ctx->scratch.c=0;
  if ((err=eggdev_rom_encode(&ctx->scratch,&ctx->rom))<0) {
    if (err!=-2) fprintf(stderr,"%s: Failed to reencode ROM.\n",ctx->modrompath);
    return -2;
  }
  if (file_write(ctx->modrompath,ctx->scratch.v,ctx->scratch.c)<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes\n",ctx->modrompath,ctx->scratch.c);
    return -2;
  }
  return 0;
}

/* True native.
 */
 
static int eggdev_bundle_true(struct eggdev_bundle_context *ctx) {
  int err;
  if ((err=eggdev_bundle_add_modrompath(ctx))<0) return err;
  if ((err=eggdev_bundle_rewrite_rom_without_code(ctx))<0) return err;
  if ((err=eggdev_bundle_generate_asm(ctx))<0) return err;
  if ((err=eggdev_bundle_assemble(ctx))<0) return err;
  if ((err=eggdev_run_shell(
    #if USE_macos
      // MacOS ld doesn't have --start-group.
      "%s -ObjC -o%s %s %s/out/%s/libegg-true.a %s %s",
    #else
      "%s -o%s -Wl,--start-group %s %s/out/%s/libegg-true.a %s -Wl,--end-group %s",
    #endif
    eggdev_buildcfg.LD,
    ctx->dstpath,
    ctx->objpath,
    eggdev_buildcfg.EGG_SDK,
    eggdev_buildcfg.NATIVE_TARGET,
    ctx->clientlibpath,
    eggdev_buildcfg.LDPOST
  ))<0) return err;
  return 0;
}

/* Fake native.
 */
 
static int eggdev_bundle_fake(struct eggdev_bundle_context *ctx) {
  // I would prefer to do this with 'objcopy' rather than this awkward assembly dance.
  // But objcopy doesn't give us sufficient control over the object's exported name.
  int err;
  if ((err=eggdev_buildcfg_assert(WAMR_SDK))<0) return err;
  if ((err=eggdev_bundle_generate_asm(ctx))<0) return err;
  if ((err=eggdev_bundle_assemble(ctx))<0) return err;
  if ((err=eggdev_run_shell(
    "%s -o%s %s %s/out/%s/libegg-fake.a %s %s/build/libvmlib.a",
    eggdev_buildcfg.LD,
    ctx->dstpath,
    ctx->objpath,
    eggdev_buildcfg.EGG_SDK,
    eggdev_buildcfg.NATIVE_TARGET,
    eggdev_buildcfg.LDPOST,
    eggdev_buildcfg.WAMR_SDK
  ))<0) return err;
  return 0;
}

/* Pipe code:1 thru WABT's wasm2c into a temporary C file, then compile that.
 * It is of course possible to do this without the intermediate C file.
 * I'm keeping it this way so we can interrupt and examine the C, and also because it makes the gcc invocation a bit simpler.
 */
 
static int eggdev_bundle_wasm2c(struct eggdev_bundle_context *ctx) {
  int resp=eggdev_rom_search(&ctx->rom,EGG_TID_code,1);
  if (resp<0) {
    fprintf(stderr,"%s: code:1 not found\n",ctx->rompath);
    return -2;
  }
  const uint8_t *src=ctx->rom.resv[resp].serial;
  int srcc=ctx->rom.resv[resp].serialc;
  
  char cmd[1024];
  int cmdc=snprintf(cmd,sizeof(cmd),
    "%s/bin/wasm2c - -o %s --module-name=mm",
    eggdev_buildcfg.WABT_SDK,
    ctx->cpath
  );
  if ((cmdc<1)||(cmdc>=sizeof(cmd))) return -1;
  FILE *pipe=popen(cmd,"w");
  if (!pipe) {
    fprintf(stderr,"%s: Failed to open pipe for Wasm decompilation:\n%s\n",eggdev.exename,cmd);
    return -2;
  }
  
  int srcp=0;
  while (srcp<srcc) {
    int err=fwrite(src+srcp,1,srcc-srcp,pipe);
    if (err<=0) {
      fprintf(stderr,"%s: Writing to Wasm decompilation pipe failed:\n%s\n",eggdev.exename,cmd);
      pclose(pipe);
      return -2;
    }
    srcp+=err;
  }
  pclose(pipe);
  
  return eggdev_run_shell(
    "%s -o%s %s -I%s/wasm2c -I%s/third_party/wasm-c-api/include",
    eggdev_buildcfg.CC,
    ctx->cobjpath,
    ctx->cpath,
    eggdev_buildcfg.WABT_SDK,
    eggdev_buildcfg.WABT_SDK
  );
}

/* Recompile.
 */
 
static int eggdev_bundle_recompile(struct eggdev_bundle_context *ctx) {
  int err;
  if ((err=eggdev_buildcfg_assert(WABT_SDK))<0) return err;
  if ((err=eggdev_buildcfg_assert(CC))<0) return err;
  if ((err=eggdev_bundle_add_modrompath(ctx))<0) return err;
  if ((err=eggdev_bundle_add_wasm2c_paths(ctx))<0) return err;
  if ((err=eggdev_bundle_wasm2c(ctx))<0) return err; // Must do before rewriting ROM.
  if ((err=eggdev_bundle_rewrite_rom_without_code(ctx))<0) return err;
  if ((err=eggdev_bundle_generate_asm(ctx))<0) return err;
  if ((err=eggdev_bundle_assemble(ctx))<0) return err;
  if ((err=eggdev_run_shell(
    "%s -o%s %s %s %s/out/%s/libegg-recom.a %s",
    eggdev_buildcfg.LD,
    ctx->dstpath,
    ctx->objpath,
    ctx->cobjpath,
    eggdev_buildcfg.EGG_SDK,
    eggdev_buildcfg.NATIVE_TARGET,
    eggdev_buildcfg.LDPOST
  ))<0) return err;
  return 0;
}

/* Main entry point, all three variations.
 */
 
int eggdev_bundle_native() {
  struct eggdev_bundle_context ctx={0};
  if (!eggdev.dstpath) {
    fprintf(stderr,"%s: Please specify output path as '-oPATH'\n",eggdev.exename);
    return -2;
  }
  ctx.dstpath=eggdev.dstpath;
  if (!eggdev.srcpathc) {
    fprintf(stderr,"%s: ROM file required\n",eggdev.exename);
    return -2;
  }
  ctx.rompath=eggdev.srcpathv[0];
  int err=-1;
  
  if ((err=eggdev_bundle_add_assembly_paths(&ctx))<0) {
  } else if ((err=eggdev_bundle_acquire_rom(&ctx))<0) {
  } else if ((err=eggdev_buildcfg_assert(LD))<0) {
  } else if ((err=eggdev_buildcfg_assert(EGG_SDK))<0) {
  } else if ((err=eggdev_buildcfg_assert(NATIVE_TARGET))<0) {
  } else if ((err=eggdev_buildcfg_assert(AS))<0) {
  
  } else if (eggdev.srcpathc==2) {
    if (eggdev.recompile) {
      fprintf(stderr,"%s: '--recompile' and native library are mutually exclusive\n",eggdev.exename);
      err=-2;
    } else {
      ctx.clientlibpath=eggdev.srcpathv[1];
      err=eggdev_bundle_true(&ctx);
    }
    
  } else if (eggdev.srcpathc!=1) {
    fprintf(stderr,"%s: Unexpected extra inputs to 'bundle'\n",eggdev.exename);
    err=-2;
    
  } else if (eggdev.recompile) {
    err=eggdev_bundle_recompile(&ctx);
    
  } else {
    err=eggdev_bundle_fake(&ctx);
  }
  
  eggdev_bundle_context_cleanup(&ctx);
  return err;
}
