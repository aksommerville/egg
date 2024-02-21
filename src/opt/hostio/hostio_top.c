#include "hostio_internal.h"

/* Delete.
 */
 
void hostio_del(struct hostio *hostio) {
  if (!hostio) return;
  hostio_video_del(hostio->video);
  hostio_audio_del(hostio->audio);
  hostio_input_del(hostio->input);
  free(hostio);
}

/* New.
 */

struct hostio *hostio_new() {
  struct hostio *hostio=calloc(1,sizeof(struct hostio));
  if (!hostio) return 0;
  return hostio;
}

/* Init driver.
 * They all work the same way.
 */
 
#define HOSTIO_DRIVER_INIT(intf) \
  int hostio_##intf##_init( \
    struct hostio *hostio, \
    const char *names,int namesc, \
    const struct hostio_##intf##_delegate *delegate, \
    const struct hostio_##intf##_setup *setup \
  ) { \
    int err=-1; \
    if (!names) namesc=0; else if (namesc<0) { namesc=0; while (names[namesc]) namesc++; } \
    if (namesc) { \
      int namesp=0; \
      while (namesp<namesc) { \
        if ((unsigned char)names[namesp]<=0x20) { namesp++; continue; } \
        if (names[namesp]==',') { namesp++; continue; } \
        const char *name=names+namesp; \
        int namec=0; \
        while ((namesp<namesc)&&(names[namesp++]!=',')) namec++; \
        const struct hostio_##intf##_type *type=hostio_##intf##_type_by_name(name,namec); \
        if (!type) { \
          fprintf(stderr,"%s driver '%.*s' not found, skipping.\n",#intf,namec,name); \
          continue; \
        } \
        if (hostio->intf=hostio_##intf##_new(type,delegate,setup)) return 0; \
      } \
    } else { \
      int p=0; \
      for (;;p++) { \
        const struct hostio_##intf##_type *type=hostio_##intf##_type_by_index(p); \
        if (!type) return -1; \
        if (type->appointment_only) continue; \
        if (hostio->intf=hostio_##intf##_new(type,delegate,setup)) return 0; \
      } \
    } \
    return -1; \
  }

HOSTIO_DRIVER_INIT(video)
HOSTIO_DRIVER_INIT(audio)
HOSTIO_DRIVER_INIT(input)

#undef HOSTIO_DRIVER_INIT

/* Update.
 */

int hostio_update(struct hostio *hostio) {
  int err;
  if (hostio->video&&hostio->video->type->update) {
    if ((err=hostio->video->type->update(hostio->video))<0) return err;
  }
  if (hostio->audio&&hostio->audio->type->update) {
    if ((err=hostio->audio->type->update(hostio->audio))<0) return err;
  }
  if (hostio->input&&hostio->input->type->update) {
    if ((err=hostio->input->type->update(hostio->input))<0) return err;
  }
  return 0;
}
