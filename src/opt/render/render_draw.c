#include "render_internal.h"
#include <math.h>

/* raw
 */
 
static const char render_raw_vsrc[]=
  "uniform vec2 screensize;\n"
  "uniform vec4 tint;\n"
  "uniform float alpha;\n"
  "attribute vec2 apos;\n"
  "attribute vec4 acolor;\n"
  "varying vec4 vcolor;\n"
  "void main() {\n"
    "vec2 npos=((apos+0.25)*2.0)/screensize-1.0;\n"
    "gl_Position=vec4(npos,0.0,1.0);\n"
    "vcolor=vec4(mix(acolor.rgb,tint.rgb,tint.a),acolor.a*alpha);\n"
  "}\n"
"";

static const char render_raw_fsrc[]=
  "varying vec4 vcolor;\n"
  "void main() {\n"
    "gl_FragColor=vcolor;\n"
  "}\n"
"";

/* decal
 */
 
static const char render_decal_vsrc[]=
  "uniform vec2 screensize;\n"
  "attribute vec2 apos;\n"
  "attribute vec2 atexcoord;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
    "vec2 npos=(apos*2.0)/screensize-1.0;\n"
    "gl_Position=vec4(npos,0.0,1.0);\n"
    "vtexcoord=atexcoord;\n"
  "}\n"
"";

static const char render_decal_fsrc[]=
  "uniform sampler2D sampler;\n"
  "uniform float alpha;\n"
  "uniform vec4 texlimit;\n" // For mode7, establish firm boundaries in texture space.
  "uniform vec4 tint;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
    "if (vtexcoord.x<texlimit.x) discard;\n"
    "if (vtexcoord.y<texlimit.y) discard;\n"
    "if (vtexcoord.x>=texlimit.z) discard;\n"
    "if (vtexcoord.y>=texlimit.w) discard;\n"
    "gl_FragColor=texture2D(sampler,vtexcoord);\n"
    "gl_FragColor=vec4(mix(gl_FragColor.rgb,tint.rgb,tint.a),gl_FragColor.a*alpha);\n"
  "}\n"
"";

/* tile
 */
 
static const char render_tile_vsrc[]=
  "uniform vec2 screensize;\n"
  "uniform float pointsize;\n"
  "attribute vec2 apos;\n"
  "attribute float atileid;\n"
  "attribute float axform;\n"
  "varying vec2 vsrcp;\n"
  "varying mat2 vmat;\n"
  "void main() {\n"
    "vec2 npos=(apos*2.0)/screensize-1.0;\n"
    "gl_Position=vec4(npos,0.0,1.0);\n"
    "vsrcp=vec2(\n"
      "mod(atileid,16.0),\n"
      "floor(atileid/16.0)\n"
    ")/16.0;\n"
         "if (axform<0.5) vmat=mat2( 1.0, 0.0, 0.0, 1.0);\n" // no xform
    "else if (axform<1.5) vmat=mat2(-1.0, 0.0, 0.0, 1.0);\n" // XREV
    "else if (axform<2.5) vmat=mat2( 1.0, 0.0, 0.0,-1.0);\n" // YREV
    "else if (axform<3.5) vmat=mat2(-1.0, 0.0, 0.0,-1.0);\n" // XREV|YREV
    "else if (axform<4.5) vmat=mat2( 0.0, 1.0, 1.0, 0.0);\n" // SWAP
    "else if (axform<5.5) vmat=mat2( 0.0, 1.0,-1.0, 0.0);\n" // SWAP|XREV
    "else if (axform<6.5) vmat=mat2( 0.0,-1.0, 1.0, 0.0);\n" // SWAP|YREV
    "else if (axform<7.5) vmat=mat2( 0.0,-1.0,-1.0, 0.0);\n" // SWAP|XREV|YREV
                    "else vmat=mat2( 1.0, 0.0, 0.0, 1.0);\n" // invalid; use identity
    "gl_PointSize=pointsize;\n"
  "}\n"
