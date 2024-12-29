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
- ...but it is possible to build without WAMR or WABT; you'll only be able to build native executables.
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

## Examples

- [Season of Penance](https://github.com/aksommerville/penance)
- [Spelling Bee](https://github.com/aksommerville/spellingbee)
- [The Secret of the Seven Sauces](https://github.com/aksommerville/sevensauces)

## TODO

- [ ] Windows
- - [ ] Link is failing due to GL2 functions not found. I'm stumped.
- - [ ] Drivers, build config
- - [ ] Home path (eggrt_configure.c:eggrt_input_path_home_egg)
- - [ ] Language (eggrt_configure.c:eggrt_get_user_languages)
- - [ ] poll() substitute (http_internal.h). For now eggdev can't run a server.
- [ ] Audio
- - [ ] native: Neuter synth when dummy output.
- - [ ] Editor: Live feedback of synth playhead, via WebSocket.
- - [ ] Editor: Adjust synth config real time.
- - [ ] SongEditor trimEndTime is subject to rounding errors that cause it to think trimming is necessary sometimes when it's not. Can anything be done?
- - [ ] Confirm we're ignoring ignorable things, for future-proofing.
- - [ ] eggdev_compile_song_gm.c: Canned GM instruments and drums.
- - [ ] DrumChannelModal.js: Similar canned drums.
- - [ ] Doors Without Walls, the drum channel didn't play. Had GM and volume=128. Was it mistakenly flagged as ignore? Changing volume fixed it.
- - [ ] native: playhead for WAV songs
- - [ ] web: playhead for both types
- - [ ] SongEditor: Auto end time accounting for envelopes (esp for sound effects)
- - [ ] SongEditor: With empty input, create a default sound effect. Making the Note and EOT events every time gets tedious.
- - [ ] AudioService.play: Detect formats playable directly and don't call /api/compile for them.
- - [ ] Ensure parity between web and native.
- - [ ] editor: MIDI in
- - [ ] Channel.oscillateSub: Sub width stubbed out, figure this out.
- - [ ] Envelope editor: Don't create points on a single click. Require double-click or a modifier key.
- - [ ] Envelope editor: When dragging, keep tattle updating even when OOB.
- - [ ] Envelope editor: Option to sync time scale across all envelope editors.
- [ ] Clean up demo, make it a useful features test.
- [ ] Ship client libraries eg stdlib as static archives ready to link.
- [ ] Eggsamples
- - [ ] Carefully rename the old eggsamples repo from 202408.
- - [ ] Shoot-em-up
- - [ ] Formal RPG
- - [ ] Adventure
- - [ ] Platformer
- - [ ] Something in WAT instead of C.
- - [ ] Rhythm
- - [ ] Beat-em-up with Paper Mario style graphics.
- [ ] Eggzotics: Build for Egg and eg TinyArcade, for platforms that can't run Egg properly.
- [ ] Validate eject.
- [ ] linux: App icon stopped showing up. I think after upgrading to Ubuntu 24.04. Full Moon still works tho.

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
- MD5 (`sr_encodings.c`) by Christophe Devine (GPL)
- SHA-1 (`sr_encodings.c`) by Steve Reid (Public Domain)
- `synth_voice_sub.c`, IIR algorithms copied from _The Scientist and Engineer's Guide to Digital Signal Processing_ by Steven W Smith
