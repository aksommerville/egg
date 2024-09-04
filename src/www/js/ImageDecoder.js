/* ImageDecoder.js
 * Would be awesome if we could use browser facilities for this but alas
 * that is not possible because we have to be synchronous.
 * TODO Add qoi, rawimg, and rlead. Those are easy to do synchronously, and our tooling does produce them.
 */
 
import { imaya } from "./inflate.min.js";
 
export class ImageDecoder {
  constructor() {
  }
  
  /* Returns {w,h,stride,fmt} or throws.
   * We reject images larger than 32767 on either axis, even if otherwise valid.
   */
  decodeHeader(src) {
    if (this.isPng(src)) return this.decodeHeaderPng(src);
    throw new Error(`Image format unknown`);
  }
  
  /* Returns {w,h,stride,fmt,v:Uint8Array} or throws.
   * We reject images larger than 32767 on either axis, even if otherwise valid.
   */
  decode(src) {
    if (this.isPng(src)) return this.decodePng(src);
    throw new Error(`Image format unknown`);
  }
  
  /* PNG.
   ***************************************************************************/
   
  isPng(src) {
    return (
      (src.length >= 8) &&
      (src[0] === 0x89) &&
      (src[1] === 0x50) &&
      (src[2] === 0x4e) &&
      (src[3] === 0x47) &&
      (src[4] === 0x0d) &&
      (src[5] === 0x0a) &&
      (src[6] === 0x1a) &&
      (src[7] === 0x0a)
    );
  }
   
  decodeHeaderPng(src) {
    // We require IHDR to be the first chunk.
    // The spec does say that, but I've seen violations before.
    if (src.length < 26) throw new Error("Invalid PNG");
    const w = (src[16] << 24) | (src[17] << 16) | (src[18] << 8) | src[19];
    const h = (src[20] << 24) | (src[21] << 16) | (src[22] << 8) | src[23];
    if ((w < 1) || (h < 1) || (w > 0x7fff) || (h > 0x7fff)) throw new Error("Invalid PNG");
    let depth = src[24];
    const colortype = src[25];
    switch (colortype) {
      case 0: break;
      case 2: depth *= 3; break;
      case 3: break;
      case 4: depth *= 2; break;
      case 6: depth *= 4; break;
      default: throw new Error("Invalid PNG");
    }
    let fmt;
    switch (depth) {
      case 1: fmt = 3; break;
      case 8: fmt = 2; break;
      case 32: fmt = 1; break;
      // Anything that isn't 1, 8, or 32 bits/pixel, we coerce to 32 at decode.
      case 2: case 4: case 16: case 24: case 48: case 64: fmt = 1; depth = 32; break;
      default: throw new Error("Invalid PNG");
    }
    const stride = (w * depth + 7) >> 3;
    return {w, h, stride, fmt};
  }
  
  decodePng(src) {
    const chunks = this.dechunkPng(src);
    const ihdr = this.decodePngIhdr(chunks.ihdr);
    let fmt;
    switch (ihdr.pixelsize) {
      case 32: fmt = 1; break;
      case 8: fmt = 2; break;
      case 1: fmt = 3; break;
      default: fmt = 0; // force RGBA after preliminary decode
    }
    const filtered = new Zlib.Inflate(chunks.idat).decompress();
    let dst = new Uint8Array(ihdr.stride * ihdr.h);
    this.unfilterPng(dst, filtered, ihdr.stride, ihdr.xstride);
    if (!fmt) {
      const rgbastride = ihdr.w << 2;
      const rgba = new Uint8Array(rgbastride * ihdr.h);
      this.forceRgba(rgba, rgbastride, dst, ihdr, chunks.plte);
      dst = rgba;
      ihdr.stride = rgbastride;
      fmt = 1;
    }
    return {
      w: ihdr.w,
      h: ihdr.h,
      stride: ihdr.stride,
      fmt,
      v: dst,
    };
  }
  