"";

static const char render_tile_fsrc[]=
  "uniform sampler2D sampler;\n"
  "uniform float alpha;\n"
  "uniform vec4 tint;\n"
  "varying vec2 vsrcp;\n"
  "varying mat2 vmat;\n"
  "void main() {\n"
    "vec2 texcoord=gl_PointCoord;\n"
    "texcoord.y=1.0-texcoord.y;\n"
    "texcoord=vmat*(texcoord-0.5)+0.5;\n"
    "texcoord=vsrcp+texcoord/16.0;\n"
    "gl_FragColor=texture2D(sampler,texcoord);\n"
    "gl_FragColor=vec4(mix(gl_FragColor.rgb,tint.rgb,tint.a),gl_FragColor.a*alpha);\n"
  "}\n"
"";

/* Compile half of one program.
 * <0 for error.
 */
 
static int render_program_compile(struct render *render,const char *name,int pid,int type,const GLchar *src,GLint srcc) {
  GLint sid=glCreateShader(type);
  if (!sid) return -1;

  GLchar preamble[256];
  GLint preamblec=0;
  #define VERBATIM(v) { memcpy(preamble+preamblec,v,sizeof(v)-1); preamblec+=sizeof(v)-1; }
  VERBATIM("#version ")
  preamble[preamblec++]='0'+(EGG_GLSL_VERSION/100)%10;
  preamble[preamblec++]='0'+(EGG_GLSL_VERSION/10 )%10;
  preamble[preamblec++]='0'+(EGG_GLSL_VERSION    )%10;
  #if USE_macos
    VERBATIM("\n")
  #else
    VERBATIM("\nprecision mediump float;\n")
  #endif
  #undef VERBATIM
  const GLchar *srcv[2]={preamble,src};
  GLint srccv[2]={preamblec,srcc};
  glShaderSource(sid,2,srcv,srccv);
  
  glCompileShader(sid);
  GLint status=0;
  glGetShaderiv(sid,GL_COMPILE_STATUS,&status);
  if (status) {
    glAttachShader(pid,sid);
    glDeleteShader(sid);
    return 0;
  }
  GLuint loga=0,logged=0;
  glGetShaderiv(sid,GL_INFO_LOG_LENGTH,&loga);
  if (loga>0) {
    GLchar *log=malloc(loga+1);
    if (log) {
      GLsizei logc=0;
      glGetShaderInfoLog(sid,loga,&logc,log);
      if ((logc>0)&&(logc<=loga)) {
        while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
        fprintf(stderr,"Failed to compile %s shader for program '%s':\n%.*s\n",(type==GL_VERTEX_SHADER)?"vertex":"fragment",name,logc,log);
        logged=1;
      }
      free(log);
    }
  }
  if (!logged) fprintf(stderr,"Failed to compile %s shader for program '%s', no further detail available.\n",(type==GL_VERTEX_SHADER)?"vertex":"fragment",name);
  glDeleteShader(sid);
  return -1;
}

/* Initialize one program.
 * Zero for error.
 */
 
static int render_program_init(
  struct render *render,const char *name,
  const char *vsrc,int vsrcc,
  const char *fsrc,int fsrcc,
  const char **attrnamev
) {
  GLint pid=glCreateProgram();
  if (!pid) return 0;
  if (render_program_compile(render,name,pid,GL_VERTEX_SHADER,vsrc,vsrcc)<0) return 0;
  if (render_program_compile(render,name,pid,GL_FRAGMENT_SHADER,fsrc,fsrcc)<0) return 0;
  if (attrnamev) {
    int attrix=0;
    for (;attrnamev[attrix];attrix++) {
      glBindAttribLocation(pid,attrix,attrnamev[attrix]);
    }
  }
  glLinkProgram(pid);
  GLint status=0;
  glGetProgramiv(pid,GL_LINK_STATUS,&status);
  if (status) return pid;
  GLuint loga=0,logged=0;
  glGetProgramiv(pid,GL_INFO_LOG_LENGTH,&loga);
  if (loga>0) {
    GLchar *log=malloc(loga+1);
    if (log) {
      GLsizei logc=0;
      glGetProgramInfoLog(pid,loga,&logc,log);
      if ((logc>0)&&(logc<=loga)) {
        while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
        fprintf(stderr,"Failed to link GLSL program '%s':\n%.*s\n",name,logc,log);
        logged=1;
      }
      free(log);
    }
  }
  if (!logged) fprintf(stderr,"Failed to link GLSL program '%s', no further detail available.\n",name);
  glDeleteProgram(pid);
  return 0;
}

