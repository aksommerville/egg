/* SynthFormats.js
 * Static helpers for our audio formats.
 */
 
export class SynthFormats {

  /* Check signatures.
   * Input must be Uint8Array.
   * Returns one of: "", "egs", "wav", "mid".
   * We don't support MIDI, but one can imagine errors where a MIDI file slips thru the pipeline,
   * so we detect them in case you want to log those specially.
   */
  static detectFormat(s) {
    const ck4 = (p, a, b, c, d) => ((s[p] === a) && (s[p+1] === b) && (s[p+2] === c) && (s[p+3] === d));
    if (s.length >= 4) {
      if (ck4(0, 0x00, 0x45, 0x47, 0x53)) return "egs";
      if (ck4(0, 0x52, 0x49, 0x46, 0x46)) return "wav";
      if (ck4(0, 0x4d, 0x54, 0x68, 0x64)) return "mid";
    }
    return "";
  }
  
  /* Split the parts of an EGS resource.
   * Returns null or { channels: { chid, trim, mode, v: Uint8Array }[], events: Uint8Array }.
   * Channels are indexed by chid, may be null, and may contain chid >= 0x10.
   * We don't validate events or detect the explicit EOF event.
   */
  static splitEgs(s) {
    if (!s || (s[0] !== 0x00) || (s[1] !== 0x45) || (s[2] !== 0x47) || (s[3] !== 0x53)) return null;
    const egs = {
      channels: [],
    };
    let sp = 4;
    while (sp < s.length) {
      const chid = s[sp++];
      if (chid === 0xff) break; // Events introducer.
      const trim = s[sp++];
      const mode = s[sp++];
      const configlen = (s[sp] << 16) | (s[sp+1] << 8) | s[sp+2];
      sp += 3;
      if (sp > s.length - configlen) throw new Error(`Invalid EGS, channel ${chid} config overflows.`);
      const v = s.slice(sp, sp + configlen);
      sp += configlen;
      while (chid >= egs.channels.length) egs.channels.push(null);
      egs.channels[chid] = { chid, trim, mode, v };
    }
    egs.events = s.slice(sp);
    return egs;
  }
  
  /* Create an iterator to read events off an EGS event dump (splitEgs().events).
   * eg:
   *   for (let iter=SynthFormats.iterateEgsEvents(egs.events), event; event=iter.next(); ) {
   *     switch (event.type) {
   *       case "delay": // event.delay s
   *       case "note": // event.chid 0..15, event.noteid 0..127, event.velocity 0..127, event.dur s
   *       case "future": // event.v Uint8Array full encoded event
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
      
      // Other events distinguished by top four bits.
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
          
        // Opcodes 0xb0, 0xc0, 0xd0, 0xe0, 0xf0 are not yet defined, but their shapes are, and we can skip them.
        case 0xb0: {
            const v = new Uint8Array([lead]);
            return { type: "future", v };
          }
        case 0xc0: {
            const v = new Uint8Array(iter.evv.buffer, iter.evv.byteOffset + iter.evp - 1, 2);
            iter.evp += 1;
            return { type: "future", v };
          }
        case 0xd0: {
            const v = new Uint8Array(iter.evv.buffer, iter.evv.byteOffset + iter.evp - 1, 3);
            iter.evp += 2;
            return { type: "future", v };
          }
        case 0xe0: {
            const v = new Uint8Array(iter.evv.buffer, iter.evv.byteOffset + iter.evp - 1, 3);
            iter.evp += 2;
            return { type: "future", v };
          }
        case 0xf0: {
            const len = iter.evv[iter.evp+1] || 0;
            const v = new Uint8Array(iter.evv.buffer, iter.evv.byteOffset + iter.evp - 1, 3 + len);
            iter.evp += 2 + len;
            return { type: "future", v };
          }
          
        default: {
            iter.evp = iter.evv.length;
            return null;
          }
      }
    };
    return iter;
  }
  
  /* Uint8Array in, AudioBuffer out, null for errors.
   * We only accept a very specific subset of WAV, as produced by eggdev's resource compiler.
   * (to wit: 16-bit LPCM mono, any rate, with a single "data" chunk and no extra chunks).
   */
  static decodeWav(s) {
    // Check "RIFF" signature and "data" header, and take the rest on faith.
    // "data" length must reach EOF exactly.
    if (!s || (s[0] !== 0x52) || (s[1] !== 0x49) || (s[2] !== 0x46) || (s[3] !== 0x46)) return null;
    if (s.length < 44) return null;
    if ((s[36] !== 0x64) || (s[37] !== 0x61) || (s[38] !== 0x74) || (s[39] !== 0x61)) return null;
    const datalen = s[40] | (s[41] << 8) | (s[42] << 16) | (s[43] << 24);
    if ((datalen < 0) || (44 + datalen !== s.length)) return null;
    const rate = s[24] | (s[25] << 8) | (s[26] << 16) | (s[27] << 24);
    if ((rate < 200) || (rate > 200000)) return null; // Arbitrary sanity limits.
    // Looks kosher. Decode and normalize into a Float32Array, then wrap that in an AudioBuffer.
    const samplec = datalen >> 1;
    const samplev = new Float32Array(samplec);
    for (let dstp=0, sp=44; dstp<samplec; dstp++, sp+=2) {
      let sample = s[sp] | (s[sp+1] << 8);
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
