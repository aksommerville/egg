/* Encoder.js
 * Generic binary assembly.
 */
 
export class Encoder {
  constructor() {
    this.v = new Uint8Array(1024);
    this.c = 0;
  }
  
  /* Finishing trims any preallocation excess and returns
   * a copy as Uint8Array.
   */
  finish() {
    return this.v.slice(0, this.c);
  }
  
  require(addc) {
    if (addc < 1) return;
    if (this.c + addc <= this.v.length) return;
    const nc = (this.c + addc + 1024) & ~1023;
    const nv = new Uint8Array(nc);
    nv.set(this.v);
    this.v = nv;
  }
  
  /* String, Uint8Array, or ArrayBuffer.
   * We tolerate null and any other false.
   */
  insert(p, rmc, src) {
    if ((p < 0) || (p > this.c)) throw new Error(`Invalid insertion point ${p} in ${this.c}-byte Encoder.`);
    if (rmc < 0) throw new Error(`Invalid removal count ${rmc}`);
    if (!src) src = new Uint8Array(0);
    if (typeof(src) === "string") src = new TextEncoder("utf8").encode(src);
    if (src instanceof ArrayBuffer) src = new Uint8Array(src);
    if (!(src instanceof Uint8Array)) throw new Error(`Inappropriate input to Encoder.insert`);
    const srcc = src?.length || 0;
    if (!rmc && !srcc) return;
    if (srcc !== rmc) {
      this.require(srcc - rmc);
      const fromp = p + rmc;
      const top = p + srcc;
      const cpc = this.c - fromp;
      // Per MDN, it's OK for TypedArray.set() src and dst to overlap:
      const srcview = new Uint8Array(this.v.buffer, this.v.byteOffset + fromp, cpc);
      const dstview = new Uint8Array(this.v.buffer, this.v.byteOffset + top, cpc);
      dstview.set(srcview);
      this.c += srcc - rmc;
    }
    const cpview = new Uint8Array(this.v.buffer, this.v.byteOffset + p, srcc);
    cpview.set(src);
  }
  
  /* Plain integers.
   * They say 'u', but negatives are fine too; they'll be twos-complement.
   ***************************************************************************/
   
  u8(src) {
    this.require(1);
    this.v[this.c++] = src;
  }
  
  u16be(src) {
    this.require(2);
    this.v[this.c++] = src >> 8;
    this.v[this.c++] = src;
  }
  
  u24be(src) {
    this.require(3);
    this.v[this.c++] = src >> 16;
    this.v[this.c++] = src >> 8;
    this.v[this.c++] = src;
  }
  
  u32be(src) {
    this.require(4);
    this.v[this.c++] = src >> 24;
    this.v[this.c++] = src >> 16;
    this.v[this.c++] = src >> 8;
    this.v[this.c++] = src;
  }
  
  u16le(src) {
    this.require(2);
    this.v[this.c++] = src;
    this.v[this.c++] = src >> 8;
  }
  
  u24le(src) {
    this.require(3);
    this.v[this.c++] = src;
    this.v[this.c++] = src >> 8;
    this.v[this.c++] = src >> 16;
  }
  
  u32le(src) {
    this.require(4);
    this.v[this.c++] = src;
    this.v[this.c++] = src >> 8;
    this.v[this.c++] = src >> 16;
    this.v[this.c++] = src >> 24;
  }
  
  /* Variable-length integers.
   *******************************************************************/
   
  vlq(src) {
    this.require(4);
    if (src < 0) {
      throw new Error(`Inappropriate value for VLQ: ${src}`);
    } else if (src < 0x00000080) {
      this.v[this.c++] = src;
    } else if (src < 0x00004000) {
      this.v[this.c++] = 0x80 | (src >> 7);
      this.v[this.c++] = src & 0x7f;
    } else if (src < 0x00200000) {
      this.v[this.c++] = 0x80 | (src >> 14);
      this.v[this.c++] = 0x80 | ((src >> 7) & 0x7f);
      this.v[this.c++] = src & 0x7f;
    } else if (src < 0x10000000) {
      this.v[this.c++] = 0x80 | (src >> 21);
      this.v[this.c++] = 0x80 | ((src >> 14) & 0x7f);
      this.v[this.c++] = 0x80 | ((src >> 7) & 0x7f);
      this.v[this.c++] = src & 0x7f;
    } else {
      throw new Error(`Inappropriate value for VLQ: ${src}`);
    }
  }
  
