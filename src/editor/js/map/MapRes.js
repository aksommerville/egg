/* MapRes.js
 * Model of a map resource. Would have been called "Map" but that's already a thing in JS.
 */
 
import { CommandListEditor } from "./CommandListEditor.js";
import { Encoder } from "../Encoder.js";
 
export class MapRes {
  constructor(src) {
    this._init();
    if (!src) return;
    if (src instanceof MapRes) this._copy(src);
    else if (src.path && src.serial) this._initRes(src);
    else if (src instanceof Uint8Array) this._initSerial(new TextDecoder("utf8").decode(src));
    else if (typeof(src) === "string") this._initSerial(src);
    else throw new Error(`inappropriate input to MapRes ctor`);
  }
  
  _init() {
    this.path = "";
    this.rid = 0;
    this.w = 0;
    this.h = 0;
    this.v = []; // Uint8Array, but in the empty case whatever.
    this.commands = []; // MapCommand, defined below.
  }
  
  _initRes(res) {
    this.path = res.path;
    this.rid = res.rid;
    this._initSerial(new TextDecoder("utf8").decode(res.serial));
  }
  
  _copy(src) {
    // Leave (path) and (rid) unset. Same with the command IDs, they should be unique for the new one.
    this.w = src.w;
    this.h = src.h;
    this.v = new Uint8Array(this.w * this.h);
    this.v.set(src.v);
    this.commands = src.commands.map(c => new MapCommand(c));
  }
  
  // (src) is a string, one map resource in its text format.
  _initSerial(src) {
    this.w = 0;
    this.h = 0;
    this.v = null;
    this.commands = [];
    const cells = new Encoder();
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      let line = src.substring(srcp, nlp).trim();
      if (line.startsWith("#")) line = ""; // Line comments end the cells image, like blank lines.
      srcp = nlp + 1;
      
      // Done the cells image? Ignore empty lines and create a MapCommand for everything else.
      if (this.v) {
        if (!line) continue;
        this.commands.push(new MapCommand(line));
        continue;
      }
      
      // First empty line ends the cells image.
      if (!line) {
        this.v = new Uint8Array(cells.finish());
        continue;
      }
      
      // Got a row of the cells image. Initialize or validate against (w), then append to (cells).
      if ((line.length & 1) || !line.match(/^[0-9a-fA-F]*$/)) {
        throw new Error(`${this.path}:${lineno}: Expected an even count of hexadecimal digits.`);
      }
      if (this.w) {
        if (line.length !== this.w << 1) {
          throw new Error(`${this.path}:${lineno}: Expected ${this.w << 1} hex digits, found ${line.length}`);
        }
      } else {
        this.w = line.length >> 1;
      }
      for (let p=0; p<line.length; p+=2) {
        cells.u8(parseInt(line.substring(p, p +2), 16));
      }
      this.h++;
    }
    // Possible to end the loop without having finished (no commands, or a completely empty file). No worries, finish it now.
    if (!this.v) this.v = new Uint8Array(cells.finish());
  }
  
  /* Produces a Uint8Array because that's the editor's convention.
   * Our natural format is string, and you can encodeString() to get that instead.
   */
  encode() {
    return new TextEncoder("utf8").encode(this.encodeString());
  }
  
  encodeString() {
    let dst = "";
    for (let y=0, p=0; y<this.h; y++) {
      for (let x=0; x<this.w; p++, x++) {
        dst += "0123456789abcdef"[this.v[p] >> 4];
        dst += "0123456789abcdef"[this.v[p] & 15];
      }
      dst += "\n";
    }
    dst += "\n";
    for (const command of this.commands) dst += command.encode() + "\n";
    return dst;
  }
  
  replaceCommands(src) {
    if (!src) src = "";
    if (src instanceof Uint8Array) src = new TextDecoder("utf8").decode(src);
    this.commands = [];
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      if (!line || (line[0] === "#")) continue;
      this.commands.push(new MapCommand(line));
    }
  }
  
  /* Find the first command with (kw) as its opcode, and return its parameters joined by space.
   * Intended for parameter-type commands like 'image' that should only appear once.
   */
  getFirstCommand(kw) {
    for (const command of this.commands) {
      if (command.tokens[0] !== kw) continue;
      return command.tokens.slice(1).join(" ");
    }
    return "";
  }
  
  /* First command with opcode (kw), array of tokens including the keyword.
   */
  getFirstCommandTokens(kw) {
    for (const command of this.commands) {
      if (command.tokens[0] !== kw) continue;
      return command.tokens;
    }
    return [];
  }
}

export class MapCommand {
  constructor(src) {
    this._init();
    if (!src) return;
    if (src instanceof MapCommand) this._copy(src);
    else if (typeof(src) === "string") this.decode(src);
    else if (src instanceof Array) this.tokens = src.map(t => t.toString());
    else throw new Error(`Inappropriate input to MapCommand ctor`);
  }
  
  _init() {
    if (!MapCommand.nextId) MapCommand.nextId = 1;
    this.mapCommandId = MapCommand.nextId++;
    this.tokens = []; // string
  }
  
  _copy(src) {
    this.tokens = src.tokens.map(t => t);
  }
  
  decode(src) {
    this.tokens = CommandListEditor.splitCommand(src);
  }
  
  encode() {
    return this.tokens.join(" ");
  }
  
  adjustPosition(dx, dy, paramIndex) {
    if (!paramIndex) paramIndex = 0;
    for (let i=0; i<this.tokens.length; i++) {
      const match = this.tokens[i].match(/^@(\d+),(\d+)(,[\d,]*)?$/);
      if (!match) continue;
      if (paramIndex--) continue;
      const x = +match[1];
      const y = +match[2];
      const rem = match[3] || "";
      this.tokens[i] = `@${x + dx},${y + dy}${rem}`;
      return true;
    }
    return false;
  }
}
