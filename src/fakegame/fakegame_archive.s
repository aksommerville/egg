.globl egg_bundled_rom
.globl egg_bundled_rom_size

egg_bundled_rom:
.incbin "archive"
end_rom:

egg_bundled_rom_size:
.int (end_rom-egg_bundled_rom)
