# Egg

Portable game engine.
Games compile to WebAssembly and pack into a single ROM file with all their assets.
Linkage to the outside world is controlled entirely by the Egg Platform, and we're stingy about what's available.

## Requirements

- WASM Micro Runtime
- - https://github.com/bytecodealliance/wasm-micro-runtime
- - `cmake .. -DWAMR_BUILD_INTERP=1 -DWAMR_BUILD_AOT=0 -DWAMR_BUILD_LIBC_BUILTIN=0 -DWAMR_BUILD_LIBC_WASI=0 -DWAMR_BUILD_FAST_INTERP=0`
- - Current recommendation is to build with interpretter only. I'll examine the JIT and AOT options in more detail later.
- WABT
- - https://github.com/WebAssembly/wabt
- I/O drivers, varies by platform.

## Limits

Reasons you would *not* want to use Egg.

- Networking. We don't support at all, and won't.
- Access to filesystem or exotic hardware.
- Custom synthesizer. We might support in some fashion, but it will never be a great choice.
- - We provide an opinionated synthesizer in the platform, and it may or may not be adequate for your game.
- Custom renderer. We might support direct access to OpenGL (or WebGPU, Vulkan, ...), but won't be a great choice.
- - We provide an opinionated renderer in the platform, geared for 2D sprite-oriented games.
- Enormous data set. You're limited to 64k resources of each type and 4 MB per resource.

## Features

Reasons you *would* want to use Egg, and goals I'm designing toward.

- Small distributable size. Typical games should be <1 MB for the ROM, and add no more than 1 MB for the wrapper.
- - Obviously, the ROM size can go much larger than that if needed.
- Fast load time.
- Pixelly sprites and beepy music, and no recorded sound effects or music.
- - High resolution video and recorded sound should be possible, but I'm not optimizing for those.
- Uniform input. I consider the Standard Mapping Gamepad canonical. We'll also support keyboard, mouse, touch, and accelerometer.
- Portable. Build self-contained executables for web and any native platform where you can run eggdev.
- Easy eject. If a project outgrows Egg, you have the source. You can drop the Egg Runtime source in your project and season to taste, if need be.

## TODO

- [x] Build framework.
- - [x] Can I arrange for `make` from scratch to guess the initial configuration? Would be great if a fresh clone can just `make`.
- - - It's going to get hairy when there's actual real stuff to configure, but yep the groundwork is laid.
- - [x] Avoid baking config into `eggdev`. Did that with Pebble, and it's painful when you tweak the config.
- - - Maybe a config file adjacent to the eggdev binary.
- - - ...ended up with basically the same thing, a bunch of '-D' at build time, but with eggdev it's all isolated in one file, might be less awkward.
- - - [x] While we're there, can we export more of Egg's build config for games' consumption?
- - - - Thinking, a game should be able to borrow CC, LD, LDPOST from whatever Egg used to build itself.
- [x] Define API.
- [ ] Define data types.
- - [x] ROM: Use Pebble's format, or something like it. Inlining payloads seems a good move.
- - [x] Metadata: Pebble and egg-202408 had basically the same format, and I think no reason to change.
- - [x] Song.
- - [ ] Sound effects.
- - [x] Image.
- [ ] Dev tool.
- - [x] bundle html: CSS and Javascript
- - [x] serve
- - - [x] Make before serving (eggdev_main_serve.c:eggdev_cb_get_api_make)
- - [x] validate: metadata. Currently checking framing and encoding, there's much more we could do.
- - [x] validate: image
- - [ ] validate: sounds
- - [x] validate: song
- - [ ] Synth helper. Maybe as part of 'serve'.
- [ ] Editor.
- - [x] Scaffolding. Allow games to override, same way egg-202408 did.
- - [x] Generic: Text, hex, image
- - [x] Manifest
- - [x] Metadata
- - [x] Strings
- - [x] Song. Probably just channel headers, and don't touch note data.
- - [ ] Song: Playback.
- - [ ] Song: Live MIDI input.
- - [ ] Sound effects.
- - [ ] Launch game.
- [ ] Native runner.
- - [x] Configure
- - [x] Load ROM
- - [ ] Timing
- - [x] Execute
- - [ ] Egg API
- - [ ] Drivers
- - [ ] Input manager
- - [ ] Live input mapping
- - [ ] Config files
- [ ] Web runner.
