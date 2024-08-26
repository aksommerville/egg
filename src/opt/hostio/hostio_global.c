#include "hostio_internal.h"

/* Delete.
 */

void hostio_del(struct hostio *hostio) {
  if (!hostio) return;
  hostio_video_del(hostio->video);
  hostio_audio_del(hostio->audio);
  if (hostio->inputv) {
    while (hostio->inputc-->0) hostio_input_del(hostio->inputv[hostio->inputc]);
    free(hostio->inputv);
  }
  free(hostio);
}

/* New.
 */

struct hostio *hostio_new(
  const struct hostio_video_delegate *video_delegate,
  const struct hostio_audio_delegate *audio_delegate,
  const struct hostio_input_delegate *input_delegate
) {
  struct hostio *hostio=calloc(1,sizeof(struct hostio));
  if (!hostio) return 0;
  if (video_delegate) hostio->video_delegate=*video_delegate;
  if (audio_delegate) hostio->audio_delegate=*audio_delegate;
  if (input_delegate) hostio->input_delegate=*input_delegate;
  return hostio;
}

/* Driver initialization context.
 */
 
struct hostio_init_context {
  struct hostio *hostio;
  const char *names; // for reference, so typed initializer knows how we were invoked
  const void *setup;
  const void *(*type_by_index)(int p);
  const void *(*type_by_name)(const char *name,int namec);
  int (*init)(const void *type,struct hostio_init_context *ctx);
};

/* Try initialization.
 */
 
static int hostio_init_with_names(const char *names,struct hostio_init_context *ctx) {
  while (*names) {
    if ((unsigned char)(*names)<=0x20) { names++; continue; }
    if (*names==',') { names++; continue; }
    const char *name=names;
    int namec=0;
    while (((unsigned char)(*names)>0x20)&&(*names!=',')) { names++; namec++; }
    const void *type=ctx->type_by_name(name,namec);
    if (type) {
      int err=ctx->init(type,ctx);
      if (err) return err;
    }
  }
  return 0;
}

static int hostio_init_with_defaults(struct hostio_init_context *ctx) {
  int p=0;
  for (;;p++) {
    const void *type=ctx->type_by_index(p);
    if (!type) break;
    int err=ctx->init(type,ctx);
    if (err) return err;
  }
  return 0;
}

/* Init video.
 */
 
static int hostio_video_init_1(const void *_type,struct hostio_init_context *ctx) {
  const struct hostio_video_type *type=_type;
  if (type->appointment_only&&!ctx->names) return 0;
  if (ctx->hostio->video=hostio_video_new(type,&ctx->hostio->video_delegate,ctx->setup)) {
    return 1;
  }
  return 0;
}

int hostio_init_video(
  struct hostio *hostio,
  const char *names,
  const struct hostio_video_setup *setup
) {
  if (hostio->video) {
    hostio_video_del(hostio->video);
    hostio->video=0;
  }
  struct hostio_init_context ctx={
    .hostio=hostio,
    .setup=setup,
    .names=names,
    .type_by_index=(void*)hostio_video_type_by_index,
    .type_by_name=(void*)hostio_video_type_by_name,
    .init=hostio_video_init_1,
  };
  if (names) {
    int err=hostio_init_with_names(names,&ctx);
    if (err<0) return err;
  } else {
    int err=hostio_init_with_defaults(&ctx);
    if (err<0) return err;
  }
  if (!hostio->video) return -1;
  return 0;
}

/* Init audio.
 */
 
static int hostio_audio_init_1(const void *_type,struct hostio_init_context *ctx) {
  const struct hostio_audio_type *type=_type;
  if (type->appointment_only&&!ctx->names) return 0;
  if (ctx->hostio->audio=hostio_audio_new(type,&ctx->hostio->audio_delegate,ctx->setup)) {
    return 1;
  }
  return 0;
}

int hostio_init_audio(
  struct hostio *hostio,
  const char *names,
  const struct hostio_audio_setup *setup
) {
  if (hostio->audio) {
    hostio_audio_del(hostio->audio);
    hostio->audio=0;
  }
  struct hostio_init_context ctx={
    .hostio=hostio,
    .setup=setup,
    .names=names,
    .type_by_index=(void*)hostio_audio_type_by_index,
    .type_by_name=(void*)hostio_audio_type_by_name,
    .init=hostio_audio_init_1,
  };
  if (names) {
    int err=hostio_init_with_names(names,&ctx);
    if (err<0) return err;
  } else {
    int err=hostio_init_with_defaults(&ctx);
    if (err<0) return err;
  }
  if (!hostio->audio) return -1;
  return 0;
}

