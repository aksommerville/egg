#ifndef ARCHIVE_INTERNAL_H
#define ARCHIVE_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"
#include "opt/arw/arw.h"

extern struct archive {

  const char *exename;
  const char *dstpath;
  
  struct arw arw;
  struct sr_encoder dst;
  const char *root;
  int rootc;
} archive;

/* Input file formats.
 * Generally these match path suffixes.
 * Some are aliased; see archive_main.c:archive_format_for_suffix()
 * Values are not important, they only need to be positive and unique.
 * We don't necessarily support all of these formats, only we recognize their names.
 */
#define ARCHIVE_FORMAT_js      1
#define ARCHIVE_FORMAT_png     2
#define ARCHIVE_FORMAT_qoi     3
#define ARCHIVE_FORMAT_gif     4
#define ARCHIVE_FORMAT_bmp     5
#define ARCHIVE_FORMAT_ico     6
#define ARCHIVE_FORMAT_jpeg    7
#define ARCHIVE_FORMAT_mid     8
#define ARCHIVE_FORMAT_txt     9
#define ARCHIVE_FORMAT_html   10
#define ARCHIVE_FORMAT_wasm   11
#define ARCHIVE_FORMAT_wav    12

// Other format values for internal use.
#define ARCHIVE_FORMAT_string -1

#endif
