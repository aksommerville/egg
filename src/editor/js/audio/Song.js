/* Song.js
 * Loose model accomodating both MIDI and EGS.
 */
 
import { Encoder } from "../Encoder.js";
 
export class Song {
  constructor(src) {
    if (!src) this._init();
    else if (src instanceof Song) this._copy(src);
    else if (src instanceof ArrayBuffer) this._decode(new Uint8Array(src));
    else if (src instanceof Uint8Array) this._decode(src);
    else throw new Error(`Unexpected input type for Song`);
  }
  
  encode() {
    switch (this.format) {
      case "egs": return this.encodeEgs();
      case "mid": return this.encodeMidi();
      // Egg's default format is EGS, but for editor purposes we prefer the more portable MIDI:
      default: return this.encodeMidi();
    }
  }
  
  getTrackIds() {
    const ids = new Set();
    for (const event of this.events) {
      if (typeof(event.trackid) !== "number") continue;
      ids.add(event.trackid);
    }
    return Array.from(ids).sort((a, b) => a - b);
  }
  
  getChannelIds() {
    const dst = [];
    for (let chid=0; chid<this.channels.length; chid++) {
      if (!this.channels[chid]) continue;
      dst.push(chid);
    }
    return dst;
  }
  
  /* Visibility classification.
   */
  isNoteEvent(event) {
    switch (event.eopcode) {
      case 0x80: case 0x90: case 0xa0: return true;
    }
    switch (event.mopcode) {
      case 0x80: case 0x90: return true;
    }
    return false;
  }
  isConfigEvent(event) {
    // EGS doesn't have a concept of "config event", only MIDI does.
    switch (event.mopcode) {
      case 0xb0: case 0xc0: return true; // Control, Program
      case 0xf0: case 0xf7: case 0xff: return true; // Sysex, Meta
    }
    return false;
  }
  isAdjustEvent(event) {
    // I'm calling EGS Future events "adjust" because whatever.
    switch (event.eopcode) {
      case 0xb0: case 0xc0: case 0xd0: case 0xe0: case 0xf0: return true;
    }
    switch (event.mopcode) {
      case 0xa0: case 0xd0: case 0xe0: return true; // Aftertouch, Pressure, Wheel
    }
    return false;
  }
  
  /* Update (channel.mode) and (channel.v).
   * Returns true if anything changed.
   * We'll do some secret bookkeeping behind the scenes with these goals:
   *  - Preserve configuration of modes if you toggle back and forth.
   *  - Forbid changing into an unknown mode.
   *  - Supply a sensible default configuration on a fresh change.
   * Mode -1 is perfectly legal, that's the GM Fallback mode.
   * All the rest are defined by EGS: (0,1,2,3,4)=(noop,drum,wave,fm,sub)
   */
  changeChannelMode(channel, mode) {
    if (!channel) return false;
    if (channel.mode === mode) return false;
    switch (mode) {
      case -1: case 0: case 1: case 2: case 3: case 4: break;
      default: return false;
    }
    if (!channel._modeConfigBackup) channel._modeConfigBackup = {};
    channel._modeConfigBackup[channel.mode] = channel.v;
    channel.mode = mode;
    if (channel._modeConfigBackup[mode]) {
      channel.v = channel._modeConfigBackup[mode];
    } else switch (mode) {
      case -1: channel.v = new Uint8Array(0); break;
      case 0: channel.v = new Uint8Array(0); break;
      case 1: channel.v = new Uint8Array(Song.DEFAULT_DRUM_CONFIG); break;
      case 2: channel.v = new Uint8Array(Song.DEFAULT_WAVE_CONFIG); break;
      case 3: channel.v = new Uint8Array(Song.DEFAULT_FM_CONFIG); break;
      case 4: channel.v = new Uint8Array(Song.DEFAULT_SUB_CONFIG); break;
      default: channel.v = new Uint8Array(0); break;
    }
    return true;
  }
  
  defineChannel(chid) {
    if ((chid < 0) || (chid >= 0x10)) return null;
    if (this.channels[chid]) return null;
    const channel = {
      chid,
      trim: 0x80,
      mode: -1,
      pid: 0,
    };
    this.channels[chid] = channel;
    return channel;
  }
  
  getDuration() {
    let lastEventTime = 0;
    for (const event of this.events) {
      if (event.time > lastEventTime) lastEventTime = event.time;
    }
    return lastEventTime / 1000;
  }
  
