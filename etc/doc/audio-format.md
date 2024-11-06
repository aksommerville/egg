# Egg Audio Formats

`sound` and `song` resources are the same thing, except:
- `sound` are limited to 5 seconds duration.
- `sound` may be sourced from WAV, `song` may only be from MIDI.

In the ROM, they are stored as either WAV or EGS.

## From WAV

`sound` resources in WAV format must be mono 16-bit LPCM with a header of exactly 44 bytes.
The only meaningful fields in the header are its signature and the sample rate.
Our compiler will accept a broader variety of WAV and apply those constraints.

## From MIDI

File type must be 1, ie concurrent tracks.
Division must be in 1..0x7fff, ie ticks/qnote, not SMPTE timecodes.
A special Meta event 0x7f at time zero contains the full EGS header.
This is everything between the "\0EGS" signature and the 0xff events introducer, exclusive.
Our compiler will generate an EGS header based on standard MIDI events,
but if one is already present it overrides all standard MIDI configuration.

MIDI files must have a Set Tempo event at time zero, or accept 500 ms/qnote.
They are not allowed to change tempo mid-song.

## EGS

Signature "\0EGS", Channel Headers, Events Introducer, Events.

Channel Header:
- u8 chid 0..15. Don't need to be sorted but must be unique.
- u8 trim 0..255
- u8 mode, see below
- u24 len
- ... payload

Channels not named by a header are implicitly `noop` and any events on that channel will be ignored.
That is mostly a cudgel for the editor, so there's a simple nondestructive way to mute any channel.
`chid` 16..254 are reserved for future use and must be ignored.
Channels with unrecognized mode must be ignored.

Events Introducer:
- u8 0xff

Events are distinguishable by high bits of the first byte:
```
00000000                   : EOF
00tttttt                   : Short Delay, (t) ms
01tttttt                   : Long Delay, (t+1)*64 ms
1000cccc nnnnnnnv vvvvvvtt : Short Note. (t*16) ms.
1001cccc nnnnnnnv vvvttttt : Medium Note. (t+1)*64 ms.
1010cccc nnnnnnnv vvvttttt : Long Note. (t+1)*512 ms.
1011xxxx                   : Reserved, ignore.
1100xxxx xxxxxxxx          : Reserved, ignore.
1101xxxx xxxxxxxx xxxxxxxx : Reserved, ignore.
1110xxxx xxxxxxxx xxxxxxxx : Reserved, ignore.
1111xxxx xxxxxxxx llllllll : Reserved, ignore. Followed by (l) bytes of payload.
```

Channel Mode:
- 0: `noop`
- 1: `drum`
- 2: `wave`
- 3: `fm`
- 4: `sub`

Channel Mode 1 `drum`. Zero or more of:
- u8 noteid 0..127. Must be unique. Stop reading at anything >=0x80 -- it's reserved, not an error.
- u8 trimlo
- u8 trimhi
- u16 len
- ... EGS or WAV file

Channel Mode 2 `wave`. May terminate between fields:
- ... Level env.
- u8 Shape: (0,1,2,3,4)=(custom,sine,square,saw,triangle)
- (u8) Harmonics count, if shape==0
- (...) Harmonics, u16 each.
- ... Pitch env. Value 0x0000..0x8000..0xffff = -2400..0..2400 cents.

Channel Mode 3 `fm`. May terminate between fields:
- ... Level env.
- u8.8 Rate, relative to note.
- u8.8 Range peak.
- ... Pitch env. Value 0x0000..0x8000..0xffff = -2400..0..2400 cents.
- ... Range env.

Channel Mode 4 `sub`:
- ... Level env.
- u16 width, hz

Envelope:
- u8 flags:
- - 01 init: Explicit initial value, otherwise starts at zero.
- - 02 velocity: Contains two paths, interpolate between them based on velocity.
- - 04 sustain
- - f8 reserved, zero.
- (u16) inivlo, if (init)
- (u16) inivhi, if (init) and (velocity)
- (u8) sustain index, if (sustain). Initial point is not sustainable; this is an index in the counted points below.
- u8 pointc 0..16. Zero is legal, for a constant. If (sustain), the limit is 15.
- ... points:
- - vlq2 delay ms. Sequences longer than 2 bytes are forbidden. ie range is 0..16383 ms. Low or only.
- - u16 v
- - (vlq2) delay ms hi, if (velocity).
- - (u16) v hi, if (velocity).
