/* MidiChannelFieldModal.js XXX
 * Edits one line of a channel header, eg volume or pan.
 * We modify the song directly but do not talk to Data.
 */
 
import { Dom } from "./Dom.js";
import { MidiFile } from "./MidiFile.js";

export class MidiChannelFieldModalXXX {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.file = null;
    this.chid = 0;
    this.k = ""; // "program","volume","pan","egs"
    this.events = [];
  }
  
  setup(file, chid, k) {
    this.file = file;
    this.chid = chid;
    this.k = k;
    this.events = this.getRelevantEvents();
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", ["title"], `${this.chid}.${this.k}`);
    const table = this.dom.spawn(this.element, "TABLE", { "on-input": e => this.onInput(e) });
    const presentCheckbox = this.dom.spawn(null, "INPUT", { type: "checkbox", name: "present" });
    if (this.events.length > 0) presentCheckbox.checked = true;
    this.spawnRow(table, "Present", presentCheckbox);
    switch (this.k) {
      case "program": this.buildTableProgram(table); break;
      case "volume": this.buildTableVolume(table); break;
      case "pan": this.buildTablePan(table); break;
      case "egs": this.buildTableEgs(table); break;
    }
    const controlsRow = this.dom.spawn(this.element, "DIV", ["controlsRow"]);
    this.dom.spawn(controlsRow, "INPUT", { type: "button", value: "Save", "on-click": () => this.onSave() });
  }
  
  buildTableProgram(table) {
    let bankMsb=0, bankLsb=0, pid=0;
    for (const event of this.events) {
      switch (event.opcode) {
        case 0xb0: switch (event.a) {
            case 0x00: bankMsb = event.b; break;
            case 0x20: bankLsb = event.b; break;
          } break;
        case 0xc0: pid = event.a; break;
      }
    }
    this.spawnRow(table, "Bank MSB", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 127, name: "bankMsb", value: bankMsb }));
    this.spawnRow(table, "Bank LSB", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 127, name: "bankLsb", value: bankLsb }));
    this.spawnRow(table, "PID", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 127, name: "pid", value: pid }));
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["tip", "reprProgram"], { colspan: 2 }, this.reprProgram(bankMsb, bankLsb, pid));
  }
  
  buildTableVolume(table) {
    let value=0x40;
    if (this.events.length > 0) {
      value = this.events[0].b;
    }
    this.spawnRow(table, "Volume", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 127, name: "number", value }));
    this.spawnRow(table, "", this.dom.spawn(null, "INPUT", { type: "range", min: 0, max: 127, name: "slider", value }));
  }
  
  buildTablePan(table) {
    let value=0x40;
    if (this.events.length > 0) {
      value = this.events[0].b;
    }
    this.spawnRow(table, "Pan", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 127, name: "number", value }));
    this.spawnRow(table, "", this.dom.spawn(null, "INPUT", { type: "range", min: 0, max: 127, name: "slider", value }));
  }
  
  buildTableEgs(table) {
    //TODO We ought to show the whole EGS channel header editor.
    // Probably that should be its own modal, not MidiChannelFieldModal.
  }
  
  spawnRow(table, label, control) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["key"], label);
    this.dom.spawn(tr, "TD", ["value"], control || "");
  }
  
  getRelevantEvents() {
    const events = [];
    if (!this.file) return events;
    for (const track of this.file.tracks) {
      for (const event of track) {
        if (event.chid !== this.chid) continue;
        switch (this.k) {
          case "program": {
              if ((event.opcode === 0xb0) && (event.a === 0x00)) events.push(event);
              else if ((event.opcode === 0xb0) && (event.a === 0x20)) events.push(event);
              else if (event.opcode === 0xc0) events.push(event);
            } break;
          case "volume": {
              if ((event.opcode === 0xb0) && (event.a === 0x07)) events.push(event);
            } break;
          case "pan": {
              if ((event.opcode === 0xb0) && (event.a === 0x0a)) events.push(event);
            } break;
          case "egs": {
              if ((event.opcode === 0xff) && (event.a === 0xf0)) events.push(event);
            } break;
        }
      }
    }
    return events;
  }
  
  reprProgram(bankMsb, bankLsb, pid) {
    const gm = MidiFile.GM_PROGRAM_NAMES[pid];
    if (!gm) return "";
    if (bankMsb || bankLsb) return `(maybe) ${gm}`;
    return gm;
  }
  
  onInput(event) {
  
    /* Determine what just changed.
     * (v) may be number or boolean, or string by default.
     */
    let k="", v="";
    for (let element=event.target; element; element=element.parentNode) {
      const name = element.getAttribute?.("name");
      if (name) {
        k = name;
        if (element.type === "checkbox") {
          v = element.checked;
        } else {
          const n = +element.value;
          if (isNaN(n)) v = element.value;
          else v = n;
        }
        break;
      }
    }
    if (!k) return;
    
    /* "present" can change freely.
     * Arguably we should set all the others to their default when it goes false, but I think that would be inconvenient to the user.
     */
    if (k === "present") return;
    
    /* A change to any other field forces "present" true (even if you're changing it to the default).
     */
    const present = this.element.querySelector("input[name='present']");
    if (present) present.checked = true;
    
    /* If there are "number" and "slider" fields, they share a value.
     */
    if (k === "number") {
      const slider = this.element.querySelector("input[name='slider']");
      if (slider) slider.value = v;
    } else if (k === "slider") {
      const number = this.element.querySelector("input[name='number']");
      if (number) number.value = v;
    }
    
    /* Any change to program, update the tip.
     */
    if (this.k === "program") {
      const dst = this.element.querySelector(".reprProgram");
      if (dst) {
        const bankMsb = +this.element.querySelector("input[name='bankMsb']")?.value;
        const bankLsb = +this.element.querySelector("input[name='bankLsb']")?.value;
        const pid = +this.element.querySelector("input[name='pid']")?.value;
        dst.innerText = this.reprProgram(bankMsb, bankLsb, pid);
      }
    }
  }
  
  readModelFromDom() {
    const model = {};
    for (const element of this.element.querySelectorAll("input[name]")) {
      if (element.type === "checkbox") {
        model[element.name] = element.checked;
      } else {
        const n = +element.value;
        if (isNaN(n)) model[element.name] = element.value;
        else model[element.name] = n;
      }
    }
    return model;
  }
  
  applyProgram(model) {
    // To keep things neat, let's delete whatever is present, then create new events as needed.
    // The bank events are a bit unusual, we won't create them for zero, even though "present" was set.
    for (const event of this.events) this.file.deleteEvent(event.id, false);
    this.file.addZEvent(this.chid, { opcode: 0xc0, a: model.pid });
    if (model.bankLsb) this.file.addZEvent(this.chid, { opcode: 0xb0, a: 0x20, b: model.bankLsb });
    if (model.bankMsb) this.file.addZEvent(this.chid, { opcode: 0xb0, a: 0x00, b: model.bankMsb });
  }
  
  applyControl(k, v) {
    if (!this.events.length) {
      this.file.addZEvent(this.chid, { opcode: 0xb0, a: k, b: v });
    } else {
      this.file.replaceEvent({
        ...this.events[0],
        b: v,
      });
      for (let i=1; i<this.events.length; i++) {
        this.file.deleteEvent(this.events[i].id, false);
      }
    }
  }
  
  applyChanges(model) {
    switch (this.k) {
      case "program": this.applyProgram(model); break;
      case "volume": this.applyControl(0x07, model.number); break;
      case "pan": this.applyControl(0x0a, model.number); break;
      default: console.warn(`MidiChannelFieldModal.applyChanges: Unexpected key ${JSON.stringify(this.k)}`);
    }
  }
  
  removeEvents() {
    for (const event of this.events) {
      this.file.deleteEvent(event.id, false);
    }
  }
  
  onSave() {
    if (!this.file) return this.reject();
    const model = this.readModelFromDom();
    if (model.present) this.applyChanges(model);
    else this.removeEvents();
    this.resolve(true);
  }
}