  /* Setup, private.
   *******************************************************************************/
  
  _init() {
    this.channels = []; // Indexed by chid. null | { chid,trim,mode?,pid?,name?... }
    this.events = []; // { id,time,chid,mopcode?,eopcode?,noteid?,velocity?,dur?,k?,v?,pid?,wheel?,trackid? }
    this.format = 1; // MIDI only
    this.trackCount = 0; // ''
    this.division = 96; // ''
    this.nextEventId = 1;
    this.format = ""; // "", "egs", "mid"
  }
  
  _copy(src) {
    this.channels = src.channels.map(c => ({...c}));
    this.events = src.events.map(e => {
      if (e.v instanceof Uint8Array) return { ...e, v: new Uint8Array(e.v) };
      return { ...e };
    });
  }
  
  _decode(src) {
    if (!src || !src.length) return this._init();
    if (src.length >= 4) {
      if ((src[0] === 0x00) && (src[1] === 0x45) && (src[2] === 0x47) && (src[3] === 0x53)) return this.decodeEgs(src);
      if ((src[0] === 0x4d) && (src[1] === 0x54) && (src[2] === 0x68) && (src[3] === 0x64)) return this.decodeMidi(src);
    }
    throw new Error(`Failed to decode song as EGS or MIDI`);
  }
  
  /* EGS.
   *******************************************************************/
   
  encodeEgs() {
  
    const dst = new Encoder();
    dst.raw("\0EGS");
    
    for (const channel of this.channels) {
      if (!channel) continue;
      dst.u8(channel.chid);
      dst.u8(channel.trim);
      dst.u8(channel.mode);
      if (channel.v) {
        dst.u24be(channel.v.length);
        dst.raw(channel.v);
      } else {
        dst.u24be(0);
      }
    }
    dst.u8(0xff);
    
    let time = 0;
    for (const event of this.egsEvents()) {
    
      let delay = event.time - time;
      while (delay >= 4096) {
        dst.u8(0x7f);
        delay -= 4096;
      }
      if (delay >= 64) {
        dst.u8(0x40 | ((delay >> 6) - 1));
        delay &= 63;
      }
      if (delay > 0) {
        dst.u8(delay);
      }
      time = event.time;
      
      // There can and should be an End of Track event whose only purpose is to mark the delay.
      if (!event.eopcode) continue;
      
      switch (event.eopcode) {
        case 0x80: {
            dst.u8(0x80 | event.chid);
            dst.u8((event.noteid << 1) | (event.velocity >> 6));
            dst.u8((event.velocity << 2) | (event.dur >> 4));
          } break;
        case 0x90: {
            dst.u8(0x90 | event.chid);
            dst.u8((event.noteid << 1) | (event.velocity >> 6));
            dst.u8(((event.velocity << 2) & 0xe0) | ((event.dur >> 6) - 1));
          } break;
        case 0xa0: {
            dst.u8(0xa0 | event.chid);
            dst.u8((event.noteid << 1) | (event.velocity >> 6));
            dst.u8(((event.velocity << 2) & 0xe0) | ((event.dur >> 9) - 1));
          } break;
        // All "future" events have the full encoded event in (v).
        default: dst.raw(event.v); break;
      }
    }
    
    return dst.finish();
  }
  
  /* Phrase all our events with `eopcode`, and sort by time.
   * Does not change (this.events).
   * For Note events, (dur) will still be in ms but we'll ensure agreement between (dur) and (eopcode).
   */
  egsEvents() {
    const dst = [];
    const held = [];
    for (const event of this.events) {
      if (event.eopcode) {
        dst.push(event);
        continue;
      }
      switch (event.mopcode) {
        case 0x80: {
            const p = held.findIndex(e => ((e.chid === event.chid) && (e.noteid === event.noteid)));
            if (p >= 0) {
              const noteOn = held[p];
              held.splice(p, 1);
              noteOn.dur = event.time - noteOn.time;
              if (noteOn.dur < 64) {
                noteOn.eopcode = 0x80;
              } else if (noteOn.dur <= 2048) {
                noteOn.eopcode = 0x90;
              } else {
                noteOn.eopcode = 0xa0;
                if (noteOn.dur > 16384) noteOn.dur = 16384;
              }
            }
          } break;
        case 0x90: {
            const nevent = {
              id: event.id,
              time: event.time,
              opcode: 0x80,
              chid: event.chid,
              noteid: event.noteid,
              velocity: event.velocity,
              dur: 0,
            };
            dst.push(nevent);
            held.push(nevent);
          } break;
        case 0xff: switch (event.k) {
            case 0x2f: dst.push(event); break;
          } break;
      }
    }
    dst.sort((a, b) => a.time - b.time);
    return dst;
  }
  
