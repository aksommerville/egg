/* ImageDecoder.js
 * Would be awesome if we could use browser facilities for this but alas
 * that is not possible because we have to be synchronous.
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
    if (this.isQoi(src)) return this.decodeHeaderQoi(src);
    if (this.isRlead(src)) return this.decodeHeaderRlead(src);
    if (this.isRawimg(src)) return this.decodeHeaderRawimg(src);
    throw new Error(`Image format unknown`);
  }
  
  /* Returns {w,h,stride,fmt,v:Uint8Array} or throws.
   * We reject images larger than 32767 on either axis, even if otherwise valid.
   */
  decode(src) {
    if (this.isPng(src)) return this.decodePng(src);
    if (this.isQoi(src)) return this.decodeQoi(src);
    if (this.isRlead(src)) return this.decodeRlead(src);
    if (this.isRawimg(src)) return this.decodeRawimg(src);
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
  
  /* QOI.
   *******************************************************************/
   
  isQoi(src) {
    if (src.length < 14) return 0;
    if (src[0] !== 0x71) return 0;
    if (src[1] !== 0x6f) return 0;
    if (src[2] !== 0x69) return 0;
    if (src[3] !== 0x66) return 0;
    return 1;
  }
  
  decodeHeaderQoi(src) {
    if (src.length < 14) throw new Error("Invalid QOI");
    const w = (src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7];
    const h = (src[8] << 24) | (src[9] << 16) | (src[10] << 8) | src[11];
    if ((w < 1) || (w > 0x7fff) || (h < 1) || (h > 0x7fff)) throw new Error("Invalid QOI");
    return { w, h, stride:w<<2, fmt:1 };
  }
  
  decodeQoi(src) {
    if (src.length < 14) throw new Error("Invalid QOI");
    const w = (src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7];
    const h = (src[8] << 24) | (src[9] << 16) | (src[10] << 8) | src[11];
    if ((w < 1) || (w > 0x7fff) || (h < 1) || (h > 0x7fff)) throw new Error("Invalid QOI");
    
    const dstc = w * h * 4;
    const dst = new Uint8Array(dstc);
    const cache = new Uint8Array(64 * 4);
    const prev = [0, 0, 0, 0xff];
    let dstp=0, srcp=14;
    const prevToCache = () => {
      let p = (prev[0] * 3 + prev[1] * 5 + prev[2] * 7 + prev[3] * 11) & 0x3f;
      p <<= 2;
      cache[p++] = prev[0];
      cache[p++] = prev[1];
      cache[p++] = prev[2];
      cache[p] = prev[3];
    };
    
    while ((dstp < dstc) && (srcp < src.length)) {
      const lead = src[srcp++];
      
      if (lead === 0xfe) { // QOI_OP_RGB
        const r = src[srcp++];
        const g = src[srcp++];
        const b = src[srcp++];
        dst[dstp++] = prev[0] = r;
        dst[dstp++] = prev[1] = g;
        dst[dstp++] = prev[2] = b;
        dst[dstp++] = prev[3];
        prevToCache();
        continue;
      }
      
      if (lead === 0xff) { // QOI_OP_RGBA
        const r = src[srcp++];
        const g = src[srcp++];
        const b = src[srcp++];
        const a = src[srcp++];
        dst[dstp++] = prev[0] = r;
        dst[dstp++] = prev[1] = g;
        dst[dstp++] = prev[2] = b;
        dst[dstp++] = prev[3] = a;
        prevToCache();
        continue;
      }
      
      switch (lead & 0xc0) {
        case 0x00: { // QOI_OP_INDEX
            let p = lead << 2;
            dst[dstp++] = prev[0] = cache[p++];
            dst[dstp++] = prev[1] = cache[p++];
            dst[dstp++] = prev[2] = cache[p++];
            dst[dstp++] = prev[3] = cache[p];
          } continue;
          
        case 0x40: { // QOI_OP_DIFF
            const dr = ((lead >> 4) & 3) - 2;
            const dg = ((lead >> 2) & 3) - 2;
            const db = (lead & 3) - 2;
            prev[0] += dr;
            prev[1] += dg;
            prev[2] += db;
            dst[dstp++] = prev[0];
            dst[dstp++] = prev[1];
            dst[dstp++] = prev[2];
            dst[dstp] = prev[3];
            prevToCache();
          } continue;
         
        case 0x80: { // QOI_OP_LUMA
            const dg = (lead & 0x3f) - 32;
            const dr = (src[srcp] >> 4) - 8 + dg;
            const db = (src[srcp] & 15) - 8 + dg;
            srcp++;
            prev[0] += dr;
            prev[1] += dg;
            prev[2] += db;
            dst[dstp++] = prev[0];
            dst[dstp++] = prev[1];
            dst[dstp++] = prev[2];
            dst[dstp] = prev[3];
            prevToCache();
          } continue;
          
        case 0xc0: { // QOI_OP_RUN
            let c = (lead & 0x3f) + 1;
            while (c-- > 0) {
              dst[dstp++] = prev[0];
              dst[dstp++] = prev[1];
              dst[dstp++] = prev[2];
              dst[dstp++] = prev[3];
            }
          } continue;
      }
    }
    return { v:dst, w, h, stride:w<<2, fmt:1 };
  }
  
  /* RLEAD.
   ***********************************************************************/
   
  isRlead(src) {
    if (src.length < 9) return 0;
    if ((src[0] !== 0x00) || (src[1] !== 0x72) || (src[2] !== 0x6c) || (src[3] !== 0x64)) return 0;
    return 1;
  }
  
  decodeHeaderRlead(src) {
    if ((src.length < 9) || (src[0] !== 0x00) || (src[1] !== 0x72) || (src[2] !== 0x6c) || (src[3] !== 0x64)) throw new Error("Invalid RLEAD");
    const w = (src[4] << 8) | src[5];
    const h = (src[6] << 8) | src[7];
    if ((w < 1) || (w > 0x7fff) || (h < 1) || (h > 0x7fff)) throw new Error("Invalid RLEAD");
    const stride = (w + 7) >> 3;
    return { w, h, stride, fmt:3 };
  }
  
  rleadUnfilter(v, stride, h) {
    let rp=0, wp=stride;
    for (; wp<v.length; rp++, wp++) {
      v[wp] ^= v[rp];
    }
  }
  
  decodeRlead(src) {
    if ((src.length < 9) || (src[0] !== 0x00) || (src[1] !== 0x72) || (src[2] !== 0x6c) || (src[3] !== 0x64)) throw new Error("Invalid RLEAD");
    const w = (src[4] << 8) | src[5];
    const h = (src[6] << 8) | src[7];
    if ((w < 1) || (w > 0x7fff) || (h < 1) || (h > 0x7fff)) throw new Error("Invalid RLEAD");
    const stride = (w + 7) >> 3;
    const flags = src[8];
    const dst = new Uint8Array(stride * h);
    let color = (flags & 2) ? 1 : 0;
    let dstx=0, dstp=0, srcp=9, dstmask=0x80, srcmask=0x80;
    
    /* Reading and writing are done one bit at a time.
     * This could be done more efficiently, but I'd probably mess it up.
     */
    const readBits = (len) => {
      let word = 0;
      while (len-- > 0) {
        word <<= 1;
        if (src[srcp] & srcmask) word |= 1;
        if (srcmask === 1) { srcmask = 0x80; srcp++; }
        else srcmask >>= 1;
      }
      return word;
    };
    const writeOnes = (len) => {
      while (len-- > 0) {
        dst[dstp] |= dstmask;
        if (++dstx >= w) {
          dstx = 0;
          dstmask = 0x80;
          dstp++;
        } else if (dstmask === 1) {
          dstmask = 0x80;
          dstp++;
        } else {
          dstmask >>= 1;
        }
      }
    };
    const writeZeroes = (len) => {
      while (len-- > 0) {
        if (++dstx >= w) {
          dstx = 0;
          dstmask = 0x80;
          dstp++;
        } else if (dstmask === 1) {
          dstmask = 0x80;
          dstp++;
        } else {
          dstmask >>= 1;
        }
      }
    };
    
    while ((dstp < dst.length) && (srcp < src.length)) {
      
      /* Acquire the next run length.
       */
      let runlen = 1;
      let wordlen = 3;
      for (;;) {
        const word = readBits(wordlen);
        runlen += word;
        if (word !== (1 << wordlen) - 1) break;
        wordlen++;
        if (wordlen > 30) throw new Error("Invalid RLEAD");
      }
      
      /* Emit the run.
       */
      if (color) {
        writeOnes(runlen);
        color = 0;
      } else {
        writeZeroes(runlen);
        color = 1;
      }
    }
    if (flags & 1) this.rleadUnfilter(dst, stride, h);
    return { v:dst, w, h, stride, fmt:3 };
  }
  
  /* Rawimg.
   *******************************************************************/
   
  isRawimg(src) {
    if (src.length < 9) return 0;
    if ((src[0] !== 0x00) || (src[1] !== 0x72) || (src[2] !== 0x49) || (src[3] !== 0x6d)) return 0;
    return 1;
  }
  
  decodeHeaderRawimg(src) {
    if ((src.length < 9) || (src[0] !== 0x00) || (src[1] !== 0x72) || (src[2] !== 0x49) || (src[3] !== 0x6d)) throw new Error("Invalid rawimg");
    const w = (src[4] << 8) | src[5];
    const h = (src[6] << 8) | src[7];
    const pixelsize = src[8];
    if ((w < 1) || (w > 0x7fff) || (h < 1) || (h > 0x7fff)) throw new Error("Invalid rawimg");
    let fmt;
    switch (pixelsize) {
      case 1: fmt = 3; break;
      case 2: fmt = 0x102; break;
      case 4: fmt = 0x104; break;
      case 8: fmt = 2; break;
      case 16: fmt = 0x110; break;
      case 24: fmt = 0x118; break;
      case 32: fmt = 1; break;
      default: throw new Error("Invalid rawimg");
    }
    const stride = (w * pixelsize + 7) >> 3;
    return { w, h, stride, fmt };
  }
  
  decodeRawimg(src) {
    if ((src.length < 9) || (src[0] !== 0x00) || (src[1] !==0x72) || (src[2] !==0x49) || (src[3] !== 0x6d)) throw new Error("Invalid rawimg");
    const w = (src[4] << 8) | src[5];
    const h = (src[6] << 8) | src[7];
    const pixelsize = src[8];
    if ((w < 1) || (w > 0x7fff) || (h < 1) || (h > 0x7fff)) throw new Error("Invalid rawimg");
    let fmt;
    switch (pixelsize) {
      case 1: fmt = 3; break;
      case 2: fmt = 0x102; break;
      case 4: fmt = 0x104; break;
      case 8: fmt = 2; break;
      case 16: fmt = 0x110; break;
      case 24: fmt = 0x118; break;
      case 32: fmt = 1; break;
      default: throw new Error("Invalid rawimg");
    }
    const stride = (w * pixelsize + 7) >> 3;
    const v = new Uint8Array(stride * h);
    const pixels = new Uint8Array(src.buffer, src.byteOffset + 9, stride * h);
    v.set(pixels);
    return { v, w, h, stride, fmt };
  }
}
