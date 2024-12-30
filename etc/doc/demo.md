# Egg Demo Guide

After building Egg for a new platform, run the included demo ROM to test it.
Just `make run` or `make web-run` in this repo.

In general: dpad to move cursor, SOUTH to activate, and WEST to cancel.

Follow the notes below to validate each test.

## Video/Primitives

You'll see two outlined shapes, two solid squares, and a rectangle formed by two triangles with a visible gap.
That same pattern is then repeated twice over a chaotic background: Once with per-vertex alpha and once with global alpha.

The outlined shapes might miss pixels at the corner. That's a bug but I don't have a good fix yet.

Below all that is a row of 8 squares which vary smoothly from yellow to black.

## Video/Transforms

You'll see the same tile rendered with all 8 axiswise transforms, using 4 different strategies:
- Pretransformed
- Tile
- Decal
- Mode7

Confirm that each column contains four identical tiles.

## Video/Aux Buffer

You'll see a box with some lines and an egg man, twice.
The left side is rendered directly to framebuffer and the right side goes through an intermediate texture.
They must be identical.

## Video/Stress

Render an arbitrary number of sprites as tile, decal, or mode7.
Dpad to adjust counters, and hold SOUTH to double/halve instead of increment/decrement.

Observe CPU and GPU consumption externally.
You should notice that tiles are substantially cheaper than the other strategies, and mode7 is more expensive than decal.
Modern hardware should have no problem rendering a thousand of any type.

## Audio

Play songs and sound effects.
We read the ROM dynamically here -- you can add song or sound resources and rebuild.

Playing a song with "Force" must restart it when already playing; without Force it must not restart.

Playing song zero must stop the current song.

## Input

Current input state is displayed along the bottom.
First gamepad is the aggregate state, then player 1 thru player 7.
Player states with Carrier Detect off are rendered partly transparent.

Changes to the aggregate state are displayed as a text log with timestamps.

Note that games do not see raw input: These player state bitmaps are all we can get.

Press the same button three times quickly to exit.

## Input Config

Launches the platform's input configurator, ie `egg_input_configure()`.

This will prompt you to press all the buttons of any device.
Must terminate if you leave it idle.

These settings must persist. To localStorage for web, or the file named in `--input-config` for native (`~/.egg/input.cfg` by default).

The exact details of the configurator UI may vary across platforms (currently there's Native and Web, and they're just a little bit different).

## Storage

All available data from the store will be displayed in the lower right.
Should be nothing, the first time you run.
Set either field, and it will take a value derived from the system's local time.
Values must persist across runs (unless the platform is configured otherwise).
Confirm that a deleted value stays deleted.

It's not important, but if curious: You can manually edit the store file, and those values must appear in this report too.

## Storage/Resources

Shows all resource in the ROM.
Left/Right to change page, WEST to return.

There's a pointless cursor, selecting a resource won't do anything.
(I considered showing a hex dump of the resource or something, but that doesn't seem useful).

## Miscellaneous

Confirm that logging and termination work.
The exit status should propagate all the way out when running natively (ie check `$?` after it terminates).

English and French should both be presented as options, and it should auto-select from your system context (eg set `LANG` when launching).
Some example text at the bottom of the screen will display in the chosen language.
You should also see the window's title change.
Changes to language do not persist across launches -- users are encouraged to use operating system facilities for that.
Note that the demo's UI in general is hard-coded in English.

"Real" and "local" timestamps will display in the upper right corner.

## Regression

Will contain one-off tests for bugs I discover later.