  decodeEgs(src) {
    this._init();
    if ((src.length < 4) || (src[0] !== 0x00) || (src[1] !== 0x45) || (src[2] !== 0x47) || (src[3] !== 0x53)) throw new Error(`Invalid EGS`);
    this.format = "egs";
    
    // Measure headers and pass them out for processing.
    let srcp = 4;
    const hdrp = srcp;
    while (srcp < src.length) {
      const chid = src[srcp];
      if (chid === 0xff) break;
      srcp += 3; // skip chid, trim, and mode
      const paylen = (src[srcp] << 16) | (src[srcp+1] << 8) | src[srcp+2];
      srcp += 3;
      if (srcp > src.length - paylen) throw new Error(`Invalid EGS`);
      srcp += paylen;
    }
    this.buildChannelsFromEgsHeader(src.slice(hdrp, srcp));
    if ((srcp < src.length) && (src[srcp] === 0xff)) srcp++;
    
    // Events...
    let time = 0;
    while (srcp < src.length) {
      const lead = src[srcp++];
      if (!lead) break;
      
      if (!(lead & 0x80)) {
        if (lead & 0x40) time += ((lead & 0x3f) + 1) * 64;
        else time += lead;
        continue;
      }
      
      switch (lead & 0xf0) {
        
        case 0x80:
        case 0x90:
        case 0xa0: {
            if (srcp > src.length - 2) throw new Error(`Invalid EGS`);
            const a = src[srcp++];
            const b = src[srcp++];
            const chid = lead & 0x0f;
            const noteid = a >> 1;
            let velocity = ((a & 1) << 6) | (b >> 2);
            let dur;
            switch (lead & 0xf0) {
              case 0x80: dur = (b & 0x03) * 16; break;
              case 0x90: velocity &= 0x78; velocity |= velocity >> 4; dur = ((b & 0x1f) + 1) * 64; break;
              case 0xa0: velocity &= 0x78; velocity |= velocity >> 4; dur = ((b & 0x1f) + 1) * 512; break;
            }
            this.events.push({ id: this.nextEventId++, time, chid, eopcode: lead & 0xf0, noteid, velocity, dur });
            if (!this.channels[chid]) { // Create a channel header if there are notes but the header was omitted.
              while (this.channels.length <= chid) this.channels.push(null);
              this.channels[chid] = { chid };
            }
          } break;
        
        // Reserved events which do have a preordained length.
        case 0xb0: this.events.push({ id: this.nextEventId++, time, eopcode: 0xb0, v: [lead] }); break;
        case 0xc0: this.events.push({ id: this.nextEventId++, time, eopcode: 0xc0, v: src.slice(srcp-1, srcp+1) }); srcp += 1; break;
        case 0xd0: this.events.push({ id: this.nextEventId++, time, eopcode: 0xd0, v: src.slice(srcp-1, srcp+2) }); srcp += 2; break;
        case 0xe0: this.events.push({ id: this.nextEventId++, time, eopcode: 0xe0, v: src.slice(srcp-1, srcp+2) }); srcp += 2; break;
        case 0xf0: this.events.push({ id: this.nextEventId++, time, eopcode: 0xf0, v: src.slice(srcp-1, srcp+2+src[srcp+1]) }); srcp += 2 + src[srcp+1]; break;
        
        default: throw new Error(`Invalid EGS`);
      }
    }
    
    /* It's very likely that the last event was a delay, but our model only captures delays as part of the subsequent event.
     * So emit a dummy End of Track event to mark the end, like MIDI files do.
     */
    this.events.push({ id: this.nextEventId++, time, mopcode: 0xff, k: 0x2f, v: [] });
  }
  
