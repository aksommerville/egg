/* SynthFormats.js
 * Static helpers for our audio formats.
 */
 
export class SynthFormats {

  /* Check signatures.
   * Input must be Uint8Array.
   * Returns one of: "", "egs", "msf", "wav"
   */
  static detectFormat(s) {
    const ck4 = (p, a, b, c, d) => ((s[p] === a) && (s[p+1] === b) && (s[p+2] === c) && (s[p+3] === d));
    if (s.length >= 4) {
      if (ck4(0, 0x00, 0x45, 0x47, 0x53)) return "egs";
      if (ck4(0, 0x00, 0x4d, 0x53, 0x46)) return "msf";
      if ((s.length >= 12) && ck4(0, 0x52, 0x49, 0x46, 0x46) && ck4(8, 0x57, 0x41, 0x56, 0x45)) return "wav";
    }
    return "";
  }
  
  /* Call (cb) with (id, serial) for each member of this MSF archive,
   * or call it once with all of (serial) and ID 0.
   * (cb) may return anything true to stop iteration, and we'll return the same.
   */
  static forEach(s, cb) {
    if ((s?.length >= 4) && (s[0] === 0x00) && (s[1] === 0x4d) && (s[2] === 0x53) && (s[3] === 0x46)) {
      let sp = 4;
      let index = 0;
      while (sp < s.length) {
        const d = s[sp++];
        index += d + 1;
        const len = (s[sp] << 16) | (s[sp+1] << 8) | s[sp+2];
        sp += 3;
        if (sp > s.length - len) break;
        const err = cb(index, s.slice(sp, sp + len));
        if (err) return err;
        sp += len;
      }
    } else {
      // Not MSF. Report it as a single member with ID zero.
      return cb(0, serial);
    }
  }
  
  /* Split the parts of an EGS resource.
   * Returns null or { channels: Uint8Array[], events: Uint8Array }.
   * Leading noop events are trimmed from Channel Headers.
   */
  static splitEgs(s) {
    if (!s || (s[0] !== 0x00) || (s[1] !== 0x45) || (s[2] !== 0x47) || (s[3] !== 0x53)) return null;
    let sp = 4;
    const egs = {
      channels: [],
    };
    while (sp < s.length) {
      let chhlen = (s[sp] << 8) | s[sp+1];
      sp += 2;
      if (!chhlen) break;
      if (sp > s.length - chhlen) return null;
      // Channel Headers are not allowed to be empty.
      // If one is unused, encoders should add a "noop" 0x00 field.
      // Trim any noops from the start of the Channel Header.
      while ((chhlen >= 2) && (s[sp] === 0x00)) {
        chhlen -= 2 + s[sp + 1];
        sp += 2 + s[sp + 1];
      }
      egs.channels.push(s.slice(sp, sp + chhlen));
      sp += chhlen;
    }
    egs.events = s.slice(sp);
    return egs;
  }
  
  /* Call (cb(opcode,payload)) for each field in this Channel Header.
   * (s) should come from splitEgs().channels.
   */
  static forEachChannelHeaderField(s, cb) {
    for (let sp=0; sp<s.length; ) {
      const opcode = s[sp++];
      const len = s[sp++] || 0;
      if (sp > s.length - len) break;
      const err = cb(opcode, s.slice(sp, sp + len));
      if (err) return err;
      sp += len;
    }
  }
  
