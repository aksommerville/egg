#include "eggrt_internal.h"
#include <signal.h>
#include <unistd.h>

struct eggrt eggrt={0};

/* Signal handler.
 */
 
static void eggrt_signal(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(eggrt.terminate)>=3) {
        fprintf(stderr,"%s: Too many unprocessed signals.\n",eggrt.exename);
        exit(1);
      } break;
  }
}

/* Quit.
 */
 
static void eggrt_quit() {
  int err;
  hostio_audio_play(eggrt.hostio,0);
  eggrt_exec_client_quit(eggrt.exitstatus);
  if (eggrt.store_dirty&&((err=eggrt_store_save())<0)) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error saving game.\n",eggrt.storepath);
  }
  if (!eggrt.exitstatus) {
    eggrt_clock_report();
  }
  eggrt_drivers_quit();
  eggrt_store_quit();
  eggrt_exec_quit();
  eggrt_romsrc_quit();
}

/* Init.
 */
 
static int eggrt_init() {
  int err;

  signal(SIGINT,eggrt_signal);
  
  if ((err=eggrt_romsrc_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error acquiring ROM.\n",eggrt.exename);
    return -2;
  }
  
  // With the ROM online, now we can select default language.
  if (!eggrt.lang) eggrt.lang=eggrt_configure_guess_language();
  
  if ((err=eggrt_exec_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing execution core.\n",eggrt.exename);
    return -2;
  }
  
  if ((err=eggrt_store_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing store.\n",eggrt.exename);
    return -2;
  }

  if ((err=eggrt_drivers_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing platform drivers.\n",eggrt.exename);
    return -2;
  }
  
  if ((err=eggrt_exec_client_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing game.\n",eggrt.rptname);
    return -2;
  }
  
  hostio_audio_play(eggrt.hostio,1);
  eggrt_clock_init();
  
  return 0;
}

/* Update.
 */
 
static int eggrt_update() {
  int err;
  
  // Tick the master clock.
  double elapsed=eggrt_clock_update();
  
  // Update drivers.
  if ((err=eggrt_drivers_update())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error updating platform drivers.\n",eggrt.exename);
    return -2;
  }
  if (eggrt.terminate) return 0;
  if (eggrt.hardpause) {
    inmgr_drop_redundant_events(eggrt.inmgr);
    return 0;
  }
  
  // Update client.
  if ((err=eggrt_exec_client_update(elapsed))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error updating game model.\n",eggrt.rptname);
    return -2;
  }
  if (eggrt.store_dirty&&((err=eggrt_store_save())<0)) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error saving game.\n",eggrt.storepath);
  }
  if (eggrt.terminate) return 0;
  
  // Render.
  if (eggrt.hostio->video->type->gx_begin(eggrt.hostio->video)<0) return -1;
  egg_draw_globals(0,0xff);
  if ((err=eggrt_exec_client_render())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error rendering frame.\n",eggrt.rptname);
    return -2;
  }
  render_draw_to_main(eggrt.render,eggrt.hostio->video->w,eggrt.hostio->video->h,1);
  if (eggrt.hostio->video->type->gx_end(eggrt.hostio->video)<0) return -1;
  
  return 0;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  int err;
  
  if ((err=eggrt_configure(argc,argv))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error acquiring configuration.\n",eggrt.exename);
    return 1;
  }
  if (eggrt.terminate) return eggrt.exitstatus; // eg --help
  
  if ((err=eggrt_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error starting up.\n",eggrt.exename);
    return 1;
  }
  
  while (!eggrt.terminate) {
    if ((err=eggrt_update())<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified runtime error.\n",eggrt.exename);
      if (!eggrt.exitstatus) eggrt.exitstatus=1;
      break;
    }
  }
  
  eggrt_quit();
  return eggrt.exitstatus;
}