  // Provide just the headers: No EGS Signature or Events Introducer.
  buildChannelsFromEgsHeader(src) {
    for (let srcp=0; srcp<src.length; ) {
      const chid = src[srcp++];
      if (chid === 0xff) break;
      const trim = src[srcp++] || 0;
      const mode = src[srcp++] || 0;
      const paylen = (src[srcp] << 16) | (src[srcp+1] << 8) | src[srcp+2];
      srcp += 3;
      if (srcp > src.length - paylen) throw new Error(`Invalid EGS headers`);
      const v = new Uint8Array(src.buffer, src.byteOffset + srcp, paylen);
      while (this.channels.length <= chid) this.channels.push(null);
      this.channels[chid] = { chid, trim, mode, v };
      srcp += paylen;
    }
  }
  
  /* MIDI.
   **********************************************************************/
   
  encodeMidi() {
    const dst = new Encoder();
    
    dst.raw("MThd");
    dst.u32be(6);
    dst.u16be(this.format);
    dst.u16be(this.trackCount);
    dst.u16be(this.division);
    
    const track = this.midiTracks();
    for (const track of this.midiTracks()) {
      dst.raw("MTrk");
      dst.intbelen(4, () => {
        let time = 0;
        let status = 0;
        for (const event of track) {
          dst.vlq(event.time - time);
          time = event.time;
          switch (event.mopcode) {
          
            case 0x80: {
                if (status === (0x90 | event.chid)) {
                  dst.u8(event.noteid);
                  dst.u8(0x00);
                } else {
                  dst.u8(0x80 | event.chid);
                  dst.u8(event.noteid);
                  dst.u8(event.velocity);
                  status = 0x80 | event.chid;
                }
              } break;
              
            case 0x90: {
                const st = 0x90 | event.chid;
                if (status !== st) {
                  dst.u8(st);
                  status = st;
                }
                dst.u8(event.noteid);
                dst.u8(event.velocity);
              } break;
              
            case 0xa0: {
                dst.u8(status = (0xa0 | event.chid));
                dst.u8(event.noteid);
                dst.u8(event.velocity);
              } break;
              
            case 0xb0: {
                dst.u8(status = (0xb0 | event.chid));
                dst.u8(event.k);
                dst.u8(event.v);
              } break;
              
            case 0xc0: {
                dst.u8(status = (0xc0 | event.chid));
                dst.u8(event.pid);
              } break;
              
            case 0xd0: {
                dst.u8(status = (0xd0 | event.chid));
                dst.u8(event.velocity);
              } break;
              
            case 0xe0: {
                dst.u8(status = (0xe0 | event.chid));
                dst.u8(event.wheel & 0x7f);
                dst.u8((event.wheel >> 7) & 0x7f);
              } break;
              
            case 0xf0:
            case 0xf7:
            case 0xff: {
                status = 0;
                dst.u8(event.mopcode);
                if (event.mopcode === 0xff) {
                  dst.u8(event.k);
                }
                dst.vlqlen(event.v);
              } break;
            
            default: throw new Error(`Illegal MIDI opcode ${event.mopcode}`);
          }
        }
      });
    }
    
    return dst.finish();
  }
  
