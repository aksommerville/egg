#include "graf.h"
#include "egg/egg.h"

/* Reset.
 */
 
void graf_reset(struct graf *graf) {
  graf->gtint=graf->tint=0;
  graf->galpha=graf->alpha=0xff;
  graf->dsttexid=1;
  graf->srctexid=0;
  graf->mode=0;
  graf->vtxc=0;
}

/* Flush.
 */
 
void graf_flush(struct graf *graf) {
  if (!graf->vtxc) return;
  if ((graf->gtint!=graf->tint)||(graf->galpha!=graf->alpha)) {
    egg_draw_globals(graf->tint,graf->alpha);
    graf->gtint=graf->tint;
    graf->galpha=graf->alpha;
  }
       if (graf->mode==egg_draw_line ) egg_draw_line (graf->dsttexid,(void*)graf->vtxv,graf->vtxc/sizeof(struct egg_draw_line));
  else if (graf->mode==egg_draw_rect ) egg_draw_rect (graf->dsttexid,(void*)graf->vtxv,graf->vtxc/sizeof(struct egg_draw_rect));
  else if (graf->mode==egg_draw_trig ) egg_draw_trig (graf->dsttexid,(void*)graf->vtxv,graf->vtxc/sizeof(struct egg_draw_trig));
  else if (graf->mode==egg_draw_decal) egg_draw_decal(graf->dsttexid,graf->srctexid,(void*)graf->vtxv,graf->vtxc/sizeof(struct egg_draw_decal));
  else if (graf->mode==egg_draw_tile ) egg_draw_tile (graf->dsttexid,graf->srctexid,(void*)graf->vtxv,graf->vtxc/sizeof(struct egg_draw_tile));
  else if (graf->mode==egg_draw_mode7) egg_draw_mode7(graf->dsttexid,graf->srctexid,(void*)graf->vtxv,graf->vtxc/sizeof(struct egg_draw_mode7),graf->interpolate);
  graf->mode=0;
  graf->vtxc=0;
  graf->srctexid=0;
}

/* Globals.
 */

void graf_set_output(struct graf *graf,int dsttexid) {
  if (graf->dsttexid==dsttexid) return;
  graf_flush(graf);
  graf->dsttexid=dsttexid;
}

void graf_set_tint(struct graf *graf,uint32_t rgba) {
  if (graf->tint==rgba) return;
  graf_flush(graf);
  graf->tint=rgba;
}

void graf_set_alpha(struct graf *graf,uint8_t a) {
  if (graf->alpha==a) return;
  graf_flush(graf);
  graf->alpha=a;
}

/* Add command.
 */

void graf_draw_line(struct graf *graf,int16_t ax,int16_t ay,int16_t bx,int16_t by,uint32_t rgba) {
  if (graf->mode&&(graf->mode!=egg_draw_line)) graf_flush(graf);
  else if (graf->vtxc>GRAF_BUFFER_SIZE-sizeof(struct egg_draw_line)) graf_flush(graf);
  graf->mode=egg_draw_line;
  struct egg_draw_line *vtx=(struct egg_draw_line*)(graf->vtxv+graf->vtxc);
  graf->vtxc+=sizeof(struct egg_draw_line);
  vtx->ax=ax;
  vtx->ay=ay;
  vtx->bx=bx;
  vtx->by=by;
  vtx->r=rgba>>24;
  vtx->g=rgba>>16;
  vtx->b=rgba>>8;
  vtx->a=rgba;
}

void graf_draw_rect(struct graf *graf,int16_t x,int16_t y,int16_t w,int16_t h,uint32_t rgba) {
  if (graf->mode&&(graf->mode!=egg_draw_rect)) graf_flush(graf);
  else if (graf->vtxc>GRAF_BUFFER_SIZE-sizeof(struct egg_draw_rect)) graf_flush(graf);
  graf->mode=egg_draw_rect;
  struct egg_draw_rect *vtx=(struct egg_draw_rect*)(graf->vtxv+graf->vtxc);
  graf->vtxc+=sizeof(struct egg_draw_rect);
  vtx->x=x;
  vtx->y=y;
  vtx->w=w;
  vtx->h=h;
  vtx->r=rgba>>24;
  vtx->g=rgba>>16;
  vtx->b=rgba>>8;
  vtx->a=rgba;
}

void graf_draw_trig(struct graf *graf,int16_t ax,int16_t ay,int16_t bx,int16_t by,int16_t cx,int16_t cy,uint32_t rgba) {
  if (graf->mode&&(graf->mode!=egg_draw_trig)) graf_flush(graf);
  else if (graf->vtxc>GRAF_BUFFER_SIZE-sizeof(struct egg_draw_trig)) graf_flush(graf);
  graf->mode=egg_draw_trig;
  struct egg_draw_trig *vtx=(struct egg_draw_trig*)(graf->vtxv+graf->vtxc);
  graf->vtxc+=sizeof(struct egg_draw_trig);
  vtx->ax=ax;
  vtx->ay=ay;
  vtx->bx=bx;
  vtx->by=by;
  vtx->cx=cx;
  vtx->cy=cy;
  vtx->r=rgba>>24;
  vtx->g=rgba>>16;
  vtx->b=rgba>>8;
  vtx->a=rgba;
}

