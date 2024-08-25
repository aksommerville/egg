/* MidiFile.js
 * Live representation of a MIDI file, for SongEditor.
 */
 
const MIDI_CHUNK_MThd = 0x4d546864;
const MIDI_CHUNK_MTrk = 0x4d54726b;
 
export class MidiFile {
  constructor(src) {
    if (!src) this._init();
    else if (src instanceof Uint8Array) this._decode(src);
    else if (src instanceof ArrayBuffer) this._decode(new Uint8Array(src));
    else throw new Error(`Input to MidiFile must be Uint8Array, ArrayBuffer, or null`);
  }
  
  setTempo(usperqnote) {
    if ((usperqnote < 1) || (usperqnote > 0xffffff)) throw new Error(`Invalid tempo ${usperqnote} us/qnote`);
    this.setZEvent({ chid: 0xff, opcode: 0xff, a: 0x51 }, { v: new Uint8Array([usperqnote >> 16, usperqnote >> 8, usperqnote]) });
  }
  
  getTempo() {
    const event = this.getZEvent({ chid: 0xff, opcode: 0xff, a: 0x51 });
    if (!event || (event.v?.length !== 3)) return 500000;
    return (event.v[0] << 16) | (event.v[1] << 8) | event.v[2];
  }
  
  /* Find an event at time zero where every field in (match) matches exactly.
   * If not found, create it on the first track.
   * Then replace all fields in (replace).
   */
  setZEvent(match, replace) {
    const mkv = Object.keys(match);
    for (const track of this.tracks) {
      for (const event of track) {
        if (event.time) break;
        let ok = true;
        for (const k of mkv) {
          if (event[k] !== match[k]) {
            ok = false;
            break;
          }
        }
        if (!ok) continue;
        for (const k of Object.keys(replace)) {
          event[k] = replace[k];
        }
        return;
      }
    }
    if (!this.tracks.length) this.tracks.push([]);
    const track = this.tracks[0];
    track.splice(0, 0, {
      time: 0,
      chid: 0xff,
      a:0, b:0,
      ...match,
      ...replace,
    });
  }
  
  getZEvent(match) {
    const mkv = Object.keys(match);
    for (const track of this.tracks) {
      for (const event of track) {
        if (event.time) break;
        let ok = true;
        for (const k of mkv) {
          if (event[k] !== match[k]) {
            ok = false;
            break;
          }
        }
        if (ok) return event;
      }
    }
    return null;
  }
  
  encode() {
    let dst = new Uint8Array(4096);
    let dstc = 0;
    const require = (c) => {
      if (dstc > dst.length - c) {
        const na = (dstc + c + 4096) & ~4095;
        const nv = new Uint8Array(na);
        nv.set(dst);
        dst = nv;
      }
    };
    const be32 = (src) => {
      require(4);
      dst[dstc++] = src >> 24;
      dst[dstc++] = src >> 16;
      dst[dstc++] = src >> 8;
      dst[dstc++] = src;
    };
    const u8 = (src) => {
      require(1);
      dst[dstc++] = src;
    };
    const vlq = (src) => {
      require(4);
      if (src >= 0x10000000) throw new Error(`Not representable as VLQ: ${src}`);
      if (src >= 0x00200000) {
        dst[dstc++] = 0x80 | (src >> 21);
        dst[dstc++] = 0x80 | (src >> 14);
        dst[dstc++] = 0x80 | (src >> 7);
        dst[dstc++] = 0x7f & src;
      } else if (src >= 0x00004000) {
        dst[dstc++] = 0x80 | (src >> 14);
        dst[dstc++] = 0x80 | (src >> 7);
        dst[dstc++] = 0x7f & src;
      } else if (src >= 0x00000080) {
        dst[dstc++] = 0x80 | (src >> 7);
        dst[dstc++] = 0x7f & src;
      } else {
        dst[dstc++] = src;
      }
    };
    const raw = (src) => {
      require(src.length);
      const dstview = new Uint8Array(dst.buffer, dstc, src.length);
      dstview.set(src);
      dstc += src.length;
    };
    
    // MThd.
    be32(MIDI_CHUNK_MThd);
    be32(6);
    dst[dstc++] = this.format >> 8;
    dst[dstc++] = this.format;
    dst[dstc++] = this.trackCount >> 8;
    dst[dstc++] = this.trackCount;
    dst[dstc++] = this.division >> 8;
    dst[dstc++] = this.division;
    
    // MTrk.
    for (const track of this.tracks) {
      be32(MIDI_CHUNK_MTrk);
      const lenp = dstc;
      be32(0);
      let time = 0;
      for (const event of track) {
        vlq(event.time - time);
        time = event.time;
        switch (event.opcode) {
          case 0x80:
          case 0x90:
          case 0xa0:
          case 0xb0:
          case 0xe0: u8(event.opcode | event.chid); u8(event.a); u8(event.b); break;
          case 0xc0:
          case 0xd0: u8(event.opcode | event.chid); u8(event.a); break;
          case 0xff: u8(0xff); u8(event.a); vlq(event.v.length); raw(event.v); break;
          case 0xf0:
          case 0xf7: u8(event.opcode); vlq(event.v.length); raw(event.v); break;
          default: throw new Error(`Unexpected opcode ${event.opcode}`);
        }
      }
      const len = dstc - lenp - 4;
      dst[lenp + 0] = len >> 24;
      dst[lenp + 1] = len >> 16;
      dst[lenp + 2] = len >> 8;
      dst[lenp + 3] = len;
    }
    
    return new Uint8Array(dst.buffer, 0, dstc);
  }
  
