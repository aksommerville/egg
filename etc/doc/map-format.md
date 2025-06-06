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
- `location`: u8 x, u8 y, [u8 z] -- Absolute position of map in world. Don't use with `neighbors`.
- `neighbors`: u16 west, u16 east, u16 north, u16 south (mapid). Don't use with `location`.
- Any other command with a `@X,Y` argument, editor should display as a Point Of Interest.
- Any other command with a `@X,Y,W,H` argument, editor should display as a Region Of Interest.
- `song`: We don't care about the payload, but we copy when creating a neighbor map.

If you use `location` or `neighbors`, editor may assume that all maps are the same size.
