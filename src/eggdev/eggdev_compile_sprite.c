#include "eggdev_internal.h"

/* Sprite binary from text.
 */
 
static int eggdev_sprite_bin_from_text(struct sr_encoder *dst,const char *src,int srcc,const char *path,struct eggdev_rom *rom) {
  int err;
  if (sr_encode_raw(dst,"\0ESP",4)<0) return -1;
  const struct eggdev_command_list_schema *schema=0;
  int schemac=eggdev_command_list_schema_lookup(&schema,EGG_TID_sprite,rom);
  if ((err=eggdev_command_list_compile(dst,src,srcc,path,1,schema,schemac,rom))<0) return err;
  return 0;
}

/* Sprite text from binary.
 */
 
static int eggdev_sprite_text_from_bin(struct sr_encoder *dst,const uint8_t *src,int srcc,const char *path,struct eggdev_rom *rom) {
  int err;
  if (!src||(srcc<4)||memcmp(src,"\0ESP",4)) return -1;
  const struct eggdev_command_list_schema *schema=0;
  int schemac=eggdev_command_list_schema_lookup(&schema,EGG_TID_sprite,rom);
  if ((err=eggdev_command_list_uncompile(dst,src+4,srcc-4,path,schema,schemac,rom))<0) return err;
  return 0;
}

/* Sprite resource, main entry point.
 */
 
int eggdev_compile_sprite(struct eggdev_res *res,struct eggdev_rom *rom) {
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0ESP",4)) return 0;
  struct sr_encoder dst={0};
  int err=eggdev_sprite_bin_from_text(&dst,res->serial,res->serialc,res->path,rom);
  if (err<0) {
    sr_encoder_cleanup(&dst);
    return err;
  }
  eggdev_res_handoff_serial(res,dst.v,dst.c);
  return 0;
}

int eggdev_uncompile_sprite(struct eggdev_res *res,struct eggdev_rom *rom) {
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0ESP",4)) {
    struct sr_encoder dst={0};
    int err=eggdev_sprite_text_from_bin(&dst,res->serial,res->serialc,res->path,rom);
    if (err<0) {
      sr_encoder_cleanup(&dst);
      return err;
    }
    eggdev_res_handoff_serial(res,dst.v,dst.c);
    return 0;
  }
  // Assume that anything else is already in text format.
  return 0;
}
