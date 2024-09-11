/* synth_formats.h
 * Helpers for compiling and decoding audio files.
 * Particular focus on our private formats.
 */
 
#ifndef SYNTH_FORMATS_H
#define SYNTH_FORMATS_H

#include <stdint.h>
#include "opt/serial/serial.h"

struct synth_midi_reader;

/* High-level binary format conversion.
 ********************************************************************/

/* Given sounds or song in our text format, produce either "\0MSF" sounds or "\0EGS" song.
 * See etc/doc/audio-format.md for details.
 */
int synth_egg_from_text(struct sr_encoder *dst,const void *src,int srcc,const char *path);

/* Produce our text format from either of our binary formats.
 */
int synth_text_from_egg(struct sr_encoder *dst,const void *src,int srcc,const char *path);

/* Convert MIDI to EGS.
 */
int synth_egg_from_midi(struct sr_encoder *dst,const void *src,int srcc,const char *path);

/* Produce a portable MIDI file from our "\0EGS" song format.
 */
int synth_midi_from_egg(struct sr_encoder *dst,const void *src,int srcc,const char *path);

/* Multi-sound text files, precursor to either EGS or MSF.
 *******************************************************************/
 
/* Initialize (src,srcc) and the rest zero.
 * Nexting skips empties and returns zero at EOF, no errors.
 * For MSF files, you'll get (*index) in 1..0xfff.
 * For EGS (ie started with a "song" line), (*index) will be zero.
 */
struct synth_text_reader {
  const char *src;
  int srcc;
  int lineno;
  int srcp;
};
int synth_text_reader_next(int *index,int *lineno,void *dstpp,struct synth_text_reader *reader);

/* Multi-sound binary files: "\0MSF"
 ****************************************************************/
 
struct synth_sounds_writer {
  struct sr_encoder dst;
  int pvindex;
};

/* Write an MSF file incrementally.
 * You must provide files sorted by (index).
 * You may yoink (writer->dst) in lieu of cleanup.
 */
void synth_sounds_writer_cleanup(struct synth_sounds_writer *writer);
int synth_sounds_writer_init(struct synth_sounds_writer *writer);
int synth_sounds_writer_add(struct synth_sounds_writer *writer,const void *src,int srcc,int index,const char *path);
 
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

/* Individual sound or song: "\0EGS"
 **********************************************************************/
 
struct synth_song_parts {
  struct synth_song_channel_part {
    const uint8_t *v;
    int c;
  } channels[16];
  const uint8_t *events;
  int eventsc;
};

int synth_song_split(struct synth_song_parts *parts,const void *src,int srcc);

/* Initialize (src,srcc) yourself, and (srcp==0).
 * Returns <0 on completion. Zero-length fields are legal.
 */
struct synth_song_channel_reader {
  const uint8_t *src;
  int srcc;
  int srcp;
};
int synth_song_channel_reader_next(
  int *opcode,void *dstpp,
  struct synth_song_channel_reader *reader
);

/* Returns length consumed, 0 if complete, or <0 if malformed.
 * Advance (src) yourself.
 * (opcode) is: 0=Delay, 0x90=Note, 0xe0=Wheel. Note that these are MIDI mnemonics, not related to EGS's encoded form.
 * Delay: (duration)
 * Note: (chid,noteid,velocity,duration). Velocity in 0..127 regardless of encoded size.
 * Wheel: (chid,wheel). Wheel in 0..0x3fff to match MIDI. Encoded resolution is 8 bits.
 * Adjacent delays are combined.
 */
struct synth_song_event {
  uint8_t opcode,chid;
  uint8_t noteid,velocity;
  int duration,wheel;
};
int synth_song_event_next(struct synth_song_event *event,const void *src,int srcc);

/* MIDI file.
 ******************************************************************/
 
struct synth_midi_event {
  uint8_t chid,opcode,a,b;
  const void *v; // sysex and meta
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
