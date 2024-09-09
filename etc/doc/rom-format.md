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

| tid     | Name     | Comment |
|---------|----------|---------|
| 0       |          | Illegal. |
| 1       | metadata | Required, rid must be 1. See metadata-format.md. |
| 2       | code     | Required, rid must be 1. WebAssembly module. |
| 3       | strings  | rid is 6 bits, with language in the top 10 bits. See strings-format.md. |
| 4       | image    | Recommend PNG. See image-format.md. |
| 5       | sounds   | See audio-format.md. |
| 6       | song     | See audio-format.md. |
| 7..15   |          | Reserved for future standard types. |
| 16..127 |          | Reserved for client use. |
| 128..   |          | Reserved, no plan. |

## Layout On Disk

Our tooling will expect a specific strict layout of data files to produce ROMs from.

```
DATAROOT/
  manifest
  metadata
  code.wasm
  strings/
    LANG-RID[-RNAME]
  TNAME/
    RID[-RNAME][[.COMMENT].FORMAT]
```

The optional `manifest` file declares custom types.
Every directory under DATAROOT must be named for a type, and they must either be standard, numeric, or named in the manifest.
```
type TNAME TID
```
