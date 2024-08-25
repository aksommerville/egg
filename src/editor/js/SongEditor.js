/* SongEditor.js
 * I doubt we'll do a full MIDI sequencer.
 * But certainly must provide UI for the channel headers, and playback.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";
import { MidiFile } from "./MidiFile.js";

export class SongEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    
    this.res = null;
    this.midiFile = null;
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
  }
  
  static checkResource(res) {
    if (res.type === "song") return 2;
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.midiFile = new MidiFile(res.serial);
    this.populateUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    
    const topRow = this.dom.spawn(this.element, "DIV", ["topRow"]);
    this.dom.spawn(topRow, "INPUT", { type: "button", value: ">", "on-click": () => this.onPlay() });
    this.dom.spawn(topRow, "INPUT", { type: "button", value: "||", "on-click": () => this.onPause() });
    this.dom.spawn(topRow, "INPUT", { type: "number", name: "tempo", min: 1, max: 0xffffff, value: "0", "on-input": e => this.onTempoInput(e) });
    this.dom.spawn(topRow, "SPAN", "us/qnote");
    
    const mixer = this.dom.spawn(this.element, "TABLE", ["mixer"], { "on-input": e => this.onMixerInput(e) });
    this.dom.spawn(mixer, "TR", ["mixerHeader"],
      this.dom.spawn(null, "TH", "Chid"),
      this.dom.spawn(null, "TH", "Volume"),
      this.dom.spawn(null, "TH", "Pan"),
      this.dom.spawn(null, "TH", "Sus"),
      this.dom.spawn(null, "TH", "atk0"),
      this.dom.spawn(null, "TH", "sus0"),
      this.dom.spawn(null, "TH", "rls0"),
      this.dom.spawn(null, "TH", "atk1"),
      this.dom.spawn(null, "TH", "sus1"),
      this.dom.spawn(null, "TH", "rls1"),
      this.dom.spawn(null, "TH", "Shape"),
      this.dom.spawn(null, "TH", "Mod"),
      this.dom.spawn(null, "TH", "mod0"),
      this.dom.spawn(null, "TH", "mod1"),
      this.dom.spawn(null, "TH", "fmenv"),
      this.dom.spawn(null, "TH", "drate"),
      this.dom.spawn(null, "TH", "Delay"),
      this.dom.spawn(null, "TH", "Over"),
    );
    for (let chid=0; chid<16; chid++) {
      const channelRow = this.dom.spawn(mixer, "TR", ["channelRow"], { "data-chid": chid });
      this.dom.spawn(channelRow, "TD", ["chid"], chid);
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "INPUT", { type: "range", min: 0, max: 127, name: "volume" }));
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "INPUT", { type: "range", min: 0, max: 127, name: "pan" }));
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "INPUT", { type: "checkbox", name: "sustain" }));
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 15, name: "atktlo" }));
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 15, name: "susvlo" }));
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 15, name: "rlstlo" }));
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 15, name: "atkthi" }));
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 15, name: "susvhi" }));
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 15, name: "rlsthi" }));
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "SELECT", { name: "shape" },
        this.dom.spawn(null, "OPTION", { value: "0" }, "Sine"),
        this.dom.spawn(null, "OPTION", { value: "1" }, "Square"),
        this.dom.spawn(null, "OPTION", { value: "2" }, "Saw"),
        this.dom.spawn(null, "OPTION", { value: "3" }, "Triangle"),
        this.dom.spawn(null, "OPTION", { value: "4" }, "4"),
        this.dom.spawn(null, "OPTION", { value: "5" }, "5"),
        this.dom.spawn(null, "OPTION", { value: "6" }, "6"),
        this.dom.spawn(null, "OPTION", { value: "7" }, "7"),
        this.dom.spawn(null, "OPTION", { value: "8" }, "8"),
        this.dom.spawn(null, "OPTION", { value: "9" }, "9"),
        this.dom.spawn(null, "OPTION", { value: "10" }, "10"),
        this.dom.spawn(null, "OPTION", { value: "11" }, "11"),
        this.dom.spawn(null, "OPTION", { value: "12" }, "12"),
        this.dom.spawn(null, "OPTION", { value: "13" }, "13"),
        this.dom.spawn(null, "OPTION", { value: "14" }, "14"),
        this.dom.spawn(null, "OPTION", { value: "15" }, "15"),
      ));
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "SELECT", { name: "modrate" },
        this.dom.spawn(null, "OPTION", { value: "0" }, "1/4"),
        this.dom.spawn(null, "OPTION", { value: "1" }, "1/2"),
        this.dom.spawn(null, "OPTION", { value: "2" }, "1"),
        this.dom.spawn(null, "OPTION", { value: "3" }, "2"),
        this.dom.spawn(null, "OPTION", { value: "4" }, "3"),
        this.dom.spawn(null, "OPTION", { value: "5" }, "4"),
        this.dom.spawn(null, "OPTION", { value: "6" }, "en1"),
        this.dom.spawn(null, "OPTION", { value: "7" }, "en2"),
      ));
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 15, name: "modrangelo" }));
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 15, name: "modrangehi" }));
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "INPUT", { type: "checkbox", name: "fmenv" }));
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "SELECT", { name: "drate" },
        this.dom.spawn(null, "OPTION", { value: "0" }, "1/4"),
        this.dom.spawn(null, "OPTION", { value: "1" }, "1/3"),
        this.dom.spawn(null, "OPTION", { value: "2" }, "1/2"),
        this.dom.spawn(null, "OPTION", { value: "3" }, "1"),
        this.dom.spawn(null, "OPTION", { value: "4" }, "3/2"),
        this.dom.spawn(null, "OPTION", { value: "5" }, "2"),
        this.dom.spawn(null, "OPTION", { value: "6" }, "3"),
        this.dom.spawn(null, "OPTION", { value: "7" }, "4"),
      ));
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 3, name: "damt" }));
      this.dom.spawn(channelRow, "TD", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 3, name: "overdrive" }));
    }
  }
  
  populateUi() {
    this.element.querySelector("input[name='tempo']").value = this.midiFile.getTempo();
    const channelsInUse = [];
    for (const track of this.midiFile.tracks) {
      for (const event of track) {
        if (event.opcode === 0x90) {
          channelsInUse[event.chid] = true;
        }
      }
    }
    for (let chid=0; chid<16; chid++) this.populateChannel(chid, channelsInUse[chid]);
  }
  
  populateChannel(chid, inUse) {
    const channelRow = this.element.querySelector(`.channelRow[data-chid='${chid}']`);
    if (!channelRow) return;
    if (inUse) channelRow.classList.remove("unused");
    else channelRow.classList.add("unused");
    const packed = this.readChannelFromMidiFile(this.midiFile, chid);
    const setv = (name, bitp, size) => {
      let dstmask = 1 << (size - 1);
      let srcmask = 0x80 >> (bitp & 7);
      let bytep = bitp >> 3;
      let v = 0;
      for (; size-->0; dstmask>>=1) {
        if (packed[bytep] & srcmask) v |= dstmask;
        if (srcmask === 1) { bytep++; srcmask = 0x80; }
        else srcmask >>= 1;
      }
      const input = channelRow.querySelector(`*[name='${name}']`);
      if (input.type === "checkbox") input.checked = !!v;
      else input.value = v;
    };
    setv("volume", 1, 7);
    setv("pan", 8, 7);
    setv("sustain", 15, 1);
    setv("atktlo", 16, 4);
    setv("susvlo", 20, 4);
    setv("rlstlo", 24, 4);
    setv("atkthi", 28, 4);
    setv("susvhi", 32, 4);
    setv("rlsthi", 36, 4);
    setv("shape", 40, 4);
    setv("modrate", 44, 3);
    setv("modrangelo", 47, 4);
    setv("modrangehi", 51, 4);
    setv("fmenv", 55, 1);
    setv("drate", 56, 3);
    setv("damt", 59, 2);
    setv("overdrive", 61, 3);
  }
  
  /* Returns Uint8Array(8) with the packed channel header fields.
   * These are sourced from standard MIDI events, then packed and unpacked into smaller fields at runtime.
   * The 8-byte packed format that we return is the intermediate format.
   * What we display is the runtime format.
   * See etc/doc/song-format.md for details.
   */
  readChannelFromMidiFile(midiFile, chid) {
    const dst = new Uint8Array([0x40, 0x81, 0x88, 0x88, 0x88, 0x00, 0x00, 0x00]);
    for (const track of midiFile.tracks) {
      for (const event of track) {
        if (event.time) break;
        if (event.chid !== chid) continue;
        switch (event.opcode) {
          case 0xb0: switch (event.a) {
              case 0x07: dst[0] = (dst[0] & 0x80) | event.b; break;
              case 0x0a: dst[1] = (dst[1] & 0x01) | (event.b << 1); break;
              case 0x00: dst[1] = (dst[1] & 0xfe) | (event.b >> 6); dst[2] = (dst[2] & 0x03) | (event.b << 2); break;
              case 0x20: dst[2] = (dst[2] & 0xfc) | (event.b >> 5); dst[3] = (dst[3] & 0x07) | (event.b << 3); break;
              case 0x10: dst[4] = (dst[4] & 0xf0) | (event.b >> 3); dst[5] = (dst[5] & 0x1f) | (event.b << 5); break;
              case 0x11: dst[5] = (dst[5] & 0xe0) | (event.b >> 2); dst[6] = (dst[6] & 0x3f) | (event.b << 6); break;
              case 0x12: dst[6] = (dst[6] & 0xc0) | (event.b >> 1); dst[7] = (dst[7] & 0x7f) | (event.b << 7); break;
              case 0x13: dst[7] = (dst[7] & 0x80) | event.b; break;
            } break;
          case 0xc0: dst[3] = (dst[3] & 0xf8) | (event.a >> 4); dst[4] = (dst[4] & 0x0f) | (event.a << 4); break;
        }
      }
    }
    return dst;
  }

  /* Uint8Array(8) from values in DOM.
   */
  readChannelFromUi(chid) {
    const dst = new Uint8Array(8);
    const row = this.element.querySelector(`.channelRow[data-chid='${chid}']`);
    if (!row) return;
    const rdv = (name, bitp, size) => {
      let v = 0;
      const input = row.querySelector(`*[name='${name}']`);
      if (input.type === "checkbox") v = input.checked ? 1 : 0; // $#@! checkboxes, why don't they use "value" sanely?
      else v = +input.value || 0;
      let srcmask = 1 << (size - 1);
      let dstmask = 0x80 >> (bitp & 7);
      let bytep = bitp >> 3;
      for (; size-->0; srcmask>>=1) {
        if (v & srcmask) dst[bytep] |= dstmask;
        if (dstmask === 1) { bytep++; dstmask = 0x80; }
        else dstmask >>= 1;
      }
    };
    rdv("volume", 1, 7);
    rdv("pan", 8, 7);
    rdv("sustain", 15, 1);
    rdv("atktlo", 16, 4);
    rdv("susvlo", 20, 4);
    rdv("rlstlo", 24, 4);
    rdv("atkthi", 28, 4);
    rdv("susvhi", 32, 4);
    rdv("rlsthi", 36, 4);
    rdv("shape", 40, 4);
    rdv("modrate", 44, 3);
    rdv("modrangelo", 47, 4);
    rdv("modrangehi", 51, 4);
    rdv("fmenv", 55, 1);
    rdv("drate", 56, 3);
    rdv("damt", 59, 2);
    rdv("overdrive", 61, 3);
    return dst;
  }

  onMixerInput(event) {
    if (!this.midiFile) return;
    let chid = 0xff;
    for (let element=event?.target; element; element=element.parentNode) {
      const src = element.getAttribute("data-chid");
      if (!src) continue;
      chid = +src;
      break;
    }
    if (isNaN(chid) || (chid >= 16)) return;
    const packed = this.readChannelFromUi(chid);
    // Now unpack as MIDI event values and apply them to the file:
    const setv = (bitp, ctlk) => {
      let dstmask = 0x40;
      let srcmask = 0x80 >> (bitp & 7);
      let bytep = bitp >> 3;
      let v = 0;
      for (let i=7; i-->0; dstmask>>=1) {
        if (packed[bytep] & srcmask) v |= dstmask;
        if (srcmask === 1) { bytep++; srcmask = 0x80; }
        else srcmask >>= 1;
      }
      if (ctlk < 0) this.midiFile.setZEvent({ chid, opcode: 0xc0 }, { a: v });
      else this.midiFile.setZEvent({ chid, opcode: 0xb0, a: ctlk }, { b: v });
    };
    setv(1, 0x07);
    setv(8, 0x0a);
    setv(15, 0x00);
    setv(22, 0x20);
    setv(29, -1);
    setv(36, 0x10);
    setv(43, 0x11);
    setv(50, 0x12);
    setv(57, 0x13);
    this.dirty();
  }
  
  onPlay() {
    console.log(`SongEditor.onPlay`);
  }
  
  onPause() {
    console.log(`SongEditor.onPause`);
  }
  
  onTempoInput(event) {
    if (!this.midiFile) return;
    const usperqnote = +event.target.value;
    if (isNaN(usperqnote) || (usperqnote < 1) || (usperqnote > 0xffffff)) return;
    this.midiFile.setTempo(usperqnote);
    this.dirty();
  }
  
  dirty() {
    if (!this.res || !this.midiFile) return;
    this.data.dirty(this.res.path, () => this.midiFile.encode());
  }
}