  dechunkPng(src) {
    const chunks = {};
    for (let srcp=8; srcp<src.length; ) {
      const len = (src[srcp] << 24) | (src[srcp+1] << 16) | (src[srcp+2] << 8) | src[srcp+3];
      srcp += 4;
      const cid = (src[srcp] << 24) | (src[srcp+1] << 16) | (src[srcp+2] << 8) | src[srcp+3];
      srcp += 4;
      switch (cid) {
        case 0x49484452: chunks.ihdr = src.slice(srcp, srcp + len); break;
        case 0x504c5445: chunks.plte = src.slice(srcp, srcp + len); break;
        case 0x49444154: { // IDAT
            if (chunks.idat) {
              const nv = new Uint8Array(chunks.idat.length + len);
              nv.set(chunks.idat);
              const dstview = new Uint8Array(nv.buffer, chunks.idat.length, len);
              dstview.set(src.slice(srcp, srcp + len));
              chunks.idat = nv;
            } else {
              chunks.idat = src.slice(srcp, srcp + len);
            }
          } break;
      }
      srcp += len;
      srcp += 4;
    }
    return chunks;
  }
  
  decodePngIhdr(src) {
    if (!src || (src.length < 13)) throw new Error("Invalid PNG");
    const w = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
    const h = (src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7];
    if ((w < 1) || (h < 1) || (w > 0x7fff) || (h > 0x7fff)) throw new Error("Invalid PNG");
    const depth = src[8];
    const colortype = src[9];
    if (src[10] || src[11] || src[12]) {
      // We're not supporting interlaced PNG. Compression and filter, there's only one defined for each.
      throw new Error(`Unsupported PNG compression, filter, or interlace (${src[10]}, ${src[11]}, ${src[12]}`);
    }
    let pixelsize = depth;
    switch (colortype) {
      case 0: break;
      case 2: pixelsize *= 3; break;
      case 3: break;
      case 4: pixelsize *= 2; break;
      case 6: pixelsize *= 4; break;
      default: throw new Error("Invalid PNG");
    }
    const xstride = Math.max(1, pixelsize >> 3);
    const stride = (pixelsize * w + 7) >> 3;
    return { w, h, stride, depth, colortype, pixelsize, xstride };
  }
  
  unfilterPng(dst, src, dststride, xstride) {
    const srcstride = 1 + dststride;
    let dstp=0, srcp=0, dstppv=0;
    const paeth = (a, b, c) => {
      const p = a + b - c;
      const pa = Math.abs(p - a);
      const pb = Math.abs(p - b);
      const pc = Math.abs(p - c);
      if ((pa <= pb) && (pa <= pc)) return a;
      if (pb <= pc) return b;
      return c;
    };
    while (dstp < dst.length) {
      const filter = src[srcp++];
      if (dstp) dstppv = dstp - dststride;
      switch (filter) {
        case 0: {
            for (let i=dststride; i-->0; dstp++, srcp++) dst[dstp] = src[srcp];
          } break;
        case 1: {
            let i=0;
            for (; i<xstride; i++, dstp++, srcp++) dst[dstp] = src[srcp];
            for (; i<dststride; i++, dstp++, srcp++) dst[dstp] = src[srcp] + dst[dstp-xstride];
          } break;
        case 2: {
            if (dstp) {
              for (let i=dststride; i-->0; dstp++, dstppv++, srcp++) dst[dstp] = src[srcp] + dst[dstppv];
            } else {
              for (let i=dststride; i-->0; dstp++, srcp++) dst[dstp] = src[srcp];
            }
          } break;
        case 3: {
            let i=0;
            if (dstp) {
              for (; i<xstride; i++, dstp++, srcp++, dstppv++) dst[dstp] = src[srcp] + (dst[dstppv] >> 1);
              for (; i<dststride; i++, dstp++, srcp++, dstppv++) dst[dstp] = src[srcp] + ((dst[dstp-xstride] + dst[dstppv]) >> 1);
            } else {
              for (; i<xstride; i++, dstp++, srcp++) dst[dstp] = src[srcp];
              for (; i<dststride; i++, dstp++, srcp++) dst[dstp] = src[srcp] + (dst[dstp-xstride] >> 1);
            }
          } break;
        case 4: {
            let i=0;
            if (dstp) {
              for (; i<xstride; i++, dstp++, srcp++, dstppv++) dst[dstp] = src[srcp] + dst[dstppv];
              for (; i<dststride; i++, dstp++, srcp++, dstppv++) dst[dstp] = src[srcp] + paeth(dst[dstp-xstride], dst[dstppv], dst[dstppv-xstride]);
            } else { // PAETH is exactly SUB on the first row, but it is legal.
              for (; i<xstride; i++, dstp++, srcp++) dst[dstp] = src[srcp];
              for (; i<dststride; i++, dstp++, srcp++) dst[dstp] = src[srcp] + dst[dstp-xstride];
            }
          } break;
      }
    }
  }
  
