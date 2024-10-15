/* Exec.js
 * Manages the WebAssembly context.
 */
 
export class Exec {
  constructor(rt) {
    this.rt = rt;
    this.mem8 = this.mem32 = this.memf64 = [];
    this.fntab = [];
    this.textDecoder = new TextDecoder("utf8");
    this.textEncoder = new TextEncoder("utf8");
  }
  
  load(serial) {
    const options = { env: {
      egg_log: msgp => this.rt.egg_log(msgp),
      egg_terminate: s => this.rt.egg_terminate(s),
      egg_time_real: () => Date.now() / 1000,
      egg_time_local: (dstp, dsta) => this.rt.egg_time_local(dstp, dsta),
      egg_get_language: () => this.rt.lang,
      egg_set_language: l => this.rt.egg_set_language(l),
      egg_get_rom: (dstp, dsta) => this.rt.egg_get_rom(dstp, dsta),
      egg_store_get: (vp, va, kp, kc) => this.rt.egg_store_get(vp, va, kp, kc),
      egg_store_set: (kp, kc, vp, vc) => this.rt.egg_store_set(kp, kc, vp, vc),
      egg_store_key_by_index: (kp, ka, p) => this.rt.egg_store_key_by_index(kp, ka, p),
      egg_input_get_all: (dstp, dsta) => this.rt.input.egg_input_get_all(dstp, dsta),
      egg_input_get_one: plid => this.rt.input.egg_input_get_one(plid),
      egg_input_configure: () => this.rt.egg_input_configure(),
      egg_play_sound: (rid) => this.rt.audio.egg_play_sound(rid),
      egg_play_song: (rid, f, r) => this.rt.audio.egg_play_song(rid, f, r),
      egg_audio_event: (chid, op, a, b, dms) => this.rt.audio.egg_audio_event(chid, op, a, b, dms),
      egg_audio_get_playhead: () => this.rt.audio.egg_audio_get_playhead(),
      egg_audio_set_playhead: s => this.rt.audio.egg_audio_set_playhead(s),
      egg_image_decode_header: (wp, hp, psp, srcp, srcc) => this.rt.egg_image_decode_header(wp, hp, psp, srcp, srcc),
      egg_image_decode: (dstp, dsta, srcp, srcc) => this.rt.egg_image_decode(dstp, dsta, srcp, srcc),
      egg_texture_del: texid => this.rt.video.egg_texture_del(texid),
      egg_texture_new: () => this.rt.video.egg_texture_new(),
      egg_texture_get_status: (wp, hp, texid) => this.rt.video.egg_texture_get_status(wp, hp, texid),
      egg_texture_get_pixels: (dstp, dsta, texid) => this.rt.video.egg_texture_get_pixels(dstp, dsta, texid),
      egg_texture_load_image: (texid, rid) => this.rt.video.egg_texture_load_image(texid, rid),
      egg_texture_load_serial: (texid, srcp, srcc) => this.rt.video.egg_texture_load_serial(texid, srcp, srcc),
      egg_texture_load_raw: (texid, fmt, w, h, stride, srcp, srcc) => this.rt.video.egg_texture_load_raw(texid, fmt, w, h, stride, srcp, srcc),
      egg_draw_globals: (t, a) => this.rt.video.egg_draw_globals(t, a),
      egg_draw_clear: (dt, rgba) => this.rt.video.egg_draw_clear(dt, rgba),
      egg_draw_line: (dt, vp, c) => this.rt.video.egg_draw_line(dt, vp, c),
      egg_draw_rect: (dt, vp, c) => this.rt.video.egg_draw_rect(dt, vp, c),
      egg_draw_trig: (dt, vp, c) => this.rt.video.egg_draw_trig(dt, vp, c),
      egg_draw_decal: (dt, st, vp, c) => this.rt.video.egg_draw_decal(dt, st, vp, c),
      egg_draw_tile: (dt, st, vp, c) => this.rt.video.egg_draw_tile(dt, st, vp, c),
      egg_draw_mode7: (dt, st, vp, c, i) => this.rt.video.egg_draw_mode7(dt, st, vp, c, i),
    }};
    return WebAssembly.instantiate(serial, options).then(result => {
      const yoink = name => {
        if (!result.instance.exports[name]) {
          throw new Error(`ROM does not export required symbol '${name}'`);
        }
        this[name] = result.instance.exports[name];
      };
      yoink("memory");
      yoink("egg_client_quit");
      yoink("egg_client_init");
      yoink("egg_client_update");
      yoink("egg_client_render");
      this.mem8 = new Uint8Array(this.memory.buffer);
      this.mem32 = new Uint32Array(this.memory.buffer);
      this.memf64 = new Float64Array(this.memory.buffer);
      this.fntab = result.instance.exports.__indirect_function_table;
    });
  }
  
  /* Memory access.
   *******************************************************************/
  
  /* Uint8Array view of some portion of memory, or null if OOB.
   * Zero length is legal but not useful.
   * Negative length to run to the limit.
   */
  getMemory(p, c) {
    if (p < 0) return null;
    if (p > this.mem8.length) return null;
    if (c < 0) c = this.mem8.length - p;
    if (p > this.mem8.length - c) return null;
    return new Uint8Array(this.mem8.buffer, this.mem8.byteOffset + p, c);
  }
  
  /* UTF-8 string from (p) to the first NUL, but never longer than (limit).
   * Omit (limit) for no explicit limit.
   * Empty string if OOB or misencoded.
   */
  getString(p, limit) {
    if (p < 0) return "";
    let c = 0;
    if (typeof(limit) === "number") {
      while ((c < limit) && this.mem8[p + c]) c++;
    } else {
      while (this.mem8[p + c]) c++;
    }
    try {
      return this.textDecoder.decode(this.getMemory(p, c));
    } catch (e) {
      return "";
    }
  }
  
  setString(p, a, src) {
    src = this.textEncoder.encode(src);
    if (a < 0) return src.length;
    const dst = this.getMemory(p, a);
    if (src.length > dst.length) return src.length;
    dst.set(src);
    return src.length;
  }
}
