#ifndef RENDER_INTERNAL_H
#define RENDER_INTERNAL_H

#include "render.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include "egg/egg.h"
#include "opt/image/image.h"
#include "GLES2/gl2.h"

struct render_vertex_raw {
  GLshort x,y;
  GLubyte r,g,b,a;
};

struct render_vertex_decal {
  GLshort x,y;
  GLfloat tx,ty;
};

struct render_texture {
  GLuint texid;
  GLuint fbid;
  int w,h,fmt;
  int qual,rid; // Commentary we can report later, for saving state.
};

struct render {

  /* texid as shown to our client is the index in this list plus one.
   */
  struct render_texture *texturev;
  int texturec,texturea;
  
  uint32_t tint;
  uint8_t alpha;
  
  GLint pgm_raw;
  GLint pgm_decal;
  GLint pgm_tile;
  
  GLuint u_raw_screensize;
  GLuint u_raw_alpha;
  GLuint u_raw_tint;
  GLuint u_decal_screensize;
  GLuint u_decal_sampler;
  GLuint u_decal_alpha;
  GLuint u_decal_tint;
  GLuint u_decal_texlimit;
  GLuint u_tile_screensize;
  GLuint u_tile_sampler;
  GLuint u_tile_alpha;
  GLuint u_tile_tint;
  GLuint u_tile_pointsize;
  
  // Temporary buffer for expanding 1-bit textures.
  void *textmp;
  int textmpa;
  
  // Frame of last output framebuffer, in window coords.
  int outx,outy,outw,outh;
  
  // Temporary buffer for raw vertices.
  struct render_vertex_raw *rvtxv;
  int rvtxa;
};

int render_init_programs(struct render *render);
int render_texture_require_fb(struct render_texture *texture);

#endif
