/* Video.js
 */
 
export class Video {
  constructor(rt) {
    this.rt = rt;
  }
  
  /* Public entry points, for Runtime's consumption.
   ******************************************************************************/
  
  /* Read framebuffer size from metadata:1 and apply to the canvas.
   * Acquire a graphics context.
   */
  load() {
    const [fbw, fbh] = this.rt.rom.getMetadata("fb").split('x').map(v => +v);
    if (isNaN(fbw) || (fbw < 1) || (fbw > 4096) || isNaN(fbh) || (fbh < 1) || (fbh > 4096)) throw new Error("Invalid framebuffer size.");
    console.log(`Video.load: framebuffer ${fbw} x ${fbh}`);
    this.rt.canvas.width = fbw;
    this.rt.canvas.height = fbh;
    this.context = this.rt.canvas.getContext("webgl");
  }
  
  start() {
  }
  
  stop() {
  }
  
  beginFrame() {
  }
  
  endFrame() {
  }
  
  /* Egg Platform API.
   ********************************************************************************/
  
  egg_texture_del(texid) {
    console.log(`TODO Video.egg_texture_del(${texid})`);
  }
  
  egg_texture_new() {
    console.log(`TODO Video.egg_texture_new`);
    return -1;
  }
  
  egg_texture_get_status(wp, hp, texid) {
    console.log(`TODO Video.egg_texture_get_status(${texid})`);
    return -1;
  }
  
  egg_texture_get_pixels(dstp, dsta, texid) {
    console.log(`TODO Video.egg_texture_get_pixels(${texid})`);
    return -1;
  }
  
  egg_texture_load_image(texid, rid) {
    console.log(`TODO Video.egg_texture_load_image(${texid}, ${rid})`);
    return -1;
  }
  
  egg_texture_load_serial(texid, srcp, srcc) {
    console.log(`TODO Video.egg_texture_load_serial(${texid}, ${srcc})`);
    return -1;
  }
  
  egg_texture_load_raw(texid, fmt, w, h, stride, srcp, srcc) {
    console.log(`TODO Video.egg_texture_load_raw(${texid}, ${w}, ${h})`);
    return -1;
  }
  
  egg_draw_globals(tint, alpha) {
    //TODO
  }
  
  egg_draw_clear(dsttexid, rgba) {
    //TODO
  }
  
  egg_draw_line(dsttexid, vp, vc) {
    //TODO
  }
  
  egg_draw_rect(dsttexid, vp, vc) {
    //TODO
  }
  
  egg_draw_trig(dsttexid, vp, vc) {
    //TODO
  }
  
  egg_draw_decal(dsttexid, srctexid, vp, vc) {
    //TODO
  }
  
  egg_draw_tile(dsttexid, srctexid, vp, vc) {
    //TODO
  }
  
  egg_draw_mode7(dsttexid, srctexid, vp, vc) {
    //TODO
  }
}