/* Initialize programs.
 */
 
int render_init_programs(struct render *render) {
  #define PGM1(tag,...) { \
    const char *attrnamev[]={__VA_ARGS__,0}; \
    if (!(render->pgm_##tag=render_program_init( \
      render,#tag, \
      render_##tag##_vsrc,sizeof(render_##tag##_vsrc)-1, \
      render_##tag##_fsrc,sizeof(render_##tag##_fsrc)-1, \
      attrnamev \
    ))) return -1; \
  }
  PGM1(raw,"apos","acolor")
  PGM1(decal,"apos","atexcoord")
  PGM1(tile,"apos","atileid","axform")
  #undef PGM1
  
  glUseProgram(render->pgm_raw);
  render->u_raw_screensize=glGetUniformLocation(render->pgm_raw,"screensize");
  render->u_raw_tint=glGetUniformLocation(render->pgm_raw,"tint");
  render->u_raw_alpha=glGetUniformLocation(render->pgm_raw,"alpha");
  
  glUseProgram(render->pgm_decal);
  render->u_decal_screensize=glGetUniformLocation(render->pgm_decal,"screensize");
  render->u_decal_sampler=glGetUniformLocation(render->pgm_decal,"sampler");
  render->u_decal_alpha=glGetUniformLocation(render->pgm_decal,"alpha");
  render->u_decal_tint=glGetUniformLocation(render->pgm_decal,"tint");
  render->u_decal_texlimit=glGetUniformLocation(render->pgm_decal,"texlimit");
  
  glUseProgram(render->pgm_tile);
  render->u_tile_screensize=glGetUniformLocation(render->pgm_tile,"screensize");
  render->u_tile_sampler=glGetUniformLocation(render->pgm_tile,"sampler");
  render->u_tile_alpha=glGetUniformLocation(render->pgm_tile,"alpha");
  render->u_tile_tint=glGetUniformLocation(render->pgm_tile,"tint");
  render->u_tile_pointsize=glGetUniformLocation(render->pgm_tile,"pointsize");
  
  return 0;
}

/* Clear.
 */

