/* Video.js
 */
 
import { Rom } from "./Rom.js";
 
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
    this.rt.canvas.width = fbw;
    this.rt.canvas.height = fbh;
    this.gl = this.rt.canvas.getContext("webgl");
    
    this.pgm_raw = null;
    this.pgm_decal = null;
    this.pgm_tile = null;
    
    this.u_raw = {};
    this.u_decal = {};
    this.u_tile = {};
    
    this.textures = [null]; // Indexed by texid; zero is reserved. {id,w,h,fmt}
    
    this.tint = 0;
    this.alpha = 1;
    
    // Scratch storage for eg the main dump, where we have a fixed-size buffer.
    this.requireVbuf(1024);
    
    this.gl.blendFunc(this.gl.SRC_ALPHA,this.gl.ONE_MINUS_SRC_ALPHA);
    this.gl.enable(this.gl.BLEND);
    if (!(this.buffer = this.gl.createBuffer())) throw new Error(`Failed to create WebGL vertex buffer.`);
    
    if (
      (this.egg_texture_new() !== 1) ||
      (this.egg_texture_load_raw(1, Video.TEX_FMT_RGBA, fbw, fbh, fbw << 2, 0, 0) < 0)
    ) throw new Error(`Failed to create main framebuffer.`);
    
    this.compileShaders();
  }
  
  start() {
  }
  
  stop() {
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, null);
    this.gl.viewport(0, 0, this.rt.canvas.width, this.rt.canvas.height);
    this.gl.clearColor(0.0, 0.0, 0.0, 1.0);
    this.gl.clear(this.gl.COLOR_BUFFER_BIT);
  }
  
  beginFrame() {
    this.egg_draw_globals(0x00000000, 0xff);
  }
  
  endFrame() {
    const srctex = this.textures[1];
    if (!srctex) return;

    const fbw=this.rt.canvas.width, fbh=this.rt.canvas.height;
    const aposv = this.vbufs16;
    const tcv = this.vbuff32;
    aposv[ 0] = 0;   aposv[ 1] = 0;   tcv[ 1] = 0.0; tcv[ 2] = 1.0;
    aposv[ 6] = 0;   aposv[ 7] = fbh; tcv[ 4] = 0.0; tcv[ 5] = 0.0;
    aposv[12] = fbw; aposv[13] = 0;   tcv[ 7] = 1.0; tcv[ 8] = 1.0;
    aposv[18] = fbw; aposv[19] = fbh; tcv[10] = 1.0; tcv[11] = 0.0;
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.buffer);
    this.gl.bufferData(this.gl.ARRAY_BUFFER, this.vbuf, this.gl.STREAM_DRAW);
    
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, null);
    this.gl.viewport(0, 0, fbw, fbh);
    this.gl.useProgram(this.pgm_decal);
    this.gl.uniform4f(this.u_decal.texlimit, 0, 0, 1, 1);
    this.gl.uniform2f(this.u_decal.screensize, fbw, fbh);
    this.gl.uniform1i(this.u_decal.sampler, 0);
    this.gl.activeTexture(this.gl.TEXTURE0);
    this.gl.bindTexture(this.gl.TEXTURE_2D, srctex.id);
    this.gl.uniform4f(this.u_decal.tint, 0.0, 0.0, 0.0, 0.0);
    this.gl.uniform1f(this.u_decal.alpha, 1.0);
    this.gl.disable(this.gl.BLEND);
    this.gl.enableVertexAttribArray(0);
    this.gl.enableVertexAttribArray(1);
    this.gl.vertexAttribPointer(0, 2, this.gl.SHORT, false, 12, 0);
    this.gl.vertexAttribPointer(1, 2, this.gl.FLOAT, false, 12, 4);
    this.gl.drawArrays(this.gl.TRIANGLE_STRIP, 0, 4);
    this.gl.disableVertexAttribArray(0);
    this.gl.disableVertexAttribArray(1);
    this.gl.enable(this.gl.BLEND);
  }
  
  /* Private renderry bits.
   *************************************************************************/
   
  compileShaders() {
    this.pgm_raw = this.compileShader("raw", ["apos", "acolor"], ["screensize", "alpha", "tint"]);
    this.pgm_decal = this.compileShader("decal", ["apos", "atexcoord"], ["screensize", "sampler", "alpha", "tint", "texlimit"]);
    this.pgm_tile = this.compileShader("tile", ["apos", "atileid", "axform"], ["screensize", "sampler", "alpha", "tint", "pointsize"]);
  }
  
  compileShader(name, aNames, uNames) {
    const pid = this.gl.createProgram();
    if (!pid) throw new Error(`Failed to create new WebGL program for ${JSON.stringify(name)}`);
    try {
      this.compileShader1(name, pid, this.gl.VERTEX_SHADER, Video.glsl[name + "_v"]);
      this.compileShader1(name, pid, this.gl.FRAGMENT_SHADER, Video.glsl[name + "_f"]);
      for (let i=0; i<aNames.length; i++) {
        this.gl.bindAttribLocation(pid, i, aNames[i]);
      }
      this.gl.linkProgram(pid);
      if (!this.gl.getProgramParameter(pid, this.gl.LINK_STATUS)) {
        const log = this.gl.getProgramInfoLog(pid);
        throw new Error(`Failed to link program ${JSON.stringify(name)}:\n${log}`);
      }
      this.gl.useProgram(pid);
      const udst = this["u_" + name];
      for (const uName of uNames) {
        udst[uName] = this.gl.getUniformLocation(pid, uName);
      }
    } catch (e) {
      this.gl.deleteProgram(pid);
      throw e;
    }
    return pid;
  }
  
  compileShader1(name, pid, type, src) {
    const sid = this.gl.createShader(type);
    if (!sid) throw new Error(`Failed to create new WebGL shader for ${JSON.stringify(name)}`);
    try {
      this.gl.shaderSource(sid, src);
      this.gl.compileShader(sid);
      if (!this.gl.getShaderParameter(sid, this.gl.COMPILE_STATUS)) {
        const log = this.gl.getShaderInfoLog(sid);
        throw new Error(`Failed to link ${(type === this.gl.VERTEX_SHADER) ? "vertex" : "fragment"} shader for ${JSON.stringify(name)}:\n${log}`);
      }
      this.gl.attachShader(pid, sid);
    } finally {
      this.gl.deleteShader(sid);
    }
  }
  
  expandOneBit(src, w, h, stride) {
    const dst = new Uint8Array(w * h * 4);
    for (let yi=h, dstp=0, srcrowp=0; yi-->0; srcrowp+=stride) {
      for (let xi=w, srcp=srcrowp, srcmask=0x80; xi-->0; ) {
        if (src[srcp] & srcmask) {
          dst[dstp++] = 0xff;
          dst[dstp++] = 0xff;
          dst[dstp++] = 0xff;
          dst[dstp++] = 0xff;
        } else {
          dst[dstp++] = 0x00;
          dst[dstp++] = 0x00;
          dst[dstp++] = 0x00;
          dst[dstp++] = 0x00;
        }
        if (srcmask === 1) { srcmask = 0x80; srcp++; }
        else srcmask >>= 1;
      }
    }
    return dst;
  }
  
  loadTexture(tex, fmt, w, h, stride, src) {
    
    if ((w < 1) || (w > Video.TEXTURE_SIZE_LIMIT) || (h < 1) || (h > Video.TEXTURE_SIZE_LIMIT)) return -1;
    
    this.gl.bindTexture(this.gl.TEXTURE_2D, tex.id);
    
    let ifmt = this.gl.RGBA, glfmt = this.gl.RGBA, type = this.gl.UNSIGNED_BYTE;
    if ((src instanceof Image) || (src instanceof HTMLCanvasElement)) {
      // WebGL accepts these directly.
      this.gl.texImage2D(this.gl.TEXTURE_2D, 0, ifmt, glfmt, type, src);
    } else {
      // Validate raw pixels.
      let minstride = w << 2;
      switch (fmt) {
        case Video.TEX_FMT_RGBA: break;
        case Video.TEX_FMT_A8: ifmt = this.gl.ALPHA; glfmt = this.gl.ALPHA; minstride = w; break;
        case Video.TEX_FMT_A1: {
            if (src) src = this.expandOneBit(src, w, h, stride);
            fmt = Video.TEX_FMT_RGBA;
            stride = w << 2;
          } break;
        default: return -1;
      }
      if (src) {
        if (stride < 1) stride = minstride;
        else if (stride < minstride) return -1;
        if (h * stride > src.length) return -1;
        if (stride !== minstride) {
          if (!(src = this.restride(src, minstride, stride, h))) return -1;
        }
      }
      this.gl.texImage2D(this.gl.TEXTURE_2D, 0, ifmt, w, h, 0, glfmt, type, src);
    }
    
    tex.w = w;
    tex.h = h;
    tex.fmt = fmt;
    return 0;
  }
  
  restride(src, dststride, srcstride, h) {
    const cpc = Math.min(dststride, srcstride);
    const dst = new Uint8Array(dststride * h);
    for (let yi=h, dstrowp=0, srcrowp=0; yi-->0; dstrowp+=dststride, srcrowp+=srcstride) {
      for (let xi=cpc, dstp=dstrowp, srcp=srcrowp; xi-->0; dstp++, srcp++) {
        dst[dstp] = src[srcp];
      }
    }
    return dst;
  }
  
  requireFramebuffer(tex) {
    if (tex.fbid) return 1;
    if (!(tex.fbid = this.gl.createFramebuffer())) return 0;
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, tex.fbid);
    this.gl.framebufferTexture2D(this.gl.FRAMEBUFFER, this.gl.COLOR_ATTACHMENT0, this.gl.TEXTURE_2D, tex.id, 0);
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, null);
    return 1;
  }
  
  requireVbuf(bytes) {
    if (this.vbuf && (bytes <= this.vbuf.byteLength)) return;
    this.vbuf = new ArrayBuffer((bytes + 1024) & ~1023);
    this.vbufu8 = new Uint8Array(this.vbuf);
    this.vbufs16 = new Int16Array(this.vbuf);
    this.vbuff32 = new Float32Array(this.vbuf);
  }
  
  /* Egg Platform API.
   ********************************************************************************/
  
  egg_texture_del(texid) {
    const tex = this.textures[texid];
    if (!tex) return;
    this.gl.deleteTexture(tex.id);
    this.textures[texid] = null;
  }
  
  egg_texture_new() {
    const texid = this.textures.length;
    const id = this.gl.createTexture();
    if (!id) return -1;
    this.gl.bindTexture(this.gl.TEXTURE_2D, id);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_S, this.gl.CLAMP_TO_EDGE);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_T, this.gl.CLAMP_TO_EDGE);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, this.gl.NEAREST);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, this.gl.NEAREST);
    this.textures[texid] = { id, w:0, h:0, fmt:0 };
    return texid;
  }
  
  egg_texture_get_status(wp, hp, texid) {
    const tex = this.textures[texid];
    if (!tex) return -1;
    if (wp) this.rt.exec.mem32[wp >> 2] = tex.w;
    if (hp) this.rt.exec.mem32[hp >> 2] = tex.h;
    return 1;
  }
  
  egg_texture_get_pixels(dstp, dsta, texid) {
    const tex = this.textures[texid];
    if (!tex) return -1;
    if (!this.requireFramebuffer(tex)) return -1;
    const dstc = tex.w * 4 * tex.h;
    if (dstc > dsta) return dstc;
    const dst = this.rt.exec.getMemory(dstp, dstc);
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, tex.fbid);
    this.gl.readPixels(0, 0, tex.w, tex.h, this.gl.RGBA, this.gl.UNSIGNED_BYTE, dst);
    return dstc;
  }
  
  egg_texture_load_image(texid, rid) {
    if (texid < 2) return -1;
    const tex = this.textures[texid];
    if (!tex) return -1;
    const res = this.rt.rom.getResourceEntry(Rom.TID_image, rid);
    if (!res || !res.image) return -1;
    return this.loadTexture(tex, 1/*RGBA*/, res.image.naturalWidth, res.image.naturalHeight, 0, res.image);
  }
  
  egg_texture_load_raw(texid, fmt, w, h, stride, srcp, srcc) {
    const tex = this.textures[texid];
    if (!tex) return -1;
    
    // Uploading to [1] is legal but only if size was unset, or new size matches.
    if (texid === 1) {
      if (tex.w && ((w !== tex.w) || (h !== tex.h))) return -1;
    }
    
    let src = null;
    if (srcp || srcc) {
      if (!(src = this.rt.exec.getMemory(srcp, srcc))) return -1;
    }
    
    return this.loadTexture(tex, fmt, w, h, stride, src);
  }
  
  egg_draw_globals(tint, alpha) {
    this.tr = ((tint >> 24) & 0xff) / 255.0;
    this.tg = ((tint >> 16) & 0xff) / 255.0;
    this.tb = ((tint >> 8) & 0xff) / 255.0;
    this.ta = (tint & 0xff) / 255.0;
    this.alpha = alpha / 255.0;
  }
  
  egg_draw_clear(dsttexid, rgba) {
    const dsttex = this.textures[dsttexid];
    if (!this.requireFramebuffer(dsttex)) return;
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, dsttex.fbid);
    this.gl.viewport(0, 0, dsttex.w, dsttex.h);
    if (rgba) this.gl.clearColor(((rgba >> 24) & 0xff) / 255.0, ((rgba >> 16) & 0xff) / 255.0, ((rgba >> 8) & 0xff) / 255.0, (rgba & 0xff) / 255.0);
    else this.gl.clearColor(0.0, 0.0, 0.0, 0.0);
    this.gl.clear(this.gl.COLOR_BUFFER_BIT);
  }
  
  /* s16 ax
   * s16 ay
   * s16 bx
   * s16 by
   * u32 rgba
   * =12
   */
  egg_draw_line(dsttexid, vp, vc) {
    if (vc < 1) return;
    const dsttex = this.textures[dsttexid];
    if (!dsttex) return;
    const src = this.rt.exec.getMemory(vp, vc * 12);
    if (!src) return;
    const vtxc = vc * 2; // 2 GL vertices per input primitive.
    this.requireVbuf(vtxc * 8);
    const s16 = (n) => ((n & 0x8000) ? (n | ~0x7fff) : n);
    for (let i=vc, p16=0, p8=4, srcp=0; i-->0; p16+=8, p8+=16, srcp+=12) {
      this.vbufs16[p16+0] = s16(src[srcp+0] | (src[srcp+1] << 8));
      this.vbufs16[p16+1] = s16(src[srcp+2] | (src[srcp+3] << 8));
      this.vbufs16[p16+4] = s16(src[srcp+4] | (src[srcp+5] << 8));
      this.vbufs16[p16+5] = s16(src[srcp+6] | (src[srcp+7] << 8));
      this.vbufu8[p8+0] = this.vbufu8[p8+8] = src[srcp+8];
      this.vbufu8[p8+1] = this.vbufu8[p8+9] = src[srcp+9];
      this.vbufu8[p8+2] = this.vbufu8[p8+10] = src[srcp+10];
      this.vbufu8[p8+3] = this.vbufu8[p8+11] = src[srcp+11];
    }
    this.requireFramebuffer(dsttex);
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, dsttex.fbid);
    this.gl.useProgram(this.pgm_raw);
    this.gl.viewport(0, 0, dsttex.w, dsttex.h);
    this.gl.uniform2f(this.u_raw.screensize, dsttex.w, dsttex.h);
    this.gl.uniform1f(this.u_raw.alpha, this.alpha);
    this.gl.uniform4f(this.u_raw.tint, this.tr, this.tg, this.tb, this.ta);
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.buffer);
    this.gl.bufferData(this.gl.ARRAY_BUFFER, this.vbuf, this.gl.STREAM_DRAW);
    this.gl.enableVertexAttribArray(0);
    this.gl.enableVertexAttribArray(1);
    this.gl.vertexAttribPointer(0, 2, this.gl.SHORT, false, 8, 0);
    this.gl.vertexAttribPointer(1, 4, this.gl.UNSIGNED_BYTE, true, 8, 4);
    this.gl.drawArrays(this.gl.LINES, 0, vtxc);
    this.gl.disableVertexAttribArray(0);
    this.gl.disableVertexAttribArray(1);
  }
  
  /* s16 x
   * s16 y
   * s16 w
   * s16 h
   * u32 rgba
   * =12
   */
  egg_draw_rect(dsttexid, vp, vc) {
    if (vc < 1) return;
    const dsttex = this.textures[dsttexid];
    if (!dsttex) return;
    const src = this.rt.exec.getMemory(vp, vc * 12);
    if (!src) return;
    this.requireVbuf(32); // 4 vertices * 8 bytes each; we do a separate call for each rect.
    this.requireFramebuffer(dsttex);
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, dsttex.fbid);
    this.gl.useProgram(this.pgm_raw);
    this.gl.viewport(0, 0, dsttex.w, dsttex.h);
    this.gl.uniform2f(this.u_raw.screensize, dsttex.w, dsttex.h);
    this.gl.uniform1f(this.u_raw.alpha, this.alpha);
    this.gl.uniform4f(this.u_raw.tint, this.tr, this.tg, this.tb, this.ta);
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.buffer);
    this.gl.enableVertexAttribArray(0);
    this.gl.enableVertexAttribArray(1);
    this.gl.vertexAttribPointer(0, 2, this.gl.SHORT, false, 8, 0);
    this.gl.vertexAttribPointer(1, 4, this.gl.UNSIGNED_BYTE, true, 8, 4);
    const s16 = (n) => ((n & 0x8000) ? (n | ~0x7fff) : n);
    for (let i=vc, srcp=0; i-->0; srcp+=12) {
      const x0 = s16(src[srcp+0] | (src[srcp+1] << 8));
      const y0 = s16(src[srcp+2] | (src[srcp+3] << 8));
      const x1 = s16(x0 + (src[srcp+4] | (src[srcp+5] << 8)));
      const y1 = s16(y0 + (src[srcp+6] | (src[srcp+7] << 8)));
      const r = src[srcp+8];
      const g = src[srcp+9];
      const b = src[srcp+10];
      const a = src[srcp+11];
      this.vbufs16[0] = this.vbufs16[4] = x0;
      this.vbufs16[8] = this.vbufs16[12] = x1;
      this.vbufs16[1] = this.vbufs16[9] = y0;
      this.vbufs16[5] = this.vbufs16[13] = y1;
      this.vbufu8[4] = this.vbufu8[12] = this.vbufu8[20] = this.vbufu8[28] = r;
      this.vbufu8[5] = this.vbufu8[13] = this.vbufu8[21] = this.vbufu8[29] = g;
      this.vbufu8[6] = this.vbufu8[14] = this.vbufu8[22] = this.vbufu8[30] = b;
      this.vbufu8[7] = this.vbufu8[15] = this.vbufu8[23] = this.vbufu8[31] = a;
      this.gl.bufferData(this.gl.ARRAY_BUFFER, this.vbuf, this.gl.STREAM_DRAW);
      this.gl.drawArrays(this.gl.TRIANGLE_STRIP, 0, 4);
    }
    this.gl.disableVertexAttribArray(0);
    this.gl.disableVertexAttribArray(1);
  }
  
  /* s16 ax
   * s16 ay
   * s16 bx
   * s16 by
   * s16 cx
   * s16 cy
   * u32 rgba
   * =16
   */
  egg_draw_trig(dsttexid, vp, vc) {
    if (vc < 1) return;
    const dsttex = this.textures[dsttexid];
    if (!dsttex) return;
    const src = this.rt.exec.getMemory(vp, vc * 16);
    if (!src) return;
    const vtxc = vc * 3;
    this.requireVbuf(vtxc * 8);
    this.requireFramebuffer(dsttex);
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, dsttex.fbid);
    this.gl.useProgram(this.pgm_raw);
    this.gl.viewport(0, 0, dsttex.w, dsttex.h);
    this.gl.uniform2f(this.u_raw.screensize, dsttex.w, dsttex.h);
    this.gl.uniform1f(this.u_raw.alpha, this.alpha);
    this.gl.uniform4f(this.u_raw.tint, this.tr, this.tg, this.tb, this.ta);
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.buffer);
    this.gl.enableVertexAttribArray(0);
    this.gl.enableVertexAttribArray(1);
    this.gl.vertexAttribPointer(0, 2, this.gl.SHORT, false, 8, 0);
    this.gl.vertexAttribPointer(1, 4, this.gl.UNSIGNED_BYTE, true, 8, 4);
    const s16 = (n) => ((n & 0x8000) ? (n | ~0x7fff) : n);
    for (let i=vc, p16=0, p8=0, srcp=0; i-->0; srcp+=16, p16+=12, p8+=24) {
      this.vbufs16[p16+0] = s16(src[srcp+0] | (src[srcp+1] << 8));
      this.vbufs16[p16+1] = s16(src[srcp+2] | (src[srcp+3] << 8));
      this.vbufs16[p16+4] = s16(src[srcp+4] | (src[srcp+5] << 8));
      this.vbufs16[p16+5] = s16(src[srcp+6] | (src[srcp+7] << 8));
      this.vbufs16[p16+8] = s16(src[srcp+8] | (src[srcp+9] << 8));
      this.vbufs16[p16+9] = s16(src[srcp+10] | (src[srcp+11] << 8));
      this.vbufu8[p8+4] = this.vbufu8[p8+12] = this.vbufu8[p8+20] = src[srcp+12];
      this.vbufu8[p8+5] = this.vbufu8[p8+13] = this.vbufu8[p8+21] = src[srcp+13];
      this.vbufu8[p8+6] = this.vbufu8[p8+14] = this.vbufu8[p8+22] = src[srcp+14];
      this.vbufu8[p8+7] = this.vbufu8[p8+15] = this.vbufu8[p8+23] = src[srcp+15];
    }
    this.gl.bufferData(this.gl.ARRAY_BUFFER, this.vbuf, this.gl.STREAM_DRAW);
    this.gl.drawArrays(this.gl.TRIANGLES, 0, vtxc);
    this.gl.disableVertexAttribArray(0);
    this.gl.disableVertexAttribArray(1);
  }
  
  /* s16 dstx
   * s16 dsty
   * s16 srcx
   * s16 srcy
   * s16 w
   * s16 h
   * u8 xform
   * u8 dummy
   * =14
   */
  egg_draw_decal(dsttexid, srctexid, vp, vc) {
    if (vc < 1) return;
    const dsttex = this.textures[dsttexid];
    if (!dsttex) return;
    this.requireFramebuffer(dsttex);
    const srctex = this.textures[srctexid];
    if (!srctex) return;
    const src = this.rt.exec.getMemory(vp, vc * 14);
    if (!src) return;
    this.requireVbuf(48); // 4 vertices * 12 bytes each; one call per primitive.
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, dsttex.fbid);
    this.gl.bindTexture(this.gl.TEXTURE_2D, srctex.id);
    this.gl.useProgram(this.pgm_decal);
    this.gl.viewport(0, 0, dsttex.w, dsttex.h);
    this.gl.uniform2f(this.u_decal.screensize, dsttex.w, dsttex.h);
    this.gl.uniform1f(this.u_decal.alpha, this.alpha);
    this.gl.uniform4f(this.u_decal.tint, this.tr, this.tg, this.tb, this.ta);
    this.gl.uniform4f(this.u_decal.texlimit, 0, 0, 1, 1); // No need for this since output is axis-aligned.
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.buffer);
    this.gl.enableVertexAttribArray(0);
    this.gl.enableVertexAttribArray(1);
    this.gl.vertexAttribPointer(0, 2, this.gl.SHORT, false, 12, 0);
    this.gl.vertexAttribPointer(1, 2, this.gl.FLOAT, false, 12, 4);
    const s16 = (n) => ((n & 0x8000) ? (n | ~0x7fff) : n);
    for (let i=vc, srcp=0; i-->0; srcp+=14) {
      const dstx = s16(src[srcp+0] | (src[srcp+1] << 8));
      const dsty = s16(src[srcp+2] | (src[srcp+3] << 8));
      const srcx = s16(src[srcp+4] | (src[srcp+5] << 8));
      const srcy = s16(src[srcp+6] | (src[srcp+7] << 8));
      const w = src[srcp+8] | (src[srcp+9] << 8);
      const h = src[srcp+10] | (src[srcp+11] << 8);
      const xform = src[srcp+12];
      let dx0=dstx, dy0=dsty, sx0=srcx, sy0=srcy;
      let dx1=dstx+w, dy1=dsty+h, sx1=srcx+w, sy1=srcy+h;
      if (xform & 4) { // SWAP
        if (xform & 1) { // XREV
          const tmp=dy0; dy0=dy1; dy1=tmp;
        }
        if (xform & 2) { // YREV
          const tmp=dx0; dx0=dx1; dx1=tmp;
        }
        this.vbufs16[0] = dx0;
        this.vbufs16[1] = dy0;
        this.vbufs16[6] = dx1;
        this.vbufs16[7] = dy0;
        this.vbufs16[12] = dx0;
        this.vbufs16[13] = dy1;
        this.vbufs16[18] = dx1;
        this.vbufs16[19] = dy1;
      } else {
        if (xform & 1) { // XREV
          const tmp=dx0; dx0=dx1; dx1=tmp;
        }
        if (xform & 2) { // YREV
          const tmp=dy0; dy0=dy1; dy1=tmp;
        }
        this.vbufs16[0] = dx0;
        this.vbufs16[1] = dy0;
        this.vbufs16[6] = dx0;
        this.vbufs16[7] = dy1;
        this.vbufs16[12] = dx1;
        this.vbufs16[13] = dy0;
        this.vbufs16[18] = dx1;
        this.vbufs16[19] = dy1;
      }
      this.vbuff32[1] = this.vbuff32[4] = srcx / srctex.w;
      this.vbuff32[7] = this.vbuff32[10] = (srcx + w) / srctex.w;
      this.vbuff32[2] = this.vbuff32[8] = srcy / srctex.h;
      this.vbuff32[5] = this.vbuff32[11] = (srcy + h) / srctex.h;
      this.gl.bufferData(this.gl.ARRAY_BUFFER, this.vbuf, this.gl.STREAM_DRAW);
      this.gl.drawArrays(this.gl.TRIANGLE_STRIP, 0, 4);
    }
    this.gl.disableVertexAttribArray(0);
    this.gl.disableVertexAttribArray(1);
  }
  
  /* s16 dstx
   * s16 dsty
   * u8 tileid
   * u8 xform
   * =6
   * This one is extra special because the intake format can be passed along verbatim.
   */
  egg_draw_tile(dsttexid, srctexid, vp, vc) {
    if (vc < 1) return;
    const dsttex = this.textures[dsttexid];
    if (!dsttex) return;
    this.requireFramebuffer(dsttex);
    const srctex = this.textures[srctexid];
    if (!srctex) return;
    const src = this.rt.exec.getMemory(vp, vc * 6);
    if (!src) return;
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, dsttex.fbid);
    this.gl.useProgram(this.pgm_tile);
    this.gl.viewport(0, 0, dsttex.w, dsttex.h);
    this.gl.uniform2f(this.u_tile.screensize, dsttex.w, dsttex.h);
    this.gl.bindTexture(this.gl.TEXTURE_2D, srctex.id);
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.buffer);
    this.gl.bufferData(this.gl.ARRAY_BUFFER, src, this.gl.STREAM_DRAW);
    this.gl.uniform4f(this.u_tile.tint, this.tr, this.tg, this.tb, this.ta);
    this.gl.uniform1f(this.u_tile.alpha, this.alpha);
    this.gl.uniform1f(this.u_tile.pointsize, srctex.w >> 4);
    this.gl.enableVertexAttribArray(0);
    this.gl.enableVertexAttribArray(1);
    this.gl.enableVertexAttribArray(2);
    this.gl.vertexAttribPointer(0, 2, this.gl.SHORT, false, 6, 0);
    this.gl.vertexAttribPointer(1, 1, this.gl.UNSIGNED_BYTE, false, 6, 4);
    this.gl.vertexAttribPointer(2, 1, this.gl.UNSIGNED_BYTE, false, 6, 5);
    this.gl.drawArrays(this.gl.POINTS, 0, vc);
    this.gl.disableVertexAttribArray(0);
    this.gl.disableVertexAttribArray(1);
    this.gl.disableVertexAttribArray(2);
  }
  
  /* s16 dstx
   * s16 dsty
   * s16 srcx
   * s16 srcy
   * s16 w
   * s16 h
   * f32 xscale
   * f32 yscale
   * f32 rotate
   * =24
   */
  egg_draw_mode7(dsttexid, srctexid, vp, vc, interpolate) {
    if (vc < 1) return;
    const dsttex = this.textures[dsttexid];
    if (!dsttex) return;
    this.requireFramebuffer(dsttex);
    const srctex = this.textures[srctexid];
    if (!srctex) return;
    const src = this.rt.exec.getMemory(vp, vc * 24);
    if (!src) return;
    this.requireVbuf(48); // 4 vertices * 12 bytes per
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.buffer);
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, dsttex.fbid);
    this.gl.viewport(0, 0, dsttex.w, dsttex.h);
    this.gl.useProgram(this.pgm_decal);
    this.gl.uniform2f(this.u_decal.screensize, dsttex.w, dsttex.h);
    this.gl.uniform1i(this.u_decal.sampler, 0);
    this.gl.activeTexture(this.gl.TEXTURE0);
    this.gl.bindTexture(this.gl.TEXTURE_2D, srctex.id);
    if (interpolate) {
      this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, this.gl.LINEAR);
      this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, this.gl.LINEAR);
    }
    this.gl.uniform4f(this.u_decal.tint, this.tr, this.tg, this.tb, this.ta);
    this.gl.uniform1f(this.u_decal.alpha, this.alpha);
    this.gl.enableVertexAttribArray(0);
    this.gl.enableVertexAttribArray(1);
    this.gl.vertexAttribPointer(0, 2, this.gl.SHORT, false, 12, 0);
    this.gl.vertexAttribPointer(1, 2, this.gl.FLOAT, false, 12, 4);
    
    const aposv = this.vbufs16;
    const tcv = this.vbuff32;
    const src16 = new Int16Array(src.buffer, src.byteOffset, vc * 12);
    const src32 = new Float32Array(src.buffer, src.byteOffset, vc * 6);
    for (let i=vc, srcp16=0, srcp32=3; i-->0; srcp16+=12, srcp32+=6) {
      const dstx = src16[srcp16+0];
      const dsty = src16[srcp16+1];
      const srcx = src16[srcp16+2];
      const srcy = src16[srcp16+3];
      const w = src16[srcp16+4];
      const h = src16[srcp16+5];
      const xscale = src32[srcp32+0];
      const yscale = src32[srcp32+1];
      const rotate = src32[srcp32+2];
      
      const cost = Math.cos(-rotate);
      const sint = Math.sin(-rotate);
      const halfw = w * xscale * 0.5;
      const halfh = h * yscale * 0.5;
      const nwx = Math.round( cost * halfw + sint * halfh);
      const nwy = Math.round(-sint * halfw + cost * halfh);
      const swx = Math.round( cost * halfw - sint * halfh);
      const swy = Math.round(-sint * halfw - cost * halfh);
      aposv[ 0] = dstx - nwx; aposv[ 1] = dsty - nwy; tcv[ 1] = 0.0; tcv[ 2] = 0.0;
      aposv[ 6] = dstx - swx; aposv[ 7] = dsty - swy; tcv[ 4] = 0.0; tcv[ 5] = 1.0;
      aposv[12] = dstx + swx; aposv[13] = dsty + swy; tcv[ 7] = 1.0; tcv[ 8] = 0.0;
      aposv[18] = dstx + nwx; aposv[19] = dsty + nwy; tcv[10] = 1.0; tcv[11] = 1.0;
      const tx0 = srcx / srctex.w;
      const tx1 = w / srctex.w;
      const ty0 = srcy / srctex.h;
      const ty1 = h / srctex.h;
      for (let i=1; i<12; i+=3) {
        tcv[i] = tx0 + tx1 * tcv[i];
        tcv[i+1] = ty0 + ty1 * tcv[i+1];
      }
      this.gl.bufferData(this.gl.ARRAY_BUFFER, this.vbuf, this.gl.STREAM_DRAW);
      this.gl.uniform4f(this.u_decal.texlimit, tx0, ty0, tx0 + tx1, ty0 + ty1);
      this.gl.drawArrays(this.gl.TRIANGLE_STRIP, 0, 4);
    }
    this.gl.disableVertexAttribArray(0);
    this.gl.disableVertexAttribArray(1);
    if (interpolate) {
      this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, this.gl.NEAREST);
      this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, this.gl.NEAREST);
    }
  }
}

