/* Rom.js
 */
 
export class Rom {
  constructor(serial) {
    // Allow shallow copies as a convenience, so we can pass Rom instances around where serial is expected.
    if (serial instanceof Rom) {
      this.resv = serial.resv;
      this.serial = serial.serial;
      this.textDecoder = serial.textDecoder;
      return;
    }
    if (typeof(serial) === "string") serial = this.decodeBase64(serial);
    if (serial instanceof ArrayBuffer) serial = new Uint8Array(serial);
    if (!(serial instanceof Uint8Array)) throw new Error(`Rom input must be string, ArrayBuffer, or Uint8Array`);
    this.serial = serial;
    this.textDecoder = new TextDecoder("utf8");
    this.decode();
  }
  
  /* Decodes all image resources and adds an Image object to (resv[]).
   * Returns a Promise.
   */
  preload() {
    const promises = [];
    for (const res of this.resv) {
      switch (res.tid) {
        case Rom.TID_image: {
            promises.push(this.preloadImage(res.serial).then(img => {
              res.image = img;
            }));
          } break;
      }
    }
    return Promise.all(promises);
  }
  
  preloadImage(serial) {
    const blob = new Blob([serial]);
    const url = URL.createObjectURL(blob);
    const image = new Image();
    //const image = document.createElement("IMG");
    return new Promise((resolve, reject) => {
      image.onload = () => {
        URL.revokeObjectURL(url);
        resolve(image);
      };
      image.onerror = e => {
        URL.revokeObjectURL(url);
        reject(e);
      };
      image.src = url;
    });
  }
  
  getResource(tid, rid) {
    return this.resv.find(r => ((r.tid === tid) && (r.rid === rid)))?.serial || [];
  }
  
  getResourceEntry(tid, rid) {
    return this.resv.find(r => ((r.tid === tid) && (r.rid === rid)));
  }
  
  /* (lang) nonzero to resolve 'k$', if present.
   * (lang) zero, we only return exactly what's in metadata:1.
   */
  getMetadata(k, lang) {
    const src = this.getResource(Rom.TID_metadata, 1);
    if ((src[0] !== 0x00) || (src[1] !== 0x45) || (src[2] !== 0x4d) || (src[3] !== 0xff)) return "";
    const strk = lang ? (k + "$") : "";
    let srcp=4, natural="";
    while (srcp < src.length) {
      const kc = src[srcp++];
      if (!kc) break;
      const vc = src[srcp++] || 0;
      if (srcp > src.length - vc - kc) break;
      const qk = this.textDecoder.decode(src.slice(srcp, srcp + kc));
      srcp += kc;
      if (qk === k) {
        natural = this.textDecoder.decode(src.slice(srcp, srcp + vc));
        if (!lang) return natural;
      } else if (qk === strk) {
        const index = +this.textDecoder.decode(src.slice(srcp, srcp + vc));
        if (!isNaN(index)) {
          const strv = this.getString(lang, 1, index);
          if (strv) return strv;
          else if (natural) return natural;
        }
      }
      srcp += vc;
    }
    return natural;
  }
  
  /* (lang) is only here to adjust (rid). It can be zero, if you want to use absolute (rid).
   */
  getString(lang, rid, index) {
    if (index < 0) return "";
    rid |= lang << 6;
    const src = this.getResource(Rom.TID_strings, rid);
    if ((src[0] !== 0x00) || (src[1] !== 0x45) || (src[2] !== 0x53) || (src[3] !== 0xff)) return "";
    let srcp = 4;
    while (srcp < src.length) {
      let len = src[srcp++];
      if (len & 0x80) {
        len = (len & 0x7f) << 8;
        len |= src[srcp++] || 0;
      }
      if (srcp > src.length - len) break;
      if (!index--) return this.textDecoder.decode(src.slice(srcp, srcp + len));
      srcp += len;
    }
    return "";
  }
  
