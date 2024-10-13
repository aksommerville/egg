/* MidiEventModal.js
 * For events from the verbatim event list in MidiEditor.
 * We are set up with an event from MidiFile.
 * We don't touch the owning MidiFile (don't even get a peek at it).
 */
 
import { Dom } from "./Dom.js";
import { MidiFile } from "./MidiFile.js";

export class MidiEventModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.event = null;
  }
  
  setup(event) {
    this.event = event;
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    if (!this.event) return;
    this.dom.spawn(this.element, "DIV", ["title"], `Event ${this.event.id}: ${this.reprOpcode(this.event.opcode)}`);
    this.spawnInnards();
    const submitRow = this.dom.spawn(this.element, "DIV", ["submitRow"]);
    this.dom.spawn(submitRow, "INPUT", { type: "button", value: "Save", "on-click": () => this.onSave() });
    this.dom.spawn(submitRow, "DIV", ["tip"], "Esc or OOB click to cancel.");
    this.dom.spawn(submitRow, "DIV", ["spacer"]);
    if ((this.event.opcode === 0x80) || (this.event.opcode === 0x90)) {
      this.dom.spawn(submitRow, "INPUT", { type: "button", value: "Delete Partner Too", "on-click": () => this.onDelete(true) });
    }
    this.dom.spawn(submitRow, "INPUT", { type: "button", value: "Delete", "on-click": () => this.onDelete(false) });
  }
  
  spawnInnards() {
    const table = this.dom.spawn(this.element, "TABLE");
    this.spawnRow(table, "Time", "time", v => this.reprTime(v));
    switch (this.event.opcode) {
      case 0x80: {
          this.spawnRow(table, "Note", "a", v => MidiFile.reprNote(v, false));
          this.spawnRow(table, "Velocity", "b");
        } break;
      case 0x90: {
          this.spawnRow(table, "Note", "a", v => MidiFile.reprNote(v, false));
          this.spawnRow(table, "Velocity", "b");
        } break;
      case 0xa0: {
          this.spawnRow(table, "Note", "a", v => MidiFile.reprNote(v, false));
          this.spawnRow(table, "Pressure", "b");
        } break;
      case 0xb0: {
          this.spawnRow(table, "Key", "a", v => (MidiFile.CONTROL_KEYS[v] || ""));
          this.spawnRow(table, "Value", "b");
        } break;
      case 0xc0: {
          this.spawnRow(table, "Program", "a", v => (MidiFile.GM_PROGRAM_NAMES[v] || ""));
        } break;
      case 0xd0: {
          this.spawnRow(table, "Pressure", "a");
        } break;
      case 0xe0: {
          //TODO Three views: Encoded, Numeric, Slider.
          this.spawnRow(table, "LSB", "a");
          this.spawnRow(table, "MSB", "b");
        } break;
      case 0xf0:
      case 0xf7: {
          this.spawnHexDump(table);
        } break;
      case 0xff: {
          this.spawnRow(table, "Type", "a", v => (MidiFile.META_TYPES[v] || ""));
          this.spawnHexDump(table);
        } break;
    }
  }
  
  spawnRow(table, label, key, repr) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["key"], label);
    const tdv = this.dom.spawn(tr, "TD", ["value"]);
    const input = this.dom.spawn(tdv, "INPUT", { type: "number", min: 0, max: 127, value: this.event[key] || 0, name: key, "on-input": () => input.classList.remove("invalid") });
    if (repr) {
      const tdadvice = this.dom.spawn(tr, "TD", ["advice"], repr(input.value));
      input.addEventListener("input", e => tdadvice.innerText = repr(input.value));
    }
  }
  
  spawnHexDump(table) {
    const tr = this.dom.spawn(table, "TR");
    const td = this.dom.spawn(tr, "TD", { colspan: 3 });
    const textarea = this.dom.spawn(td, "TEXTAREA", ["hexdump"], { name: "v", "on-input": () => textarea.classList.remove("invalid") });
    textarea.value = this.reprHex(this.event.v);
  }
  
  // String from Uint8Array
  reprHex(src) {
    if (!src) return "";
    let dst = "";
    for (let i=0; i<src.length; i++) {
      dst += "0123456789abcdef"[src[i] >> 4];
      dst += "0123456789abcdef"[src[i] & 15];
    }
    return dst;
  }
  
  // Uint8Array from string, or null if invalid.
  evalHex(src) {
    let hi=-1;
    const dst = [];
    for (let srcp=0; srcp<src.length; srcp++) {
      let ch = src.charCodeAt(srcp);
      if (ch <= 0x20) continue;
           if ((ch >= 0x30) && (ch <= 0x39)) ch = ch - 0x30;
      else if ((ch >= 0x61) && (ch <= 0x66)) ch = ch - 0x61 + 10;
      else if ((ch >= 0x41) && (ch <= 0x46)) ch = ch - 0x41 + 10;
      else return null;
      if (hi < 0) {
        hi = ch;
      } else {
        dst.push((hi << 4) | ch);
        hi = -1;
      }
    }
    if (hi >= 0) return null;
    return new Uint8Array(dst);
  }
  
  reprTime(ticks) {
    return ticks;
  }
  
  reprOpcode(opcode) {
    switch (opcode) {
      case 0x80: return "Note Off";
      case 0x90: return "Note On";
      case 0xa0: return "Note Adjust";
      case 0xb0: return "Control Change";
      case 0xc0: return "Program Change";
      case 0xd0: return "Pressure";
      case 0xe0: return "Wheel";
      case 0xf0: return "Sysex";
      case 0xf7: return "Sysex";
      case 0xff: return "Meta";
      default: return opcode;
    }
  }
  
  readEventFromUi() {
    if (!this.event) return null;
    const dst = { ...this.event };
    for (const k of Object.keys(this.event)) {
      const element = this.element.querySelector(`*[name='${k}']`);
      if (!element) continue;
      let v = element.value;
      let valid = true;
      switch (k) {
        case "time": if (isNaN(v = +v) || (v < 0)) valid = false;; break;
        case "a": if (isNaN(v = +v) || (v < 0) || (v > 0x7f)) valid = false; break;
        case "b": if (isNaN(v = +v) || (v < 0) || (v > 0x7f)) valid = false; break;
        case "v": if (!(v = this.evalHex(v))) valid = false; break;
        default: continue;
      }
      if (!valid) {
        element.classList.add("invalid");
        return null;
      }
      dst[k] = v;
    }
    return dst;
  }
  
  onDelete(partnerToo) {
    this.resolve({ delete: true, id: this.event.id, partnerToo });
  }
  
  onSave() {
    const event = this.readEventFromUi();
    if (!event) return;
    this.resolve(event);
  }
}
