# Egg Audio Formats

There are two resource types "sounds" and "song".
They are almost the same thing.

By convention, sounds:1 should contain the GM drum kit in 35..81.

## Multi-Sound File

`sounds` resources are always in this MSF format.
It's a dumb archive that indexes members by a 12-bit nonzero ID.
Members should be in EGGSND or WAV format. (though the MSF itself doesn't care).
Size limit 16 MB per member.

Rationale for using an extra layer of archiving is that you'll often want all the MIDI drums packed together.
And it might help when sharing assets.

Starts with 4-byte signature: "\0MSF"

Read the rest with a state machine: `u16 index = 0`

Rest of file is sounds, each starting with a 4-byte introducer:
```
u8 index delta - 1
u24 length
... body
```

## MSF/EGGSND Text

Line-oriented text.
'#' begins a line comment, start of line only.

If the first non-empty line is exactly "song", the remainder is a single EGGSND.
Otherwise, the remainder is organized into blocks beginning `sound INDEX [NAME]`.
INDEX in 1..4095 and must be in order.

Lines "channel CHID" begin a Channel Header section.
Channel Headers must come before Events, and must be in order by CHID.
It's OK to skip CHIDs.
See the table below ("EGGSND Channel Headers").
Lines may begin with a `name` or `fldid` from there.
After that key, the rest of the line is a hex dump.
I've resisted the temptation to provide sensible per-field argument formats,
reasoning that even if we got that right, it would still be painful to actually use.
Use the GUI editor.

A line containing exactly "events" ends the channel headers and begins event dump:
```
delay MS
note CHID NOTEID VELOCITY(0..127) DURATION
wheel CHID 0..128..255
```

## EGGSND Binary Songs and Sounds

Our preferred format EGGSND is suitable for both sound effects and songs.
It comprises up to 16 channel headers, followed by an arbitrary event stream.
Timing inside the song is always in milliseconds and we don't preserve tempo information.

Starts with 4-byte signature: "\0EGS"

Followed by Channel Headers, sequentially:
```
u16 length, nonzero
... body
```
Followed by 2 zero bytes as a terminator.
The Channel Headers list implicitly terminates only at EOF, ie if you don't have a terminator there can't be any events.
But a file consisting only of the signature is legal.
Decoders must accept and ignore Channel Headers above chid 15.

Followed by Events, distinguishable by their leading byte:
```
00000000                   : End of Song.
00tttttt                   : Fine Delay, (t) ms. (t) nonzero.
01tttttt                   : Coarse Delay, ((t+1)*64) ms.
1000cccc nnnnnnnv vvvvvvxx : Fire and Forget. (c) chid, (n) noteid, (v) velocity.
1001cccc nnnnnnnv vvvttttt : Short Note. (c) chid, (n) noteid, (v) velocity 4-bit, (t) hold time (t+1)*16ms
1010cccc nnnnnnnv vvvttttt : Long Note. (c) chid, (n) noteid, (v) velocity 4-bit, (t) hold time (t+1)*128ms
1011cccc wwwwwwww          : Pitch Wheel. (c) chid, (w) wheel position. 0x80 is neutral.
11xxxxxx                   : Reserved, decoder must fail.
```

A song's duration is the sum of its Delay events.
It's technically possible for Note events to throw their hold time beyond that duration.
That is an error, and decoders' behavior in that case is undefined.
The reasonable options are (1) let the note play past the song's end, (2) clamp note end to song end, or (3) ignore that note.
Or (4) something else. It's undefined.

### EGGSND Channel Headers

The header is a stream of fields, each beginning:
```
u8 fldid
u8 length
```
Decoder is free to pad fields with zeroes to reach its expected length, and also free to truncate fields.
Fields 0x80 and above are post-process operations that execute in order, against the mixed voices.
Fields 0x60..0x7f and 0xe0..0xff are reserved for "critical" fields that decoders must not ignore.
Others may be ignored if unknown.
(note that most fields currently defined should be "critical"; that's not necessary because this is the first version of this spec)

| fldid | Name        | Args                            | Desc |
|-------|-------------|---------------------------------|------|
| 0x00  | noop        | ()                              | Dummy. |
| 0x01  | master      | (u0.8)                          | Trim to apply at the very end. MIDI Control Change 0x07. |
| 0x02  | pan         | (s0.8)                          | Natural zero means "mono". Else 1..128..255 = left..center..right. MIDI Control Change 0x0a. |
| 0x03  | drums       | (u16 rid,s8 bias,u0.8 trimlo,u0.8 trimhi) | Set up as a drum channel. Each note is a sound effect. Most other fields become illegal. |
| 0x04  | wheel       | (u16 cents)                     | Range of pitch wheel. |
| 0x05  | sub         | (u16 width(hz))                 | Set up as subtractive synth. Most other fields become illegal. |
| 0x06  | shape       | (u8)                            | Initial wave shape. 0,1,2,3 = sine,square,saw,triangle. Default sine. |
| 0x07  | harmonics   | (u0.16...)                      | Replace wave with harmonics of itself. |
| 0x08  | fm          | (u8.8 rate,u8.8 range)          | Set up as FM. (rate) relative to note rate. |
| 0x09  | fmenv       | (env...)                        | FM range envelope, normalized values. |
| 0x0a  | fmlfo       | (u16 ms,u0.16 depth,u0.8 phase) | FM range LFO. Shared among the channel's voices. |
| 0x0b  | pitchenv    | (env...)                        | Per-note pitch adjust envelope. Values in cents. |
| 0x0c  | pitchlfo    | (u16 ms,u16 cents,u0.8 phase)   | Pitch LFO. Shared among the channel's voices. |
| 0x0d  | level       | (env...)                        | Per-note level envelope. Required, except for drums. |
| 0x80  | gain        | (u8.8 gain,u0.8 clip,u0.8 gate) | Multiply and clamp, for crude distortion. |
| 0x81  | waveshaper  | (s0.16...)                      | Signal linearly interpolates between two adjacent values here, for more refined distortion. |
| 0x82  | delay       | (u16 ms,u0.8 dry,u0.8 wet,u0.8 store,u0.8 feedback) | Simple delay with feedback. |
| 0x83  | ---         | ()                              | Unused (formerly detune) |
| 0x84  | tremolo     | (u16 ms,u0.16 depth,u0.8 phase) | Attenuate combined output per LFO. |
| 0x85  | lopass      | (u16 hz)                        | Attenuate frequencies above arg. |
| 0x86  | hipass      | (u16 hz)                        | Attenuate frequencies below arg. |
| 0x87  | bpass       | (u16 hz,u16 width)              | Attenuate frequencies outside this band. |
| 0x88  | notch       | (u16 hz,u16 width)              | Attenuate frequencies within this band. |

Envelopes:
```
u8 sustain index, OOB for none
u16 init lo
u16 init hi
repeat:
  u16 delay lo
  u16 delay hi
  u16 level lo
  u16 level hi
```

## Standard Formats

WAV files may be merged into sounds resources.
Name them `"RID-INDEX[-NAME].wav"`.

MIDI files are recommended for music.
You may configure channels in the MIDI file, via Meta 0xf0.
Meta 0xf0 must contain a binary Channel Header verbatim.
You should precede it with Meta 0x20 MIDI Channel Prefix, to identify the channel.

Regular MIDI configuration events at time zero override the verbatim Channel Header:
- Control 0x07 Volume.
- Control 0x0a Pan.
- Control 0x00 Bank Select MSB.
- Control 0x20 Bank Select LSB.
- Program Change.

If you explicitly set Volume zero for a channel, we discard the whole channel.
Headers for channels with no notes are also discarded.

A special file `DATAROOT/instruments` contains definitions for instruments accessible by Program ID.
The format is basically MSF text, except each block is introduced: `instrument [BANK:]PID [NAME]`, and is followed by a single Channel Header.
These do not ship with the ROM. Instead, they get baked into songs during compile as needed.
