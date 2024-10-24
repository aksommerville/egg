/* MidiFile.js
 * Live representation of a MIDI file, for MidiEditor.
 */
 
const MIDI_CHUNK_MThd = 0x4d546864;
const MIDI_CHUNK_MTrk = 0x4d54726b;
 
export class MidiFile {
  constructor(src) {
    this.nextEventId = 1;
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
    const event = this.getZEvent({ opcode: 0xff, a: 0x51 });
    if (!event || (event.v?.length !== 3)) return 500000;
    return (event.v[0] << 16) | (event.v[1] << 8) | event.v[2];
  }
  
  setTempoBpm(bpm) {
    this.setTempo(Math.floor(60000000 / bpm));
  }
  
  getTempoBpm() {
    return Math.floor(60000000 / this.getTempo());
  }
  
  setChannelName(chid, name) {
    let preferTrack = this.tracks[0];
    for (const track of this.tracks) {
      for (const event of track) {
        if ((event.chid === chid) && (event.opcode === 0xff) && (event.a === 0x04)) {
          if (name) {
            event.v = new TextEncoder("utf8").encode(name);
          } else {
            this.deleteEvent(event.id);
          }
          return;
        } else if (event.chid === chid) {
          preferTrack = track;
        }
      }
    }
    if (!preferTrack) {
      preferTrack = [];
      this.tracks.push(preferTrack);
    }
    let pfxp = preferTrack.findIndex(e => ((e.opcode === 0xff) && (e.a === 0x20) && (e.v?.[0] === chid)));
    if (pfxp < 0) {
      preferTrack.splice(0, 0, {
        time: 0,
        id: this.nextEventId++,
        chid: chid,
        opcode: 0xff,
        a: 0x20,
        b: 0,
        v: new Uint8Array([chid]),
      });
      pfxp = 0;
    }
    preferTrack.splice(pfxp + 1, 0, {
      time: preferTrack[pfxp].time,
      id: this.nextEventId++,
      chid: chid,
      opcode: 0xff,
      a: 0x04,
      b: 0,
      v: new TextEncoder("utf8").encode(name),
    });
  }
  
  eventById(id) {
    for (const track of this.tracks) {
      const event = track.find(e => e.id === id);
      if (event) return event;
    }
    return null;
  }
  
  addEvent(event) {
    event = { ...event, id: this.nextEventId++ };
    for (const track of this.tracks) {
      if (track.find(e => e.chid === event.chid)) {
        this.insertEvent(track, event);
        return;
      }
    }
    if (!this.tracks.length) this.tracks.push([]);
    this.insertEvent(this.tracks[0], event);
  }
  
  insertEvent(track, event) {
    track.push(event);
    track.sort((a, b) => a.time - b.time);
  }
  
  // Must have an ID already present in my lists.
  replaceEvent(event) {
    if (!event?.id) return false;
    for (const track of this.tracks) {
      const p = track.findIndex(e => e.id === event.id);
      if (p >= 0) {
        const timeChanged = (track[p].time !== event.time);
        track[p] = event;
        if (timeChanged) track.sort((a, b) => a.time - b.time);
        return true;
      }
    }
    return false;
  }
  
  /* (partnerToo) is for Note On and Note Off only, to delete the other one, if it's on the same track.
   */
  deleteEvent(id, partnerToo) {
    if (!id) return false;
    for (const track of this.tracks) {
      const p = track.findIndex(e => e.id === id);
      if (p >= 0) {
        const event = track[p];
        track.splice(p, 1);
        if (partnerToo) switch (event.opcode) {
          case 0x80: { // Note Off. Search backward in this track for Note On.
              for (let op=p; op-->0; ) {
                const o = track[op];
                if ((o.chid === event.chid) && (o.a === event.a)) {
                  if (o.opcode === 0x80) break;
                  if (o.opcode === 0x90) {
                    track.splice(op, 1);
                    break;
                  }
                }
              }
            } break;
          case 0x90: { // Note On. Search forward in this track for Note Off.
              for (let op=p+1; op<track.length; op++) {
                const o = track[op];
                if ((o.chid === event.chid) && (o.a === event.a)) {
                  if (o.opcode === 0x90) break;
                  if (o.opcode === 0x80) {
                    track.splice(op, 1);
                    break;
                  }
                }
              }
            } break;
        }
        return true;
      }
    }
    return false;
  }
  
