# Egg Audio Formats

There are two resource types "sounds" and "song".
They are actually the same thing, except "sounds" contains multiple resources, and has a few extra constraints.

By convention, sounds:1 should contain the GM drum kit in 35..81.

## Sounds, Binary Format

Starts with 4-byte signature: "\0ESS"

Read the rest with a state machine:
```
u16 index = 0
```

Then read sounds:
```
u8 index delta - 1
u16 length
... payload
```

It is not possible for index to be out of order, duplicated, or <1.
Index >0xffff is an error.
Zero length is legal and necessary, if you need to skip more than 256 indices.
Zero length should be treated as "missing".
Payload of each sound is a song as defined below. (including its own signature and everything)
Extra constraints:
- Mono output only. Channel pans are ignored, and all post ops emit mono.
- Drum channels are forbidden.
- Length limit 5 seconds.

## Song, Binary Format

- 4-byte signature: "\xbe\xee\xeep"
- Channel Headers:
- - u16 length.
- - Any number of Channel Header fields, each starting (u8 id,u8 len).
- 2 zero bytes to terminate Channel Headers.
- Events.

Up to 8 channels are allowed, and you can't skip IDs.
The header list does not implicitly terminate after the eighth, and decoders should tolerate extra headers.

| CHFID | Length | Description |
|-------|--------|-------------|
| 0x00  | 0      | End of Header. Skip the remainder. |
| 0x01  | 1      | (u0.8) Master. |
| 0x02  | 1      | (u8) Pan. 0=mono, 1=left..128=center..255=right. |
| 0x03  | 5      | (u16 rid,s8 bias,u8 trimlo,u8 trimhi) Drums. |
| 0x04  | 2      | (u16) Wheel Range, cents. |
| 0x05  | 2      | (u16) Subtractive Bandwidth, hz. |
| 0x06  | 1      | (u8) Shape. 0..3 = sine,square,saw,triangle |
| 0x07  | ...    | (u0.16...) Harmonics. |
| 0x08  | 4      | (u8.8 rate,u8.8 maxrange) FM. |
| 0x09  | ...    | FM Range Env. Values 0..1. |
| 0x0a  | 5      | FM Range LFO. |
| 0x0b  | ...    | Pitch Env. Values signed, cents. |
| 0x0c  | 5      | Pitch LFO. Values in cents. |
| 0x0d  | ...    | Level Env. Values 0..1. |
| 0x0e  | 5      | Level LFO. |
| 0x80  | 4      | (u8.8 gain,u0.8 clip,u0.8 gate) Gain. |
| 0x81  | ...    | (s0.16...) Waveshaper. |
| 0x82  | 6      | (u16 ms,u0.8 dry,u0.8 wet,u0.8 store,u0.8 feedback) Delay. |
| 0x83  | 5      | (u16 ms,u16 cents,u0.8 phase) Detune. |
| 0x84  | 5      | (u16 ms,u0.16 depth,u0.8 phase) Tremolo. |
| 0x85  | 2      | (u16 hz) Lopass. |
| 0x86  | 2      | (u16 hz) Hipass. |
| 0x87  | 4      | (u16 hz,u16 width) Bandpass. |
| 0x88  | 4      | (u16 hz,u16 width) Notch. |

All "Env" fields have the same shape, terminated implicitly:
```
  2 init lo.
  2 init hi.
  1 Sustain index, OOB for no sustain. [0] is the 'init' point; first proper point is [1].
  ... Points:
       2 ms lo.
       2 ms hi.
       2 value lo.
       2 value hi.
```

All "LFO" fields have the same shape:
```
  2 Period, ms.
  2 Depth.
  1 Phase.
```

Fields 0x00..0x7f may only appear once each.

If field 0x03 (Drums) is present, fields 0x04..0x0e are forbidden.

If field 0x05 (Subtractive Bandwidth) is present, fields 0x06..0x0c are forbidden.

Field 0x0d (Level Env) is required, unless 0x03 (Drums) in play.

Order is significant for fields 0x80 and above, and no field <0x80 is permitted after the first >=0x80.

