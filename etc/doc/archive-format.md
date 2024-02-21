# Egg Archive Format

An egg archive is typically the entire game.
For native games, it contains data but not code. (TODO: Can native games run Javascript resources? Decide).
Bundled and external games, it also contains the code (Javascript or WebAssembly).

Considered using tarballs, that would satisify our needs, but it seems wasteful.
We can reconsider that later. Would be nice for developers, to use tools they already have...

Integers are big-endian.

Starts with a header, minimum 20 bytes:

```
0000   4 Signature: "\xff\x0egg" (0xff,0x0e,0x67,0x67)
0004   4 Header length, minimum 20.
0008   4 TOC length.
000c   4 Paths length.
0010   4 Bodies length.
0014
```

Decoders should ignore any excess content in header, we might expand it in the future.

Header is followed immediately by TOC, then Paths, then Bodies, all of whose lengths are now known.

TOC is zero or more of:

```
0000   1 Path length.
0001   3 Body length.
0004
```

Decoder must hold pointers into the Paths and Bodies section.
Each TOC entry advances both pointers.
Decoder is required to check the length of each entry, obviously, and should fail hard if anything overruns its section.

Paths and Bodies are raw data.

## Path Conventions

- Do not use the empty path, though it's technically legal.
- All resources that Egg standardizes will be named "TYPE/ID" or "TYPE/QUALIFIER/ID", where:
- - TYPE is a single lowercase letter.
- - QUALIFIER is any amount of ASCII G0 except slash.
- - ID is a decimal integer 1..65535 without leading zeroes.
- In addition to one special resource, which should be listed first: "details"
- Strongly recommend that paths use only G0. But technically anything goes.
- Duplicate paths are an error. Decoder may break the tie either way, or fail.

## Standard Resources

Game is free to put whatever it wants in the archive, and name them whatever it wants.
Try not to collide with these standard conventions.

```
details   : See game-details.md
a/ID      : Sound effect.
i/LANG/ID : Image. QOI or rlead. (see src/opt/qoi and src/opt/rlead for details)
i/ID      : Image.
j/ID      : Javascript.
j/1       : Main entry point, Javascript. TODO How does linkage work?
s/LANG/ID : String. See strings-format.md
t/ID      : Song.
w/ID      : WebAssembly.
w/1       : Main entry point, WebAssembly. Must export everything from egg_client.h
b/* : reserved
c/* : reserved
d/* : reserved
e/* : reserved
f/* : reserved
g/* : reserved
h/* : reserved
k/* : reserved
l/* : reserved
m/* : reserved
n/* : reserved
o/* : reserved
p/* : reserved
q/* : reserved
r/* : reserved
u/* : reserved
v/* : reserved
x/* : reserved
y/* : reserved
z/* : reserved
```
