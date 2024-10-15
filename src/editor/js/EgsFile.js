/* EgsFile.js
 * We accept text and binary format, and we're cool with sliced bits.
 * If it's binary, it must arrive as Uint8Array.
 */

export class EgsFile {
  constructor(src) {
    this.type = "sound";
    this.id = 0;
    this.name = "";
    this.channels = [];
    this.events = []; // {type:"delay",ms:number} or {type:"note",chid,noteid,velocity,durms} or {type:"wheel,chid,v:-1..1}
    this.nextEventId = 1;
    if (!src) {
    } else if (typeof(src) === "string") {
      this._decodeText(src);
    } else if ((src instanceof Uint8Array) && (src[0] === 0x00) && (src[1] === 0x45) && (src[2] === 0x47) && (src[3] === 0x53)) {
      this._decodeBinary(src);
    } else if ((src instanceof Uint8Array) || (src instanceof ArrayBuffer)) {
      this._decodeText(new TextDecoder("utf8").decode(src));
    } else {
      throw new Error("Unsuitable input for EgsFile");
    }
  }
  
  _decodeBinary(src) {
    let srcp = 4;
    while (srcp <= src.length - 2) {
      const len = (src[srcp] << 8) | src[srcp+1];
      srcp += 2;
      if (!len) break;
      if (srcp > src.length - len) throw new Error(`Unexpected EOF in binary EGS`);
      this.channels.push(EgsChannel.fromBinary(src.slice(srcp, srcp + len)));
      srcp += len;
    }
    while (srcp < src.length) {
      const lead = src[srcp++];
      if (!lead) break;
      const event = {};
      this.events.push(event);
      
      if (!(lead & 0x80)) {
        event.type = "delay";
        event.ms = lead & 0x3f;
        if (lead & 0x40) event.ms = (event.ms + 1) * 64;
        continue;
      }
      
      switch (lead & 0xf0) {
        case 0x80: { // Fire and Forget
            if (srcp > src.length - 2) throw new Error(`Unexpected EOF in binary EGS`);
            const a = src[srcp++];
            const b = src[srcp++];
            event.type = "note";
            event.chid = lead & 0x0f;
            event.noteid = a >> 1;
            event.velocity = ((a & 0x01) << 6) | (b >> 2);
            event.durms = 0;
          } break;
        case 0x90: { // Short Note
            if (srcp > src.length - 2) throw new Error(`Unexpected EOF in binary EGS`);
            const a = src[srcp++];
            const b = src[srcp++];
            event.type = "note";
            event.chid = lead & 0x0f;
            event.noteid = a >> 1;
            event.velocity = ((a & 0x01) << 6) | ((b & 0xe0) >> 2);
            event.velocity |= event.velocity >> 4;
            event.durms = ((b & 0x1f) + 1) * 16;
          } break;
        case 0xa0: { // Long Note
            if (srcp > src.length - 2) throw new Error(`Unexpected EOF in binary EGS`);
            const a = src[srcp++];
            const b = src[srcp++];
            event.type = "note";
            event.chid = lead & 0x0f;
            event.noteid = a >> 1;
            event.velocity = ((a & 0x01) << 6) | ((b & 0xe0) >> 2);
            event.velocity |= event.velocity >> 4;
            event.durms = ((b & 0x1f) + 1) * 128;
          } break;
        case 0xb0: { // Wheel
            if (srcp > src.length - 1) throw new Error(`Unexpected EOF in binary EGS`);
            const a = src[srcp++];
            event.type = "wheel";
            event.chid = lead & 0x0f;
            event.v = (a - 0x80) / 0x80;
          } break;
        default: throw new Error(`Unexpected EGS event opcode ${lead}`);
      }
    }
  }
  
  _decodeText(src) {
    this.type = "";
    let reading = ""; // "", "events", or a channel
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      let line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      if (!line || line.startsWith("#")) continue;
      const words = line.split(/\s+/g);
      
      if (!this.type) {
        this.type = words[0];
        this.id = 0;
        const idsrc = (words[1] || "0").split(":").map(v => +v);
        for (const n of idsrc) {
          this.id <<= 7;
          this.id |= n;
        }
        this.name = words.slice(2).join(" ");
        if (this.type === "instrument") {
          const channel = new EgsChannel(0);
          this.channels.push(channel);
          reading = channel;
        }

      } else if (words[0] === "channel") {
        const chid = +words[1];
        const channel = new EgsChannel(chid);
        this.channels.push(channel);
        reading = channel;
        
      } else if (words[0] === "events") {
        reading = "events";
        
      } else if (reading === "events") {
        this.decodeEvent(words);
        
      } else if (reading) {
        reading.decodeLine(words);
        
      } else {
        throw new Error(`${lineno}: Unexpected EGS command ${JSON.stringify(words[0])}`);
      }
    }
  }
  
  decodeEvent(words) {
    const event = { type: words[0], id: this.nextEventId++ };
    switch (event.type) {
      case "delay": {
          event.ms = +words[1] || 0;
        } break;
      case "note": {
          event.chid = +words[1] || 0;
          event.noteid = this.evalNote(words[2]);
          event.velocity = +words[3] || 0;
          event.durms = +words[4] || 0;
        } break;
      case "wheel": {
          event.chid = +words[1] || 0;
          event.v = +words[2] || 0; // -1..1
        } break;
      default: throw new Error(`EGS: Unexpected event type ${JSON.stringify(event.type)}`);
    }
    this.events.push(event);
  }
  
  // I allowed EGS text to use musical note names like "a#4". what was i thinking...
  evalNote(src) {
    let srcp=0, tone=0;
    switch (src[srcp++].toLowerCase()) {
      case "c": tone = 0; break;
      case "d": tone = 2; break;
      case "e": tone = 4; break;
      case "f": tone = 5; break;
      case "g": tone = 7; break;
      case "a": tone = 9; break;
      case "b": tone = 10; break;
    }
    if ((src[srcp] === 's') || (src[srcp] === '#')) { srcp++; tone++; }
    else if (src[srcp] === 'b') { srcp++; tone--; }
    const octave = +src.slice(srcp) || 0;
    const v = ((octave + 1) * 12 + tone) || 0;
    if (v < 0) return 0;
    if (v > 0x7f) return 0x7f;
    return v;
  }
}

