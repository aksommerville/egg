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
A special Meta event 0xf0 at time zero contains the full EGS header.
That is exactly an EGS file, signature and all, but events will be ignored and should be omitted.
Our compiler will generate an EGS header based on standard MIDI events,
but if one is already present it overrides all standard MIDI configuration.

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
1011xxxx                   : Reserved, illegal.
11xxxxxx                   : Reserved, illegal.
```

Channel Mode:
- 0: `noop`
- 1: `drum`
- 2: `wave`
- 3: `fm`
- 4: `sub`

Channel Mode 1 `drum`. Zero or more of:
- u8 noteid 0..127. Must be unique.
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
- - vlq2 delay ms. Sequences longer than 2 bytes are forbidden. ie range is 0..16383 ms.
- - u16 v
- - (vlq2) delay ms hi, if (velocity).
- - (u16) v hi, if (velocity).

## XXX ------------------------------------------------------------------------------------------------------ XXX

There are two standard resource types "sound" and "song".

`sound` resources can be sourced from either WAV (if you insist) or MIDI (recommended).
Duration is limited to 5 seconds.
If MIDI, Drum channels are forbidden.
Stored in ROM as EGS (from MIDI) or PCM (from WAV).

`song` resources are sourced from MIDI files.
Stored in ROM as EGS.

## PCM Format, from WAV

```
u32 Signature = "\0PCM"
u32 Rate, hz
... Samples, s16le
```

**Note that samples are little-endian, and everything else is big-endian.**

Our compiler will drop extra channels and force 16-bit LPCM.
Beyond that, the relation between WAV and PCM is obvious.

## From MIDI

Meta 0x20 MIDI Channel Prefix, followed by Meta 0xf0 EGS Header (our invention).
Payload of 0xf0 is the EGS Channel Header defined below, starting from "u8 Mode".
Channel ID from the previous 0x20, Master from Control 0x07, Pan from Control 0x0a.

If any of those events occurs after time zero, they will be ignored.
Compiler issues a warning in that case.

If the EGS header is missing for a channel, compiler should issue a warning and emit a canned default.
Do not use Program Change.

Channel config is omitted for channels with no notes.

If you set a channel's volume to zero, compiler omits all of its config and notes.

Meta 0x04 Instrument Name is preserved as a comment for the editor. It is dropped during compile.

## EGS Format

Universal Header, multiple Channel Header, u8 0xff Events Introducer, multiple Event.

Song ends immediately after the last Delay event.
Note durations exceeding that length is an error.

Universal Header:
-  u32 Signature = "\0EGS"
-  u16 Tempo, ms/qnote. Largely advisory. Maybe allow delay etc to scale against it?

Event, leading byte describes it:
- `00000000`: EOF. stop reading.
- `00tttttt`: Short Delay. (t) ms
- `01tttttt`: Long Delay. (t+1)*64 ms
- `1000cccc nnnnnnnv vvvvvvtt`: Short Note. (t)*16 ms. Typically zero.
- `1001cccc nnnnnnnv vvvttttt`: Medium Note. (t+1)*64 ms.
- `1010cccc nnnnnnnv vvvttttt`: Long Note. (t+1)*512 ms. Note that Medium and Long overlap a little.
- `1011cccc vvvvvvvv`: Wheel. 0..128..255
- `11xxxxxx`: Reserved, illegal.
  
Channel Header:
-  u8  Channel ID 0..15
-  u8  Master 0..255
-  u8  Pan 0..128..255=Left..Center..Right
-  u8  Mode
-  u16 Mode config length: Mandatory lengths per mode, you don't get to make something up here.
-  ... Mode config
-  u16 Post length
-  ... Post
  
Mode:
- 0: Noop. Channel is ignored, and no sense providing Master, Mode Config, or Post either.
- 1: Drums. Forbidden in sounds.
- 2: Flat Wave.
- 3: FM.
- 4: Subtractive.

Mode 1: Drums, zero or more of:
- u8  noteid
- u8  pan 0..128..255
- u8  trimlo
- u8  trimhi
- u16 length
- ... EGS file
- Channel pan is ignored.
- Inner files are like `sound` resources: No drums, and 5-second limit.
- Entries need not be sorted, but duplicate (noteid) is an error.
    
Mode 2: Flat Wave:
-  u16 wheelrange, cents
-  12  levelenv
-  16  pitchenv. Cents, bias 0x8000.
-  u8  shape: (0,1,2,3,4)=(custom,sine,square,saw,triangle)
-  u8  coefc. Must be zero if (shape) nonzero.
-  ... coefv. u16 * coefc
  
Mode 3: FM:
-  u16 wheelrange, cents
-  12  levelenv
-  16  pitchenv. Cents, bias 0x8000.
-  u16 modrate. Relative, unit 0x0100.
-  u16 modrange. Unit 0x0100.
-  16  rangeenv
  
Mode 4: Subtractive:
- 12  levelenv
- u16 width, hz
- Decoder applies appropriate gain.
- No wheel events.
  
levelenv, 12 bytes:
-  u8  atktlo
-  u8  atkthi
-  u8  atkvlo
-  u8  atkvhi
-  u8  dectlo
-  u8  decthi
-  u8  susvlo
-  u8  susvhi
-  u16 rlstlo
-  u16 rlsthi
  
altenv, 16 bytes:
-  u16 inivlo
-  u16 inivhi
-  u16 atkvlo
-  u16 atkvhi
-  u16 susvlo
-  u16 susvhi
-  u16 rlsvlo
-  u16 rlsvhi

Post is packed fields:
-  u8  opcode
-  u8  length
-  ... param
  
Post ops:
- 0x01 waveshaper
- - u16 output levels starting at zero. Positive side only; decoder infers the negatives. No bias: values are 0..65535
- 0x02 delay
- - u16 period ms
- - u8  mix. 0..255=dry..wet
- - u8  store
- - u8  feedback
- 0x03 tremolo
- - u16 period ms
- - u8  depth
- - u8  phase
- 0x04 lopass
- - u16 cutoff, hz
- 0x05 hipass
- - u16 cutoff, hz
- 0x06 bpass
- - u16 center, hz
- - u16 width, hz
- 0x07 notch
- - u16 center, hz
- - u16 width, hz