  /* Return a copy of (this.events) sorted into tracks, with time in ticks.
   * So array of array of event, and all events will have a valid (mopcode).
   * (this.events) is not modified.
   */
  midiTracks() {
  
    /* Bucket events by track and split EGS note events into NoteOn/NoteOff.
     * Drop EGS Future and Header events.
     */
    const tracks = [];
    let usperqnote = 500000;
    for (const event of this.events) {
      const trackid = event.trackid || 0;
      while (trackid >= tracks.length) tracks.push([]);
      const track = tracks[trackid];
      if (event.mopcode) {
        // Drop Volume, Program, and EGS Header events -- we're going to synthesize these.
        if ((event.mopcode === 0xff) && (event.k === 0x7f)) continue;
        if ((event.mopcode === 0xb0) && (event.k === 0x07)) continue;
        if (event.mopcode === 0xc0) continue;
        // A bit controversial perhaps, but also drop Channel Prefix and Instrument Name. (A fair concern, Prefix might have been there for other purposes...)
        if ((event.mopcode === 0xff) && (event.k === 0x04)) continue;
        if ((event.mopcode === 0xff) && (event.k === 0x20)) continue;
        // Capture tempo. It's only allowed at time zero. If there's a tempo event anywhere else we go weird.
        if ((event.mopcode === 0xff) && (event.k === 0x51) && (event.v?.length === 3)) {
          usperqnote = (event.v[0] << 16) | (event.v[1] << 8) | event.v[2];
        }
        track.push({ ...event });
        continue;
      }
      switch (event.eopcode) {
        case 0x80:
        case 0x90:
        case 0xa0: {
            const base = { chid: event.chid, noteid: event.noteid, velocity: event.velocity };
            track.push({ ...base, mopcode: 0x90, time: event.time });
            track.push({ ...base, mopcode: 0x80, time: event.time + event.dur });
          } break;
      }
    }
    
    /* Generate the EGS header from (this.channels), ignoring whatever's in (this.events).
     */
    if (!tracks[0]) tracks[0] = [];
    tracks[0].splice(0, 0, {
      time: 0,
      trackid: 0,
      chid: 0xff,
      mopcode: 0xff,
      k: 0x7f,
      v: this.synthesizeEgsHeader(),
    });
    
    /* If there are nondefault volume or pid values, add them at time zero.
     * These usually are ignored by our compiler, but they are used with the fake "GM" voice mode, and third-party tools will use them.
     * Ditto for Instrument Name.
     */
    for (const channel of this.channels) {
      if (!channel) continue;
      if (channel.name) {
        tracks[0].push({
          time: 0,
          trackid: 0,
          chid: channel.chid,
          mopcode: 0xff,
          k: 0x20,
          v: new Uint8Array([channel.chid]),
        });
        tracks[0].push({
          time: 0,
          trackid: 0,
          chid: channel.chid,
          mopcode: 0xff,
          k: 0x04,
          v: new TextEncoder("utf-8").encode(channel.name),
        });
      }
      if (channel.pid) {
        tracks[0].push({
          time: 0,
          trackid: 0,
          chid: channel.chid,
          mopcode: 0xc0,
          pid: channel.pid,
        });
      }
      if (channel.trim !== 0x80) {
        tracks[0].push({
          time: 0,
          trackid: 0,
          chid: channel.chid,
          mopcode: 0xb0,
          k: 0x07,
          v: channel.trim >> 1,
        });
      }
    }
    
    /* Rephrase event times as ticks.
     */
    const msperqnote = usperqnote / 1000;
    const ticksperms = this.division / msperqnote;
    for (const track of tracks) {
      for (const event of track) {
        event.time = Math.max(0, Math.round(event.time * ticksperms));
      }
    }
    
    /* Sort each track by time.
     */
    for (let i=tracks.length; i-->0; ) {
      tracks[i].sort((a, b) => a.time - b.time);
    }
    
    return tracks;
  }
  
  synthesizeEgsHeader() {
    const dst = new Encoder();
    for (const channel of this.channels) {
      if (!channel) continue;
      if (channel.mode < 0) continue; // Unencodable mode is a signal to let the compiler default this channel per Program Change.
      dst.u8(channel.chid);
      dst.u8(channel.trim);
      dst.u8(channel.mode);
      if (channel.v) {
        dst.u24be(channel.v.length);
        dst.raw(channel.v);
      } else {
        dst.u24be(0);
      }
    }
    return dst.finish();
  }
  