  /* Put a new event at time zero on whichever track already has events for (chid), defaulting to the first track.
   * (event) may be partial. We can default (chid,time,id,a,b).
   */
  addZEvent(chid, event) {
    if (event.time) throw new Error("MidiFile.addZEvent, must be time zero");
    let track = null;
    for (const q of this.tracks) {
      if (q.find(e => e.chid === chid)) {
        track = q;
        break;
      }
    }
    if (!track) {
      if (this.tracks.length) {
        track = this.tracks[0];
      } else {
        track = [];
        this.tracks = [track];
      }
    }
    track.splice(0, 0, {
      chid,
      time: 0,
      id: this.nextEventId++,
      a: 0, b: 0,
      ...event,
    });
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
      id: this.nextEventId++,
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
  
  /* Iterate events in chronological order, as if they were a single track.
   */
  *mergeEvents() {
    const pv = this.tracks.map(() => 0);
    for (;;) {
      let nextEvent = null;
      let nextTrackid = -1;
      for (let trackid=0; trackid<pv.length; trackid++) {
        const event = this.tracks[trackid][pv[trackid]];
        if (!event) continue;
        if (!nextEvent || (event.time < nextEvent.time)) {
          nextEvent = event;
          nextTrackid = trackid;
        }
      }
      if (!nextEvent) break;
      pv[nextTrackid]++;
      yield nextEvent;
    }
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
    let chpfx = 0xff;
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
              a, b,
              id: this.nextEventId++,
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
              id: this.nextEventId++,
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
            if ((a === 0x20) && (v.length === 1)) chpfx = v[0];
            track.push({
              time,
              chid: chpfx,
              opcode: status,
              a, v,
              b: 0,
              id: this.nextEventId++,
            });
            status = 0;
          }
      }
    }
    this.tracks.push(track);
  }
  
  static reprNote(noteid, withNumber) {
    let dst = "0x" + noteid.toString(16).padStart(2, '0');
    if ((noteid >= 0) && (noteid < 0x80)) { // Note zero is c-1, and octaves are "cdefgab" (not "abcdefg", $@! musicians...)
      if (!withNumber) dst = "";
      const octave = Math.floor(noteid / 12) - 1;
      let tone, accidental="";
      switch (noteid % 12) {
        case 0: tone = "c"; break;
        case 1: tone = "c"; accidental = "s"; break;
        case 2: tone = "d"; break;
        case 3: tone = "d"; accidental = "s"; break;
        case 4: tone = "e"; break;
        case 5: tone = "f"; break;
        case 6: tone = "f"; accidental = "s"; break;
        case 7: tone = "g"; break;
        case 8: tone = "g"; accidental = "s"; break;
        case 9: tone = "a"; break;
        case 10: tone = "a"; accidental = "s"; break;
        case 11: tone = "b"; break;
      }
      dst += " " + tone + accidental + octave;
    }
    return dst;
  }
}

