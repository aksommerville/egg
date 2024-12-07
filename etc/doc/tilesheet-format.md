# Egg Tilesheet Resource

`tilesheet` resources are parallel to `image` resources, and describe properties for the 256 tiles contained by an image.

Conceptually, the tilesheet is a set of 256-entry tables, 1 byte per entry, and each table has a 1-byte nonzero identifier.

# Binary Format

Signature: "\0ETS"

Followed by any count of chunks:
```
   1 tableid, >0
   1 tileid
   1 count-1
 ... values
```

All tables are initially zeroed.
Chunks run in order. If they overlap, the later chunk overwrites the earlier one.
Chunks are not required to be sorted, any order is legal.

# Text Format

Line-oriented text.
Hash begins a comment, start of line only.

First read the table identifier, by itself on one line.
Followed by 16 lines of 32 hex digits each.
The binary format is allowed to drop zeroes, but the text format must list them, to keep it simple.

Listing the same table twice is an error.

# Table ID

Define a namespace 'tilesheet' mapping table names to IDs.
You can map multiple names to zero, which will cause the compiler to quietly drop them.
That's advisable for editor-only tables: neighbors, family, weight.
(of course, if you want those tables to survive into runtime, just give them nonzero IDs).

Egg only uses tilesheets in the editor, so it doesn't care about IDs in the binary format.
It will look for these tables by name:
- `physics`: We don't really care what the values mean, but we can use it to color cells on the map.
- `neighbors`: 8 bits for neighbor joining: 0x80..0x01 = NW,N,NE,W,E,SW,S,SE
- `family`: Arbitrary ID for neighbor joining. Zero means none.
- `weight`: For selecting randomly among candidate tiles. 0..254,255 = Likeliest..Unlikeliest, Appointment-Only
