#include "eggdev_internal.h"
#include "opt/sfgc/sfgc.h"

/* Main entry points.
 */
 
int eggdev_compile_sounds(struct eggdev_res *res) {
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0SFG",4)) return 0;
  {
    struct sr_encoder dst={0};
    int err=sfg_compile(&dst,res->serial,res->serialc,res->path);
    if (err<0) {
      sr_encoder_cleanup(&dst);
      return err;
    }
    eggdev_res_handoff_serial(res,dst.v,dst.c);
    return 0;
  }
  return 0;
}

int eggdev_uncompile_sounds(struct eggdev_res *res) {
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0SFG",4)) {
    struct sr_encoder dst={0};
    int err=sfg_uncompile(&dst,res->serial,res->serialc,res->path);
    if (err<0) {
      sr_encoder_cleanup(&dst);
      return err;
    }
    eggdev_res_handoff_serial(res,dst.v,dst.c);
    return 0;
  }
  return 0;
}
