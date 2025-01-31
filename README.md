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
- - [The Secret of the Seven Sauces](https://github.com/aksommerville/sevensauces)

## TODO

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
- - [ ] Channel.oscillateSub: Sub width stubbed out, figure this out.
- - - [x] I need a convenient and fast way to compare native vs web output.
- - - - We have `eggdev sound` already, for native synth. Just need a web correllary.
- - - - This tool will have to run in a browser. We can have the server do the native bit.
- - - - New editor action "Compare Sounds". And right off the bat "sound/4-tom.mid" is obviously different Native vs Web.
- - [ ] web: Wave voice not using pitchenv (see snake)
- [ ] Ship client libraries eg stdlib as static archives ready to link.
- - This is more complex than it sounds. Clients would have to be able to tell eggdev which units they're interested in.
- - Otherwise we're building the libraries only for the benefit of clients that don't use the provided build process, which is hopefully none of them.
- [ ] Validate eject.
- [ ] linux: App icon stopped showing up. I think after upgrading to Ubuntu 24.04. Full Moon still works tho.
- - I'm stumped! Our window init and icon init are almost identical to Full Moon. It's xegl, not glx, but that shouldn't matter for this X11 stuff.
- - One difference is FM sets a Colormap at init. Tried that, and no difference (and why would there be?).
- - XChangeProperty returns 1 as expected.
- - Tried a minimal image, tried various modifications of the high bits... nothing.
- - ...updates: Full Moon is not actually working. If we change it now, it keeps the old icon. WM must have it cached somewhere.
- - ...Also, tried XSetWMHints instead of XChangeProperty: No dice.
- [ ] MacOS: On the iMac, I get a warning about monitoring keyboard when not focussed. We don't need that, how to turn it off?
- - Testing: Settings/Privacy & Security/Input Monitoring: Remove from list. Will retrigger next launch.
- - machid is causing it. Stub machid_new and it goes away.
- - I've seen others asking Apple about this but haven't seen any answers. IOHIDManager docs don't mention it.
- - Leaving unfixed. We'll have to advise users that it's fine to click "Deny".
- [ ] stdlib produces a 16 MB file due to fake-malloc's heap space. Can we avoid that somehow?

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
