/* MidiEditor.js
 * No need for a full sequencer, users should find third party tooling for that.
 * But we do need to expose all the Egg-specific config.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";
import { MidiFile } from "./MidiFile.js";
import { MidiEventModal } from "./MidiEventModal.js";
 
export class MidiEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    
    this.res = null;
    this.file = null;
    
    this.buildUi();
  }
  
  static checkResource(res) {
    if (
      (res.serial.length >= 4) &&
      (res.serial[0] === 0x4d) &&
      (res.serial[1] === 0x54) &&
      (res.serial[2] === 0x68) &&
      (res.serial[3] === 0x64)
    ) return 2;
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.file = new MidiFile(res.serial);
    console.log(`MidiEditor.setup`, { res, file: this.file });
    this.populateUi();
  };
  
  buildUi() {
    this.element.innerHTML = "";
    
    const globals = this.dom.spawn(this.element, "DIV", ["globals"]);
    this.dom.spawn(globals, "DIV", "TODO:tempo");
    this.dom.spawn(globals, "DIV", "TODO:operations");
    
    const channelsSizer = this.dom.spawn(this.element, "DIV", ["channelsSizer"]);
    const channelsScroller = this.dom.spawn(channelsSizer, "DIV", ["channelsScroller"]);
    const channels = this.dom.spawn(channelsScroller, "DIV", ["channels"]);
    
    const eventsScroller = this.dom.spawn(this.element, "DIV", ["eventsScroller"]);
    const events = this.dom.spawn(eventsScroller, "TABLE", ["events"], { "on-click": e => this.onClickEvents(e) });
  }
  
  populateUi() {
    const globals = this.element.querySelector(".globals");
    const channels = this.element.querySelector(".channels");
    const events = this.element.querySelector(".events");
    channels.innerHTML = "";
    events.innerHTML = "";
    if (this.file) {
    
      //TODO populate tempo etc in globals
      
      const channelCards = [];
      for (let chid=0; chid<16; chid++) {
        channelCards.push(this.buildBlankChannelCard(channels, chid));
      }
      
      for (const event of this.file.mergeEvents()) {
        if (!event.time && (event.chid < 16)) {
          this.addEventToChannelCard(channelCards[event.chid], event);
        }
        this.addEventToTable(events, event);
      }
    }
  }
  
  buildBlankChannelCard(parent, chid) {
    const card = this.dom.spawn(parent, "DIV", ["channel"], { "data-chid": chid });
    this.dom.spawn(card, "DIV", ["title"], `Channel ${chid}`);
    return card;
  }
  
  addEventToChannelCard(card, event) {
    card.classList.add("present");
  }
  
  addEventToTable(table, event) {
    const tr = this.dom.spawn(table, "TR", { "data-eventid": event.id });
    tr.style.backgroundColor = this.colorForChid(event.chid);
    this.dom.spawn(tr, "TD", ["time"], event.time);
    this.dom.spawn(tr, "TD", ["chid"], event.chid);
    this.dom.spawn(tr, "TD", ["opcode"], this.reprOpcode(event.opcode));
    this.dom.spawn(tr, "TD", ["a"], this.reprA(event.opcode, event.a));
    this.dom.spawn(tr, "TD", ["b"], this.reprB(event.opcode, event.a, event.b));
    this.dom.spawn(tr, "TD", ["v"], event.v ? this.shortHexDump(event.v) : "");
  }
  
  colorForChid(chid) {
    switch (chid) {
      case 0: return "#400";
      case 1: return "#730";
      case 2: return "#660";
      case 3: return "#060";
      case 4: return "#046";
      case 5: return "#00f";
      case 6: return "#60a";
      case 7: return "#444";
      case 8: return "#642";
      case 9: return "#465";
      case 10: return "#469";
      case 11: return "#777";
      case 12: return "#800";
      case 13: return "#005";
      case 14: return "#030";
      case 15: return "#366";
      default: return "#000";
    }
  }
  
  reprOpcode(opcode) {
    switch (opcode) {
      case 0x80: return "80 Note Off";
      case 0x90: return "90 Note On";
      case 0xa0: return "a0 Note Adjust";
      case 0xb0: return "b0 Control Change";
      case 0xc0: return "c0 Program Change";
      case 0xd0: return "d0 Pressure";
      case 0xe0: return "e0 Wheel";
      case 0xf0: return "f0 Sysex";
      case 0xf7: return "f7 Sysex";
      case 0xff: return "ff Meta";
      default: return (
        "0123456789abcdef"[(opcode>>4) & 15] +
        "0123456789abcdef"[opcode & 15] + 
        " Unknown"
      );
    }
  }
  
  reprA(opcode, a) {
    switch (opcode) {
      case 0x80: return MidiFile.reprNote(a, true);
      case 0x90: return MidiFile.reprNote(a, true);
      case 0xa0: return MidiFile.reprNote(a, true);
      case 0xb0: return this.reprControlKey(a);
      case 0xc0: return this.reprGmProgram(a);
      case 0xd0: break; // Pressure, numeric
      case 0xe0: break; // Wheel, numeric
      case 0xf0: return "";
      case 0xf7: return "";
      case 0xff: return this.reprMetaType(a);
    }
    return "0x" + a.toString(16).padStart(2, "0");
  }
  
  reprControlKey(a) {
    return "0x" + a.toString(16).padStart(2, "0") + " " + (MidiFile.CONTROL_KEYS[a] || "");
  }
  
  reprGmProgram(pid) {
    return "0x" + pid.toString(16).padStart(2, "0") + " " + (MidiFile.GM_PROGRAM_NAMES[pid] || "");
  }
  
  reprMetaType(type) {
    return "0x" + type.toString(16).padStart(2, "0") + " " + (MidiFile.META_TYPES[type] || "");
  }
  
  reprB(opcode, a, b) {
    // Some control keys have explicit semantics (eg on/off vs continuous).
    // Not sure there's much value in picking those off.
    switch (opcode) {
      case 0xc0: return "";
      case 0xd0: return "";
      case 0xf0: return "";
      case 0xf7: return "";
      case 0xff: return "";
    }
    return "0x" + b.toString(16).padStart(2, "0");
  }
  
  shortHexDump(src) {
    if (src.length > 8) {
      return `(${src.length} bytes)`;
    }
    let dst = "";
    for (let i=0; i<src.length; i++) {
      dst += "0123456789abcdef"[src[i] >> 4];
      dst += "0123456789abcdef"[src[i] & 15];
    }
    return dst;
  }
  
  onEditEvent(event) {
    const modal = this.dom.spawnModal(MidiEventModal);
    modal.setup(event);
    modal.result.then(newEvent => {
      if (!newEvent) return;
      if (newEvent.delete) {
        if (this.file.deleteEvent(newEvent.id, newEvent.partnerToo)) {
          this.data.dirty(this.res.path, () => this.file.encode());
        }
      } else if (this.file.replaceEvent(newEvent)) {
        this.data.dirty(this.res.path, () => this.file.encode());
      }
      this.populateUi();
    }).catch(e => this.dom.modalError(e));
  }
  
  onClickEvents(domEvent) {
    if (!this.file) return;
    let id = 0;
    for (let element=domEvent.target; element; element=element.parentNode) {
      if (id = +element.getAttribute?.("data-eventid")) break;
    }
    if (!id) return;
    const midiEvent = this.file.eventById(id);
    if (!midiEvent) return;
    this.onEditEvent(midiEvent);
  }
}