MidiFile.GM_PROGRAM_NAMES = [
  /* 00 */ "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano", "Honky Tonk Piano", "EP 1 (Rhodes)", "EP 2 (Chorused)", "Harpsichord", "Clavinet",
  /* 08 */ "Celesta", "Glockenspiel", "Music Box", "Vibraphone", "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
  /* 10 */ "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ", "Reed Organ", "French Accordion", "Harmonica", "Tango Accordion",
  /* 18 */ "Nylon Acoustic Guitar", "Steel Acoustic Guitar", "Jazz Electric Guitar", "Clean Electric Guitar", "Muted Electric Guitar", "Overdrive Guitar", "Distortion Guitar", "Guitar Harmonics",
  /* 20 */ "Acoustic Bass", "Fingered Bass", "Picked Bass", "Fretless Bass", "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2",
  /* 28 */ "Violin", "Viola", "Cello", "Contrabass", "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani",
  /* 30 */ "String Ens 1", "String Ens 2", "Synth Strings 1", "Synth Strings 2", "Choir Aahs", "Voice Oohs", "Synth Voice", "Orchestra Hit",
  /* 38 */ "Trumpet", "Trombone", "Tuba", "Muted Trumpet", "French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2",
  /* 40 */ "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax", "Oboe", "English Horn", "Bassoon", "Clarinet",
  /* 48 */ "Piccolo", "Flute", "Recorder", "Pan Flute", "Blown Bottle", "Shakuhachi", "Whistle", "Ocarina",
  /* 50 */ "Square Lead", "Saw Lead", "Calliope", "Chiffer", "Charang", "Voice Solo", "Fifths", "Bass+Lead",
  /* 58 */ "Fantasia Pad", "Warm Pad", "Polysynth Pad", "Choir Space Voice", "Bowed Glass", "Metallic Pro Pad", "Halo Pad", "Sweep Pad",
  /* 60 */ "Rain", "Soundtrack", "Crystal", "Atmosphere", "Brightness", "Goblins", "Echoes, Drops", "Sci-Fi",
  /* 68 */ "Sitar", "Banjo", "Shamisen", "Koto", "Kalimba", "Bagpipe", "Fiddle", "Shanai",
  /* 70 */ "Tinkle Bell", "Agogo", "Steel Drums", "Wood Block", "Taiko Drums", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
  /* 78 */ "Fret Noise", "Breath Noise", "Seashore", "Tweet", "Telephone", "Helicopter", "Applause", "Gunshot",
];

MidiFile.GM_DRUM_NAMES = [
  /* 00 */ "", "", "", "", "", "", "", "",
  /* 08 */ "", "", "", "", "", "", "", "",
  /* 10 */ "", "", "", "", "", "", "", "",
  /* 18 */ "", "", "", "", "", "", "", "",
  /* 20 */ "", "", "", "Acoustic Bass Drum", "Bass Drum 1", "Side Stick", "Acoustic Snare", "Hand Clap",
  /* 28 */ "Electric Snare", "Low Floor Tom", "Closed Hi Hat", "High Floor Tom", "Pedal Hi Hat", "Low Tom", "Open Hi Hat", "Low Mid Tom",
  /* 30 */ "Hi Mid Tom", "Crash 1", "High Tom", "Ride 1", "Chinese Cymbal", "Ride Bell", "Tambourine", "Splash",
  /* 38 */ "Cowbell", "Crash 2", "Vibraslap", "Ride 2", "Hi Bongo", "Low Bongo", "Mute High Conga", "Open High Conga",
  /* 40 */ "Low Conga", "High Timbale", "Low Timbale", "High Agogo", "Low Agogo", "Cabasa", "Maracas", "Short Whistle",
  /* 48 */ "Long Whistle", "Short Guiro", "Long Guiro", "Claves", "High Wood Block", "Low Wood BLock", "Mute Cuica", "Open Cuica",
  /* 50 */ "Mute Triangle", "Open Triangle", "", "", "", "", "", "",
];

