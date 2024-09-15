/* synth_node.h
 * Abstract interface to a node of the signal graph.
 * The event graph is also represented by nodes, but the operations involved have concrete interfaces.
 */
 
#ifndef SYNTH_NODE_H
#define SYNTH_NODE_H

#include <stdint.h>

struct synth;
struct synth_pcm;
struct synth_wave;
struct synth_node;
struct synth_node_type;

/* Generic node and type.
 **********************************************************************/

struct synth_node {
  const struct synth_node_type *type;
  struct synth *synth; // WEAK
  int chanc; // 1 or 2
  int ready;
  int finished; // If nonzero, node asserts that all further updates would be noop.
  
  /* Caller must initialize (v).
   * Nodes that generate a new signal must add to (v), not replace it.
   * (node->chanc) is how many channels are expected; caller must be aware.
   */
  void (*update)(float *v,int framec,struct synth_node *node);
};

struct synth_node_type {
  const char *name;
  int objlen;
  void (*del)(struct synth_node *node);
  int (*init)(struct synth_node *node); // (synth,chanc) are set before.
  int (*ready)(struct synth_node *node); // Configuration complete, last chance to fail. Must set (node->update).
};

void synth_node_del(struct synth_node *node);

struct synth_node *synth_node_new(struct synth *synth,const struct synth_node_type *type,int chanc);

int synth_node_ready(struct synth_node *node);

/* Node types.
 *********************************************************************/
 
// Event graph.
extern const struct synth_node_type synth_node_type_bus; // Song or sound effect, child of synth.
extern const struct synth_node_type synth_node_type_channel; // Child of bus.

// Voices.
extern const struct synth_node_type synth_node_type_pcm; // Verbatim playback.
extern const struct synth_node_type synth_node_type_sub; // Noise+bandpass.
extern const struct synth_node_type synth_node_type_wave; // No modulation.
extern const struct synth_node_type synth_node_type_fm; // Bells and whistles.

// Filters.
extern const struct synth_node_type synth_node_type_pipe; // Filter container.
extern const struct synth_node_type synth_node_type_gain; // Multiply and clamp.
extern const struct synth_node_type synth_node_type_waveshaper; // Refined gain alternative.
extern const struct synth_node_type synth_node_type_delay; // Simple delay and feedback.
extern const struct synth_node_type synth_node_type_detune; // Ping-pong in time.
extern const struct synth_node_type synth_node_type_tremolo; // Level LFO.
extern const struct synth_node_type synth_node_type_iir3; // 3-point IIR (lopass,hipass,bpass,notch).

/* Type-specific API.
 ***********************************************************************/

/* Provide an EGS song for this new (pre-ready) bus.
 * We borrow it weakly; caller ensures it will not change while playing.
 */
int synth_node_bus_configure(struct synth_node *node,const void *src,int srcc);

void synth_node_bus_set_songid(struct synth_node *node,int songid,int repeat);
int synth_node_bus_get_songid(struct synth_node *node);

/* Fade out a bus to gradually reduce its master level to zero, then signal completion.
 * Fading is the only way to get a bus "finished". (framec==0) is valid for a hard stop.
 * (force) fade out to change its timing even if already fading out.
 * Wait a bus to noop it for so many frames, typically on a new song that you want to wait for the last song to fade out.
 */
void synth_node_bus_fade_out(struct synth_node *node,int framec,int force);
void synth_node_bus_cancel_fade(struct synth_node *node);
void synth_node_bus_wait(struct synth_node *node,int framec);

/* Manually shove events onto the bus.
 * These must compete with the song if there is one.
 */
void synth_node_bus_event(struct synth_node *node,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,int durms);

/* Channels must have a bus set before ready.
 * (chid) is optional. 0xff for none.
 */
void synth_node_channel_setup(struct synth_node *node,uint8_t chid,struct synth_node *bus);

/* Provide one EGS Channel Header for this new (pre-ready) channel.
 * We may borrow things weakly.
 */
int synth_node_channel_configure(struct synth_node *node,const void *src,int srcc);

void synth_node_channel_event(struct synth_node *node,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,int durms);

/* Ensure everything running on this channel has an expiration date.
 * If (framec>=0), force it to end by that time, with a master fade-out if necessary.
 * Channel promises to eventually set its (finished) flag.
 */
void synth_node_channel_terminate(struct synth_node *node,int framec);

int synth_node_pcm_setup(struct synth_node *node,struct synth_pcm *pcm,float trim,float pan);

int synth_node_wave_setup(
  struct synth_node *node,
  const struct synth_wave *wave,
  float rate,float velocity,int durframes,
  const struct synth_env *env,
  float trim,float pan
);
void synth_node_wave_set_pitch_adjustment(struct synth_node *node,const struct synth_env *env,const float *lfo); // (lfo) is a shared buffer
void synth_node_wave_adjust_rate(struct synth_node *node,float multiplier);

int synth_node_fm_setup(
  struct synth_node *node,
  const struct synth_wave *wave,
  float fmrate,float fmrange,
  float rate,float velocity,int durframes,
  const struct synth_env *env,
  float trim,float pan
);
void synth_node_fm_set_pitch_adjustment(struct synth_node *node,const struct synth_env *env,const float *lfo);
void synth_node_fm_set_modulation_adjustment(struct synth_node *node,const struct synth_env *env,const float *lfo);
void synth_node_fm_adjust_rate(struct synth_node *node,float multiplier);

int synth_node_sub_setup(
  struct synth_node *node,
  float width,
  float rate,float velocity,int durframes,
  const struct synth_env *env,
  float trim,float pan
);
void synth_node_sub_adjust_rate(struct synth_node *node,float multiplier);

int synth_node_pipe_add_op(struct synth_node *node,uint8_t opcode,const uint8_t *arg,int argc);

int synth_node_gain_setup(struct synth_node *node,const uint8_t *arg,int argc);
int synth_node_waveshaper_setup(struct synth_node *node,const uint8_t *arg,int argc);
int synth_node_delay_setup(struct synth_node *node,const uint8_t *arg,int argc);
int synth_node_detune_setup(struct synth_node *node,const uint8_t *arg,int argc);
int synth_node_tremolo_setup(struct synth_node *node,const uint8_t *arg,int argc);
int synth_node_iir3_setup_lopass(struct synth_node *node,float norm);
int synth_node_iir3_setup_hipass(struct synth_node *node,float norm);
int synth_node_iir3_setup_bpass(struct synth_node *node,float norm,float width);
int synth_node_iir3_setup_notch(struct synth_node *node,float norm,float width);

#endif
