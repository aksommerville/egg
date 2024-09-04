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
- Custom synthesizer. You get beepy old-fashioned synth effects.
- Custom renderer. You get a nice set of blitting tools. But no 3D support, custom shaders, etc.
- Enormous data set. You're limited to 64k resources of each type and 4 MB per resource.
- Input from sources other than gamepads. eg Mouse, Touch, or digested text from keyboards.

## Features

Reasons you *would* want to use Egg, and goals I'm designing toward.

- Small distributable size. Typical games should be <1 MB for the ROM, and add no more than 1 MB for the wrapper.
- - Obviously, the ROM size can go much larger than that if needed.
- Fast load time.
- Pixelly sprites and beepy music, and no recorded sound effects or music.
- - High resolution video and recorded sound should be possible, but I'm not optimizing for those.
- Uniform input. I consider the Standard Mapping Gamepad canonical.
- Portable. Build self-contained executables for web and any native platform where you can run eggdev.
- Easy eject. If a project outgrows Egg, you have the source. You can drop the Egg Runtime source in your project and season to taste, if need be.

## TODO

- [ ] Define data types.
- - [ ] Sound effects.
- [ ] Dev tool.
- - [ ] validate: sounds
- - [ ] Synth helper. Maybe as part of 'serve'.
- - [ ] Server is going to need some adjustment to enable editor and runtime to run together. Get a working runtime first.
- [ ] Editor.
- - [ ] Song: Playback.
- - [ ] Song: Live MIDI input.
- - [ ] Sound effects.
- - [ ] Launch game.
- [ ] Native runner.
- - [ ] Synthesizer
- - [ ] Live input mapping
- - [ ] Save and load input templates.
- - [ ] Config files
- - [ ] User's language for MacOS and Windows.
- [ ] Web runner.
- - [ ] Synth.
- - [ ] Input config.
- [ ] Rich set of client-side helper libraries.
- [ ] Example projects.
- [ ] Validate eject.
- [ ] Consider adding a uniform "linearFilter" parameter to mode7 blit.

## Third-Party Code

Dependencies required for build:
- The usual gcc tools (gcc, make, ...)
- LLVM with wasm32 support, for compiling ROMs.
- [wasm-micro-runtime](https://github.com/bytecodealliance/wasm-micro-runtime)
- [WABT](https://github.com/WebAssembly/wabt)
- (Optional) pulse
- (Optional) libasound
- (Optional) libdrm, libgbm
- GLESv2
- zlib

Code baked into our source:
- zlib.js by imaya. [Source](https://github.com/imaya/zlib.js). In the Web runtime.
- [Newlib](https://sourceware.org/newlib/). In client libraries.