void render_texture_clear(struct render *render,int texid) {
  if ((texid<1)||(texid>render->texturec)) return;
  struct render_texture *texture=render->texturev+texid-1;
  if (render_texture_require_fb(texture)<0) return;
  glBindFramebuffer(GL_FRAMEBUFFER,texture->fbid);
  glClearColor(0.0f,0.0f,0.0f,0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

/* Set globals.
 */

void render_tint(struct render *render,uint32_t rgba) {
  render->tint=rgba;
}

void render_alpha(struct render *render,uint8_t a) {
  render->alpha=a;
}

/* Flat rect.
 */

void render_draw_rect(struct render *render,int dsttexid,const struct egg_draw_rect *v,int c) {
  if ((dsttexid<1)||(dsttexid>render->texturec)) return;
  struct render_texture *dsttex=render->texturev+dsttexid-1;
  if (render_texture_require_fb(dsttex)<0) return;
  glBindFramebuffer(GL_FRAMEBUFFER,dsttex->fbid);
  glUseProgram(render->pgm_raw);
  glViewport(0,0,dsttex->w,dsttex->h);
  glUniform2f(render->u_raw_screensize,dsttex->w,dsttex->h);
  glUniform4f(render->u_raw_tint,(render->tint>>24)/255.0f,((render->tint>>16)&0xff)/255.0f,((render->tint>>8)&0xff)/255.0f,(render->tint&0xff)/255.0f);
  glUniform1f(render->u_raw_alpha,render->alpha/255.0f);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  for (;c-->0;v++) {
    struct render_vertex_raw vtxv[]={
      {v->x     ,v->y     ,v->r,v->g,v->b,v->a},
      {v->x,     v->y+v->h,v->r,v->g,v->b,v->a},
      {v->x+v->w,v->y     ,v->r,v->g,v->b,v->a},
      {v->x+v->w,v->y+v->h,v->r,v->g,v->b,v->a},
    };
    glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct render_vertex_raw),&vtxv[0].x);
    glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(struct render_vertex_raw),&vtxv[0].r);
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  }
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
}

/* Line strip and triangle strip.
 */
 
static int render_rvtxv_require(struct render *render,int rvtxc) {
  if (rvtxc<=render->rvtxa) return 0;
  if (rvtxc>INT_MAX/sizeof(struct render_vertex_raw)) return -1;
  void *nv=realloc(render->rvtxv,sizeof(struct render_vertex_raw)*rvtxc);
  if (!nv) return -1;
  render->rvtxv=nv;
  render->rvtxa=rvtxc;
  return 0;
}
 
static void render_draw_raw(struct render *render,int texid,int mode,const struct render_vertex_raw *v,int c) {
  if (c<1) return;
  if ((texid<1)||(texid>render->texturec)) return;
  struct render_texture *texture=render->texturev+texid-1;
  if (render_texture_require_fb(texture)<0) return;
  glBindFramebuffer(GL_FRAMEBUFFER,texture->fbid);
  glUseProgram(render->pgm_raw);
  glViewport(0,0,texture->w,texture->h);
  glUniform2f(render->u_raw_screensize,texture->w,texture->h);
  glUniform4f(render->u_raw_tint,(render->tint>>24)/255.0f,((render->tint>>16)&0xff)/255.0f,((render->tint>>8)&0xff)/255.0f,(render->tint&0xff)/255.0f);
  glUniform1f(render->u_raw_alpha,render->alpha/255.0f);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct render_vertex_raw),&v[0].x);
  glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(struct render_vertex_raw),&v[0].r);
  glDrawArrays(mode,0,c);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
}

void render_draw_line(struct render *render,int texid,const struct egg_draw_line *v,int c) {
  if (c<1) return;
  if (c>INT_MAX>>1) return;
  if (render_rvtxv_require(render,c<<1)<0) return;
  struct render_vertex_raw *dst=render->rvtxv;
  int i=c; for (;i-->0;v++) {
    dst[0].r=dst[1].r=v->r;
    dst[0].g=dst[1].g=v->g;
    dst[0].b=dst[1].b=v->b;
    dst[0].a=dst[1].a=v->a;
    dst->x=v->ax;
    dst->y=v->ay;
    dst++;
    dst->x=v->bx;
    dst->y=v->by;
    dst++;
  }
  render_draw_raw(render,texid,GL_LINES,render->rvtxv,c<<1);
}
 
void render_draw_trig(struct render *render,int texid,const struct egg_draw_trig *v,int c) {
  if (c<1) return;
  if (c>INT_MAX/3) return;
  if (render_rvtxv_require(render,c*3)<0) return;
  struct render_vertex_raw *dst=render->rvtxv;
  int i=c; for (;i-->0;v++) {
    dst[0].r=dst[1].r=dst[2].r=v->r;
    dst[0].g=dst[1].g=dst[2].g=v->g;
    dst[0].b=dst[1].b=dst[2].b=v->b;
    dst[0].a=dst[1].a=dst[2].a=v->a;
    dst->x=v->ax;
    dst->y=v->ay;
    dst++;
    dst->x=v->bx;
    dst->y=v->by;
    dst++;
    dst->x=v->cx;
    dst->y=v->cy;
    dst++;
  }
  render_draw_raw(render,texid,GL_TRIANGLES,render->rvtxv,c*3);
}

