/* MsfFile.js
 */
 
export class MsfFile {
  constructor(src) {
    if (!src) {
      this._init();
    } else if (src instanceof MsfFile) {
      this._copy(src);
    } else if (typeof(src) === "string") {
      this._decode(src);
    } else if ((src instanceof Uint8Array) || (src instanceof ArrayBuffer)) {
      this._decode(new TextDecoder("utf8").decode(src));
    } else {
      throw new Error("Inappropriate input to MsfFile");
    }
  }
  
  _init() {
    this.sounds = []; // Order not specified.
  }
  
  _copy(src) {
    this.sounds = src.sounds.map(s => s.copy());
  }
  
  _decode(src) {
    this.sounds = [];
    let sound = null;
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      let line = src.substring(srcp, nlp).trim();
      if (line.startsWith("#")) line = "";
      srcp = nlp + 1;
      if (!line) continue;
      const words = line.split(/\s+/g);
      try {
        if (words[0] === "instrument") {
          sound = new MsfSound("instrument", words[1], words.slice(2).join(" "));
          this.sounds.push(sound);
        } else if (words[0] === "sound") {
          sound = new MsfSound("sound", words[1], words.slice(2).join(" "));
          this.sounds.push(sound);
        } else if (sound) {
          sound.addLine(words);
        } else {
          throw new Error("Expected 'instrument' or 'sound'");
        }
      } catch (e) {
        e.message = `MsfFile:${lineno}: ` + e.message;
        throw e;
      }
    }
  }
  
  encode() {
    let dst = "";
    for (const sound of this.sounds) {
      dst += sound.encode();
      dst += "\n";
    }
    return dst;
  }
}

export class MsfSound {
  constructor(type, idsrc, name) {
    this.type = type;
    this.name = name;
    const ids = idsrc.split(":").map(v => +v);
    this.id = 0;
    for (let id of ids) {
      this.id <<= 7;
      this.id |= id;
    }
    this.channels = []; // Sparse, indexed by chid. Each is an array of arrays of tokens.
    this.events = []; // Arrays of tokens.
    this.reading = null;
    if (this.type === "instrument") {
      this.reading = [];
      this.channels[0] = this.reading;
    }
  }
  
  copy() {
    const dst = new MsfSound(this.type, this.id.toString(), this.name);
    dst.channels = this.channels.map(c => (c && [...c]));
    dst.events = this.events.map(e => [...e]);
    dst.reading = null;
    return dst;
  }
  
  addLine(words) {
    if (!words.length) return;
    if (words[0] === "channel") {
      const chid = +words[1];
      if (isNaN(chid) || (chid < 0) || (chid >= 16)) throw new Error(`Invalid channel id ${words[1]}`);
      this.reading = [];
      this.channels[chid] = this.reading;
    } else if (words[0] === "events") {
      this.reading = "events";
    } else if (!this.reading) {
      throw new Error(`Expected 'channel CHID' or 'events', found ${JSON.stringify(words[0])}`);
    } else if (this.reading === "events") {
      this.events.push(words);
    } else {
      this.reading.push(words);
    }
  }
  
  encode() {
    let dst = `${this.type} ${this.id} ${this.name}\n`;
    if (this.type === "instrument") {
      for (const field of this.channels[0]) {
        dst += "  " + field.join(" ") + "\n";
      }
    } else {
      for (let chid=0; chid<this.channels.length; chid++) {
        const channel = this.channels[chid];
        if (!channel) continue;
        dst += `channel ${chid}\n`;
        for (const field of channel) {
          dst += "  " + field.join(" ") + "\n";
        }
      }
      if (this.events.length) {
        dst += "events\n";
        for (const event of this.events) {
          dst += "  " + event.join(" ") + "\n";
        }
      }
    }
    return dst;
  }
}
