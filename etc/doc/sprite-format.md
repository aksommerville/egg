# Egg Sprite Resource

These are command lists (see `command-list-format.md`), with a 4-byte signature: "\0ESP"

Egg only defines this type because it's convenient for the map editor.
We keep pretty agnostic about whatever "sprite" actually means.

Commands are yours to define, but the editor will look for:
- `image`: u16 imageid
- `tile`: u8 tileid, u8 xform