/* Decal.
 */

void render_draw_decal(struct render *render,int dsttexid,int srctexid,const struct egg_draw_decal *v,int c) {
  if (dsttexid==srctexid) return;
  if ((dsttexid<1)||(dsttexid>render->texturec)) return;
  if ((srctexid<1)||(srctexid>render->texturec)) return;
  struct render_texture *dsttex=render->texturev+dsttexid-1;
  struct render_texture *srctex=render->texturev+srctexid-1;
  if ((srctex->w<1)||(srctex->h<1)) return;
  if (render_texture_require_fb(dsttex)<0) return;
  glBindFramebuffer(GL_FRAMEBUFFER,dsttex->fbid);
  glViewport(0,0,dsttex->w,dsttex->h);
  glUseProgram(render->pgm_decal);
  glUniform2f(render->u_decal_screensize,dsttex->w,dsttex->h);
  glBindTexture(GL_TEXTURE_2D,srctex->texid);
  glUniform4f(render->u_decal_tint,(render->tint>>24)/255.0f,((render->tint>>16)&0xff)/255.0f,((render->tint>>8)&0xff)/255.0f,(render->tint&0xff)/255.0f);
  glUniform1f(render->u_decal_alpha,render->alpha/255.0f);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  
  for (;c-->0;v++) {
    int dstw=v->w,dsth=v->h;
    if (v->xform&EGG_XFORM_SWAP) {
      dstw=v->h;
      dsth=v->w;
    }
    struct render_vertex_decal vtxv[]={
      {v->dstx     ,v->dsty     ,0.0f,0.0f},
      {v->dstx     ,v->dsty+dsth,0.0f,1.0f},
      {v->dstx+dstw,v->dsty     ,1.0f,0.0f},
      {v->dstx+dstw,v->dsty+dsth,1.0f,1.0f},
    };
    if (v->xform&EGG_XFORM_SWAP) {
      struct render_vertex_decal *vtx=vtxv;
      int i=4; for (;i-->0;vtx++) {
        GLfloat tmp=vtx->tx;
        vtx->tx=vtx->ty;
        vtx->ty=tmp;
      }
    }
    if (v->xform&EGG_XFORM_XREV) {
      struct render_vertex_decal *vtx=vtxv;
      int i=4; for (;i-->0;vtx++) vtx->tx=1.0f-vtx->tx;
    }
    if (v->xform&EGG_XFORM_YREV) {
      struct render_vertex_decal *vtx=vtxv;
      int i=4; for (;i-->0;vtx++) vtx->ty=1.0f-vtx->ty;
    }
    GLfloat tx0=(GLfloat)v->srcx/(GLfloat)srctex->w;
    GLfloat tx1=(GLfloat)v->w/(GLfloat)srctex->w;
    GLfloat ty0=(GLfloat)v->srcy/(GLfloat)srctex->h;
    GLfloat ty1=(GLfloat)v->h/(GLfloat)srctex->h;
    struct render_vertex_decal *vtx=vtxv;
    int i=4; for (;i-->0;vtx++) {
      vtx->tx=tx0+tx1*vtx->tx;
      vtx->ty=ty0+ty1*vtx->ty;
    }
    glUniform4f(render->u_decal_texlimit,tx0,ty0,tx0+tx1,ty0+ty1);
    glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct render_vertex_decal),&vtxv[0].x);
    glVertexAttribPointer(1,2,GL_FLOAT,0,sizeof(struct render_vertex_decal),&vtxv[0].tx);
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  }
  
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
}

