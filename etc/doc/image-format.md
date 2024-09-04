# Egg Image Formats

We impose a size limit 32767x32767 for all images.

Images are accepted in these formats:
- PNG: Well-defined standard, won't detail here. We do not allow interlacing or >32-bit pixels.
- QOI: RGBA only. Pretty good compression and excellent code size. See below.
- rlead: My invention for 1-bit images. PNG compresses better, but rlead has much more modest requirements. See below.
- rawimg: No compression, and a trivial 9-byte header. See below.

If you're going to decode images client-side, PNG might be a bad choice, since it requires you to include zlib.
But if you let the platform do all the decoding, or you don't mind including zlib in the client code, PNG is the best choice for almost all images.

Input images may contain a comment with conversion instructions.
eg "1-my_image.rlead.a.png" means convert to rlead with the Alpha hint.

## rawimg

Simplest possible thing, so we always have a neat fallback.

```
0000   4 Signature: "\x00rIm"
0004   2 Width 1..0x7fff
0006   2 Height 1..0x7fff
0008   1 Pixel size in bits: 1,2,4,8,16,24,32
0009
```

Followed by pixels, LRTB, rows padded to one byte.
Sub-byte pixels are packed big-endianly.
Short pixels are not allowed; the file length must match the header exactly.

## qoi

[Official spec](https://qoiformat.org)

For RGBA images. Decent run-length compression, overall performance is not great.

Integers are big-endian.

Header:
```
0000   4 Signature: "qoif"
0004   4 Width. 1..0x7fff for consistency with our other formats.
0008   4 Height. ''
000c   1 Channel count (3,4)
000d   1 Colorspace (0,1)=(sRGB,linear)
000e
```

Maintain a cache of 64 pixels, indexed by a hash:
```
index_position = (r * 3 + g * 5 + b * 7 + a * 11) % 64
```

Payload is in chunks distinguishable by their leading byte.
Check in order:
```
11111110 rrrrrrrr gggggggg bbbbbbbb          : QOI_OP_RGB. Alpha from previous pixel.
11111111 rrrrrrrr gggggggg bbbbbbbb aaaaaaaa : QOI_OP_RGBA
00iiiiii                                     : QOI_OP_INDEX. Emit cache[i].
01rrggbb                                     : QOI_OP_DIFF. (r,g,b) are (-2..1), difference from previous pixel.
10gggggg rrrrbbbb                            : QOI_OP_LUMA. (g) in (-32..31), (r,b) in (-8..7)+(g), difference from previous pixel.
11llllll                                     : QOI_OP_RUN. Emit previous pixel (l+1) times, 1..62. 63 and 64 are illegal.
```

End of file:
```
00 00 00 00 00 00 00 01
```

## rlead

Adaptive RLE, my own invention for 1-bit images.

Header:
```
0000   4 Signature: "\x00rld"
0004   2 Width 1..0x7fff
0006   2 Height 1..0x7fff
0008   1 Flags:
          f8 Reserved, zero
          04 Alpha. Otherwise gray.
          02 Initial color
          01 Filter
0009
```

Followed by payload, which must be read bitwise big-endianly:
 - First read a 3-bit leading word.
 - Emit so many pixels of the current color, plus 1. ie 1..8 pixels.
 - If the leading word is 7, begin extension words:
 - - Increment word length by 1. ie first extension word is 4 bits.
 - - Read a word of that length, and emit so many pixels. Do not add 1.
 - - If the word contains a zero, the run is over.
 - - Otherwise, do another extension word (increasing the word size by 1 bit each time).
 - Reverse the color and start over.
 
This arrangement has a clean break-even point at 3-pixel runs.
Meaning, runs of 1 or 2 pixels encode longer than raw, and runs of 4 or more encode smaller.
 
Payload is allowed to be short, anything unvisited is zero (regardless of Initial color).
At the limit, a straight black image may have no payload at all.
 
If (Filter) set, each row must be XOR'd against the previous row.
Do this before encoding or after decoding.