Fields 0x40..0x7f and 0xc0..0xff are reserved and decoders must fail if unknown.
Other unknown fields may be ignored.

Events are identifiable by their leading byte:
```
00000000                   : End of Song.
0mmmmmmm                   : (m) nonzero. Delay (m) ms.
100cccnn nnnnnvvv          : Fire and Forget.
101cccnn nnnnnvvv dddddddd : Short Note. (d+1) ms.
110cccnn nnnnnvvv dddddddd : Long Note. (d+1) 32ms +256.
111cccvv vvvvvvvv          : Pitch Wheel.
```

## Input From MIDI Files

By and large, you can give eggdev plain MIDI files and it will produce a sensible song.

- Meta 0xf0 contains a single Channel Header verbatim. Precede with Meta 0x20 Channel Prefix.
- Control 0x07 (Volume) and 0x0a (Pan) at time zero override any prior Channel Header.
- Bank Select and Program Change may be used to select Channel Headers defined elsewhere at build time. TODO Define the mechanism for this.
- Aftertouch and most Control Change are discarded.
- Notes can't be held longer than 2.25 seconds.
- Pitch Wheel and Velocity quantize a bit coarser than MIDI.
- Only channels 0..7 can be used. Our compiler will change channel IDs if necessary, and fail only if more than 8 channels are actually used.

## Input From Text Files

Sounds should be stored in plain text files.
These are not intended for human consumption; we're only using text because it's a little easier on version control and web tooling.
Line-oriented text, '#' begins a comment at start of line only.

Single-sound files must begin with "song" on its own line.

Multi-sound files must precede each sound with "sound INDEX [NAME]".
INDEX in 1..65535 and must be in order.
NAME is dropped at compile, used only by editor.

Sound begins with Channel Headers, each beginning with "channel CHID".
Must be in order. Gaps are permitted (NB not permitted in binary).

In Channel Header, each line corresponds to one field:
```
0x01: master TRIM(0..1)
0x02: pan PAN(-1..1 or "mono")
0x03: drums RID BIAS(-128..127) TRIMLO(0..1) TRIMHI(0..1)
0x04: wheel CENTS(0..65535)
0x05: sub HZ(0..65535)
0x06: shape sine|square|saw|triangle|U8
0x07: harmonics COEF(0..1)...
0x08: fm RATE(u8.8) RANGE(u8.8)
0x09: fmenv INITLO [MS VLO...] [.. INITHI [MS VHI...]] # '*' after one value to mark sustain
0x0a: fmlfo MS(0..65535) DEPTH(0..1) PHASE(0..1)
0x0b: pitchenv CENTSLO [MS CENTSLO...] [.. CENTSHI [MS CENTSHI...]] # '*' after one value to mark sustain
0x0c: pitchlfo MS(0..65535) CENTS(0..65535) PHASE(0..1)
0x0d: level ZERO [MS VLO...] [.. ZERO [MS VHI...]] # '*' after one value to mark sustain; First and last value must be zero.
0x0e: levellfo MS(0..65535) DEPTH(0..1) PHASE(0..1)
0x80: gain GAIN(u8.8) CLIP(0..1) GATE(0..1)
0x81: waveshaper LEVEL(s0.16)...
0x82: delay MS(0..65535) DRY(0..1) WET(0..1) STORE(0..1) FEEDBACK(0..1)
0x83: detune MS(0..65535) CENTS(0..65535) PHASE(0..1)
0x84: tremolo MS(0..65535) DEPTH(0..1) PHASE(0..1)
0x85: lopass HZ(0..65535)
0x86: hipass HZ(0..65535)
0x87: bpass HZ(0..65535) WIDTH(0..65535)
0x88: notch HZ(0..65535) WIDTH(0..65535)
```

Channel Header lines may also be a plain hex dump, including opcode but excluding length.
(this is only provided so that the return trip, binary to text, has a universal fallback).

After Channel Headers, "events" on its own line to begin events:
```
delay MS
CHID wheel V(-1.0..1.0)
CHID NOTEID VELOCITY(0..127) DURMS
```