/* Decal with free scale and rotation.
 */
 
static void render_draw_mode7_1(struct render *render,struct render_texture *srctex,const struct egg_draw_mode7 *v) {
  // Transform the output vertices right here, CPU-side.
  double cost=cos(-v->rotate);
  double sint=sin(-v->rotate);
  double halfw=v->w*v->xscale*0.5;
  double halfh=v->h*v->yscale*0.5;
  int nwx=lround( cost*halfw+sint*halfh);
  int nwy=lround(-sint*halfw+cost*halfh);
  int swx=lround( cost*halfw-sint*halfh);
  int swy=lround(-sint*halfw-cost*halfh);
  struct render_vertex_decal vtxv[]={
    {v->dstx-nwx,v->dsty-nwy,0.0f,0.0f},
    {v->dstx-swx,v->dsty-swy,0.0f,1.0f},
    {v->dstx+swx,v->dsty+swy,1.0f,0.0f},
    {v->dstx+nwx,v->dsty+nwy,1.0f,1.0f},
  };
  GLfloat tx0=((GLfloat)v->srcx)/(GLfloat)srctex->w;
  GLfloat tx1=((GLfloat)v->w)/(GLfloat)srctex->w;
  GLfloat ty0=((GLfloat)v->srcy)/(GLfloat)srctex->h;
  GLfloat ty1=((GLfloat)v->h)/(GLfloat)srctex->h;
  struct render_vertex_decal *vtx=vtxv;
  int i=4; for (;i-->0;vtx++) {
    vtx->tx=tx0+tx1*vtx->tx;
    vtx->ty=ty0+ty1*vtx->ty;
  }
  glUniform4f(render->u_decal_texlimit,vtxv[0].tx,vtxv[0].ty,vtxv[3].tx,vtxv[3].ty);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct render_vertex_decal),&vtxv[0].x);
  glVertexAttribPointer(1,2,GL_FLOAT,0,sizeof(struct render_vertex_decal),&vtxv[0].tx);
  glDrawArrays(GL_TRIANGLE_STRIP,0,4);
}

void render_draw_mode7(struct render *render,int dsttexid,int srctexid,const struct egg_draw_mode7 *v,int c,int interpolate) {
  if (dsttexid==srctexid) return;
  if ((dsttexid<1)||(dsttexid>render->texturec)) return;
  if ((srctexid<1)||(srctexid>render->texturec)) return;
  struct render_texture *dsttex=render->texturev+dsttexid-1;
  struct render_texture *srctex=render->texturev+srctexid-1;
  if ((srctex->w<1)||(srctex->h<1)) return;
  if (render_texture_require_fb(dsttex)<0) return;
  glBindFramebuffer(GL_FRAMEBUFFER,dsttex->fbid);
  glViewport(0,0,dsttex->w,dsttex->h);
  glUseProgram(render->pgm_decal);
  glUniform2f(render->u_decal_screensize,dsttex->w,dsttex->h);
  glBindTexture(GL_TEXTURE_2D,srctex->texid);
  if (interpolate) {
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  }
  glUniform4f(render->u_decal_tint,(render->tint>>24)/255.0f,((render->tint>>16)&0xff)/255.0f,((render->tint>>8)&0xff)/255.0f,(render->tint&0xff)/255.0f);
  glUniform1f(render->u_decal_alpha,render->alpha/255.0f);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  for (;c-->0;v++) render_draw_mode7_1(render,srctex,v);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  if (interpolate) {
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  }
}

/* Tiles.
 */

