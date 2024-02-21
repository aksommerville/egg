#include "egg_native_internal.h"
#include <unistd.h>
#include <signal.h>

struct egg egg={0};

/* Signal.
 */
 
static void egg_native_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(egg.sigc)>=3) {
        fprintf(stderr,"%s: Too many unprocessed signals.\n",egg.exename);
        exit(1);
      } break;
  }
}

/* Cleanup.
 */
 
static void egg_native_quit() {
  egg_native_rom_farewell();
  
  if (egg.terminate>=0) {
    //TODO Performance log.
  }
  
  hostio_del(egg.hostio);
  egg_native_rom_cleanup();
  
  if (egg.storepath) free(egg.storepath);
  if (egg.storev) {
    while (egg.storec-->0) {
      struct egg_store_entry *entry=egg.storev+egg.storec;
      if (entry->k) free(entry->k);
      if (entry->v) free(entry->v);
    }
    free(egg.storev);
  }
}

/* Init. Config has already been loaded.
 */
 
static int egg_native_init() {
  int err;
  
  if ((err=egg_native_rom_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error loading ROM.\n",egg.exename);
    return -2;
  }
  
  if (!(egg.hostio=hostio_new())) return -1;
  if ((err=egg_native_drivers_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializaing drivers.\n",egg.exename);
    return -2;
  }
  
  if ((err=egg_native_rom_start())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error starting game.\n",egg.exename);
    return -2;
  }

  return 0;
}

/* Update. Must block.
 */
 
static int egg_native_update() {
  int err;
  
  usleep(100000);
  double elapsed=0.100;
  //TODO Tick clock.
  
  if ((err=hostio_update(egg.hostio))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error updating drivers.\n",egg.exename);
    return -2;
  }
  if (egg.terminate) return 0;
  
  if ((err=egg_native_rom_update(elapsed))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error updating game.\n",egg.exename);
    return -2;
  }
  if (egg.terminate) return 0;
  
  if (egg.hostio->video->type->begin_frame) {
    if (egg.hostio->video->type->begin_frame(egg.hostio->video)<0) return -1;
  }
  egg_native_rom_render();
  if (egg.hostio->video->type->end_frame) {
    if (egg.hostio->video->type->end_frame(egg.hostio->video)<0) return -1;
  }
  
  egg_native_store_update();
  
  return 0;
}

/* Main.
 */

int main(int argc,char **argv) {

  signal(SIGINT,egg_native_rcvsig);
  
  int err=egg_native_config(argc,argv);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error loading configuration.\n",egg.exename);
    return 1;
  }
  if (egg.terminate||egg.sigc) return 0;
  
  if ((err=egg_native_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error during initialization.\n",egg.exename);
    return 1;
  }
  
  fprintf(stderr,"%s: Running. SIGINT to quit.\n",egg.exename);
  while (!egg.sigc&&!egg.terminate) {
    if ((err=egg_native_update())<0) {
      fprintf(stderr,"%s: Unspecified error during run.\n",egg.exename);
      egg_native_quit();
      return 1;
    }
  }
  
  egg_native_quit();
  if (egg.terminate<0) {
    fprintf(stderr,"%s: Abnormal exit.\n",egg.exename);
    return 1;
  }
  fprintf(stderr,"%s: Normal exit.\n",egg.exename);
  return 0;
}