MidiFile.CONTROL_KEYS = [
  /* 00 */ "Bank MSB", "Mod MSB", "Breath MSB", "", "Foot MSB", "Porta Time MSB", "Data Entry MSB", "Volume MSB", 
  /* 08 */ "Balance MSB", "", "Pan MSB", "Expression MSB", "Effect 1 MSB", "Effect 2 MSB", "Effect 3 MSB", "Effect 4 MSB",
  /* 10 */ "GP 1 MSB", "GP 2 MSB", "GP 3 MSB", "GP 4 MSB", "", "", "", "",
  /* 18 */ "", "", "", "", "", "", "", "",
  /* 20 */ "Bank LSB", "Mod LSB", "Breath LSB", "", "Foot LSB", "Porta Time LSB", "Data Entry LSB", "Volume LSB", 
  /* 28 */ "Balance LSB", "", "Pan LSB", "Expression LSB", "Effect 1 LSB", "Effect 2 LSB", "Effect 3 LSB", "Effect 4 LSB",
  /* 30 */ "GP 1 LSB", "GP 2 LSB", "GP 3 LSB", "GP 4 LSB", "", "", "", "",
  /* 38 */ "", "", "", "", "", "", "", "",
  /* 40 */ "Sustain", "Portamento", "Sustenuto", "Soft", "Legato", "Hold 2", "Ctl 1 (Variation)", "Ctl 2 (Timbre)",
  /* 48 */ "Ctl 3 (Release Time)", "Ctl 4 (Attack Time)", "Ctl 5 (Brightness)", "Ctl 6", "Ctl 7", "Ctl 8", "Ctl 9", "Ctl 10",
  /* 50 */ "GP 5", "GP 6", "GP 7", "GP 8", "Porta Ctl (noteid)", "", "", "",
  /* 58 */ "", "", "", "Effect 1 Depth", "Effect 2 Depth", "Effect 3 Depth", "Effect 4 Depth", "Effect 5 Depth",
  /* 60 */ "", "", "", "", "", "", "", "",
  /* 68 */ "", "", "", "", "", "", "", "",
  /* 70 */ "", "", "", "", "", "", "", "",
  /* 78 */ "All Sound Off", "Reset Controller", "Local Switch", "All Notes Off", "Omni Off", "Omni On", "Poly Switch", "Poly On",
];

MidiFile.META_TYPES = [
  /* 00 */ "", "Text", "Copyright", "Track Name", "Instrument Name", "Lyrics", "Marker", "Cue Point",
  /* 08 */ "", "", "", "", "", "", "", "",
  /* 18 */ "", "", "", "", "", "", "", "",
  /* 18 */ "", "", "", "", "", "", "", "",
  /* 20 */ "MIDI Channel Prefix", "", "", "", "", "", "", "",
  /* 28 */ "", "", "", "", "", "", "", "End of Track",
  /* 30 */ "", "", "", "", "", "", "", "",
  /* 38 */ "", "", "", "", "", "", "", "",
  /* 40 */ "", "", "", "", "", "", "", "",
  /* 48 */ "", "", "", "", "", "", "", "",
  /* 50 */ "", "Set Tempo", "", "", "SMPTE Offset", "", "", "",
  /* 58 */ "Time Signature", "Key Signature", "", "", "", "", "", "",
  /* 60 */ "", "", "", "", "", "", "", "",
  /* 68 */ "", "", "", "", "", "", "", "",
  /* 70 */ "", "", "", "", "", "", "", "",
  /* 78 */ "", "", "", "", "", "", "", "",
  /* 80 */ "", "", "", "", "", "", "", "",
  /* 88 */ "", "", "", "", "", "", "", "",
  /* 90 */ "", "", "", "", "", "", "", "",
  /* 98 */ "", "", "", "", "", "", "", "",
  /* a0 */ "", "", "", "", "", "", "", "",
  /* a8 */ "", "", "", "", "", "", "", "",
  /* b0 */ "", "", "", "", "", "", "", "",
  /* b8 */ "", "", "", "", "", "", "", "",
  /* c0 */ "", "", "", "", "", "", "", "",
  /* c8 */ "", "", "", "", "", "", "", "",
  /* d0 */ "", "", "", "", "", "", "", "",
  /* d8 */ "", "", "", "", "", "", "", "",
  /* e0 */ "", "", "", "", "", "", "", "",
  /* e8 */ "", "", "", "", "", "", "", "",
  /* f0 */ "Egg Channel Header", "", "", "", "", "", "", "",
  /* f8 */ "", "", "", "", "", "", "", "",
];
