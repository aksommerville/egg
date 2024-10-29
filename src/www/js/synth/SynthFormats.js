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
      if (ck4(0, 0x00, 0x50, 0x43, 0x4d)) return "pcm";
    }
    return "";
  }
  
  /* Split the parts of an EGS resource.
   * Returns null or { tempo: ms, channels: { chid, master, pan, mode, config: Uint8Array, post: Uint8Array }[], events: Uint8Array }.
   * Channels are indexed by chid, may be sparse, and may contain chid >= 0x10.
   * Leading noop events are trimmed from Channel Headers.
   */
  static splitEgs(s) {
    if (!s || (s[0] !== 0x00) || (s[1] !== 0x45) || (s[2] !== 0x47) || (s[3] !== 0x53)) return null;
    const egs = {
      tempo: (s[4] << 8) | s[5],
      channels: [],
    };
    let sp = 6;
    while (sp < s.length) {
      const chid = s[sp++];
      if (chid === 0xff) break; // Events introducer.
      const master = s[sp++];
      const pan = s[sp++];
      const mode = s[sp++];
      const configlen = (s[sp] << 8) | s[sp+1];
      sp += 2;
      if (sp > s.length - configlen) throw new Error(`Invalid EGS, channel ${chid} config overflows.`);
      const config = s.slice(sp, sp + configlen);
      sp += configlen;
      const postlen = (s[sp] << 8) | s[sp+1];
      sp += 2;
      if (sp > s.length - postlen) throw new Error(`Invalid EGS, channel ${chid} post overflows.`);
      const post = s.slice(sp, sp + postlen);
      sp += postlen;
      egs.channels[chid] = { chid, master, pan, mode, config, post };
    }
    egs.events = s.slice(sp);
    return egs;
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
      
        case 0x80: // Short Note
        case 0x90: // Medium Note
        case 0xa0: // Long Note
          {
            const chid = lead & 0x0f;
            const a = iter.evv[iter.evp++] || 0;
            const b = iter.evv[iter.evp++] || 0;
            const noteid = a >> 1;
            let velocity = ((a << 6) & 0x40) | (b >> 2);
            let dur = 0;
            if (lead >= 0x90) { // Medium or Long, reduce velocity to 4 bits and read 5-bit duration.
              velocity &= 0x78;
              velocity |= velocity >> 4;
              dur = b & 0x1f;
              if (lead >= 0xa0) dur = (dur + 1) * 512;
              else dur = (dur + 1) * 64;
            } else { // Short, keep the 7-bit velocity, and read 2-bit duration.
              dur = (b & 0x03) * 16;
            }
            velocity /= 127.0;
            return { type: "note", chid, noteid, velocity, dur: dur / 1000 };
          }
        
        case 0xc0: { // Wheel.
            const chid = lead & 0x0f;
            let wheel = iter.evv[iter.evp++] ?? 0x80;
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
  
  /* Uint8Array in, AudioBuffer out. Or null on errors.
   */
  static decodePcm(s) {
    if (!s || (s[0] !== 0x00) || (s[1] !== 0x50) || (s[2] !== 0x43) || (s[3] !== 0x4d)) return null;
    const rate = (s[4] << 24) | (s[5] << 16) | (s[6] << 8) | s[7];
    const samplec = (s.length - 8) >> 1;
    const samplev = new Float32Array(samplec);
    for (let dstp=0, sp=8; dstp<samplec; dstp++, sp+=2) {
      let sample = s[sp] | s[sp+1];
      if (sample & 0x8000) sample |= ~0xffff;
      samplev[dstp] = sample / 32768.0;
    }
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