void graf_draw_decal(struct graf *graf,int srctexid,int16_t dstx,int16_t dsty,int16_t srcx,int16_t srcy,int16_t w,int16_t h,uint8_t xform) {
  if (graf->mode&&(graf->mode!=egg_draw_decal)) graf_flush(graf);
  else if (graf->vtxc>GRAF_BUFFER_SIZE-sizeof(struct egg_draw_decal)) graf_flush(graf);
  else if (graf->srctexid!=srctexid) graf_flush(graf);
  graf->mode=egg_draw_decal;
  graf->srctexid=srctexid;
  struct egg_draw_decal *vtx=(struct egg_draw_decal*)(graf->vtxv+graf->vtxc);
  graf->vtxc+=sizeof(struct egg_draw_decal);
  vtx->dstx=dstx;
  vtx->dsty=dsty;
  vtx->srcx=srcx;
  vtx->srcy=srcy;
  vtx->w=w;
  vtx->h=h;
  vtx->xform=xform;
}

void graf_draw_tile(struct graf *graf,int srctexid,int16_t dstx,int16_t dsty,uint8_t tileid,uint8_t xform) {
  if (graf->mode&&(graf->mode!=egg_draw_tile)) graf_flush(graf);
  else if (graf->vtxc>GRAF_BUFFER_SIZE-sizeof(struct egg_draw_tile)) graf_flush(graf);
  else if (graf->srctexid!=srctexid) graf_flush(graf);
  graf->mode=egg_draw_tile;
  graf->srctexid=srctexid;
  struct egg_draw_tile *vtx=(struct egg_draw_tile*)(graf->vtxv+graf->vtxc);
  graf->vtxc+=sizeof(struct egg_draw_tile);
  vtx->dstx=dstx;
  vtx->dsty=dsty;
  vtx->tileid=tileid;
  vtx->xform=xform;
}

void graf_draw_tile_buffer(
  struct graf *graf,
  int srctexid,
  int16_t dstx,int16_t dsty,
  const uint8_t *tileidv,
  int colc,int rowc,int stride
) {
  if (graf->mode&&(graf->mode!=egg_draw_tile)) graf_flush(graf);
  else if (graf->srctexid!=srctexid) graf_flush(graf);
  int texw=0,texh=0;
  egg_texture_get_status(&texw,&texh,srctexid);
  int tilesize=texw>>4;
  graf->mode=egg_draw_tile;
  graf->srctexid=srctexid;
  int cellc=colc*rowc;
  int vtxa=(GRAF_BUFFER_SIZE-graf->vtxc)/sizeof(struct egg_draw_tile);
  if (cellc<=vtxa) {
    struct egg_draw_tile *vtx=(struct egg_draw_tile*)(graf->vtxv+graf->vtxc);
    const uint8_t *srcrow=tileidv;
    int yi=rowc,y=dsty;
    for (;yi-->0;srcrow+=stride,y+=tilesize) {
      const uint8_t *srcp=srcrow;
      int xi=colc,x=dstx;
      for (;xi-->0;srcp++,x+=tilesize,vtx++) {
        vtx->dstx=x;
        vtx->dsty=y;
        vtx->tileid=*srcp;
        vtx->xform=0;
      }
    }
    graf->vtxc+=cellc*sizeof(struct egg_draw_tile);
  } else {
    const uint8_t *srcrow=tileidv;
    int yi=rowc,y=dsty;
    for (;yi-->0;srcrow+=stride,y+=tilesize) {
      const uint8_t *srcp=srcrow;
      int xi=colc,x=dstx;
      for (;xi-->0;srcp++,x+=tilesize) {
        if (graf->vtxc>GRAF_BUFFER_SIZE-sizeof(struct egg_draw_tile)) graf_flush(graf);
        struct egg_draw_tile *vtx=(struct egg_draw_tile*)(graf->vtxv+graf->vtxc);
        vtx->dstx=x;
        vtx->dsty=y;
        vtx->tileid=*srcp;
        vtx->xform=0;
        graf->vtxc+=sizeof(struct egg_draw_tile);
      }
    }
  }
}

void graf_draw_mode7(
  struct graf *graf,
  int srctexid,
  int16_t dstx,int16_t dsty,
  int16_t srcx,int16_t srcy,
  int16_t w,int16_t h,
  float xscale,float yscale,
  float rotate,
  int interpolate
) {
  if (graf->mode&&(graf->mode!=egg_draw_mode7)) graf_flush(graf);
  else if (graf->vtxc>GRAF_BUFFER_SIZE-sizeof(struct egg_draw_mode7)) graf_flush(graf);
  else if (graf->srctexid!=srctexid) graf_flush(graf);
  interpolate=interpolate?1:0;
  if (graf->vtxc&&(interpolate!=graf->interpolate)) graf_flush(graf);
  graf->interpolate=interpolate;
  graf->mode=egg_draw_mode7;
  graf->srctexid=srctexid;
  struct egg_draw_mode7 *vtx=(struct egg_draw_mode7*)(graf->vtxv+graf->vtxc);
  graf->vtxc+=sizeof(struct egg_draw_mode7);
  vtx->dstx=dstx;
  vtx->dsty=dsty;
  vtx->srcx=srcx;
  vtx->srcy=srcy;
  vtx->w=w;
  vtx->h=h;
  vtx->xscale=xscale;
  vtx->yscale=yscale;
  vtx->rotate=rotate;
}
