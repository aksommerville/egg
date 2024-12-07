/* demo_symbols.h
 * Command names for map and sprite, and all the other namespaces that we share with the compiler and editor.
 */
 
#ifndef DEMO_SYMBOLS_H
#define DEMO_SYMBOLS_H

/* Namespace "sys" is for loose constants that the editor needs.
 */
#define NS_sys_tilesize 16 /* pixels */

/* Commands for "map" resource.
 * A few are used by the editor if you define them: image, door, sprite, neighbors
 * More detail in etc/doc/map-format.md and etc/doc/command-list-format.md.
 */
#define CMD_map_image     0x20 /* u16:imageid */
#define CMD_map_door      0x60 /* u8:x u8:y u16:mapid u8:dstx u8:dsty u16:reserved */
#define CMD_map_sprite    0x61 /* u8:x u8:y u16:spriteid u32:reserved */
#define CMD_map_neighbors 0x62 /* u16:west u16:east u16:north u16:south */

/* Commands for "sprite" resource.
 * A few are used by the editor if you define them: image, tile
 * More detail in etc/doc/sprite-format.md and etc/doc/command-list-format.md.
 */
#define CMD_sprite_image 0x20 /* u16:imageid */
#define CMD_sprite_tile  0x21 /* u8:tile u8:xform */

// You can define custom resource types using the Command List format, and enumerate the fields here.

/* Example custom namespace.
 */
#define NS_eggStyle_fried 1
#define NS_eggStyle_boiled 2
#define NS_eggStyle_scrambled 3

/* Tilesheet table IDs.
 * Use a nonzero ID 1..255 if you want the table to persist to runtime.
 * ID zero if it's only needed by the editor. eg you typically don't need neighbor, family, or weight at runtime.
 * Editor uses some tables if you define them: physics, neighbors, family, weight
 */
#define NS_tilesheet_physics 1
#define NS_tilesheet_neighbors 0
#define NS_tilesheet_family 0

#endif
