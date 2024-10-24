/* synth_node.h
 * Abstraction of all components that can generate a signal.
 */
 
#ifndef SYNTH_NODE_H
#define SYNTH_NODE_H

struct synth_node;
struct synth_node_type;

/* Generic node.
 *************************************************************************************/

struct synth_node {
  const struct synth_node_type *type;
  struct synth *synth; // WEAK
  int chanc; // READONLY. Not necessarily the same as (synth->chanc).
  struct synth_node *parent; // WEAK, OPTIONAL
  struct synth_node **childv; // STRONG
  int childc,childa;
  int refc;
  int ready;
  int finished;
  
  /* REQUIRED. You get a noop default from the generic ctor.
   * (v) is both in and out. Nodes add their own signal to it. (or filters do whatever they do).
   * (c) is in frames. Caller is responsible for providing a buffer that agrees with your (chanc).
   * (c) will never exceed SYNTH_UPDATE_LIMIT_FRAMES.
   */
  void (*update)(float *v,int framec,struct synth_node *node);
};

struct synth_node_type {
  const char *name;
  int objlen;
  void (*del)(struct synth_node *node);
  int (*init)(struct synth_node *node);
  int (*ready)(struct synth_node *node);
};

void synth_node_del(struct synth_node *node);
int synth_node_ref(struct synth_node *node);

/* (type,synth) required.
 * (chanc) may be zero to copy from (synth).
 */
struct synth_node *synth_node_new(const struct synth_node_type *type,int chanc,struct synth *synth);

/* (parent,type) required.
 * (chanc) may be zero to copy from (parent).
 * Appends to (parent->childv) and returns WEAK.
 * Newly spawned nodes are not readied yet, caller must do that or remove it.
 */
struct synth_node *synth_node_spawn(struct synth_node *parent,const struct synth_node_type *type,int chanc);

/* The meaning of child nodes depends on the parent's type.
 * Generically, it's just a retention mechanism.
 */
int synth_node_add_child(struct synth_node *parent,struct synth_node *child);
int synth_node_remove_child(struct synth_node *parent,struct synth_node *child);

/* Must call after construction and initialization, before the first update.
 */
int synth_node_ready(struct synth_node *node);

/* Node types.
 *****************************************************************************************/
 
extern const struct synth_node_type synth_node_type_bus;
extern const struct synth_node_type synth_node_type_channel;
extern const struct synth_node_type synth_node_type_pcm;
extern const struct synth_node_type synth_node_type_wave;
extern const struct synth_node_type synth_node_type_fm;
extern const struct synth_node_type synth_node_type_sub;
extern const struct synth_node_type synth_node_type_pipe;
extern const struct synth_node_type synth_node_type_waveshaper;
extern const struct synth_node_type synth_node_type_delay;
extern const struct synth_node_type synth_node_type_tremolo;
extern const struct synth_node_type synth_node_type_iir;

/* (src) is a full EGS file.
 * Bus will borrow it for the duration; caller must keep it alive and unchanged.
 * Bus nodes can "terminate", ie fade out.
 * You can cancel that termination any time before it completes, and the bus fades back in.
 * Busses set (node->finished) either when the song ends and no repeat, or when termination fades all the way out.
 * Context will have an arbitrary set of running busses, but will assume that only zero or one is active, ie not terminating.
 * (rid<0) for PCM printers. We disable global trim on all channels.
 */
int synth_node_bus_configure(struct synth_node *node,const void *src,int srcc,int repeat,int rid);
int synth_node_bus_get_rid(const struct synth_node *node);
double synth_node_bus_get_playhead(const struct synth_node *node);
void synth_node_bus_set_playhead(struct synth_node *node,double s);
int synth_node_bus_is_terminating(const struct synth_node *node); // TRUE for wrong type, on the assumption that you're ignoring terminating ones.
int synth_node_bus_terminate(struct synth_node *node);
int synth_node_bus_unterminate(struct synth_node *node);
int synth_node_bus_set_repeat(struct synth_node *node,int repeat); // May change before or after ready.
void synth_node_bus_midi_event(struct synth_node *node,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,int durms);
int synth_node_bus_get_duration(const struct synth_node *node); // => ms

/* (src) is an EGS channel header, starting with (u8 chid), running through (post).
 */
int synth_node_channel_configure(struct synth_node *node,const void *src,int srcc);
int synth_node_channel_get_chid(const struct synth_node *node);
void synth_node_channel_begin_note(struct synth_node *node,uint8_t noteid,uint8_t velocity,int durms);
void synth_node_channel_set_wheel(struct synth_node *node,uint8_t v);
void synth_node_channel_enable_global_trim(struct synth_node *node,int enable); // Enabled by default

/* We'll play (pcm) once and then signal completion.
 * (pan) is ignored if we're configured mono.
 */
int synth_node_pcm_configure(struct synth_node *node,struct synth_pcm *pcm,float trim,float pan,int rid);
int synth_node_pcm_get_rid(const struct synth_node *node);
double synth_node_pcm_get_playhead(const struct synth_node *node);
void synth_node_pcm_set_playhead(struct synth_node *node,double s);

/* By asking for (pitchenv), you're declaring intent to use it; we'll assume it has some nonzero values.
 */
struct synth_env_runner *synth_node_wave_get_levelenv(struct synth_node *node);
struct synth_env_runner *synth_node_wave_get_pitchenv(struct synth_node *node);
int synth_node_wave_configure(struct synth_node *node,const float *wave,float freq,float triml,float trimr);

/* Asking for (pitchenv) or (rangeenv) declares that they're in play.
 * If you populate (rangeenv), you must scale by the scalar (range) and we ignore the scalar.
 */
struct synth_env_runner *synth_node_fm_get_levelenv(struct synth_node *node);
struct synth_env_runner *synth_node_fm_get_pitchenv(struct synth_node *node);
struct synth_env_runner *synth_node_fm_get_rangeenv(struct synth_node *node);
int synth_node_fm_configure(struct synth_node *node,float modrate,float range,float freq,float triml,float trimr);

/* (src) is multiple (u8 opcode,u8 length,... params), the exact content of the EGS Channel Header "post" field.
 */
int synth_node_pipe_configure(struct synth_node *node,const void *src,int srcc);

/* Pipe components all work about the same, they take the payload of the op command.
 * iir covers four distinct commands, so it takes the opcode too.
 */
int synth_node_waveshaper_configure(struct synth_node *node,const void *src,int srcc);
int synth_node_delay_configure(struct synth_node *node,const void *src,int srcc);
int synth_node_tremolo_configure(struct synth_node *node,const void *src,int srcc);
int synth_node_iir_configure(struct synth_node *node,uint8_t opcode,const void *src,int srcc);

#endif