  /* Create an iterator to read events off an EGS event dump (splitEgs().events).
   * eg:
   *   for (let iter=SynthFormats.iterateEgsEvents(egs.events), event; event=iter.next(); ) {
   *     switch (event.type) {
   *       case "delay": // event.delay s
   *       case "wheel": // event.chid 0..15, event.wheel -1..1f
   *       case "note": // event.chid 0..15, event.noteid 0..127, event.velocity 0..1f, event.dur s
   *     }
   *   }
   * Adjacent delays are combined.
   */
  static iterateEgsEvents(evv) {
    const iter = {
      evv,
      evp: 0,
    };
    iter.reset = () => {
      iter.evp = 0;
    };
    iter.next = () => {
      if (iter.evp >= iter.evv.length) return null;
      const lead = iter.evv[iter.evp++];
      
      // Natural zero: Explicit EOS.
      if (!lead) {
        iter.evp = iter.evv.length;
        return null;
      }
      
      // High bit unset: Delay.
      if (!(lead & 0x80)) {
        let ms = lead & 0x3f;
        if (lead & 0x40) ms = (ms + 1) << 6;
        while ((iter.evp < iter.evv.length) && !(iter.evv[iter.evp] & 0x80)) {
          if (!iter.evv[iter.evp]) break;
          let sub = iter.evv[iter.evp] & 0x3f;
          if (iter.evv[iter.evp] & 0x40) sub = (sub + 1) << 6;
          ms += sub;
          iter.evp++;
        }
        return { type: "delay", delay: ms / 1000 };
      }
      
      // Other events distinguished by top four bits, and not exhaustive.
      switch (lead & 0xf0) {
      
        case 0x80: // Fire and forget (7-bit velocity and two bits unused).
        case 0x90: // Short
        case 0xa0: // Long
          {
            const chid = lead & 0x0f;
            const a = iter.evv[iter.evp++] || 0;
            const b = iter.evv[iter.evp++] || 0;
            const noteid = a >> 1;
            let velocity = ((a << 6) & 0x40) | (b >> 2);
            let dur = 0;
            if (lead >= 0x90) { // Short or Long, reduce velocity to 4 bits and read duration.
              velocity &= 0x78;
              velocity |= velocity >> 4;
              dur = b & 0x1f;
              if (lead >= 0xa0) dur = (dur + 1) * 128;
              else dur = (dur + 1) * 16;
            }
            velocity /= 127.0;
            return { type: "note", chid, noteid, velocity, dur: dur / 1000 };
          }
        
        case 0xc0: { // Wheel.
            const chid = lead & 0x0f;
            let wheel = iter.evv[iter.evp++] || 0x80;
            wheel = (wheel - 0x80) / 128.0;
            return { type: "wheel", chid, wheel };
          }
          
        default: {
            iter.evp = iter.evv.length;
            return null;
          }
      }
    };
    return iter;
  }
  
  /* Uint8Array in, AudioBuffer out.
   * We preserve the original sample rate, but drop extra channels.
   * null for errors.
   */
  static decodeWav(s) {
    if (!s) return null;
    if ((s[0] !== 0x52) || (s[1] !== 0x49) || (s[2] !== 0x46) || (s[3] !== 0x46)) return null;
    if ((s[8] !== 0x57) || (s[9] !== 0x41) || (s[10] !== 0x56) || (s[11] !== 0x45)) return null;
    const textDecoder = new TextDecoder("utf8");
    let samplev = null; // Float32Array
    let samplec=0, samplea=0;
    let chanc=0, rate=0; // From "fmt " chunk.
    let sp = 12;
    while (sp < s.length) {
      const chunkid = textDecoder.decode(s.slice(sp, sp + 4));
      sp += 4;
      const chunklen = s[sp] | (s[sp+1] << 8) | (s[sp+2] << 16) | (s[sp+3] << 24);
      sp += 4;
      if (sp > s.length - sp) break;
      switch (chunkid) {
      
        case "fmt ": {
            if (chunklen < 16) return null;
            const format = s[sp] | (s[sp+1] << 8);
            if (format !== 1) return null; // Only LPCM supported.
            chanc = s[sp+2] | (s[sp+3] << 8);
            rate = s[sp+4] | (s[sp+5] << 8) | (s[sp+6] << 16) | (s[sp+7] << 24);
            const samplesize = s[sp+14] | (s[sp+15] << 8);
            if (samplesize !== 16) return null; // Only s16 supported.
          } break;
          
        case "data": {
            if (!chanc) return null; // eg no "fmt " chunk
            const stride = chanc << 1;
            let addc = Math.floor(chunklen / stride);
            if (samplec > samplea - addc) {
              const nv = new Float32Array(samplec + addc);
              if (samplev) nv.set(samplev);
              samplev = nv;
              samplea = samplec + addc;
            }
            let subp = sp;
            for (; addc-->0; subp+=stride, samplec++) {
              let sample = (s[subp] | (s[subp+1] << 8));
              if (sample & 0x8000) sample |= ~0xffff;
              samplev[samplec] = sample / 32768.0;
            }
          } break;
      }
      sp += chunklen;
    }
    if (!samplec) samplev = new Float32Array([0.0]);
    if (!rate) rate = 44100;
    const buffer = new AudioBuffer({
      length: samplec,
      numberOfChannels: 1,
      sampleRate: rate,
      channelCount: 1,
    });
    buffer.copyToChannel(samplev, 0);
    return buffer;
  }
  
  static frequencyForMidiNote(noteid) {
    if ((noteid < 0) || (noteid >= 0x80)) return 0;
    if (!SynthFormats.FREQ_BY_NOTEID) {
      SynthFormats.FREQ_BY_NOTEID = [];
      for (let i=0; i<0x80; i++) {
        SynthFormats.FREQ_BY_NOTEID.push(440.0 * Math.pow(2, ((i - 0x45) / 12)));
      }
    }
    return SynthFormats.FREQ_BY_NOTEID[noteid];
  }
}