  decodeMidi(src) {
    this._init();
    this.format = "mid";
    
    /* First pass, collect events with (time) in ticks, and not sorted.
     */
    let srcp=0, trackid=0;
    for (;;) {
      if (srcp >= src.length) break;
      if (srcp > src.length - 8) throw new Error(`Unexpected EOF in MIDI file`);
      const chunkid = (src[srcp] << 24) | (src[srcp+1] << 16) | (src[srcp+2] << 8) | src[srcp+3]; srcp += 4;
      const chunklen = (src[srcp] << 24) | (src[srcp+1] << 16) | (src[srcp+2] << 8) | src[srcp+3]; srcp += 4;
      if ((chunklen < 0) || (srcp > src.length - chunklen)) throw new Error(`Unexpected EOF in MIDI file`);
      const chunk = new Uint8Array(src.buffer, src.byteOffset + srcp, chunklen);
      srcp += chunklen;
      switch (chunkid) {
        case 0x4d546864: { // MThd
            if (chunklen < 6) throw new Error(`Unexpected length ${chunklen} for MThd in MIDI file`);
            this.format = (chunk[0] << 8) | chunk[1];
            this.trackCount = (chunk[2] << 8) | chunk[3];
            this.division = (chunk[4] << 8) | chunk[5];
          } break;
        case 0x4d54726b: { // MTrk
            this.decodeMTrk(chunk, trackid);
            trackid++;
          } break;
      }
    }
    if ((this.division < 1) || (this.division >= 0x8000)) throw new Error(`Invalid MIDI division ${this.division}`);
    
    /* Find the first Set Tempo event and turn all timestamps from ticks into milliseconds.
     * In theory we could apply time progressively instead and allow intermediate Set Tempo.
     * But that would be more complicated, and our spec says not to.
     */
    let usperqnote = 500000;
    const tempoEvent = this.events.find(e => ((e.mopcode === 0xff) && (e.k === 0x51)));
    if (tempoEvent && tempoEvent.v && (tempoEvent.v.length >= 3)) {
      usperqnote = (tempoEvent.v[0] << 16) | (tempoEvent.v[1] << 8) | tempoEvent.v[2];
    }
    const mspertick = usperqnote / (this.division * 1000);
    for (const event of this.events) event.time = Math.round(event.time * mspertick);
    
    /* Sort events by time.
     * Modern browsers guarantee Array.sort() to be stable and we depend on it.
     * If this runs on an older browser, order of events at the same tick could be lost.
     */
    this.events.sort((a, b) => a.time - b.time);
    
    /* If we have an EGS event, build channels from that alone.
     * Otherwise, build them ad hoc from MIDI events.
     */
    const egsEvent = this.events.find(e => ((e.mopcode === 0xff) && (e.k === 0x7f) && (e.time === 0)));
    if (egsEvent) {
      this.buildChannelsFromEgsHeader(egsEvent.v);
      this.augmentChannelsWithMidiEvents(this.events);
    } else {
      this.buildChannelsFromMidiEvents(this.events);
    }
  }
  
  buildChannelsFromMidiEvents(events) {
    this.channels = [];
    for (const event of events) {
      if (event.chid >= 0x10) continue;
      while (event.chid >= this.channels.length) this.channels.push(null);
      if (!this.channels[event.chid]) {
        this.channels[event.chid] = { chid: event.chid, mode: -1 };
      }
      const channel = this.channels[event.chid];
      switch (event.mopcode) {
        case 0xb0: switch (event.k) {
            case 0x07: channel.trim = (event.v << 1) | ((event.v & 0x20) ? 1 : 0); break;
          } break;
        case 0xc0: channel.pid = event.pid; break;
        case 0xff: switch (event.k) {
            case 0x04: channel.name = new TextDecoder("utf8").decode(event.v); break;
          } break;
      }
    }
  }
  
  /* EGS header was present and it's the authority.
   * But there's also commentary in the MIDI events that we want to capture for the editor's use.
   */
  augmentChannelsWithMidiEvents(events) {
    for (const event of events) {
      if (event.time > 0) return;
      if (event.chid >= 0x10) continue;
      while (event.chid >= this.channels.length) this.channels.push(null);
      let channel = this.channels[event.chid];
      if (!channel) {
        channel = { chid: event.chid, mode: -1 };
        this.channels[event.chid] = channel;
      }
      switch (event.mopcode) {
        case 0xb0: switch (event.k) {
            case 0x07: if (!channel.hasOwnProperty("trim")) channel.trim = (event.v << 1) | ((event.v & 0x20) ? 1 : 0); break;
          } break;
        case 0xc0: channel.pid = event.pid; break;
        case 0xff: switch (event.k) {
            case 0x04: channel.name = new TextDecoder("utf8").decode(event.v); break;
          } break;
      }
    }
  }
  