  forceRgba(dst, dststride, src, ihdr, plte) {
    for (
      let dstrowp=0, yi=ihdr.h, srciter=this.iteratePng(src, ihdr, plte);
      yi-->0;
      dstrowp+=dststride
    ) {
      for (let dstp=dstrowp, xi=ihdr.w; xi-->0; ) {
        const rgba = srciter();
        dst[dstp++] = rgba >> 24;
        dst[dstp++] = rgba >> 16;
        dst[dstp++] = rgba >> 8;
        dst[dstp++] = rgba;
      }
    }
  }
  
  // Returns a function that returns every pixel LRTB as 32-bit big-endian RGBA.
  // TODO We're not accepting tRNS chunks. Should we?
  iteratePng(src, ihdr, plte) {
    let rowp=0, p=0, xi=ihdr.w, yi=ihdr.h, mask=0x80, shift;
    
    if (plte && (ihdr.colortype === 3)) { // INDEX
      switch (ihdr.depth) {
        case 1: return () => {
            if (xi <= 0) { if (yi <= 0) return 0; yi--; rowp += ihdr.stride; p = rowp; xi = ihdr.w; mask = 0x80; } xi--;
            let ix = (src[p] & mask) ? 1 : 0;
            ix *= 3;
            if (mask === 1) { mask = 0x80; p++; }
            else mask >>= 1;
            return (plte[ix] << 24) | (plte[ix+1] << 16) | (plte[ix+2] << 8) | 0xff;
          };
        case 2: shift = 6; return () => {
            if (xi <= 0) { if (yi <= 0) return 0; yi--; rowp += ihdr.stride; p = rowp; xi = ihdr.w; shift = 6; } xi--;
            let ix = (src[p] >> shift) & 3;
            ix *= 3;
            if (shift) shift -= 2;
            else { shift = 6; p++; }
            return (plte[ix] << 24) | (plte[ix+1] << 16) | (plte[ix+2] << 8) | 0xff;
          };
        case 4: shift = 4; return () => {
            if (xi <= 0) { if (yi <= 0) return 0; yi--; rowp += ihdr.stride; p = rowp; xi = ihdr.w; shift = 4; } xi--;
            let ix = (src[p] >> shift) & 15;
            ix *= 3;
            if (shift) shift = 0;
            else { shift = 6; p++; }
            return (plte[ix] << 24) | (plte[ix+1] << 16) | (plte[ix+2] << 8) | 0xff;
          };
        case 8: return () => {
            if (xi <= 0) { if (yi <= 0) return 0; yi--; rowp += ihdr.stride; p = rowp; xi = ihdr.w; } xi--;
            let ix = src[p++];
            ix *= 3;
            return (plte[ix] << 24) | (plte[ix+1] << 16) | (plte[ix+2] << 8) | 0xff;
          };
      }
      
    } else switch (ihdr.colortype) {
      case 0: case 3: switch (ihdr.depth) { // GRAY (or INDEX with missing PLTE)
          case 1: return () => {
              if (xi <= 0) { if (yi <= 0) return 0; yi--; rowp += ihdr.stride; p = rowp; xi = ihdr.w; mask = 0x80; } xi--;
              const luma = (src[p] & mask) ? 0xff : 0;
              if (mask === 1) { mask = 0x80; p++; }
              else mask >>= 1;
              return (luma << 24) | (luma << 16) | (luma << 8) | 0xff;
            };
          case 2: shift = 6; return () => {
              if (xi <= 0) { if (yi <= 0) return 0; yi--; rowp += ihdr.stride; p = rowp; xi = ihdr.w; shift = 6; } xi--;
              let luma = (src[p] >> shift) & 3;
              luma |= luma << 2;
              luma |= luma << 4;
              if (shift) shift -= 2;
              else { shift = 6; p++; }
              return (luma << 24) | (luma << 16) | (luma << 8) | 0xff;
            };
          case 4: shift = 4; return () => {
              if (xi <= 0) { if (yi <= 0) return 0; yi--; rowp += ihdr.stride; p = rowp; xi = ihdr.w; shift = 4; } xi--;
              let luma = (src[p] >> shift) & 15;
              luma |= luma << 4;
              if (shift) shift = 0;
              else { shift = 6; p++; }
              return (luma << 24) | (luma << 16) | (luma << 8) | 0xff;
            };
          case 8: return () => {
              if (xi <= 0) { if (yi <= 0) return 0; yi--; rowp += ihdr.stride; p = rowp; xi = ihdr.w; } xi--;
              const luma = src[p++];
              return (luma << 24) | (luma << 16) | (luma << 8) | 0xff;
            };
          case 16: return () => {
              if (xi <= 0) { if (yi <= 0) return 0; yi--; rowp += ihdr.stride; p = rowp; xi = ihdr.w; } xi--;
              const luma = src[p];
              p += 2;
              return (luma << 24) | (luma << 16) | (luma << 8) | 0xff;
            };
        } break;
      case 2: switch (ihdr.depth) { // RGB
          case 8: return () => {
              if (xi <= 0) { if (yi <= 0) return 0; yi--; rowp += ihdr.stride; p = rowp; xi = ihdr.w; } xi--;
              const r = src[p++];
              const g = src[p++];
              const b = src[p++];
              return (r << 24) | (g << 16) | (b << 8) | 0xff;
            };
          case 16: return () => {
              if (xi <= 0) { if (yi <= 0) return 0; yi--; rowp += ihdr.stride; p = rowp; xi = ihdr.w; } xi--;
              const r = src[p++]; p++;
              const g = src[p++]; p++;
              const b = src[p++]; p++;
              return (r << 24) | (g << 16) | (b << 8) | 0xff;
            };
        } break;
      case 4: switch (ihdr.depth) { // YA
          case 8: return () => {
              if (xi <= 0) { if (yi <= 0) return 0; yi--; rowp += ihdr.stride; p = rowp; xi = ihdr.w; } xi--;
              const luma = src[p++];
              const alpha = src[p++];
              return (luma << 24) | (luma << 16) | (luma << 8) | alpha;
            };
          case 16: return () => {
              if (xi <= 0) { if (yi <= 0) return 0; yi--; rowp += ihdr.stride; p = rowp; xi = ihdr.w; } xi--;
              const luma = src[p++]; p++;
              const alpha = src[p++]; p++;
              return (luma << 24) | (luma << 16) | (luma << 8) | alpha;
            };
        } break;
      case 6: switch (ihdr.depth) { // RGBA
          case 8: return () => {
              if (xi <= 0) { if (yi <= 0) return 0; yi--; rowp += ihdr.stride; p = rowp; xi = ihdr.w; } xi--;
              const r = src[p++];
              const g = src[p++];
              const b = src[p++];
              const a = src[p++];
              return (r << 24) | (g << 16) | (b << 8) | a;
            };
          case 16: return () => {
              if (xi <= 0) { if (yi <= 0) return 0; yi--; rowp += ihdr.stride; p = rowp; xi = ihdr.w; } xi--;
              const r = src[p++]; p++;
              const g = src[p++]; p++;
              const b = src[p++]; p++;
              const a = src[p++]; p++;
              return (r << 24) | (g << 16) | (b << 8) | a;
            };
        } break;
    }
    return () => 0;
  }
}