  utf8(src) {
    this.require(4);
    if (src < 0) {
      throw new Error(`Inappropriate value for UTF-8: ${src}`);
    } else if (src < 0x000080) {
      this.v[this.c++] = src;
    } else if (src < 0x000800) {
      this.v[this.c++] = 0xc0 | (src >> 6);
      this.v[this.c++] = 0x80 | (src & 0x3f);
    } else if (src < 0x010000) {
      this.v[this.c++] = 0xe0 | (src >> 12);
      this.v[this.c++] = 0x80 | ((src >> 6) & 0x3f);
      this.v[this.c++] = 0x80 | (src & 0x3f);
    } else if (src < 0x110000) {
      this.v[this.c++] = 0xf0 | (src >> 18);
      this.v[this.c++] = 0x80 | ((src >> 12) & 0x3f);
      this.v[this.c++] = 0x80 | ((src >> 6) & 0x3f);
      this.v[this.c++] = 0x80 | (src & 0x3f);
    } else {
      throw new Error(`Inappropriate value for UTF-8: ${src}`);
    }
  }
  
  /* Serial chunks.
   ***************************************************************/
   
  raw(src) {
    this.insert(this.c, 0, src);
  }
  
  zero(c) {
    this.require(c);
    for (; c-->0; this.c++) this.v[this.c] = 0;
  }
  
  intbelen(lenlen, src) {
    let lenp = this.c;
    this.zero(lenlen);
    const bodyp = this.c;
    if (typeof(src) === "function") {
      src();
    } else {
      this.raw(src);
    }
    const len = this.c - bodyp;
    switch (lenlen) {
      case 1: {
          if (len > 0xff) throw new Error(`Body length ${len} exceeds ${lenlen}-byte limit`);
          this.v[lenp] = len;
        } break;
      case 2: {
          if (len > 0xffff) throw new Error(`Body length ${len} exceeds ${lenlen}-byte limit`);
          this.v[lenp++] = len >> 8;
          this.v[lenp] = len;
        } break;
      case 3: {
          if (len > 0xffffff) throw new Error(`Body length ${len} exceeds ${lenlen}-byte limit`);
          this.v[lenp++] = len >> 16;
          this.v[lenp++] = len >> 8;
          this.v[lenp] = len;
        } break;
      case 4: {
          if (len > 0xffffffff) throw new Error(`Body length ${len} exceeds ${lenlen}-byte limit`);
          this.v[lenp++] = len >> 24;
          this.v[lenp++] = len >> 16;
          this.v[lenp++] = len >> 8;
          this.v[lenp] = len;
        } break;
      default: throw new Error(`Illegal block length length ${lenlen}`);
    }
  }
  
  intlelen(lenlen, src) {
    let lenp = this.c;
    this.zero(lenlen);
    const bodyp = this.c;
    if (typeof(src) === "function") {
      src();
    } else {
      this.raw(src);
    }
    const len = this.c - bodyp;
    switch (lenlen) {
      case 1: {
          if (len > 0xff) throw new Error(`Body length ${len} exceeds ${lenlen}-byte limit`);
          this.v[lenp] = len;
        } break;
      case 2: {
          if (len > 0xffff) throw new Error(`Body length ${len} exceeds ${lenlen}-byte limit`);
          this.v[lenp++] = len;
          this.v[lenp] = len >> 8;
        } break;
      case 3: {
          if (len > 0xffffff) throw new Error(`Body length ${len} exceeds ${lenlen}-byte limit`);
          this.v[lenp++] = len;
          this.v[lenp++] = len >> 8;
          this.v[lenp] = len >> 16;
        } break;
      case 4: {
          if (len > 0xffffffff) throw new Error(`Body length ${len} exceeds ${lenlen}-byte limit`);
          this.v[lenp++] = len;
          this.v[lenp++] = len >> 8;
          this.v[lenp++] = len >> 16;
          this.v[lenp] = len >> 24;
        } break;
      default: throw new Error(`Illegal block length length ${lenlen}`);
    }
  }
  
  vlqlen(src) {
    const lenp = this.c;
    if (typeof(src) === "function") {
      src();
    } else {
      this.raw(src);
    }
    const len = this.c - lenp;
    if (len < 0x80) {
      this.insert(lenp, 0, new Uint8Array([
        len,
      ]));
    } else if (len < 0x4000) {
      this.insert(lenp, 0, new Uint8Array([
        0x80 | (len >> 7),
        len & 0x7f,
      ]));
    } else if (len < 0x200000) {
      this.insert(lenp, 0, new Uint8Array([
        0x80 | (len >> 14),
        0x80 | ((len >> 7) & 0x7f),
        len & 0x7f,
      ]));
    } else if (len < 0x10000000) {
      this.insert(lenp, 0, new Uint8Array([
        0x80 | (len >> 21),
        0x80 | ((len >> 14) & 0x7f),
        0x80 | ((len >> 7) & 0x7f),
        len & 0x7f,
      ]));
    } else {
      throw new Error(`Block too long for VLQ (${len})`);
    }
  }
    
}