  decodeMTrk(src, trackid) {
    let srcp=0, status=0, time=0, chpfx=0xff;
    while (srcp < src.length) {
    
      let delay;
      if (!(src[srcp] & 0x80)) { delay = src[srcp]; srcp += 1; }
      else if (!(src[srcp+1] & 0x80)) { delay = ((src[srcp] & 0x7f) << 7) | src[srcp+1]; srcp += 2; }
      else if (!(src[srcp+2] & 0x80)) { delay = ((src[srcp] & 0x7f) << 14) | ((src[srcp+1] & 0x7f) << 7) | src[srcp+2]; srcp += 3; }
      else if (!(src[srcp+3] & 0x80)) { delay = ((src[srcp] & 0x7f) << 21) | ((src[srcp+1] & 0x7f) << 14) | ((src[srcp+2] & 0x7f) << 7) | src[srcp+3]; srcp += 4; }
      else throw new Error(`Invalid VLQ`);
      time += delay;
    
      let lead;
      if (src[srcp] & 0x80) lead = src[srcp++];
      else if (status) lead = status;
      else throw new Error(`Unexpected leading byte ${src[srcp]} for MIDI event`);
      status = lead;
      switch (lead & 0xf0) {
      
        case 0x80: {
            if (srcp > src.length - 2) throw new Error(`Unexpected EOF in MIDI file`);
            const noteid = src[srcp++];
            const velocity = src[srcp++];
            if ((noteid & 0x80) || (velocity & 0x80)) throw new Error(`Illegal data byte ${noteid} or ${velocity}`);
            this.events.push({ id: this.nextEventId++, trackid, time, chid: lead & 0x0f, mopcode: 0x80, noteid, velocity });
          } break;
          
        case 0x90: {
            if (srcp > src.length - 2) throw new Error(`Unexpected EOF in MIDI file`);
            const noteid = src[srcp++];
            const velocity = src[srcp++];
            if ((noteid & 0x80) || (velocity & 0x80)) throw new Error(`Illegal data byte ${noteid} or ${velocity}`);
            if (!velocity) {
              this.events.push({ id: this.nextEventId++, trackid, time, chid: lead & 0x0f, mopcode: 0x80, noteid, velocity: 0x40 });
            } else {
              this.events.push({ id: this.nextEventId++, trackid, time, chid: lead & 0x0f, mopcode: 0x90, noteid, velocity });
            }
          } break;
          
        case 0xa0: {
            if (srcp > src.length - 2) throw new Error(`Unexpected EOF in MIDI file`);
            const noteid = src[srcp++];
            const velocity = src[srcp++];
            if ((noteid & 0x80) || (velocity & 0x80)) throw new Error(`Illegal data byte ${noteid} or ${velocity}`);
            this.events.push({ id: this.nextEventId++, trackid, time, chid: lead & 0x0f, mopcode: 0xa0, noteid, velocity });
          } break;
          
        case 0xb0: {
            if (srcp > src.length - 2) throw new Error(`Unexpected EOF in MIDI file`);
            const k = src[srcp++];
            const v = src[srcp++];
            if ((k & 0x80) || (v & 0x80)) throw new Error(`Illegal data byte ${k} or ${v}`);
            this.events.push({ id: this.nextEventId++, trackid, time, chid: lead & 0x0f, mopcode: 0xb0, k, v });
          } break;
          
        case 0xc0: {
            if (srcp > src.length - 1) throw new Error(`Unexpected EOF in MIDI file`);
            const pid = src[srcp++];
            if (pid & 0x80) throw new Error(`Illegal data byte ${pid}`);
            this.events.push({ id: this.nextEventId++, trackid, time, chid: lead & 0x0f, mopcode: 0xc0, pid });
          } break;
          
        case 0xd0: {
            if (srcp > src.length - 1) throw new Error(`Unexpected EOF in MIDI file`);
            const velocity = src[srcp++];
            if (velocity & 0x80) throw new Error(`Illegal data byte ${velocity}`);
            this.events.push({ id: this.nextEventId++, trackid, time, chid: lead & 0x0f, mopcode: 0xd0, velocity });
          } break;
          
        case 0xe0: {
            if (srcp > src.length - 2) throw new Error(`Unexpected EOF in MIDI file`);
            const lo = src[srcp++];
            const hi = src[srcp++];
            if ((lo & 0x80) || (hi & 0x80)) throw new Error(`Illegal data byte ${lo} or ${hi}`);
            this.events.push({ id: this.nextEventId++, trackid, time, chid: lead & 0x0f, mopcode: 0xe0, wheel: lo | (hi << 7) });
          } break;
          
        case 0xf0: {
            status = 0;
            let k = 0;
            if (lead === 0xff) {
              if (srcp > src.length - 2) throw new Error(`Unexpected EOF in MIDI file`);
              k = src[srcp++];
            } else if ((lead !== 0xf0) && (lead !== 0xf7)) {
              throw new Error(`Unexpected MIDI leading byte ${lead}`);
            }
            let paylen;
            if (!(src[srcp] & 0x80)) { paylen = src[srcp]; srcp += 1; }
            else if (!(src[srcp+1] & 0x80)) { paylen = ((src[srcp] & 0x7f) << 7) | src[srcp+1]; srcp += 2; }
            else if (!(src[srcp+2] & 0x80)) { paylen = ((src[srcp] & 0x7f) << 14) | ((src[srcp+1] & 0x7f) << 7) | src[srcp+2]; srcp += 3; }
            else if (!(src[srcp+3] & 0x80)) { paylen = ((src[srcp] & 0x7f) << 21) | ((src[srcp+1] & 0x7f) << 14) | ((src[srcp+2] & 0x7f) << 7) | src[srcp+3]; srcp += 4; }
            else throw new Error(`Invalid VLQ`);
            if (srcp > src.length - paylen) throw new Error(`Unexpected EOF in MIDI file`);
            const v = src.slice(srcp, srcp + paylen);
            srcp += paylen;
            if ((k === 0x20) && (paylen === 1)) chpfx = v[0];
            this.events.push({ id: this.nextEventId++, trackid, time, chid: chpfx, mopcode: lead, k, v });
          } break;
          
        default: throw new Error(`MIDI lead ${lead}`);
      }
    }
  }
  
