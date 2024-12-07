# Command-List Resource

Egg provides a generic resource type composed of ordered commands with a 1-byte opcode and arbitrary payload.
We use this for `map` and `sprite` resources, and it's generalized out for use anywhere.

There's no intrinsic signature for the format; specific resource types should add their own.

## Binary Format

Packed stream of (opcode, payload).
Opcode is one byte, and the payload's length is knowable generically.

Opcode zero signals the end of stream. Optional if you know the stream length by other means.

Aside from that, this generic format doesn't care what the opcodes mean, or how their payloads split up.

| Opcode range | Payload size |
|--------------|--------------|
| 0x00         | NA. End of file. |
| 0x01..0x1f   | 0 |
| 0x20..0x3f   | 2 |
| 0x40..0x5f   | 4 |
| 0x60..0x7f   | 8 |
| 0x80..0x9f   | 12 |
| 0xa0..0xbf   | 16 |
| 0xc0..0xdf   | Next byte is payload length. |
| 0xe0..0xff   | Reserved, illegal if unknown. |

## Text Format

Line-oriented text.
Hash begins a line comment, start of line only.

Each line is: `COMMAND [ARGS...]`.
COMMAND is an integer in 1..255, or a symbol from definitions you provide.
ARGS are JSON string tokens or white-space delimited tokens defined below.

The compiled length of ARGS must match the opcode's expected length.
Compiler will not pad or truncate automatically.
For the variable-length opcodes 0xc0 thru 0xdf, compiler assumes any payload length 0..255 is OK.

Argument formats:
 - Hexadecimal integer: Big-endian integer of the size inferred from token length.
 - - These are treated more like hex dumps than integers, eg you're not limited to 4 bytes.
 - Other naked integer: 8-bit.
 - `(u8)N` `(u16)N` `(u24)N` `(u32)N`: Big-endian integer of indicated length (`(u8)N` is redundant, provided for consistency).
 - `@A,B,...`: One byte per component. The `@` marks it for editor's consumption as a point or region of interest.
 - JSON string: Raw UTF-8.
 - `TYPE:NAME`: Resource ID in 2 bytes. Resource must exist.
 - `(uL:N)V`: Big-endian integer of length `L` (8,16,24,32), by resolving token `V` in namespace `N`.
 - - For extending the editor with your custom namespaces. eg `(u16:flag)blue_door_unlocked` or `(u8:sprite_type)dragon`
 - `*` as the last argument only, indicating compiler should pad with zeroes to the required length.
 
So, reading between the lines, a command list in text format can be basically a hex dump:
```
0x20 0x01 0x02
0x21 0x0304
```

But could also be much more meaningful:
```
hero_spawn @1,2
songid song:seven_hundred_seventy_two
```

## Custom Symbols

You should provide one or more schema files to `eggdev pack` when command-list resources are involved.
These are C header files, and we extract certain defines from them.

`#define CMD_{type}_{name} {opcode} /* {args} */`: Define a command. Type's name can't contain an underscore.

`#define NS_{name}_{key} {value} /* IGNORED */`: Define a symbol (key) in namespace (name). The namespace's name can't contain an underscore.

The values must be plain integers, the compiler won't do any further lookup or arithmetic.
Comments are optional.

For example, say you have a header "my_symbols.h":
```
#define CMD_map_image 0x20
#define CMD_map_sprite 0x40
#define CMD_map_flagtile 0x40
#define CMD_sprite_image 0x20
#define CMD_sprite_tile 0x21
#define NS_flag_blue_key 1
#define NS_flag_red_key 2
#define NS_xform_none 0
#define NS_xform_xrev 1
```

Then your maps can say: `image image:my_tiles`
And: `flagtile @4,5 (u8:flag)blue_key 0`
And sprites: `tile 0x40 (u8:xform)xrev`

Which is hopefully more tractable than specifying everything numerically.
A perk of using C headers for this is that your code can include the same header and use these symbols the natural way.