void render_draw_tile(struct render *render,int dsttexid,int srctexid,const struct egg_draw_tile *v,int c) {
  if (dsttexid==srctexid) return;
  if ((dsttexid<1)||(dsttexid>render->texturec)) return;
  if ((srctexid<1)||(srctexid>render->texturec)) return;
  struct render_texture *dsttex=render->texturev+dsttexid-1;
  struct render_texture *srctex=render->texturev+srctexid-1;
  if (render_texture_require_fb(dsttex)<0) return;
  glBindFramebuffer(GL_FRAMEBUFFER,dsttex->fbid);
  glViewport(0,0,dsttex->w,dsttex->h);
  glUseProgram(render->pgm_tile);
  glUniform2f(render->u_tile_screensize,dsttex->w,dsttex->h);
  glActiveTexture(GL_TEXTURE0);
  glUniform1i(render->u_tile_sampler,0);
  glBindTexture(GL_TEXTURE_2D,srctex->texid);
  glUniform4f(render->u_tile_tint,(render->tint>>24)/255.0f,((render->tint>>16)&0xff)/255.0f,((render->tint>>8)&0xff)/255.0f,(render->tint&0xff)/255.0f);
  glUniform1f(render->u_tile_alpha,render->alpha/255.0f);
  glUniform1f(render->u_tile_pointsize,srctex->w>>4);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct egg_draw_tile),&v[0].dstx);
  glVertexAttribPointer(1,1,GL_UNSIGNED_BYTE,0,sizeof(struct egg_draw_tile),&v[0].tileid);
  glVertexAttribPointer(2,1,GL_UNSIGNED_BYTE,0,sizeof(struct egg_draw_tile),&v[0].xform);
  glDrawArrays(GL_POINTS,0,c);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
}

/* Draw to main.
 */
 
void render_draw_to_main(struct render *render,int mainw,int mainh,int texid) {
  if ((texid<1)||(texid>render->texturec)) return;
  struct render_texture *texture=render->texturev+texid-1;
  if ((texture->w<1)||(texture->h<1)) return;
  
  int dstx=0,dsty=0,w=mainw,h=mainh;
  int xscale=mainw/texture->w;
  int yscale=mainh/texture->h;
  int scale=(xscale<yscale)?xscale:yscale;
  if (scale<1) scale=1;
  if (scale<3) { // Debatable: Below 3x, scale only to multiples of the framebuffer size, leaving a border.
    w=texture->w*scale;
    h=texture->h*scale;
  } else { // If scaling large enough, cover one axis and let pixels end up different sizes.
    int wforh=(texture->w*mainh)/texture->h;
    if (wforh<=mainw) {
      w=wforh;
    } else {
      h=(texture->h*mainw)/texture->w;
    }
  }
  dstx=(mainw>>1)-(w>>1);
  dsty=(mainh>>1)-(h>>1);
  render->outx=dstx;
  render->outy=dsty;
  render->outw=w;
  render->outh=h;
  
  struct render_vertex_decal vtxv[]={
    {dstx  ,dsty  ,0.0f,1.0f},
    {dstx  ,dsty+h,0.0f,0.0f},
    {dstx+w,dsty  ,1.0f,1.0f},
    {dstx+w,dsty+h,1.0f,0.0f},
  };
  glBindFramebuffer(GL_FRAMEBUFFER,0);
  glViewport(0,0,mainw,mainh);
  if ((w<mainw)||(h<mainh)) {
    glClearColor(0.0f,0.0f,0.0f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  glUseProgram(render->pgm_decal);
  glUniform2f(render->u_decal_screensize,mainw,mainh);
  glUniform1i(render->u_decal_sampler,0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  glDisable(GL_BLEND);
  glUniform4f(render->u_decal_tint,0.0f,0.0f,0.0f,0.0f);
  glUniform1f(render->u_decal_alpha,1.0f);
  glUniform4f(render->u_decal_texlimit,0.0f,0.0f,1.0f,1.0f);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct render_vertex_decal),&vtxv[0].x);
  glVertexAttribPointer(1,2,GL_FLOAT,0,sizeof(struct render_vertex_decal),&vtxv[0].tx);
  glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glEnable(GL_BLEND);
}
