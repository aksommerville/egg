/* synth_formats.h
 * Helpers for compiling and decoding audio files.
 * Particular focus on our private formats.
 */
 
#ifndef SYNTH_FORMATS_H
#define SYNTH_FORMATS_H

#include <stdint.h>

struct sr_encoder;
struct synth_midi_reader;

/* High-level binary format conversion.
 ********************************************************************/

/* Given sounds or song in our text format, produce either "\0ESS" sounds or "\xbe\xee\xeep" song.
 * See etc/doc/audio-format.md for details.
 */
int synth_egg_from_text(struct sr_encoder *dst,const void *src,int srcc,const char *path);

/* Produce our text format from either of our binary formats.
 */
int synth_text_from_egg(struct sr_encoder *dst,const void *src,int srcc,const char *path);

/* Produce a portable MIDI file from our "\xbe\xee\xeep" song format.
 */
int synth_midi_from_egg(struct sr_encoder *dst,const void *src,int srcc,const char *path);

/* Multi-sound binary files: "\0ESS"
 ****************************************************************/
 
struct synth_sounds_reader {
  const uint8_t *src;
  int srcc;
  int srcp;
  int index;
};

/* Basically never fails.
 * If this is not a multi-sound resource, we arrange to spit its entire content back as index zero.
 */
int synth_sounds_reader_init(struct synth_sounds_reader *reader,const void *src,int srcc);

/* <0 if malformed, 0 if complete, or binary length of content at (*dstpp).
 * Zero-length entries are quietly skipped.
 */
int synth_sounds_reader_next(
  int *index,void *dstpp,
  struct synth_sounds_reader *reader
);

/* Individual sound or song: "\xbe\xee\xeep"
 **********************************************************************/
 
struct synth_song_parts {
  struct synth_song_channel_part {
    const uint8_t *v;
    int c;
  } channels[8];
  const uint8_t *events;
  int eventsc;
};

int synth_song_split(struct synth_song_parts *parts,const void *src,int srcc);

/* MIDI file.
 ******************************************************************/
 
struct synth_midi_event {
  uint8_t chid,opcode,a,b;
  const void *v; // sysex and meta
  int c;
};

void synth_midi_reader_del(struct synth_midi_reader *reader);

/* Fails if framing or MThd malformed.
 * (path) is borrowed if not null. Must remain in scope throughout reader's life.
 * Null path to suppress logging.
 */
struct synth_midi_reader *synth_midi_reader_new(const void *src,int srcc,const char *path);

/* If a delay is pending, return the delay in milliseconds and do not populate (event).
 * If an event is ready, return zero and populate (event).
 * At end of file, return SYNTH_MIDI_EOF.
 * Anything else is an error: -2 if we logged it, -1 if not.
 */
int synth_midi_reader_next(
  struct synth_midi_event *event,
  struct synth_midi_reader *reader
);

#define SYNTH_MIDI_EOF -3

#endif