  _init() {
    this.format = 1;
    this.trackCount = 0; // As written in header, not necessarily count of MTrk.
    this.division = 48; // Ticks/qnote.
    this.tracks = []; // MTrk. Each is an array of event: {time:ticks,chid,opcode,a,b,v?:Uint8Array}
  }
  
  _decode(src) {
    this._init();
    for (let srcp=0; srcp<src.length; ) {
      let id = src[srcp++] << 24;
      id |= src[srcp++] << 16;
      id |= src[srcp++] << 8;
      id |= src[srcp++];
      let len = src[srcp++] << 24;
      len |= src[srcp++] << 16;
      len |= src[srcp++] << 8;
      len |= src[srcp++];
      if (isNaN(len) || (len < 0) || (srcp > src.length -len)) throw new Error(`Framing fault in MIDI file`);
      switch (id) {
        case MIDI_CHUNK_MThd: this._decodeMThd(src, srcp, len); break;
        case MIDI_CHUNK_MTrk: this._decodeMTrk(src, srcp, len); break;
      }
      srcp += len;
    }
  }
  
  _decodeMThd(src, srcp, len) {
    if (len < 6) throw new Error(`Invalid length ${len} for MThd`);
    this.format = (src[srcp] << 8) | src[srcp + 1];
    this.trackCount = (src[srcp + 2] << 8) | src[srcp + 3];
    this.division = (src[srcp + 4] << 8) | src[srcp + 5];
  }
  
  _decodeMTrk(src, srcp, len) {
    const track = [];
    const stopp = srcp + len;
    let time = 0;
    let status = 0;
    while (srcp < stopp) {
      
      // Delay.
      let delay = 0;
      while ((srcp < stopp) && (src[srcp] & 0x80)) {
        delay <<= 7;
        delay |= src[srcp] & 0x7f;
        srcp++;
      }
      if (srcp >= stopp) break;
      delay <<= 7;
      delay |= src[srcp++];
      time += delay;
      
      // Status.
      let lead = src[srcp];
      if (lead & 0x80) srcp++;
      else if (status) lead = status;
      else throw new Error(`Expected status byte in MTrk around ${srcp}/${src.length}, found ${lead}`);
      status = lead;
      
      // Digest status and produce event.
      // We do not handle velocity-zero Note On, I think there's no need since we're not actually playing songs.
      switch (status & 0xf0) {
      
        // Channel Voice events with two data bytes:
        case 0x80: // Note Off
        case 0x90: // Note On
        case 0xa0: // Note Adjust
        case 0xb0: // Control Change
        case 0xe0: // Pitch Wheel
          {
            const a = src[srcp++];
            const b = src[srcp++];
            track.push({
              time,
              chid: status & 0x0f,
              opcode: status & 0xf0,
              a, b
            });
          } break;
          
        // Channel Voice events with one data byte:
        case 0xc0: // Program Change
        case 0xd0: // Channel Pressure
          {
            const a = src[srcp++];
            track.push({
              time,
              chid: status & 0x0f,
              opcode: status & 0xf0,
              a,
              b: 0,
            });
          } break;
          
        // Meta or Sysex:
        default: {
            let a = 0;
            if (status === 0xff) a = src[srcp++];
            else if ((status !== 0xf0) && (status !== 0xf7)) throw new Error(`Unexpected MIDI status byte ${status}`);
            let paylen = 0;
            while ((srcp < stopp) && (src[srcp] & 0x80)) {
              paylen <<= 7;
              paylen |= src[srcp++] & 0x7f;
            }
            paylen <<= 7;
            paylen |= src[srcp++];
            if (srcp > stopp - paylen) throw new Error(`Sysex/Meta length error`);
            const v = new Uint8Array(src.buffer, src.byteOffset + srcp, paylen);
            srcp += paylen;
            track.push({
              time,
              chid: 0xff,
              opcode: status,
              a, v,
              b: 0,
            });
            status = 0;
          }
      }
    }
    this.tracks.push(track);
  }
}
