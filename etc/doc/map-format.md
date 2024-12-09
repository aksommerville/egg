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

Beware: Dimensions are 16-bit but our standard commands use 8-bit coordinates for positioning.
You can of course format commands however you want, but you won't be able to use the standard editor.

# Text Format

Begins with cells image: A giant hex dump where all lines are the same length.
Followed by a blank line to terminate cells image.
Followed by the command list.

A line comment terminates the cells image just like a blank line.

# Commands

Defining commands is up to you. Provide a '--schema' argument to `eggdev pack` and `eggdev serve`.

Our editor will look for certain commands in the text (it's not privy to the binaries, so opcode assignment doesn't matter):
- `image`: u16 imageid
- `door`: u16 xy, u16 mapid, u16 dstxy
- `sprite`: u16 xy, u16 spriteid
- `neighbors`: u16 west, u16 east, u16 north, u16 south (mapid)
- Any other command with a `@X,Y` argument, editor should display as a Point Of Interest.
- Any other command with a `@X,Y,W,H` argument, editor should display as a Region Of Interest.