  // string => Uint8Array
  decodeBase64(src) {
    const dsta = Math.ceil(src.length / 4) * 3;
    const dst = new Uint8Array(dsta);
    let dstc=0, tmpc=0;
    const tmp = [0, 0, 0, 0];
    for (let srcp=0; srcp<src.length; srcp++) {
      let ch = src.charCodeAt(srcp);
      if (ch <= 0x20) continue;
           if ((ch >= 0x41) && (ch <= 0x5a)) ch = ch - 0x41;
      else if ((ch >= 0x61) && (ch <= 0x7a)) ch = ch - 0x61 + 26;
      else if ((ch >= 0x30) && (ch <= 0x39)) ch = ch - 0x30 + 52;
      else if (ch === 0x2b) ch = 62;
      else if (ch === 0x2f) ch = 63;
      else if (ch === 0x3d) continue;
      else throw new Error(`Unexpected byte ${ch} in base64-encoded ROM.`);
      tmp[tmpc++] = ch;
      if (tmpc >= 4) {
        dst[dstc++] = (tmp[0] << 2) | (tmp[1] >> 4);
        dst[dstc++] = (tmp[1] << 4) | (tmp[2] >> 2);
        dst[dstc++] = (tmp[2] << 6) | tmp[3];
        tmpc = 0;
        tmp[0] = tmp[1] = tmp[2] = tmp[3] = 0;
      }
    }
    if (tmpc) {
      dst[dstc++] = (tmp[0] << 2) | (tmp[1] >> 4);
      dst[dstc++] = (tmp[1] << 4) | (tmp[2] >> 2);
      dst[dstc++] = (tmp[2] << 6) | tmp[3];
    }
    if (dstc > dsta) throw new Error(`Base64 overflow ${dstc} > ${dsta} for input ${src.length}`);
    // We ought to allocate a new buffer of exactly (dstc) and copy to it, but there's actually no need.
    // Zero bytes are EOF, it's fine to append zeroes to a ROM.
    return dst;
  }
  
  /* Uint8Array => string.
   * Rom doesn't actually need this. Implementing here just to keep it adjacent to Rom.decodeBase64.
   * Runtime uses it to deliver the favicon.
   */
  encodeBase64(src) {
    let dst = "";
    const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const fullc = Math.floor(src.length / 3);
    let srcp=0;
    for (let i=fullc; i-->0; ) {
      const s0 = src[srcp++];
      const s1 = src[srcp++];
      const s2 = src[srcp++];
      const d0 = s0 >> 2;
      const d1 = ((s0 << 4) | (s1 >> 4)) & 0x3f;
      const d2 = ((s1 << 2) | (s2 >> 6)) & 0x3f;
      const d3 = s2 & 0x3f;
      dst += alphabet[d0];
      dst += alphabet[d1];
      dst += alphabet[d2];
      dst += alphabet[d3];
    }
    switch (src.length - srcp) {
      case 1: {
          const s0 = src[srcp];
          const d0 = s0 >> 2;
          const d1 = (s0 << 4) & 0x3f;
          dst += alphabet[d0];
          dst += alphabet[d1];
          dst += "==";
        } break;
      case 2: {
          const s0 = src[srcp++];
          const s1 = src[srcp++];
          const d0 = s0 >> 2;
          const d1 = ((s0 << 4) | (s1 >> 4)) & 0x3f;
          const d2 = (s1 << 2) & 0x3f;
          dst += alphabet[d0];
          dst += alphabet[d1];
          dst += alphabet[d2];
          dst += "=";
        } break;
    }
    return dst;
  }
  
  decode() {
    this.resv = [];
    if (
      (this.serial[0] !== 0x00) ||
      (this.serial[1] !== 0x45) ||
      (this.serial[2] !== 0x47) ||
      (this.serial[3] !== 0x47)
    ) throw new Error(`Invalid ROM, signature mismatch`);
    for (let srcp=4, tid=1, rid=1; srcp<this.serial.length; ) {
      const lead = this.serial[srcp++];
      if (!lead) break;
      switch (lead & 0xc0) {
        case 0x00: {
            tid += lead;
            rid = 1;
            if (tid > 0xff) throw new Error("Invalid ROM");
          } break;
        case 0x40: {
            let d = (lead & 0x3f) << 8;
            d |= this.serial[srcp++];
            if (!d) throw new Error("Invalid ROM");
            rid += d;
            if (rid > 0xffff) throw new Error("Invalid ROM");
          } break;
        case 0x80: {
            let l = (lead & 0x3f) << 8;
            l |= this.serial[srcp++] || 0;
            l += 1;
            if (srcp > this.serial.length - l) throw new Error("Invalid ROM");
            if (rid > 0xffff) throw new Error("Invalid ROM");
            this.resv.push({ tid, rid, serial: this.serial.slice(srcp, srcp + l) });
            srcp += l;
            rid++;
          } break;
        case 0xc0: {
            let l = (lead & 0x3f) << 16;
            l |= (this.serial[srcp++] || 0) << 8;
            l |= this.serial[srcp++] || 0;
            l += 16385;
            if (srcp > this.serial.length - l) throw new Error("Invalid ROM");
            if (rid > 0xffff) throw new Error("Invalid ROM");
            this.resv.push({ tid, rid, serial: this.serial.slice(srcp, srcp + l) });
            srcp += l;
            rid++;
          } break;
      }
    }
  }
}

Rom.TID_metadata = 1;
Rom.TID_code = 2;
Rom.TID_strings = 3;
Rom.TID_image = 4;
Rom.TID_sounds = 5;
Rom.TID_song = 6;
