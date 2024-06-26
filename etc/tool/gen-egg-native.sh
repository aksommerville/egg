#!/bin/sh

if [ "$#" -ne 1 ] ; then
  echo "Usage: $0 OUTPUT"
  exit 1
fi

DSTPATH="$1"

cat - >"$DSTPATH" <<EOF
#!/bin/sh
if [ "\$#" -ne 3 ] ; then
  echo "Usage: \$0 OUTPUT ROMFILE CODELIB"
  exit 1
fi
TMPASM=tmp-egg-rom.s
cat - >"\$TMPASM" <<EOFASM
.globl egg_rom_bundled
.globl egg_rom_bundled_length
egg_rom_bundled: .incbin "\$2"
egg_rom_bundled_length: .int (egg_rom_bundled_length-egg_rom_bundled)
EOFASM
$LD -o\$1 \$TMPASM -Wl,--start-group \$3 $LDPOST -Wl,--end-group || (
  rm \$TMPASM
  exit 1
)
rm \$TMPASM
EOF

chmod +x "$DSTPATH"
