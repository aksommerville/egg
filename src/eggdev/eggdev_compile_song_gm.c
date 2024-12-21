#include "eggdev_internal.h"
#include "opt/synth/synth_formats.h"

/* Instrument data.
 */
 
static const struct eggdev_canned_gm_instrument {
  int mode;
  int c;
  const void *v;
} eggdev_canned_gm_instruments[128]={
#define _(pid,_mode,src) [pid]={.mode=_mode,.c=sizeof(src)-1,.v=src},

  /* 0..7: Piano */

  _(0x00, // Acoustic Grand Piano TODO
    EGS_MODE_WAVE,
    "\x04" // level env flags
    "\x01" // sustain index
    "\x03" // point count
      "\x10\xff\xff" // attack
      "\x10\x40\x00" // decay
      "\x70\x00\x00" // release
    "\x01" // shape
  )
  _(0x01,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Bright Acoustic Piano
  _(0x02,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Electric Grand Piano
  _(0x03,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Honky-Tonk
  _(0x04,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO EP1
  _(0x05,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO EP2
  _(0x06,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Harpsichord
  _(0x07,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Clavinet
  
  /* 8..15: Chromatic */
  
  _(0x08,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Celesta
  _(0x09,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Glockenspiel
  _(0x0a,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Music Box
  _(0x0b,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Vibraphone
  _(0x0c,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Marimba
  _(0x0d,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Xylophone
  _(0x0e,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Tubular Bells
  _(0x0f,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Dulcimer
  
  /* 16..23: Organ */
  
  _(0x10,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Drawbar Organ
  _(0x11,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Percussive Organ
  _(0x12,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Rock Organ
  _(0x13,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Church Organ
  _(0x14,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Reed Organ
  _(0x15,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO French Accordion
  _(0x16,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Harmonica
  _(0x17,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Tango Accordion
  
  /* 24..31: Guitar */
  
  _(0x18,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Nylon Guitar
  _(0x19,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Steel Guitar
  _(0x1a,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Jazz Guitar
  _(0x1b,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Clean Electric Guitar
  _(0x1c,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Muted Electric Guitar
  _(0x1d,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Overdrive Guitar
  _(0x1e,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Distortion Guitar
  _(0x1f,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Guitar Harmonics
  
  /* 32..39: Bass */
  
  _(0x20,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Acoustic Bass
  _(0x21,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Finger Bass
  _(0x22,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Picked Bass
  _(0x23,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Fretless Bass
  _(0x24,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Slap Bass 1
  _(0x25,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Slap Bass 2
  _(0x26,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Synth Bass 1
  _(0x27,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Synth Bass 2
  
  /* 40..47: Solo String */
  
  _(0x28,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Violin
  _(0x29,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Viola
  _(0x2a,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Cello
  _(0x2b,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Contrabass
  _(0x2c,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Tremolo Strings
  _(0x2d,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Pizzicato Strings
  _(0x2e,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Harp
  _(0x2f,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Timpani
  
  /* 48..55: String Ensemble */
  
  _(0x30,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Strings 1
  _(0x31,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Strings 2
  _(0x32,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Synth Strings 1
  _(0x33,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Synth Strings 2
  _(0x34,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Aaah
  _(0x35,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Oooh
  _(0x36,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Synth Voice
  _(0x37,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Orchestra Hit
  
  /* 56..63: Brass */
  
  _(0x38,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Trumpet
  _(0x39,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Trombone
  _(0x3a,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Tuba
  _(0x3b,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Muted Trumpet
  _(0x3c,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO French Horn
  _(0x3d,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Brass Section
  _(0x3e,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Synth Brass 1
  _(0x3f,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Synth Brass 2
  
  /* 64..71: Solo Reed */
  
  _(0x40,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Soprano Sax
  _(0x41,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Alto Sax
  _(0x42,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Tenor Sax
  _(0x43,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Baritone Sax
  _(0x44,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Oboe
  _(0x45,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO English Horn
  _(0x46,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Bassoon
  _(0x47,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Clarinet
  
  /* 72..79: Solo Flute */
  
  _(0x48,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Piccolo
  _(0x49,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Flute
  _(0x4a,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Recorder
  _(0x4b,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Pan Flute
  _(0x4c,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Bottle
  _(0x4d,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Shakuhachi
  _(0x4e,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Whistle
  _(0x4f,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Ocarina
  
  /* 80..87: Synth Lead */
  
  _(0x50,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Square
  _(0x51,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Saw
  _(0x52,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Calliope
  _(0x53,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Chiffer
  _(0x54,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Charang
  _(0x55,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Voice Solo
  _(0x56,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Fifths
  _(0x57,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Bass+Lead
  
  /* 88..95: Synth Pad */
  
  _(0x58,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Fantasia
  _(0x59,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Warm
  _(0x5a,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Polysynth
  _(0x5b,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Space Choir
  _(0x5c,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Bowed Glass
  _(0x5d,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Metallic
  _(0x5e,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Halo
  _(0x5f,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Sweep
  
  /* 96..103: Synth Effect */
  
  _(0x60,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Rain
  _(0x61,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Soundtrack
  _(0x62,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Crystal
  _(0x63,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Atmosphere
  _(0x64,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Brightness
  _(0x65,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Goblins
  _(0x66,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Echoes
  _(0x67,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Sci-Fi
  
  /* 104..111: Ethnic */
  
  _(0x68,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Sitar
  _(0x69,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Banjo
  _(0x6a,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Shamisen
  _(0x6b,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Koto
  _(0x6c,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Kalimba
  _(0x6d,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Bagpipe
  _(0x6e,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Fiddle
  _(0x6f,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Shanai
  
  /* 112..119: Percussion */
  
  _(0x70,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Tinkle Bell
  _(0x71,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Agogo
  _(0x72,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Steel Drums
  _(0x73,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Wood Block
  _(0x74,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Taiko
  _(0x75,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Melodic Tom
  _(0x76,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Synth Tom
  _(0x77,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Reverse Cymbal
  
  /* 120..127: Insert Joke Here */
  
  _(0x78,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Fret Noise
  _(0x79,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Breath
  _(0x7a,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Seashore
  _(0x7b,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Bird
  _(0x7c,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Telephone
  _(0x7d,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Helicopter
  _(0x7e,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Applause
  _(0x7f,EGS_MODE_WAVE,"\x04\x01\x03\x10\xff\xff\x10\x40\x00\x70\x00\x00\x01")//TODO Gunshot
  
#undef _
};

/* Drum data.
 * Payloads are full EGS or WAV files. (can't imagine we'll use WAV here).
 */
 
static const struct eggdev_canned_gm_drum {
  int noteid;
  int c;
  const void *v;
} eggdev_canned_gm_drums[]={
#define _(id,src) {.noteid=id,.c=sizeof(src)-1,.v=src},
  _(0x23,"\0EGS\xff\x01")//TODO Acoustic Bass Drum
  _(0x24,"\0EGS\xff\x01")//TODO Bass Drum 1
  _(0x25,"\0EGS\xff\x01")//TODO Side Stick
  _(0x26,"\0EGS\xff\x01")//TODO Acoustic Snare
  _(0x27,"\0EGS\xff\x01")//TODO Hand Clap
  _(0x28,"\0EGS\xff\x01")//TODO Electric Snare
  _(0x29,"\0EGS\xff\x01")//TODO Lo Floor Tom
  _(0x2a,"\0EGS\xff\x01")//TODO Closed Hat
  _(0x2b,"\0EGS\xff\x01")//TODO Hi Floor Tom
  _(0x2c,"\0EGS\xff\x01")//TODO Pedal Hat
  _(0x2d,"\0EGS\xff\x01")//TODO Lo Tom
  _(0x2e,"\0EGS\xff\x01")//TODO Open Hat
  _(0x2f,"\0EGS\xff\x01")//TODO Lo Mid Tom
  _(0x30,"\0EGS\xff\x01")//TODO Hi Mid Tom
  _(0x31,"\0EGS\xff\x01")//TODO Crash 1
  _(0x32,"\0EGS\xff\x01")//TODO Hi Tom
  _(0x33,"\0EGS\xff\x01")//TODO Ride 1
  _(0x34,"\0EGS\xff\x01")//TODO Chinese Cymbal
  _(0x35,"\0EGS\xff\x01")//TODO Ride Bell
  _(0x36,"\0EGS\xff\x01")//TODO Tambourine
  _(0x37,"\0EGS\xff\x01")//TODO Splash Cymbal
  _(0x38,"\0EGS\xff\x01")//TODO Cowbell
  _(0x39,"\0EGS\xff\x01")//TODO Crash 2
  _(0x3a,"\0EGS\xff\x01")//TODO Vibraslap
  _(0x3b,"\0EGS\xff\x01")//TODO Ride 2
  _(0x3c,"\0EGS\xff\x01")//TODO Hi Bongo
  _(0x3d,"\0EGS\xff\x01")//TODO Lo Bongo
  _(0x3e,"\0EGS\xff\x01")//TODO Mute Hi Conga
  _(0x3f,"\0EGS\xff\x01")//TODO Open Hi Conga
  _(0x40,"\0EGS\xff\x01")//TODO Lo Conga
  _(0x41,"\0EGS\xff\x01")//TODO Hi Timbale
  _(0x42,"\0EGS\xff\x01")//TODO Lo Timbale
  _(0x43,"\0EGS\xff\x01")//TODO Hi Agogo
  _(0x44,"\0EGS\xff\x01")//TODO Lo Agogo
  _(0x45,"\0EGS\xff\x01")//TODO Cabasa
  _(0x46,"\0EGS\xff\x01")//TODO Maracas
  _(0x47,"\0EGS\xff\x01")//TODO Short Whistle
  _(0x48,"\0EGS\xff\x01")//TODO Long Whistle
  _(0x49,"\0EGS\xff\x01")//TODO Short Guiro
  _(0x4a,"\0EGS\xff\x01")//TODO Long Guiro
  _(0x4b,"\0EGS\xff\x01")//TODO Claves
  _(0x4c,"\0EGS\xff\x01")//TODO Hi Wood Block
  _(0x4d,"\0EGS\xff\x01")//TODO Lo Wood Block
  _(0x4e,"\0EGS\xff\x01")//TODO Mute Cuica
  _(0x4f,"\0EGS\xff\x01")//TODO Open Cuica
  _(0x50,"\0EGS\xff\x01")//TODO Mute Triangle
  _(0x51,"\0EGS\xff\x01")//TODO Open Triangle
#undef _
};

/* Instruments.
 */
 
int eggdev_encode_gm_instrument(struct sr_encoder *dst,int pid) {
  pid&=0x7f; // Strip bank.
  const struct eggdev_canned_gm_instrument *ins=eggdev_canned_gm_instruments+pid;
  if (!ins->mode) { // We shouldn't allow any noops in the data set, but just in case, replace with program zero.
    ins=eggdev_canned_gm_instruments;
  }
  if (sr_encode_u8(dst,ins->mode)<0) return -1;
  if (sr_encode_intbe(dst,ins->c,3)<0) return -1;
  if (sr_encode_raw(dst,ins->v,ins->c)<0) return -1;
  return 0;
}

/* Single drum, inner file only.
 */
 
static int eggdev_encode_gm_drum_inner(struct sr_encoder *dst,int noteid) {
  const struct eggdev_canned_gm_drum *drum=eggdev_canned_gm_drums;
  int lo=0,hi=sizeof(eggdev_canned_gm_drums)/sizeof(eggdev_canned_gm_drums[0]);
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct eggdev_canned_gm_drum *q=eggdev_canned_gm_drums+ck;
         if (noteid<q->noteid) hi=ck;
    else if (noteid>q->noteid) lo=ck+1;
    else { drum=q; break; }
  }
  return sr_encode_raw(dst,drum->v,drum->c);
}

/* Single drum, wrapper.
 */
 
static int eggdev_encode_gm_drum(struct sr_encoder *dst,int noteid) {
  if (sr_encode_u8(dst,noteid)<0) return -1;
  if (sr_encode_u8(dst,0x80)<0) return -1; // trimlo
  if (sr_encode_u8(dst,0xff)<0) return -1; // trimhi
  int lenp=dst->c;
  if (sr_encode_raw(dst,"\0\0",2)<0) return -1;
  int err=eggdev_encode_gm_drum_inner(dst,noteid);
  if (err<0) return err;
  int len=dst->c-lenp-2;
  if ((len<0)||(len>0xffff)) return -1;
  ((uint8_t*)dst->v)[lenp+0]=len>>8;
  ((uint8_t*)dst->v)[lenp+1]=len;
  return 0;
}

/* Drums.
 */
 
int eggdev_encode_gm_drums(struct sr_encoder *dst,const uint8_t *notebits/*16*/) {
  if (sr_encode_u8(dst,EGS_MODE_DRUM)<0) return -1;
  int lenp=dst->c;
  if (sr_encode_raw(dst,"\0\0\0",3)<0) return -1;
  int noteid_major=0;
  for (;noteid_major<16;noteid_major++) {
    if (!notebits[noteid_major]) continue;
    int noteid_minor=0,mask=1;
    for (;noteid_minor<8;noteid_minor++,mask<<=1) {
      if (!(notebits[noteid_major]&mask)) continue;
      if (eggdev_encode_gm_drum(dst,(noteid_major<<3)|noteid_minor)<0) return -1;
    }
  }
  int len=dst->c-lenp-3;
  if ((len<0)||(len>0xffffff)) return -1;
  ((uint8_t*)dst->v)[lenp+0]=len>>16;
  ((uint8_t*)dst->v)[lenp+1]=len>>8;
  ((uint8_t*)dst->v)[lenp+2]=len;
  return 0;
}
