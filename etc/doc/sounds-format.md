# Egg Sounds Format

For now, we will only support the Egg-specific "SFG" format.
I'll consider adding WAV (or other PCM formats) in the future, but that's complicated due to resampling.

SFG packs multiple sounds into one resource, each identified by an Index.
There is a text format and a binary format.
Only binary format will ship in ROMs.

## SFG Text Format

Line-oriented text.
'#' begins a line comment, start of line only.
All tokens are separated by whitespace.

Each sound must be fenced with `begin` and `end` lines:
```
begin INDEX [NAME]
  master 0.500
  ...
endvoice
  ...
end
```
The index of each sound must be unique, 1..65535, and they must be in order.
Gaps are OK.
NAME are dropped during compilation, they're just commentary.

The special `master` command may only be the first line of the block.
If omitted, it defaults to 1.
This gets baked into `level` commands during compile; it has no corrollary in the binary format.

Within the Sound Block, the special command `endvoice` separates distinct voices, which play simultaneously.

Voices start with one or more of the following commands, in this order only:
- `shape SHAPE`: sine, square, saw, triangle, noise. "sine" if unspecified.
- `harm COEFFICIENTS`: Apply harmonics. 0..1
- `fm RATE ENV(DEPTH)`: RATE relative to carrier.
- `fmlfo RATE DEPTH`
- `freq ENV(HZ)`: REQUIRED, unless you have `shape noise`.
- `freqlfo RATE DEPTH`

Followed by any of these commands, in any order:
- `level ENV(LEVEL)`: 0..1
- `waveshaper VALUES`: -1..1, typically symmetric and an odd count.
- `gain GAIN [CLIP [GATE]]`
- `delay MS DRY WET STO FBK`
- `detune MS DEPTH`
- `lopass FREQ`
- `hipass FREQ`
- `bpass FREQ WIDTH`
- `notch FREQ WIDTH`

`ENV` arguments are: `VALUE [MS VALUE...]`

There must be at least one `level` command per voice.
If you need a voice to extend beyond its main envelope length, eg with delay, add another `level` that holds at 1 and drops to 0 at the desired cutoff time.

## SFG Binary Format

Begins with 4-byte signature: "\0SFG"

Followed by sounds, each with a 3-byte header:
```
u8  Index, delta from previous +1
u16 Length
```
Your state machine begins at index zero, and increases by 1..256 at each sound.

Body is composed of commands, each with a one-byte opcode:
| Opcd | Name            | Args |
|------|-----------------|------|
| 0x00 | End of Voice    | |
| 0x01 | Shape           | u8:(0..4=sine,square,saw,triangle,noise) |
| 0x02 | Harmonics       | u8:count u0.16...:harmonics |
| 0x03 | FM              | u8.8:relrate ENV:depth(u4.12) |
| 0x04 | FM LFO          | u8.8:rate u4.4:depth |
| 0x05 | Frequency       | ENV:hz |
| 0x06 | Frequency LFO   | u8.8:rate u16:depth(cents) |
| 0x07 | Level           | ENV:0..1 |
| 0x08 | Waveshaper      | u8:count-2 s0.8...:levels |
| 0x09 | Gain            | u8.8:gain u0.8:clip u0.8:gate |
| 0x0a | Delay           | u8:period(4ms) u0.8:dry u0.8:wet u0.8:store u0.8:feedback |
| 0x0b | Detune          | u8:period(4ms) u8:depth |
| 0x0c | Low Pass        | u16:hz |
| 0x0d | High Pass       | u16:hz |
| 0x0e | Band Pass       | u16:center(hz) u16:width(hz) |
| 0x0f | Band Reject     | u16:center(hz) u16:width(hz) |

`ENV` is:
```
u16 Initial value.
u8  Leg count.
...u16 ms.
   u16 value.
```
