# Egg ROM Format

Canonical form is a binary file that must be read sequentially.

Starts with a 4-byte signature: "\0EGG"

Read the rest with a state machine:
```
tid = 1
rid = 1
```

High bits of first byte describe each command:

| Command                    | Name  | Desc |
|----------------------------|-------|------|
| 00000000                   | EOF   | Required at the end. Decoder must ignore any content after. |
| 00dddddd                   | TID   | (d) nonzero. `tid+=(d), rid=1` |
| 01dddddd dddddddd          | RID   | (d) nonzero. `rid+=(d)` |
| 10llllll llllllll          | SMALL | Add resource, length (l+1). `rid+=1` |
| 11llllll llllllll llllllll | LARGE | Add resource, length (l+16385). `rid+=1` |

Each resource has a unique ID composed of 8-bit tid and 16-bit rid, both nonzero.
Longest possible resource is 4210688 bytes, a bit over 4 MB.
It is impossible for resources not to be sorted by (tid,rid).
It is beneficial to assign (rid) contiguously from 1 for each type.

## Types

| tid     | Name      | Comment |
|---------|-----------|---------|
| 0       |           | Illegal. |
| 1       | metadata  | Required, rid must be 1. See metadata-format.md. |
| 2       | code      | Required, rid must be 1. WebAssembly module. |
| 3       | strings   | rid is 6 bits, with language in the top 10 bits. See strings-format.md. |
| 4       | image     | Recommend PNG. See image-format.md. |
| 5       | sound     | See audio-format.md. |
| 6       | song      | See audio-format.md. |
| 7       | map       | Convenience. See map-format.md. |
| 8       | tilesheet | Convenience. See tilesheet-format.md. |
| 9       | sprite    | Convenience. See sprite-format.md. |
| 10..15  |           | Reserved for future standard types. |
| 16..127 |           | Reserved for client use. |
| 128..   |           | Reserved, no plan. |

The ones marked "Convenience" are Standard Types, but not actually used by our runtime.
They're defined standard in order to provide tooling, because they can get pretty complex.

## Layout On Disk

Our tooling will expect a specific strict layout of data files to produce ROMs from.

```
DATAROOT/
  metadata
  code.wasm
  TNAME/
    [LANG-]RID[-RNAME][[.COMMENT].FORMAT]
```

Custom types will be assigned IDs starting at 16, in alphabetical order.
If you add or remove a type directory, beware that tids may change.
Let eggdev generate a TOC, and only use the symbolic type IDs in your code.

`LANG-` is two lowercase letters, which become the top ten bits of rid.
`strings` uses this, probably nothing else should. But it's legal anywhere.

`.COMMENT` is extra processing instructions for the compiler. You can't have a COMMENT without a FORMAT.

## Embedding in HTML

In addition to a `<script>` tag containing the Egg Runtime, there must be `<egg-rom>` whose body is a base64-encoded ROM file.
Aside from base64, there's no difference between a ROM in HTML and one loose in the filesystem.
