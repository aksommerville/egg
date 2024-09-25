#ifndef TEXT_INTERNAL_H
#define TEXT_INTERNAL_H

#include "text.h"
#include "egg/egg.h"
#include "opt/stdlib/egg-stdlib.h"
#include "opt/rom/rom.h"

extern struct strings {
  const uint8_t *rom;
  int romc;
  int lang;
  // We only record "strings" and "image" resources:
  struct text_res {
    uint32_t id; // (tid<<16)|rid
    const void *v;
    int c;
  } *resv;
  int resc,resa;
} strings;

struct font {
  int h;
  int replacement;
  struct font_page {
    void *v;
    int w,h,stride;
    int codepoint;
    struct font_glyph {
      int x,y,w;
    } *glyphv;
    int glyphc,glypha;
  } *pagev;
  int pagec,pagea;
};

#endif
