/* SongEventModal.js
 */
 
import { Dom } from "../Dom.js";
import { Encoder } from "../Encoder.js";
import { Song } from "./Song.js";

export class SongEventModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, "nonce"];
  }
  constructor(element, dom, nonce) {
    this.element = element;
    this.dom = dom;
    this.nonce = nonce;
    
    this.event = null; // Song.events. READONLY.
    
    /* All formats contain: trackid, chid, time
     * "unknown": Edit the rest of the event as JSON.
     * "metaSysex": mopcode(0xf0,0xf7,0xff), k, hex or text dump.
     * "egsFuture": Straight hex dump.
     * "note": noteid, velocity, dur
     * "noteOn","noteOff","noteAdjust": noteid, velocity
     * "pressure": velocity
     * "control": k, v
     * "program": pid
     * "wheel": wheel
     */
    this.format = "unknown";
  }
  
  setup(event) {
    this.event = event;
    
    this.format = "unknown";
    switch (this.event.eopcode) {
      case 0x80: case 0x90: case 0xa0: this.format = "note"; break;
      case 0xb0: case 0xc0: case 0xd0: case 0xe0: case 0xf0: this.format = "egsFuture"; break;
      default: switch (this.event.mopcode) {
          case 0x80: this.format = "noteOff"; break;
          case 0x90: this.format = "noteOn"; break;
          case 0xa0: this.format = "noteAdjust"; break;
          case 0xb0: this.format = "control"; break;
          case 0xc0: this.format = "program"; break;
          case 0xd0: this.format = "pressure"; break;
          case 0xe0: this.format = "wheel"; break;
          case 0xf0: case 0xf7: case 0xff: this.format = "metaSysex"; break;
        } break;
    }
    
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const table = this.dom.spawn(this.element, "TABLE");
    
    let select;
    this.dom.spawn(table, "TR", ["format"], { "data-k": "format" },
      this.dom.spawn(null, "TD", "Format"),
      this.dom.spawn(null, "TD",
        select = this.dom.spawn(null, "SELECT", { "on-change": () => this.onFormatChanged(), name: "format" },
          this.dom.spawn(null, "OPTION", { value: "unknown" }, "Unknown"),
          this.dom.spawn(null, "OPTION", { value: "metaSysex" }, "Meta or Sysex"),
          this.dom.spawn(null, "OPTION", { value: "egsFuture" }, "EGS Future"),
          this.dom.spawn(null, "OPTION", { value: "note" }, "EGS Note"),
          this.dom.spawn(null, "OPTION", { value: "noteOn" }, "Note On"),
          this.dom.spawn(null, "OPTION", { value: "noteOff" }, "Note Off"),
          this.dom.spawn(null, "OPTION", { value: "noteAdjust" }, "Note Adjust"),
          this.dom.spawn(null, "OPTION", { value: "control" }, "Control"),
          this.dom.spawn(null, "OPTION", { value: "program" }, "Program"),
          this.dom.spawn(null, "OPTION", { value: "pressure" }, "Pressure"),
          this.dom.spawn(null, "OPTION", { value: "wheel" }, "Wheel"),
        )
      ),
    );
    select.value = this.format;
    
    this.buildTablePerFormat(this.format);
    
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "OK", "on-click": () => this.onSubmit() });
  }
  
  /* Table construction, per format.
   ************************************************************************/
  
  buildTablePerFormat(format) {
    const table = this.element.querySelector("table");
    for (const tr of table.querySelectorAll("tr")) {
      if (tr.classList.contains("format")) continue;
      tr.remove();
    }
    this.spawnRow(table, "id", "ID", { readonly: true });
    this.spawnRow(table, "trackid", "Track");
    this.spawnRow(table, "time", "Time", { repr: (v) => this.reprTime(v) });
    this.spawnRow(table, "chid", "Channel");
    switch (format) {
      case "unknown": this.buildTableUnknown(table); break;
      case "metaSysex": this.buildTableMetaSysex(table); break;
      case "egsFuture": this.buildTableEgsFuture(table); break;
      case "note": this.buildTableNote(table, true); break;
      case "noteOn": this.buildTableNote(table, false); break;
      case "noteOff": this.buildTableNote(table, false); break;
      case "noteAdjust": this.buildTableNote(table, false); break;
      case "control": this.buildTableControl(table); break;
      case "program": this.buildTableProgram(table); break;
      case "pressure": this.buildTablePressure(table); break;
      case "wheel": this.buildTableWheel(table); break;
    }
  }
  
  readEventFromUi() {
    let event = {};
    let format = "";
    for (const row of this.element.querySelectorAll("tr")) {
      const k = row.getAttribute("data-k");
      if (!k) {
        console.log(`SongEventModal.readEventFromUi invalid row, no key`, row);
        continue;
      }
      let v;
      
      if (v = row.querySelector("input")) {
        if (v.type === "checkbox") v = v.checked;
        else if (isNaN(v.value)) v = v.value;
        else v = +v.value;
        
      } else if (v = row.querySelector("select")) {
        if (k === "format") {
          this.addEventFieldsPerFormat(event, format = v.value);
          continue;
        }
        v = +v.value;
        
      } else if (v = row.querySelector("textarea")) {
        if (k === "rest") {
          v = JSON.parse(v.value);
          event = { ...event, ...v };
          continue;
        } else if (k === "hexdump") {
          event.v = this.readHexDump(v.value);
          continue;
        } else {
          v = new TextEncoder("utf8").encode(v.value);
        }
        
      } else if (v = row.querySelector("td.v")) {
        v = +v.innerText;
      }
      event[k] = v;
    };
    
    if (format === "egsFuture") {
      event.eopcode = event.v?.[0] || 0;
    }
    
    return event;
  }
  
  spawnRow(table, k, label, options) {
    const tr = this.dom.spawn(table, "TR", { "data-k": k });
    this.dom.spawn(tr, "TD", ["k"], label);
    const tdv = this.dom.spawn(tr, "TD", ["v"]);
    
    let inputElement = null;
    if (options?.readonly) {
      tdv.innerText = this.event[k] || "";
      
    } else if (options?.options) {
      inputElement = this.dom.spawn(tdv, "SELECT", ...options.options.map(o => this.dom.spawn(null, "OPTION", { value: o.value }, o.label)));
      
    } else {
      inputElement = this.dom.spawn(tdv, "INPUT", { name: k, type: options?.inputType || "number" });
    }
    
    if (inputElement) inputElement.value = this.event[k];
    
    if (options?.repr) {
      const tdr = this.dom.spawn(tr, "TD", ["tip"], options.repr(this.event[k]));
      if (inputElement) {
        inputElement.addEventListener("input", () => tdr.innerText = options.repr(inputElement.value));
      }
    }
  }
  
  buildTableUnknown(table) {
    const { id, time, chid, trackid, ...rest } = this.event;
    const tr = this.dom.spawn(table, "TR", { "data-k": "rest" });
    const td = this.dom.spawn(tr, "TD", { colspan: 3 });
    const input = this.dom.spawn(td, "TEXTAREA", JSON.stringify(rest));
  }
  
  buildTableMetaSysex(table) {
    this.spawnRow(table, "mopcode", "Opcode", {
      options: [
        { value: 0xf0, label: "0xf0 Sysex Unterminated" },
        { value: 0xf7, label: "0xf7 Sysex Terminated" },
        { value: 0xff, label: "0xff Meta" },
      ],
    });
    this.spawnRow(table, "k", "Type (Meta only)", { repr: v => Song.META_TYPES[+v] || "" });
    const tr = this.dom.spawn(table, "TR", { "data-k": "hexdump" });
    const td = this.dom.spawn(tr, "TD", { colspan: 3 });
    const input = this.dom.spawn(td, "TEXTAREA", this.writeHexDump(this.event.v));
  }
  
  buildTableEgsFuture(table) {
    const tr = this.dom.spawn(table, "TR", { "data-k": "hexdump" });
    const td = this.dom.spawn(tr, "TD", { colspan: 3 });
    const input = this.dom.spawn(td, "TEXTAREA", this.writeHexDump(this.event.v));
  }
  
  // 4 events (EGS Note, Note On, Note Off, Note Adjust), all contain (noteid, velocity), and EGS Note also has (dur).
  buildTableNote(table, withDuration) {
    this.spawnRow(table, "noteid", "Note", { repr: v => Song.reprNote(+v) });
    this.spawnRow(table, "velocity", "Velocity");
    if (withDuration) {
      this.spawnRow(table, "dur", "Duration ms");
    }
  }
  
  buildTableControl(table) {
    this.spawnRow(table, "k", "Key", { repr: v => Song.CONTROL_KEYS[+v] || "" });
    this.spawnRow(table, "v", "Value");
  }
  
  buildTableProgram(table) {
    this.spawnRow(table, "pid", "Program", { repr: v => Song.GM_PROGRAM_NAMES[+v] || "" });
  }
  
  buildTablePressure(table) {
    this.spawnRow(table, "velocity", "Pressure");
  }
  
  buildTableWheel(table) {
    this.spawnRow(table, "wheel", "Wheel 0..8192..16383");
  }
  
  /* Model bits.
   ******************************************************************************/
  
  // eopcode,mopcode
  addEventFieldsPerFormat(event, format) {
    switch (format) {
      case "unknown": break;
      case "metaSysex": break; // There's a "mopcode" field in the UI.
      case "egsFuture": break; // eopcode is top 4 bits of first byte of v... have to wait until v is decoded
      case "note": event.eopcode = 0x80; break; // 80,90,a0: Doesn't matter which, encoder overrides.
      case "noteOn": event.mopcode = 0x90; break;
      case "noteOff": event.mopcode = 0x80; break;
      case "noteAdjust": event.mopcode = 0xa0; break;
      case "control": event.mopcode = 0xb0; break;
      case "program": event.mopcode = 0xc0; break;
      case "pressure": event.mopcode = 0xd0; break;
      case "wheel": event.mopcode = 0xe0; break;
    }
  }
   
  reprTime(ms) {
    let sec = Math.floor(ms / 1000);
    ms %= 1000;
    const min = Math.floor(sec / 60);
    sec %= 60;
    return `${min}:${sec.toString().padStart(2, '0')}.${ms.toString().padStart(3, '0')}`;
  }
  
  readHexDump(src) {
    if (!src) return new Uint8Array(0);
    const dst = new Encoder();
    let hi = -1;
    for (let srcp=0; srcp<src.length; srcp++) {
      let ch = src.charCodeAt(srcp);
      if (ch <= 0x20) continue;
           if ((ch >= 0x30) && (ch <= 0x39)) ch = ch - 0x30;
      else if ((ch >= 0x41) && (ch <= 0x46)) ch = ch - 0x41 + 10;
      else if ((ch >= 0x61) && (ch <= 0x66)) ch = ch - 0x61 + 10;
      else throw new Error(`Illegal byte ${ch} in hex dump`);
      if (hi < 0) hi = ch;
      else { dst.u8((hi << 4) | ch); hi = -1; }
    }
    if (hi >= 0) throw new Error(`Uneven digit count in hex dump`);
    return dst.finish();
  }
  
  writeHexDump(src) {
    if (!src) return "";
    let dst = "";
    for (let srcp=0; srcp<src.length; srcp++) {
      if (srcp) dst += " ";
      dst += "0123456789abcdef"[src[srcp] >> 4];
      dst += "0123456789abcdef"[src[srcp] & 15];
    }
    return dst;
  }
  
  /* UI events.
   ******************************************************************************/
  
  onSubmit() {
    if (!this.event) return;
    const event = this.readEventFromUi();
    this.resolve(event);
  }
  
  onFormatChanged() {
    const format = this.element.querySelector("select[name='format']").value;
    this.buildTablePerFormat(format);
  }
}