/* GLSL.
 *********************************************************************/
 
Video.glsl = {
  raw_v:
    "#version 100\n" +
    "precision mediump float;\n" +
    "uniform vec2 screensize;\n" +
    "uniform vec4 tint;\n" +
    "uniform float alpha;\n" +
    "attribute vec2 apos;\n" +
    "attribute vec4 acolor;\n" +
    "varying vec4 vcolor;\n" +
    "void main() {\n" +
      "vec2 npos=(apos*2.0)/screensize-1.0;\n" +
      "gl_Position=vec4(npos,0.0,1.0);\n" +
      "vcolor=vec4(mix(acolor.rgb,tint.rgb,tint.a),acolor.a*alpha);\n" +
    "}\n" +
    "",
  raw_f:
    "#version 100\n" +
    "precision mediump float;\n" +
    "varying vec4 vcolor;\n" +
    "void main() {\n" +
      "gl_FragColor=vcolor;\n" +
    "}\n" +
    "",
  decal_v:
    "#version 100\n" +
    "precision mediump float;\n" +
    "uniform vec2 screensize;\n" +
    "attribute vec2 apos;\n" +
    "attribute vec2 atexcoord;\n" +
    "varying vec2 vtexcoord;\n" +
    "void main() {\n" +
      "vec2 npos=(apos*2.0)/screensize-1.0;\n" +
      "gl_Position=vec4(npos,0.0,1.0);\n" +
      "vtexcoord=atexcoord;\n" +
    "}\n" +
    "",
  decal_f:
    "#version 100\n" +
    "precision mediump float;\n" +
    "uniform sampler2D sampler;\n" +
    "uniform float alpha;\n" +
    "uniform vec4 texlimit;\n" +
    "uniform vec4 tint;\n" +
    "varying vec2 vtexcoord;\n" +
    "void main() {\n" +
      "if (vtexcoord.x<texlimit.x) discard;\n" +
      "if (vtexcoord.y<texlimit.y) discard;\n" +
      "if (vtexcoord.x>=texlimit.z) discard;\n" +
      "if (vtexcoord.y>=texlimit.w) discard;\n" +
      "gl_FragColor=texture2D(sampler,vtexcoord);\n" +
      "gl_FragColor=vec4(mix(gl_FragColor.rgb,tint.rgb,tint.a),gl_FragColor.a*alpha);\n" +
    "}\n" +
    "",
  tile_v:
    "#version 100\n" +
    "precision mediump float;\n" +
    "uniform vec2 screensize;\n" +
    "uniform float pointsize;\n" +
    "attribute vec2 apos;\n" +
    "attribute float atileid;\n" +
    "attribute float axform;\n" +
    "varying vec2 vsrcp;\n" +
    "varying mat2 vmat;\n" +
    "void main() {\n" +
      "vec2 npos=(apos*2.0)/screensize-1.0;\n" +
      "gl_Position=vec4(npos,0.0,1.0);\n" +
      "vsrcp=vec2(\n" +
        "mod(atileid,16.0),\n" +
        "floor(atileid/16.0)\n" +
      ")/16.0;\n" +
           "if (axform<0.5) vmat=mat2( 1.0, 0.0, 0.0, 1.0);\n" +
      "else if (axform<1.5) vmat=mat2(-1.0, 0.0, 0.0, 1.0);\n" +
      "else if (axform<2.5) vmat=mat2( 1.0, 0.0, 0.0,-1.0);\n" +
      "else if (axform<3.5) vmat=mat2(-1.0, 0.0, 0.0,-1.0);\n" +
      "else if (axform<4.5) vmat=mat2( 0.0, 1.0, 1.0, 0.0);\n" +
      "else if (axform<5.5) vmat=mat2( 0.0, 1.0,-1.0, 0.0);\n" +
      "else if (axform<6.5) vmat=mat2( 0.0,-1.0, 1.0, 0.0);\n" +
      "else if (axform<7.5) vmat=mat2( 0.0,-1.0,-1.0, 0.0);\n" +
                      "else vmat=mat2( 1.0, 0.0, 0.0, 1.0);\n" +
      "gl_PointSize=pointsize;\n" +
    "}\n" +
    "",
  tile_f:
    "#version 100\n" +
    "precision mediump float;\n" +
    "uniform sampler2D sampler;\n" +
    "uniform float alpha;\n" +
    "uniform vec4 tint;\n" +
    "varying vec2 vsrcp;\n" +
    "varying mat2 vmat;\n" +
    "void main() {\n" +
      "vec2 texcoord=gl_PointCoord;\n" +
      "texcoord.y=1.0-texcoord.y;\n" +
      "texcoord=vmat*(texcoord-0.5)+0.5;\n" +
      "texcoord=vsrcp+texcoord/16.0;\n" +
      "gl_FragColor=texture2D(sampler,texcoord);\n" +
      "gl_FragColor=vec4(mix(gl_FragColor.rgb,tint.rgb,tint.a),gl_FragColor.a*alpha);\n" +
    "}\n" +
    "",
};

Video.TEX_FMT_RGBA = 1;
Video.TEX_FMT_A8 = 2;
Video.TEX_FMT_A1 = 3;

Video.TEXTURE_SIZE_LIMIT = 4096;
