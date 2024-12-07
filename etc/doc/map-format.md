# Egg Map Resource

Provided as a convenience, since most of my games need something like this and the tooling is complex.

A map consists of two parts: Cells image and commands.
The cells image is a rectangular grid of 1-byte cells, corresponding to tiles from some image.
Commands are extra loose data, see `command-list-format.md` for generic detail.

# Binary Format

```
  4 Signature; "\0EMP"
  2 Width, cells
  2 Height, cells
... Cells (1*Width*Height)
... Commands (command-list)
```

# Text Format

Begins with cells image: A giant hex dump where all lines are the same length.
Followed by a blank line to terminate cells image.
Followed by the command list.

# Commands

Defining commands is up to you. Provide a '--schema' argument to `eggdev pack`.

Our editor will look for certain commands in the text (it's not privy to the binaries, so opcode assignment doesn't matter):
- `image`: u16 imageid
- `door`: u8 x, u8 y, u16 mapid, u8 dstx, u8 dsty
- `sprite`: u8 x, u8 y, u16 spriteid
- `neighbors`: u16 west, u16 east, u16 north, u16 south (mapid)
- Any other command with a `@X,Y` argument, editor should display as a Point Of Interest.
- Any other command with a `@X,Y,W,H` argument, editor should display as a Region Of Interest.
