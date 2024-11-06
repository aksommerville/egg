/* DrumChannelModal.js
 */
 
import { Dom } from "../Dom.js";
import { Encoder } from "../Encoder.js";
import { AudioService } from "./AudioService.js";
import { Song } from "./Song.js";
import { EditorModal } from "../EditorModal.js";
import { WavEditor } from "./WavEditor.js";
import { SongEditor } from "./SongEditor.js";

export class DrumChannelModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, AudioService, "nonce"];
  }
  constructor(element, dom, audioService, nonce) {
    this.element = element;
    this.dom = dom;
    this.audioService = audioService;
    this.nonce = nonce;
    
    this.channel = null;
    this.song = null;
    this.serials = []; // Uint8Array indexed by noteid
    this.seq = 1;
  }
  
  setup(channel, song) {
    this.channel = channel;
    this.song = song;
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", ["title"], this.channel.name ? `${this.channel.chid}: ${this.channel.name}` : `Channel ${this.channel.chid}`);
    const scroller = this.dom.spawn(this.element, "DIV", ["scroller"]);
    const table = this.dom.spawn(scroller, "TABLE");
    for (const drum of this.decode()) {
      this.serials[drum.noteid] = drum.serial;
      const tr = this.dom.spawn(table, "TR", { "data-noteid": drum.noteid });
      this.populateRow(tr, drum);
    }
    const controls = this.dom.spawn(this.element, "DIV", ["controls"]);
    this.dom.spawn(controls, "INPUT", { type: "button", value: "OK", "on-click": () => this.onSubmit() });
    this.dom.spawn(controls, "INPUT", { type: "button", value: "+Drum", "on-click": () => this.onAddDrum() });
    this.dom.spawn(controls, "INPUT", { type: "button", value: "Match events", "on-click": () => this.onMatchEvents() });
    this.dom.spawn(controls, "INPUT", { type: "button", value: "Sort", "on-click": () => this.onSort() });
  }
  
  populateRow(tr, drum) {
    tr.innerHTML = "";
    const descId = `DrumChannelModal-${this.nonce}-${this.seq++}`;
    this.dom.spawn(tr, "TD",
      this.dom.spawn(null, "INPUT", { type: "button", value: "X", "on-click": () => this.onDeleteDrum(drum.noteid) }),
    );
    this.dom.spawn(tr, "TD",
      this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 127, name: "noteid", value: drum.noteid, "on-input": e => {
        const noteid = +e.target.value;
        this.element.querySelector("#" + descId).innerText = Song.GM_DRUM_NAMES[noteid] || "";
        tr.setAttribute("data-noteid", noteid);
      }})
    );
    this.dom.spawn(tr, "TD",
      this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 255, name: "trimlo", value: drum.trimlo }),
      this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 255, name: "trimhi", value: drum.trimhi }),
    );
    this.dom.spawn(tr, "TD",
      this.dom.spawn(null, "INPUT", { type: "button", value: ">", "on-click": () => this.onPlay(drum.noteid) })
    );
    this.dom.spawn(tr, "TD", ["edit"],
      this.dom.spawn(null, "INPUT", { type: "button", value: this.reprSerial(drum.noteid), "on-click": () => this.onEditDrum(drum.noteid) })
    );
    this.dom.spawn(tr, "TD", { id: descId }, Song.GM_DRUM_NAMES[drum.noteid] || "");
  }
  
  reprSerial(noteid) {
    const serial = this.serials[noteid];
    if (!serial?.length) return "Empty";
    if ((serial[0] === 0x00) && (serial[1] === 0x45) && (serial[2] === 0x47) && (serial[3] === 0x53)) return `EGS, ${serial.length} bytes`;
    if ((serial[0] === 0x52) && (serial[1] === 0x49) && (serial[2] === 0x46) && (serial[3] === 0x46)) return `WAV, ${serial.length} bytes`;
    return `Unknown, ${serial.length} bytes`;
  }
  
  replaceSerialButtonLabel(noteid) {
    const button = this.element.querySelector(`tr[data-noteid='${noteid}'] .edit input`);
    if (!button) return;
    button.value = this.reprSerial(noteid);
  }
  
  noteidForNewDrum() {
    const inuse = [];
    let hi = 0, lo = 128;
    for (const tr of this.element.querySelectorAll("tr[data-noteid]")) {
      const noteid = +tr.getAttribute("data-noteid");
      inuse[noteid] = true;
      if (noteid > hi) hi = noteid;
      if (noteid < lo) lo = noteid;
    }
    // GM range is 35..81.
    // If there's one available just above (hi) and it's in the GM range, use that.
    if ((hi >= 34) && (hi < 81)) return hi + 1;
    // Check the full GM range, first unused wins.
    for (let noteid=35; noteid<=81; noteid++) {
      if (!inuse[noteid]) return noteid;
    }
    // Any noteid anywhere?
    for (let noteid=0; noteid<35; noteid++) {
      if (!inuse[noteid]) return noteid;
    }
    for (let noteid=82; noteid<128; noteid++) {
      if (!inuse[noteid]) return noteid;
    }
    return -1;
  }
  
  onAddDrum() {
    const noteid = this.noteidForNewDrum();
    if (noteid < 0) return;
    const table = this.element.querySelector(".scroller > table");
    const tr = this.dom.spawn(table, "TR", { "data-noteid": noteid });
    this.populateRow(tr, { noteid, trimlo: 128, trimhi: 255, serial: [] });
  }
  
  onMatchEvents() {
    if (!this.channel || !this.song) return;
    // Determine which notes the song uses:
    const inuse = new Set();
    for (const event of this.song.events) {
      if (event.chid !== this.channel.chid) continue;
      if (!this.song.isNoteEvent(event)) continue;
      inuse.add(event.noteid);
    }
    // Remove rows if unused, and note the notes we have.
    const present = new Set();
    for (const tr of this.element.querySelectorAll("tr[data-noteid]")) {
      const noteid = +tr.getAttribute("data-noteid");
      if (!inuse.has(noteid)) {
        tr.remove();
      } else {
        present.add(noteid);
      }
    }
    // Everything in (inuse) but not (present), add a row for it.
    const table = this.element.querySelector(".scroller > table");
    for (let noteid of Array.from(inuse.difference(present))) {
      const tr = this.dom.spawn(table, "TR", { "data-noteid": noteid });
      this.populateRow(tr, { noteid, trimlo: 128, trimhi: 255, serial: [] });
    }
  }
  
  onSort() {
    const table = this.element.querySelector(".scroller > table");
    const rows = Array.from(table.querySelectorAll("tr[data-noteid]")).map(element => ({ element, noteid: +element.getAttribute("data-noteid") }));
    rows.sort((a, b) => a.noteid - b.noteid);
    table.innerHTML = "";
    for (const { element } of rows) table.appendChild(element);
  }
  
  onDeleteDrum(noteid) {
    const tr = this.element.querySelector(`tr[data-noteid='${noteid}']`);
    if (!tr) return;
    tr.remove();
    delete this.serials[noteid];
  }
  
  onPlay(noteid) {
    const serial = this.serials[noteid];
    if (!serial?.length) return;
    if (this.audioService.outputMode === "none") {
      if (this.audioService.serverAvailable) this.audioService.setOutputMode("server");
      else this.audioService.setOutputMode("client");
    }
    this.audioService.play(serial, 0/*position*/, 0/*repeat*/).then(() => {
    }).catch(e => this.dom.modalError(e));
  }
  
  onEditDrum(noteid) {
    let serial = this.serials[noteid];
    if (!serial?.length) serial = DrumChannelModal.generateDefaultSerial(noteid);
    const res = { type: "sound", serial };
    
    // WAV files open WavEditor. Everything else is SongEditor.
    let clazz = SongEditor;
    if (WavEditor.isWavSignature(serial)) clazz = WavEditor;
    
    const modal = this.dom.spawnModal(EditorModal);
    const editor = modal.setupWithResource(clazz, res);
    modal.result.then(result => {
      if (!result) return;
      this.serials[noteid] = result;
      this.replaceSerialButtonLabel(noteid);
    }).catch(e => this.dom.modalError(e));
  }
  
  onSubmit() {
    this.resolve(this.encode());
  }
  
  // Array of {noteid,trimlo,trimhi,serial} from (this.channel.v).
  decode() {
    if (!this.channel?.v) return [];
    const drums = [];
    for (let p=0; p<this.channel.v.length; ) {
      const noteid = this.channel.v[p++];
      const trimlo = this.channel.v[p++];
      const trimhi = this.channel.v[p++];
      const len = (this.channel.v[p] << 8) | this.channel.v[p+1];
      p += 2;
      if (p > this.channel.v.length - len) break;
      const serial = this.channel.v.slice(p, p + len);
      p += len;
      drums.push({ noteid, trimlo, trimhi, serial });
    }
    return drums;
  }
  
  // Return the original channel, with (v) rebuild from UI.
  encode() {
    const dst = new Encoder();
    for (const tr of this.element.querySelectorAll("tr[data-noteid]")) {
      const noteid = +tr.getAttribute("data-noteid");
      const trimlo = +tr.querySelector("input[name='trimlo']").value;
      const trimhi = +tr.querySelector("input[name='trimhi']").value;
      const serial = this.serials[noteid] || new Uint8Array(0);
      if (serial.length > 0xffff) throw new Error(`Drum serial too long (${serial.length}, limit 65535)`);
      dst.u8(noteid);
      dst.u8(trimlo);
      dst.u8(trimhi);
      dst.u16be(serial.length);
      dst.raw(serial);
    }
    const v = dst.finish();
    return { ...this.channel, v };
  }
  
  /* Default drums, when you edit an empty one.
   * It's OK to launch SongEditor with empty input, but not a great user experience.
   * We can convenient things up a bit by generating a simple EGS file to start from.
   ****************************************************************************************/
   
  static generateDefaultSerial(noteid) {
    const song = new Song();
    song.format = "egs";
    const channel = song.defineChannel(0);
    if (!channel) return new Uint8Array(0);
    channel.trim = 0xff;
    
    // TODO Consider the GM drum kit, maybe do something different based on (noteid).
    // The server already has a canned drum kit (see eggdev_compile_song_gm.c). Can we piggyback on that?
    channel.mode = 3; // FM
    channel.v = new Uint8Array([
      // ... levelenv, u8.8 rate, u8.8 range, ... pitchenv, ... rangeenv; and ok to terminate between fields.
      0x00, // levelenv flags
      0x03, // Point count.
        0x10,0xff,0xff,
        0x20,0x40,0x00,
        0x82,0x00,0x00,0x00,
      0x01,0x00, // rate
      0x01,0x00, // range
      0x01, // pitchenv flags: init
      0x80,0x00, // initial pitch bend = 0
      0x01, // point count
        0x82,0x30,0x00,0x00,
      0x01, // rangeenv flags: init
      0xff,0xff, // initial range
      0x01, // point count
        0x82,0x30,0xff,0xff,
    ]);
    song.events.push({
      id: song.nextEventId++,
      time: 0,
      eopcode: 0x80, // 80 90 a0, all equivalent from here.
      noteid: 0x40,
      velocity: 0x7f,
    });
    song.events.push({
      id: song.nextEventId++,
      time: 0x130,
      mopcode: 0xff,
      k: 0x2f,
      v: new Uint8Array(0),
    });
    
    return song.encode();
  }
}
