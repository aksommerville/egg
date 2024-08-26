/* bcm.h
 * Interface to Broadcom video, for Raspberry Pi.
 * This uses EGL to set up an OpenGL ES 2.x context.
 */

#ifndef BCM_H
#define BCM_H

#include <GLES2/gl2.h>

int bcm_init();
void bcm_quit();
int bcm_swap();
int bcm_get_width();
int bcm_get_height();

#endif