  /* General odds and ends.
   **********************************************************************************************/
   
  static reprNote(noteid) {
    if (typeof(noteid) !== "number") return "";
    if ((noteid < 0) || (noteid >= 0x80)) return noteid.toString();
    const octave = Math.floor(noteid / 12) - 1;
    switch (noteid % 12) { // Notes within each octave run 'c'..'b'. Not 'a'..'g'. Bloody musicians.
      case 0: return "c" + octave;
      case 1: return "cs" + octave;
      case 2: return "d" + octave;
      case 3: return "ds" + octave;
      case 4: return "e" + octave;
      case 5: return "f" + octave;
      case 6: return "fs" + octave;
      case 7: return "g" + octave;
      case 8: return "gs" + octave;
      case 9: return "a" + octave;
      case 10: return "as" + octave;
      case 11: return "b" + octave;
    }
    return noteid.toString();
  }
}

Song.GM_PROGRAM_NAMES = [
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

Song.GM_DRUM_NAMES = [
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

Song.CONTROL_KEYS = [
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

Song.META_TYPES = [
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
  /* 78 */ "", "", "", "", "", "", "", "Egg Channel Header",
];

Song.DEFAULT_DRUM_CONFIG = [
  // Zero or more of: u8 noteid, u8 trimlo, u8 trimhi, u16 len, ... file.
  // There will be other more interesting helpers to compose defaults. I think the default-default should be empty.
];

Song.DEFAULT_WAVE_CONFIG = [
  // ... levelenv, u8 shape, u8 harmonics count, u16... harmonics, ... pitchenv; and ok to terminate between fields.
  0x06, // levelenv flags: Velocity, Sustain
    0x01, // Sustain index.
    0x03, // Point count.
      0x18,0x60,0x00, 0x10,0xff,0xff,
      0x28,0x30,0x00, 0x20,0x40,0x00,
      0x81,0x00,0x00,0x00, 0x82,0x00,0x00,0x00,
  0x01, // Shape = sine
];

Song.DEFAULT_FM_CONFIG = [
  // ... levelenv, u8.8 rate, u8.8 range, ... pitchenv, ... rangeenv; and ok to terminate between fields.
  0x06, // levelenv flags: Velocity, Sustain
    0x01, // Sustain index.
    0x03, // Point count.
      0x18,0x60,0x00, 0x10,0xff,0xff,
      0x28,0x30,0x00, 0x20,0x40,0x00,
      0x81,0x00,0x00,0x00, 0x82,0x00,0x00,0x00,
  0x01,0x00, // rate
  0x01,0x00, // range
];

Song.DEFAULT_SUB_CONFIG = [
  // ... levelenv, u16 width
  0x06, // levelenv flags: Velocity, Sustain
    0x01, // Sustain index.
    0x03, // Point count.
      0x18,0x60,0x00, 0x10,0xff,0xff,
      0x28,0x30,0x00, 0x20,0x40,0x00,
      0x81,0x00,0x00,0x00, 0x82,0x00,0x00,0x00,
  0x00,0x80, // width, hz
];
