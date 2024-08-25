# Egg Song Format

Input format is MIDI, with some constraints:
- Only channels 0..7 are allowed. Our compiler will reassign if needed and only fail if more than 8 channels are present.
- Release velocity is not used.
- Aftertouch (both Note Adjust and Channel Pressure) are ignored.
- Program Change and Control Change are only permitted at time zero, and only 8 Control Changes are meaningful, see below.
- Instruments are described entirely within the song file, using Bank, Program, and GP.
- Set Tempo commands after time zero will be respected, but will cause skew against the declared global tempo.

## Binary Format

```
0000   4 Signature: "\0\xbe\xeeP"
0004   2 Tempo, ms/beat. Not needed for playback, only for reporting and effects.
0006   1 Channel count, 1..8
0007 ... Channel headers, 8 bytes each. See below.
.... ... Events
....
```

Events are distinguishable by their first byte:
```
00000000                    EOF.
0ttttttt                    Delay. (t) nonzero, 4ms.
100cccnn nnnnnvvv           Instant Note. Zero sustain.
101cccnn nnnnnvvv dddddddd  Short Note. (d) sustain time ms.
110cccnn nnnnnvvv dddddddd  Long Note. (d) sustain time 16ms.
111cccww wwwwwwww           Pitch Bend. (w) in 0..1023, bias by 512.
```

Bends begin at 0x100, and synth must reset them to 0x100 on repeat.

## Channel Headers

During compilation, the 63-bit channel header is composed from time-zero events:
```
0vvvvvvv PPPPPPPB BBBBBBbb bbbbbppp pppp1111 11122222 22333333 34444444
  v = Volume (ctl 0x07)
  P = Pan (ctl 0x0a)
  B = Bank Select MSB (ctl 0x00)
  b = Bank Select LSB (ctl 0x20)
  p = Program
  1..4 = GP1..GP4 (ctl 0x10..0x13)
```

At load time, it's interpretted as:
```
0vvvvvvv PPPPPPPs eeeeeeee eeeeEEEE EEEEEEEE ssssMMMr rrrRRRRf dddDDooo
  v  7 = Volume
  P  7 = Pan
  s  1 = Enable sustain
  e 12 = Low velocity envelope
  E 12 = High velocity envelope
  s  4 = Shape: Sine, Square, Saw, Triangle, 12 reserved
  M  3 = Modulator rate: 1/4, 1/2, 1, 2, 3, 4, en1, en2
  r  4 = Low velocity modulator range
  R  4 = High velocity modulator range
  f  1 = FM mode select. 0=velocity, 1=envelope
  d  3 = Delay rate in beats: 1/4, 1/3, 1/2, 1, 3/2, 2, 3, 4
  D  2 = Delay amount. Arbitrary scale, describes both feedback and mix.
  o  3 = Overdrive
  
Envelopes: aaaaSSSSrrrr
  a = Attack time, short..long, ordered but nonlinear.
  S = Sustain level relative to peak.
  r = Release time.
```

Volume and Pan both pass through directly from the expected control changes.
All the others, it's a complicated bit-bashing process.
