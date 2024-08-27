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

- [ ] Define data types.
- - [ ] Sound effects.
- [ ] Dev tool.
- - [ ] validate: sounds
- - [ ] Synth helper. Maybe as part of 'serve'.
- [ ] Editor.
- - [ ] Song: Playback.
- - [ ] Song: Live MIDI input.
- - [ ] Sound effects.
- - [ ] Launch game.
- [ ] Native runner.
- - [x] Timing
- - [x] Egg API (audio, video, input are stubs)
- - [x] Persistence
- - [ ] Synthesizer
- - [x] Renderer
- - [x] Drivers
- - [ ] Input manager
- - [ ] Live input mapping
- - [ ] Config files
- - [ ] User's language for MacOS and Windows.
- [ ] Web runner.
- [x] Remove bring-your-own-synthesizer from API. It's too risky and too complicated. (despite having proven that it technically can work).
- [ ] We might need to implement fwrite in addition to fprintf, I've seen clang substituting it when there's no formatting involved.