/* Init input.
 */
 
static int hostio_input_init_1(const void *_type,struct hostio_init_context *ctx) {
  const struct hostio_input_type *type=_type;
  if (type->appointment_only&&!ctx->names) return 0;
  if (ctx->hostio->inputc>=ctx->hostio->inputa) {
    int na=ctx->hostio->inputa+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(ctx->hostio->inputv,sizeof(void*)*na);
    if (!nv) return -1;
    ctx->hostio->inputv=nv;
    ctx->hostio->inputa=na;
  }
  struct hostio_input *driver=hostio_input_new(type,&ctx->hostio->input_delegate,ctx->setup);
  if (!driver) return 0;
  ctx->hostio->inputv[ctx->hostio->inputc++]=driver;
  return 0; // 0 not 1: Allow multiple input drivers, continue down the list.
}
 
int hostio_init_input(
  struct hostio *hostio,
  const char *names,
  const struct hostio_input_setup *setup
) {
  while (hostio->inputc>0) {
    hostio->inputc--;
    hostio_input_del(hostio->inputv[hostio->inputc]);
  }
  struct hostio_init_context ctx={
    .hostio=hostio,
    .setup=setup,
    .names=names,
    .type_by_index=(void*)hostio_input_type_by_index,
    .type_by_name=(void*)hostio_input_type_by_name,
    .init=hostio_input_init_1,
  };
  if (names) {
    int err=hostio_init_with_names(names,&ctx);
    if (err<0) return err;
  } else {
    int err=hostio_init_with_defaults(&ctx);
    if (err<0) return err;
  }
  // No assertions; it's ok to not have any input drivers.
  return 0;
}

/* Log driver selections.
 */
 
void hostio_log_driver_names(const struct hostio *hostio) {
  if (!hostio) return;
  if (hostio->video) {
    fprintf(stderr,"Using video driver '%s' size=%d,%d\n",hostio->video->type->name,hostio->video->w,hostio->video->h);
  }
  if (hostio->audio) {
    fprintf(stderr,"Using audio driver '%s' %d@%d\n",hostio->audio->type->name,hostio->audio->chanc,hostio->audio->rate);
  }
  int i=0; for (;i<hostio->inputc;i++) {
    struct hostio_input *driver=hostio->inputv[i];
    fprintf(stderr,"Using input driver '%s'\n",driver->type->name);
  }
}

/* Update.
 */
 
int hostio_update(struct hostio *hostio) {
  if (hostio->video&&hostio->video->type->update) {
    if (hostio->video->type->update(hostio->video)<0) return -1;
  }
  if (hostio->audio&&hostio->audio->type->update) {
    if (hostio->audio->type->update(hostio->audio)<0) return -1;
  }
  int i=hostio->inputc;
  while (i-->0) {
    struct hostio_input *driver=hostio->inputv[i];
    if (driver->type->update) {
      if (driver->type->update(driver)<0) return -1;
    }
  }
  return 0;
}

/* Conveniences.
 */
 
int hostio_toggle_fullscreen(struct hostio *hostio) {
  if (!hostio||!hostio->video) return 0;
  if (hostio->video->type->set_fullscreen) {
    hostio->video->type->set_fullscreen(hostio->video,hostio->video->fullscreen?0:1);
  }
  return hostio->video->fullscreen;
}

int hostio_audio_play(struct hostio *hostio,int play) {
  if (!hostio||!hostio->audio) return 0;
  if (hostio->audio->type->play) {
    hostio->audio->type->play(hostio->audio,play);
  }
  return hostio->audio->playing;
}

int hostio_audio_lock(struct hostio *hostio) {
  if (!hostio||!hostio->audio||!hostio->audio->type->lock) return 0;
  return hostio->audio->type->lock(hostio->audio);
}

void hostio_audio_unlock(struct hostio *hostio) {
  if (!hostio||!hostio->audio||!hostio->audio->type->unlock) return;
  hostio->audio->type->unlock(hostio->audio);
}
