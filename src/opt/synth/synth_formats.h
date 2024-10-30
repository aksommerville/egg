/* synth_formats.h
 * Helpers for compiling and decoding audio files.
 * Particular focus on our private formats.
 */
 
#ifndef SYNTH_FORMATS_H
#define SYNTH_FORMATS_H

#include <stdint.h>
#include "opt/serial/serial.h"

struct synth_midi_reader;
struct synth_pcm;

#define EGS_MODE_NOOP 0
#define EGS_MODE_DRUM 1
#define EGS_MODE_WAVE 2
#define EGS_MODE_FM   3
#define EGS_MODE_SUB  4

/* Read EGS.
 *******************************************************************/
 
struct synth_egs_reader {
  const uint8_t *v;
  int c,p;
  char stage; // 0,'!','c','e'
};

struct synth_egs_channel {
  uint8_t chid; // 0..254; only 0..15 are currently meaningful.
  uint8_t master; // 0..255
  uint8_t mode; // EGS_MODE_*, but we don't validate.
  const uint8_t *v;
  int c;
};

struct synth_egs_event {
  int delay; // If nonzero, it's a Delay event. ms
  int chid; // Valid for Note events.
  int noteid; // If <0x80, it's a Note event.
  int velocity; // Note events only. 1..127 (regardless of encoded resolution)
  int duration; // ms, Note events only.
  const uint8_t *v; // Full encoded event, if delay==0 and noteid==0xff
  int c;
};

/* Succeeds if there's a valid header.
 * Follow this by synth_egs_reader_next_channel().
 */
int synth_egs_reader_init(struct synth_egs_reader *reader,const void *src,int srcc);

/* Both return 0 if we're done with that section, 1 if something is returned, or <0 for real errors.
 * You must read channels to completion before starting events; events will report "done" if you haven't got there yet.
 * All errors are sticky.
 * There are events not yet defined which we nevertheless can measure.
 * Those will return as {delay:0,noteid:255} with (v,c) containing the full encoded event.
 */
int synth_egs_reader_next_channel(struct synth_egs_channel *dst,struct synth_egs_reader *reader);
int synth_egs_reader_next_event(struct synth_egs_event *dst,struct synth_egs_reader *reader);

/* Alternative to multiple synth_egs_reader_next_channel() calls; read all remaining channel headers as a single block.
 * That's the chunk we pack into a MIDI Meta event.
 */
int synth_egs_reader_all_channels(void *dstpp,struct synth_egs_reader *reader);

/* Read MIDI.
 ******************************************************************/
 
struct synth_midi_event {
  uint8_t chid,opcode,a,b;
  const uint8_t *v; // sysex and meta
  int c;
};

void synth_midi_reader_del(struct synth_midi_reader *reader);

/* Fails if framing or MThd malformed.
 * (src) must remain in scope and constant throughout reader's life.
 */
struct synth_midi_reader *synth_midi_reader_new(const void *src,int srcc);

// Return to the start of the file.
void synth_midi_reader_reset(struct synth_midi_reader *reader);

/* If a delay is pending, return the delay in milliseconds and do not populate (event).
 * If an event is ready, return zero and populate (event).
 * At end of file, return SYNTH_MIDI_EOF.
 * Anything else is an error.
 */
int synth_midi_reader_next(
  struct synth_midi_event *event,
  struct synth_midi_reader *reader
);

#define SYNTH_MIDI_EOF -3

#endif
