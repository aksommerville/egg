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

- There's a demo ROM in this repo, with a bunch of useful feature tests. Just `make run`.
- [Eggsamples](https://github.com/aksommerville/eggsamples): A bunch of little games you're free to cannibalize.
- [Eggzotics](https://github.com/aksommerville/eggzotics): Demo games targetting highly-constrained platforms, that can also build for Egg.
- Real games:
- - [Season of Penance](https://github.com/aksommerville/penance)
- - [Spelling Bee](https://github.com/aksommerville/spellingbee)
- - [Thirty Seconds Apothecary](https://github.com/aksommerville/apothecary)
- - [The Secret of the Seven Sauces](https://github.com/aksommerville/sevensauces)

## TODO

- [x] !!! CRITICAL !!! Native inmgr must support nonzero resting states. Arcade cabinet depends on it.
- - Looking closer, it looks like inmgr is doing all the right things. Just incfg assumes the zero resting state.
- - Actually the problem is fairly narrow: We're starting the device selection with an axis in the wrong state. I've been pressing Left to start config every time.
- - Confirmed. If you Hello the device at incfg with a button instead of an axis, no problems.
- - There's a few things we might do:
- - - 1. Capture caps for all devices as incfg loads. This would get an incorrect value for the button that triggered incfg.
- - - 2. Capture caps for all devices within the evdev driver, and just echo them when clients ask. Lots of wasted memory, and lots of additional I/O at startup.
- - - 3. Require a button instead of axis, to Hello a device at incfg.
- - - 4. Expect users to Hello with a button instead of an axis and continue failing confusingly when they don't.
- - Option 3 seems reasonable.
- [x] Axis with nonzero resting state, if the first state change is to zero, we miss it.
- [ ] Windows
- - [ ] Link is failing due to GL2 functions not found. I'm stumped.
- - [ ] Drivers, build config
- - [ ] Home path (eggrt_configure.c:eggrt_input_path_home_egg)
- - [ ] Language (eggrt_configure.c:eggrt_get_user_languages)
- - [ ] poll() substitute (http_internal.h). For now eggdev can't run a server.
- [ ] Audio
- - [ ] SongEditor trimEndTime is subject to rounding errors that cause it to think trimming is necessary sometimes when it's not. Can anything be done?
- - [ ] Confirm we're ignoring ignorable things, for future-proofing.
- - [ ] eggdev_compile_song_gm.c: Canned GM instruments and drums.
- - [ ] DrumChannelModal.js: Similar canned drums.
- - [ ] Doors Without Walls, the drum channel didn't play. Had GM and volume=128. Was it mistakenly flagged as ignore? Changing volume fixed it.
- - - This is proving difficult to reproduce.
- - [ ] SongEditor: Auto end time accounting for envelopes (esp for sound effects)
- - [ ] SongEditor: With empty input, create a default sound effect. Making the Note and EOT events every time gets tedious.
- - [ ] Ensure parity between web and native.
- - [ ] Option to select a GM instrument and then expand it in place, "take this violin and tweak it"
- - [ ] Drum editor: Copy one to another
- [x] Remove `EGG_TEX_FMT_` from the API. They will always be RGBA now.
- [ ] minify: Algebraic refactoring of expressions. See comment by mf_node_eval() in mf_node_eval.c.
- [ ] minify: See mf_js_digest.c, plenty more opportunities for optimization.
- [ ] Is it feasible to implement a custom builder like Berry? And if so, generalize targets all the way (web should not be special from the user's point of view).
- [ ] When using graf and texcache, a texture might get evicted by texcache before graf commits it. We need to make graf and texcache aware of each other.
- - Eliminate texcache. Move its behavior into graf. And maybe allow conveniences at graf that use imageid instead of texid?
- [x] Session recording.
- [ ] Can we make session recording work when `USE_REAL_STDLIB`? We'd have to replace `rand()` somehow.
- [x] Map editor: Add a "join outside" tool that works like Heal but only affects edge cells, healing as if there were a neighbor across the edge.
- [ ] Hacked together "join outside" as a separate tool. Eventually I'm sure we'll want some toggle to make the rainbow pencil behave like this.
- [ ] Map editor: Hold modifier while clicking to invoke a modal (eg poiMove+ctl=poiEdit), editor misses the release of the modifier.
- [ ] Map editor: Should initialize to NS_sys_mapw,NS_sys_maph. But I think it's hard-coded to 20x15
- [ ] GM piccolo out of tune
- [ ] Some way to zap input settings, or get back into input config, for web in last-resort scenarios.
- [x] (Dot's Wicked Garden) Multiple complaints about audio distortion in Firefox, presumably Windows.
- - Not just Windows, it happens in Linux too. Within about 2 minutes of play, CPU consumption approaches 100% and audio starts cutting out.
- - There's suspicious behavior in Chrome too, now that I'm looking close. RAM keeps growing, and CPU grows too, tho much slower than in Firefox.
- - [x] Check for leaks in the synthesizer.
- - FM voices, I was stopping the carrier but not the modulator. Chrome must have some emergency shut-off for dead WebAudio nodes, Firefox must not.
- [ ] Native image decoder does not properly manage indexed PNG.
- [ ] editor: MapStore should accept non-numeric arguments for `neighbors`

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
- [Newlib](https://sourceware.org/newlib/). In client libraries.
- `synth_voice_sub.c`, IIR algorithms copied from _The Scientist and Engineer's Guide to Digital Signal Processing_ by Steven W Smith
