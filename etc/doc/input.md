# Egg Input

You should always launch with `--input-config=PATH`.
TODO: Use something sensible when that isn't provided.

To remap a device interactively, launch egg or any egg game with `--configure-input`.
It will guide you through pressing all 15 buttons.

You can't currently set SIGNAL buttons via the interactive configurator (eg QUIT, SCREENCAP, PAUSE, ...).
But if there were any already configured, interactive will keep them.

## Input Config File Format

Line-oriented text, *no comments*.
Comments and formatting would be lost when we automatically rewrite the file, which we do, aggressively.

Organized in blocks which must begin `device VID PID VERSION NAME`.
Zero VID/PID/VERSION or empty NAME matches anything.
The order of blocks matters: First match wins.

After the device introducer, each line is `SRCBTNID DSTBTNID`.
SRCBTNID is an integer.
DSTBTNID may be an integer or one of these symbols:
```
LEFT RIGHT UP DOWN SOUTH WEST EAST NORTH L1 R1 L2 R2 AUX1 AUX2 AUX3
QUIT FULLSCREEN PAUSE STEP SAVESTATE LOADSTATE SCREENCAP CONFIGIN
```
