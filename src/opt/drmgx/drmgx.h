/* drmgx.h
 * Required:
 * Optional: hostio
 * Link: -ldrm -lgbm -lEGL
 *
 * Unlike most of our I/O primitive units, this one uses a global context.
 * For no particular reason.
 */
 
#ifndef DRMGX_H
#define DRMGX_H

void drmgx_quit();
int drmgx_init(const char *path);
int drmgx_swap();

void drmgx_get_size(int *w,int *h);

#endif
