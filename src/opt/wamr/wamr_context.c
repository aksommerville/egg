#include "wamr_internal.h"

/* Delete.
 */
 
static void wamr_function_cleanup(struct wamr_function *function) {
}

static void wamr_module_cleanup(struct wamr_module *module) {
  if (module->functionv) {
    while (module->functionc-->0) {
      wamr_function_cleanup(module->functionv+module->functionc);
    }
    free(module->functionv);
  }
  if (module->ee) wasm_runtime_destroy_exec_env(module->ee);
  if (module->instance) wasm_runtime_deinstantiate(module->instance);
  if (module->module) wasm_module_delete(&module->module);
}

void wamr_del(struct wamr *wamr) {
  if (!wamr) return;
  if (wamr->modulev) {
    while (wamr->modulec-->0) {
      wamr_module_cleanup(wamr->modulev+wamr->modulec);
    }
    free(wamr->modulev);
  }
  free(wamr);
}

/* New.
 */

struct wamr *wamr_new() {
  if (!wasm_runtime_init()) return 0;
  struct wamr *wamr=calloc(1,sizeof(struct wamr));
  if (!wamr) return 0;
  return wamr;
}

/* Set exports.
 */

int wamr_set_exports(struct wamr *wamr,void *symbolv,int symbolc) {
  if (!wasm_runtime_register_natives("env",symbolv,symbolc)) return -1;
  return 0;
}

/* Add module.
 */

int wamr_add_module(struct wamr *wamr,int modid,void *src,int srcc,const char *refname) {
  
  if (wamr->modulec>=wamr->modulea) {
    int na=wamr->modulea+8;
    if (na>INT_MAX/sizeof(struct wamr_module)) return -1;
    void *nv=realloc(wamr->modulev,sizeof(struct wamr_module)*na);
    if (!nv) return -1;
    wamr->modulev=nv;
    wamr->modulea=na;
  }
  struct wamr_module *module=wamr->modulev+wamr->modulec++;
  memset(module,0,sizeof(struct wamr_module));
  module->modid=modid;
  
  int stack_size=0x01000000;
  module->heap_size=0x01000000;
  char msg[1024]={0};
  if (!(module->module=wasm_runtime_load(src,srcc,msg,sizeof(msg)))) {
    if (refname) fprintf(stderr,"%s:wasm_runtime_load: %s\n",refname,msg);
    wamr->modulec--;
    wamr_module_cleanup(module);
    return -1;
  }
  if (!(module->instance=wasm_runtime_instantiate(module->module,stack_size,module->heap_size,msg,sizeof(msg)))) {
    if (refname) fprintf(stderr,"%s:wasm_runtime_instantiate: %s\n",refname,msg);
    wamr->modulec--;
    wamr_module_cleanup(module);
    return -1;
  }
  if (!(module->ee=wasm_runtime_create_exec_env(module->instance,stack_size))) {
    if (refname) fprintf(stderr,"%s:wasm_runtime_create_exec_env\n",refname);
    wamr->modulec--;
    wamr_module_cleanup(module);
    return -1;
  }
  
  return 0;
}

/* Link function.
 */
 
int wamr_link_function(struct wamr *wamr,int modid,int fnid,const char *name) {
  struct wamr_module *module=wamr->modulev;
  int modulei=wamr->modulec;
  for (;modulei-->0;module++) {
    if (module->modid!=modid) continue;
    
    if (module->functionc>=module->functiona) {
      int na=module->functiona+8;
      if (na>INT_MAX/sizeof(struct wamr_function)) return -1;
      void *nv=realloc(module->functionv,sizeof(struct wamr_function)*na);
      if (!nv) return -1;
      module->functionv=nv;
      module->functiona=na;
    }
    struct wamr_function *function=module->functionv+module->functionc++;
    memset(function,0,sizeof(struct wamr_function));
    function->fnid=fnid;
    
    if (!(function->finst=wasm_runtime_lookup_function(module->instance,name))) {
      module->functionc--;
      wamr_function_cleanup(function);
      return -1;
    }
    
    return 0;
  }
  return -1;
}

/* Call function.
 */

int wamr_call(struct wamr *wamr,int modid,int fnid,uint32_t *argv,int argc) {
  struct wamr_module *module=wamr->modulev;
  int modulei=wamr->modulec;
  for (;modulei-->0;module++) {
    if (module->modid!=modid) continue;
    struct wamr_function *function=module->functionv;
    int functioni=module->functionc;
    for (;functioni-->0;function++) {
      if (function->fnid!=fnid) continue;
      
      if (wasm_runtime_call_wasm(module->ee,function->finst,argc,argv)) {
        return 0;
      } else {
        return -1;
      }
    }
    return -1;
  }
  return -1;
}

/* Validate pointer.
 */

void *wamr_validate_pointer(struct wamr *wamr,int modid,uint32_t waddr,int reqc) {
  struct wamr_module *module=wamr->modulev;
  int modulei=wamr->modulec;
  for (;modulei-->0;module++) {
    if (module->modid!=modid) continue;
    if (wasm_runtime_validate_app_addr(module->instance,waddr,reqc)) {
      return wasm_runtime_addr_app_to_native(module->instance,waddr);
    } else {
      return 0;
    }
  }
  return 0;
}

int wamr_get_full_heap(void *dstpp,struct wamr *wamr,int modid) {
  struct wamr_module *module=wamr->modulev;
  int modulei=wamr->modulec;
  for (;modulei-->0;module++) {
    if (module->modid!=modid) continue;
    if (!wasm_runtime_validate_app_addr(module->instance,0,module->heap_size)) return -1;
    if (!(*(void**)dstpp=wasm_runtime_addr_app_to_native(module->instance,0))) return -1;
    return module->heap_size;
  }
  return -1;
}

/* Translate pointer to client space.
 */
 
int wamr_host_to_client_pointer(struct wamr *wamr,int modid,const void *src) {
  if (!src) return 0;
  struct wamr_module *module=wamr->modulev;
  int modulei=wamr->modulec;
  for (;modulei-->0;module++) {
    if (module->modid!=modid) continue;
    return wasm_runtime_addr_native_to_app(module->instance,(void*)src);
  }
  return 0;
}

/* Call something in the function table.
 */
                           
int wamr_call_table(struct wamr *wamr,int fnid,uint32_t *argv,int argc) {
  if (wamr->modulec<1) return -1;
  if (!wasm_runtime_call_indirect(wamr->modulev[0].ee,fnid,argc,argv)) {
    // Errors here appear to kill the runtime altogether. TODO Is it possible to recover?
    fprintf(stderr,"wasm_runtime_call_indirect failed! Could be caused by incorrect parameter count to a callback function.\n");
    return -1;
  }
  return 0;
}
