/* MidiEditor.js
 * No need for a full sequencer, users should find third party tooling for that.
 * But we do need to expose all the Egg-specific config.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";
import { MidiFile } from "./MidiFile.js";
import { MidiEventModal } from "./MidiEventModal.js";
import { MidiChannelFieldModal } from "./MidiChannelFieldModal.js";
import { EditorModal } from "./EditorModal.js";
import { EgsEditor } from "./EgsEditor.js";
 
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
    this.populateUi();
  };
  
  /* Presentation.
   ***************************************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    
    const globals = this.dom.spawn(this.element, "DIV", ["globals"]);
    this.dom.spawn(globals, "DIV",
      this.dom.spawn(null, "SPAN", "Tempo bpm"),
      this.dom.spawn(null, "INPUT", { type: "number", name: "tempo", min: 1, "on-input": () => this.onTempoChanged() })
    );
    this.dom.spawn(globals, "SELECT", { name: "operations", "on-input": e => this.onOperation(e) },
      this.dom.spawn(null, "OPTION", { value: "" }, "Operations:"),
      this.dom.spawn(null, "OPTION", { value: "zeroLeadTime" }, "Zero lead time"),
      this.dom.spawn(null, "OPTION", { value: "autoEndTime" }, "Auto end time"),
      this.dom.spawn(null, "OPTION", { value: "removeUselessMeta" }, "Remove useless Meta (keep Tempo and EGS)"),
      this.dom.spawn(null, "OPTION", { value: "removeSysex" }, "Remove all Sysex"),
      this.dom.spawn(null, "OPTION", { value: "removeNoteAdjust" }, "Remove all Note Adjust"),
      this.dom.spawn(null, "OPTION", { value: "removeWheel" }, "Remove all Wheel"),
      this.dom.spawn(null, "OPTION", { value: "removeChannels" }, "Remove unused channels (no Note On)"),
    );
    
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
    
      globals.querySelector("input[name='tempo']").value = this.file.getTempoBpm();
      
      const channelCards = [];
      for (let chid=0; chid<16; chid++) {
        channelCards.push(this.buildBlankChannelCard(channels, chid));
      }
      
      for (const event of this.file.mergeEvents()) {
        if (event.chid < 16) {
          this.addEventToChannelCard(channelCards[event.chid], event);
        }
        this.addEventToTable(events, event);
      }
    }
  }
  
  buildBlankChannelCard(parent, chid) {
    const card = this.dom.spawn(parent, "DIV", ["channel"], { "data-chid": chid });
    this.dom.spawn(card, "DIV", ["title"], `Channel ${chid}`);
    this.dom.spawn(card, "DIV", ["nonZeroTimeWarning", "hidden"], "Nonzero config time.");
    this.dom.spawn(card, "DIV", ["field"], { "on-click": () => this.onEditChannelField(chid, "program") },
      this.dom.spawn(null, "SPAN", ["key"], "Program:"),
      this.dom.spawn(null, "SPAN", ["value", "bankMsb"], "0"),
      this.dom.spawn(null, "SPAN", ","),
      this.dom.spawn(null, "SPAN", ["value", "bankLsb"], "0"),
      this.dom.spawn(null, "SPAN", ","),
      this.dom.spawn(null, "SPAN", ["value", "program"], "0")
    );
    this.dom.spawn(card, "DIV", ["field"], { "on-click": () => this.onEditChannelField(chid, "volume") },
      this.dom.spawn(null, "SPAN", ["key"], "Volume:"),
      this.dom.spawn(null, "SPAN", ["value", "volume"])
    );
    this.dom.spawn(card, "DIV", ["field"], { "on-click": () => this.onEditChannelField(chid, "pan") },
      this.dom.spawn(null, "SPAN", ["key"], "Pan:"),
      this.dom.spawn(null, "SPAN", ["value", "pan"])
    );
    this.dom.spawn(card, "DIV", ["field"], { "on-click": () => this.onEditChannelField(chid, "egs") },
      this.dom.spawn(null, "SPAN", ["key"], "EGS Config:"),
      this.dom.spawn(null, "SPAN", ["value", "egs"])
    );
    return card;
  }
  
  addEventToChannelCard(card, event) {
    card.classList.add("present");
    switch (event.opcode) {
      case 0xb0: switch (event.a) {
          case 0x00: card.querySelector(".value.bankMsb").innerText = event.b; break;
          case 0x07: card.querySelector(".value.volume").innerText = event.b; break;
          case 0x0a: card.querySelector(".value.pan").innerText = event.b; break;
          case 0x20: card.querySelector(".value.bankLsb").innerText = event.b; break;
          default: return;
        } break;
      case 0xc0: card.querySelector(".value.program").innerText = event.a; break;
      case 0xff: switch (event.a) {
          case 0xf0: card.querySelector(".value.egs").innerText = `${event.v.length} bytes`; break;
          default: return;
        } break;
      default: return; // eg note on, note off
    }
    if (event.time) card.querySelector(".nonZeroTimeWarning").classList.remove("hidden");
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
  
  /* Event handlers.
   ***********************************************************************************************/
  
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
  
  getChannelEgsHeader(chid) {
    if (this.file) {
      for (const track of this.file.tracks) {
        for (const event of track) {
          if (event.chid !== chid) continue;
          if (event.opcode !== 0xff) continue;
          if (event.a !== 0xf0) continue;
          const padded = new Uint8Array(6 + event.v.length);
          padded[0] = 0x00;
          padded[1] = 0x45;
          padded[2] = 0x47;
          padded[3] = 0x53;
          padded[4] = event.v.length >> 8;
          padded[5] = event.v.length;
          new Uint8Array(padded.buffer, 6, event.v.length).set(event.v);
          return padded;
        }
      }
    }
    return new Uint8Array([0x00, 0x45, 0x47, 0x53]);
  }
  
  onEditChannelField(chid, k) {
    if (k === "egs") {
      const serial = this.getChannelEgsHeader(chid);
      const modal = this.dom.spawnModal(EditorModal);
      const controller = modal.setupWithSerial(EgsEditor, serial);
      modal.result.then((result) => {
        console.log(`MidiEditor result from EgsEditor`, serial);
      }).catch(e => this.dom.modalError(e));
    } else {
      const modal = this.dom.spawnModal(MidiChannelFieldModal);
      modal.setup(this.file, chid, k);
      modal.result.then((result) => {
        if (!result) return;
        this.data.dirty(this.res.path, () => this.file.encode());
        this.populateUi();
      }).catch(e => this.dom.modalError(e));
    }
  }
  
  onTempoChanged() {
    const bpm = +this.element.querySelector("input[name='tempo']")?.value;
    if (!bpm) return;
    if ((bpm < 4) || (bpm > 0xffffff)) return;
    this.file.setTempoBpm(bpm);
    this.data.dirty(this.res.path, () => this.file.encode());
  }
  
  onOperation(event) {
    if (!this.file) return;
    if (!event?.target) return;
    const opname = event.target.value;
    event.target.value = "";
    switch (opname) {
      case "zeroLeadTime": this.op_zeroLeadTime(); break;
      case "autoEndTime": this.op_autoEndTime(); break;
      case "removeUselessMeta": this.op_removeUselessMeta(); break;
      case "removeSysex": this.op_removeSysex(); break;
      case "removeNoteAdjust": this.op_removeNoteAdjust(); break;
      case "removeWheel": this.op_removeWheel(); break;
      case "removeChannels": this.op_removeChannels(); break;
      default: this.dom.modalMessage(`Unimplemented operation ${JSON.stringify(opname)}`);
    }
  }
  
  /* Named operations.
   **********************************************************************************/
   
  op_zeroLeadTime() {
    if (!this.file) return;
    let firstNoteTime = 999999999;
    for (const track of this.file.tracks) {
      for (const event of track) {
        if (event.opcode !== 0x90) continue;
        if (event.time < firstNoteTime) {
          firstNoteTime = event.time;
        }
        break;
      }
    }
    if (firstNoteTime >= 999999999) return;
    if (!firstNoteTime) {
      this.dom.modalMessage("Already zeroed.");
      return;
    }
    for (const track of this.file.tracks) {
      for (const event of track) {
        if (event.time >= firstNoteTime) event.time -= firstNoteTime;
        else event.time = 0;
      }
    }
    this.dom.modalMessage(`Trimmed ${firstNoteTime} ticks from start.`);
    this.data.dirty(this.res.path, () => this.file.encode());
    this.populateUi();
  }
  
  op_autoEndTime() {
    if (!this.file) return;
    let endTime=0, endEventId=0;
    for (const track of this.file.tracks) {
      if (!track.length) continue;
      const event = track[track.length - 1];
      if (event.time > endTime) {
        endTime = event.time;
        endEventId = event.id;
      }
    }
    const qnotec = Math.floor(endTime / this.file.division);
    if (!(endTime % this.file.division)) {
      this.dom.modalMessage(`Already aligned to ${qnotec} beats.`);
      return;
    }
    /* Not really sure what I'm doing here...
     * Let's say if the lower option is a multiple of four, use that, and clamp all events to that time.
     * Otherwise, extend it to the next multiple of four beats.
     * Obviously that is incorrect for non-four time signatures but not sure what to do about it.
     * (note that if it aligns to a beat, we're already done, multiple-of-four or not).
     */
    if (!(qnotec % 4)) {
      const newEndTime = qnotec * this.file.division;
      let eventc = 0;
      for (const track of this.file.tracks) {
        for (let i=track.length; i-->0; ) {
          const event = track[i];
          if (event.time <= newEndTime) break;
          event.time = newEndTime;
          eventc++;
        }
      }
      this.dom.modalMessage(`Truncated end time from ${endTime} to ${newEndTime} ticks, affecting ${eventc} events. ${qnotec} beats.`);
    } else { // Extending, change just the one event.
      const newQnotec = (qnotec + 3) & ~3;
      const newEndTime = newQnotec * this.file.division;
      const event = this.file.eventById(endEventId);
      if (!event) return;
      event.time = newEndTime;
      this.dom.modalMessage(`Extended end time from ${endTime} to ${newEndTime} ticks (${qnotec} to ${newQnotec} beats).`);
    }
    this.data.dirty(this.res.path, () => this.file.encode());
    this.populateUi();
  }
      
  op_removeUselessMeta() {
    let rmc=0;
    for (const track of this.file.tracks) {
      for (let i=track.length; i-->0; ) {
        const event = track[i];
        if (event.opcode !== 0xff) continue;
        switch (event.a) {
          case 0x20: continue; // MIDI Channel Prefix
          case 0x2f: continue; // End of Track
          case 0x51: continue; // Set Tempo
          case 0xf0: continue; // EGS
        }
        track.splice(i, 1);
        rmc++;
      }
    }
    if (!rmc) {
      this.dom.modalMessage("No events removed.");
      return;
    }
    this.dom.modalMessage(`Removed ${rmc} Meta events.`);
    this.data.dirty(this.res.path, () => this.file.encode());
    this.populateUi();
  }
  
  op_removeSysex() {
    let rmc=0;
    for (const track of this.file.tracks) {
      for (let i=track.length; i-->0; ) {
        const event = track[i];
        if ((event.opcode !== 0xf0) && (event.opcode !== 0xf7)) continue;
        track.splice(i, 1);
        rmc++;
      }
    }
    if (!rmc) {
      this.dom.modalMessage("No events removed.");
      return;
    }
    this.dom.modalMessage(`Removed ${rmc} Sysex events.`);
    this.data.dirty(this.res.path, () => this.file.encode());
    this.populateUi();
  }
  
  op_removeNoteAdjust() {
    let rmc=0, chans=new Set();
    for (const track of this.file.tracks) {
      for (let i=track.length; i-->0; ) {
        const event = track[i];
        if (event.opcode !== 0xa0) continue;
        chans.add(event.chid);
        track.splice(i, 1);
        rmc++;
      }
    }
    if (!rmc) {
      this.dom.modalMessage("No events removed.");
      return;
    }
    this.dom.modalMessage(`Removed ${rmc} Note Adjust events on ${chans.size} channels.`);
    this.data.dirty(this.res.path, () => this.file.encode());
    this.populateUi();
  }
  
  op_removeWheel() {
    let rmc=0, chans=new Set();
    for (const track of this.file.tracks) {
      for (let i=track.length; i-->0; ) {
        const event = track[i];
        if (event.opcode !== 0xe0) continue;
        chans.add(event.chid);
        track.splice(i, 1);
        rmc++;
      }
    }
    if (!rmc) {
      this.dom.modalMessage("No events removed.");
      return;
    }
    this.dom.modalMessage(`Removed ${rmc} Wheel events on ${chans.size} channels.`);
    this.data.dirty(this.res.path, () => this.file.encode());
    this.populateUi();
  }
  
  op_removeChannels() {
    const eventsExist=[], notesExist=[];
    for (const track of this.file.tracks) {
      for (const event of track) {
        if (event.chid >= 0x10) continue;
        eventsExist[event.chid] = true;
        if (event.opcode === 0x90) notesExist[event.chid] = true;
      }
    }
    let rmc=0, chanc=0;
    for (let chid=0; chid<0x10; chid++) {
      if (!eventsExist[chid]) continue;
      if (notesExist[chid]) continue;
      chanc++;
      for (const track of this.file.tracks) {
        for (let i=track.length; i-->0; ) {
          const event = track[i];
          if (event.chid !== chid) continue;
          track.splice(i, 1);
          rmc++;
        }
      }
    }
    if (!rmc) {
      this.dom.modalMessage("All populated channels have at least one Note On event.");
      return;
    }
    this.dom.modalMessage(`Removed ${rmc} events from ${chanc} channels.`);
    this.data.dirty(this.res.path, () => this.file.encode());
    this.populateUi();
  }
}