class EgsChannel {
  constructor(chid) {
    this.chid = chid;
    this.pipe = []; // {op:string,v:(0..255)[]}
    // We acquire arrays of 0..255 for singleton fields: master,pan,drums,wheel,sub,shape,harmonics,fm,fmenv,fmlfo,pitchenv,pitchlfo,level
  }
  
  static fromBinary(src) {
    const channel = new EgsChannel(0);
    for (let srcp=0; srcp<src.length; ) {
      const k = src[srcp++];
      if (!k) break;
      const len = src[srcp++] || 0;
      if (srcp > src.length - len) break;
      const v = src.slice(srcp, srcp + len);
      srcp += len;
      switch (k) {
        case 0x00: break; // Explicit dummy.

        case 0x01: channel.master = v; break;
        case 0x02: channel.pan = v; break;
        case 0x03: channel.drums = v; break;
        case 0x04: channel.wheel = v; break;
        case 0x05: channel.sub = v; break;
        case 0x06: channel.shape = v; break;
        case 0x07: channel.harmonics = v; break;
        case 0x08: channel.fm = v; break;
        case 0x09: channel.fmenv = v; break;
        case 0x0a: channel.fmlfo = v; break;
        case 0x0b: channel.pitchenv = v; break;
        case 0x0c: channel.pitchlfo = v; break;
        case 0x0d: channel.level = v; break;
        
        case 0x80: channel.pipe.push({ op: "gain", v }); break;
        case 0x81: channel.pipe.push({ op: "waveshaper", v }); break;
        case 0x82: channel.pipe.push({ op: "delay", v }); break;
        case 0x84: channel.pipe.push({ op: "tremolo", v }); break;
        case 0x85: channel.pipe.push({ op: "lopass", v }); break;
        case 0x86: channel.pipe.push({ op: "hipass", v }); break;
        case 0x87: channel.pipe.push({ op: "bpass", v }); break;
        case 0x88: channel.pipe.push({ op: "notch", v }); break;
      
        // The spec allows unknown keys, but for modelling purposes we have to know what it is.
        default: throw new Error(`Unknown EGS channel header key ${k}`);
      }
    }
    return channel;
  }
  
  decodeLine(words) {
    const v = this.unhexdump(words.slice(1).join(""));
    switch (words[0]) {
    
      case "master": this.master = v; break;
      case "pan": this.pan = v; break;
      case "drums": this.drums = v; break;
      case "wheel": this.wheel = v; break;
      case "sub": this.sub = v; break;
      case "shape": this.shape = v; break;
      case "harmonics": this.harmonics = v; break;
      case "fm": this.fm = v; break;
      case "fmenv": this.fmenv = v; break;
      case "fmlfo": this.fmlfo = v; break;
      case "pitchenv": this.pitchenv = v; break;
      case "pitchlfo": this.pitchlfo = v; break;
      case "level": this.level = v; break;
      
      case "gain": this.pipe.push({ op: "gain", v }); break;
      case "waveshaper": this.pipe.push({ op: "waveshaper", v }); break;
      case "delay": this.pipe.push({ op: "delay", v }); break;
      case "tremolo": this.pipe.push({ op: "tremolo", v }); break;
      case "lopass": this.pipe.push({ op: "lopass", v }); break;
      case "hipass": this.pipe.push({ op: "hipass", v }); break;
      case "bpass": this.pipe.push({ op: "bpass", v }); break;
      case "notch": this.pipe.push({ op: "notch", v }); break;
      
      default: throw new Error(`Unknown EGS channel header command: ${JSON.stringify(words[0])}`);
    }
  }
  
  unhexdump(src) {
    const v = [];
    let hi = -1;
    for (let i=0; i<src.length; i++) {
      let ch = src.charCodeAt(i);
      if (ch <= 0x20) continue;
           if ((ch >= 0x30) && (ch <= 0x39)) ch -= 0x30;
      else if ((ch >= 0x41) && (ch <= 0x46)) ch = ch - 0x46 + 10;
      else if ((ch >= 0x61) && (ch <= 0x66)) ch = ch - 0x66 + 10;
      else throw new Error(`Unexpected byte ${ch} in hex dump`);
      if (hi < 0) {
        hi = ch;
      } else {
        v.push((hi << 4) | ch);
        hi = -1;
      }
    }
    return v;
  }
  
  getMaster() { // => 0..255
    if (this.master?.length >= 1) return this.master[0];
    return 0x80;
  }
  
  getPan() { // => 0..128..255
    if (this.pan?.length >= 1) return this.pan[0];
    return 0x80;
  }
  
  getMode() { // => "drums","sub","fm"
    if (this.drums) return "drums";
    if (this.sub) return "sub";
    return "fm";
  }
}
