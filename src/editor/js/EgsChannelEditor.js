/* EgsChannelEditor.js XXX
 * For a single EGS Channel Header, excluding chid, master, and pan.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";
import { EnvUi } from "./EnvUi.js";
import { EGS_CHANNEL_PRESETS } from "./egsPresets.js";
import { EgsOpCard } from "./EgsOpCard.js";
import { MidiFile } from "./MidiFile.js";
import { MidiEditor } from "./MidiEditor.js";
import { EditorModal } from "./EditorModal.js";

export class EgsChannelEditorXXX {
  static getDependencies() {
    return [HTMLElement, Dom, Data, "nonce"];
  }
  constructor(element, dom, data, nonce) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.nonce = nonce;
    
    this.res = null; // Never a real resource.
    this.waveEnv = null;
    this.fmEnv = null;
    this.subEnv = null;
    this.postCards = []; // BEWARE: Order will not match the UI, if user moves cards around. UI is correct.
    this.drumId = 1;
    this.drumSerials = []; // Sparse, indexed by drumId. Populates along with UI.
    this.fullSong = false;
  }
  
  static checkResource(res) {
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.fullSong = res.fullSong || false;
    this.drumSerials = [];
    this.buildUi();
  }
  
  /* UI.
   ********************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    
    const voice = this.dom.spawn(this.element, "DIV", ["voice"]);
    
    this.dom.spawn(voice, "SELECT", ["preset"], { "on-change": e => this.onSelectPreset(e) },
      this.dom.spawn(null, "OPTION", { value: "" }, "Overwrite with preset..."),
      ...EGS_CHANNEL_PRESETS.map(p => this.dom.spawn(null, "OPTION", { value: p.name }, p.name))
    );
    
    let idBase = `EgsChannelEditor-${this.nonce}-mode-`;
    const modeBar = this.dom.spawn(voice, "DIV", ["radioBar"], { "data-k": "mode", "on-change": () => this.onModeChanged() });
    this.dom.spawn(modeBar, "INPUT", { type: "radio", name: `${this.nonce}-mode`, id: idBase + "noop", value: 0 });
    this.dom.spawn(modeBar, "LABEL", { for: idBase + "noop" }, "Noop");
    if (this.fullSong) {
      this.dom.spawn(modeBar, "INPUT", { type: "radio", name: `${this.nonce}-mode`, id: idBase + "drums", value: 1 });
      this.dom.spawn(modeBar, "LABEL", { for: idBase + "drums" }, "Drums");
    }
    this.dom.spawn(modeBar, "INPUT", { type: "radio", name: `${this.nonce}-mode`, id: idBase + "wave", value: 2 });
    this.dom.spawn(modeBar, "LABEL", { for: idBase + "wave" }, "Wave");
    this.dom.spawn(modeBar, "INPUT", { type: "radio", name: `${this.nonce}-mode`, id: idBase + "fm", value: 3 });
    this.dom.spawn(modeBar, "LABEL", { for: idBase + "fm" }, "FM");
    this.dom.spawn(modeBar, "INPUT", { type: "radio", name: `${this.nonce}-mode`, id: idBase + "sub", value: 4 });
    this.dom.spawn(modeBar, "LABEL", { for: idBase + "sub" }, "Sub");
    const modeElement = voice.querySelector(`.radioBar input[value='${this.modeFromRes()}']`);
    if (modeElement) modeElement.checked = true;
    
    const modeContainer = this.dom.spawn(voice, "DIV", ["modeContainer"], { "on-input": () => this.dirty() });
    this.rebuildModeContainer();
    
    const post = this.dom.spawn(this.element, "DIV", ["post"]);
    this.rebuildPost();
  }
  
  rebuildModeContainer() {
    const modeContainer = this.element.querySelector(".modeContainer");
    modeContainer.innerHTML = "";
    const mode = +this.element.querySelector(".radioBar[data-k='mode'] input:checked")?.value;
    const serial = this.modeConfigFromResIfMode(mode);
    switch (mode) {
      case 1: this.rebuildModeDrums(modeContainer, serial); break;
      case 2: this.rebuildModeWave(modeContainer, serial); break;
      case 3: this.rebuildModeFm(modeContainer, serial); break;
      case 4: this.rebuildModeSub(modeContainer, serial); break;
    }
  }
  
  rebuildModeDrums(parent, serial) {
    this.drumSerials = [];
    for (let srcp=0; srcp<serial.length; ) {
      const noteid = serial[srcp++] || 0;
      const pan = serial[srcp++] || 0;
      const trimlo = serial[srcp++] || 0;
      const trimhi = serial[srcp++] || 0;
      const len = (serial[srcp] << 8) | serial[srcp + 1];
      srcp += 2;
      if (srcp > serial.length - len) break;
      const card = this.dom.spawn(parent, "DIV", ["drum"]);
      this.populateDrumCard(card, noteid, pan, trimlo, trimhi, serial.slice(srcp, srcp + len));
      srcp += len;
    }
    this.dom.spawn(parent, "INPUT", ["add"], { type: "button", value: "+", "on-click": () => this.onAddDrum() })
  }
  
  populateDrumCard(card, noteid, pan, trimlo, trimhi, serial) {
    const id = this.drumId++;
    this.drumSerials[id] = serial;
    card.setAttribute("data-drumId", id);
    this.dom.spawn(card, "DIV", ["title"], `${MidiFile.reprNote(noteid, true)} ${MidiFile.GM_DRUM_NAMES[noteid] || ''}`);
    const controls = this.dom.spawn(card, "DIV", ["controls"]);
    this.dom.spawn(controls, "INPUT", { type: "button", value: "X", "on-click": () => this.onDeleteDrum(id) });
    this.dom.spawn(controls, "INPUT", { type: "number", min: 0, max: 255, value: noteid, name: "noteid", "on-input": () => this.onDrumNoteidChanged(id) });
    this.dom.spawn(controls, "INPUT", { type: "number", min: 0, max: 255, value: pan, name: "pan" });
    this.dom.spawn(controls, "INPUT", { type: "number", min: 0, max: 255, value: trimlo, name: "trimlo" });
    this.dom.spawn(controls, "INPUT", { type: "number", min: 0, max: 255, value: trimhi, name: "trimhi" });
    this.dom.spawn(controls, "INPUT", { type: "button", value: "Edit...", "on-click": () => this.onEditDrum(id) });
  }
  
  rebuildModeWave(parent, serial) {
    const table = this.dom.spawn(parent, "TABLE");
    this.spawnIntRow(table, "Wheel range", "wheelrange", 0, 65535, (serial[0] << 8) | serial[1]);
    
    const trenv = this.dom.spawn(table, "TR");
    const tdenv = this.dom.spawn(trenv, "TD", { colspan: 2 });
    const level = this.waveEnv ? this.waveEnv.encodeLevel() : serial.slice(2, 14);
    const pitch = this.waveEnv ? this.waveEnv.encodePitch() : serial.slice(14, 30);
    this.waveEnv = this.dom.spawnController(tdenv, EnvUi);
    this.waveEnv.setLevel(level);
    this.waveEnv.setPitch(pitch);
    this.waveEnv.dirty = () => this.dirty();
    
    this.spawnIntRow(table, "Shape", "shape", 0, 255, serial[30] || 0);//TODO datalist or select or comment
    this.spawnTextRow(table, "Harmonics", "harmonics", this.reprHarmonics(serial.slice(32, 32 + (serial[31] * 2))));
  }
  
  rebuildModeFm(parent, serial) {
    const table = this.dom.spawn(parent, "TABLE");
    this.spawnIntRow(table, "Wheel range", "wheelrange", 0, 65535, (serial[0] << 8) | serial[1]);
    
    const trenv = this.dom.spawn(table, "TR");
    const tdenv = this.dom.spawn(trenv, "TD", { colspan: 2 });
    const level = this.fmEnv ? this.fmEnv.encodeLevel() : serial.slice(2, 14);
    const pitch = this.fmEnv ? this.fmEnv.encodePitch() : serial.slice(14, 30);
    const range = this.fmEnv ? this.fmEnv.encodeRange() : serial.slice(34, 50);
    this.fmEnv = this.dom.spawnController(tdenv, EnvUi);
    this.fmEnv.setLevel(level);
    this.fmEnv.setPitch(pitch);
    this.fmEnv.setRange(range);
    this.fmEnv.dirty = () => this.dirty();
    
    this.spawnIntRow(table, "Mod Rate", "modrate", 0, 65535, (serial[30] << 8) | serial[31]); //TODO fixed-point
    this.spawnIntRow(table, "Mod Range", "modrange", 0, 65535, (serial[32] << 8) | serial[33]); //''
  }
  
  rebuildModeSub(parent, serial) {
    const table = this.dom.spawn(parent, "TABLE");
    
    const trenv = this.dom.spawn(table, "TR");
    const tdenv = this.dom.spawn(trenv, "TD", { colspan: 2 });
    const level = this.subEnv ? this.subEnv.encodeLevel() : serial.slice(0, 12);
    this.subEnv = this.dom.spawnController(tdenv, EnvUi);
    this.subEnv.setLevel(level);
    this.subEnv.dirty = () => this.dirty();
    
    this.spawnIntRow(table, "Width (hz)", "subwidth", 0, 65535, (serial[12] << 8) | serial[13]);
  }
  
  rebuildPost() {
    const post = this.element.querySelector(".post");
    post.innerHTML = "";
    const modelen = (this.res.serial[1] << 8) | this.res.serial[2];
    const postp = 3 + modelen + 2;
    const postlen = (this.res.serial[3 + modelen] << 8) | this.res.serial[3 + modelen + 1];
    this.postCards = [];
    for (let srcp=postp; srcp<this.res.serial.length; ) {
      const k = this.res.serial[srcp++];
      const vc = this.res.serial[srcp++];
      if (srcp > this.res.serial.length - vc) break;
      const v = this.res.serial.slice(srcp, srcp + vc);
      const card = this.dom.spawnController(post, EgsOpCard);
      card.cbDelete = () => this.deletePostCard(card);
      card.cbMove = (dy) => this.movePostCard(card.element, dy);
      card.cbMute = (mute) => this.dirty(); // Mute is just "delete" but undoable.
      card.cbDirty = () => this.dirty();
      card.setup(k, v);
      this.postCards.push(card);
      srcp += vc;
    }
    this.dom.spawn(post, "INPUT", { name: "add-post-step", type: "button", value: "+", "on-click": () => this.onAddPost() });
  }
  
  spawnIntRow(table, label, name, min, max, value) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["key"], label);
    this.dom.spawn(tr, "TD", ["value"],
      this.dom.spawn(null, "INPUT", { type: "number", name, min, max, value })
    );
  }
  
  spawnTextRow(table, label, name, value) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["key"], label);
    this.dom.spawn(tr, "TD", ["value"],
      this.dom.spawn(null, "INPUT", { name, value })
    );
  }
  
  /* Model.
   *********************************************************************************/
   
  modeFromRes() {
    if (this.res?.serial.length >= 1) return this.res.serial[0];
    return 0;
  }
  
  modeConfigFromResIfMode(mode) {
    if (!this.res) return this.defaultModeConfig(mode);
    if (this.res.serial[0] !== mode) return this.defaultModeConfig(mode);
    const len = (this.res.serial[1] << 8) | this.res.serial[2];
    const serial = this.res.serial.slice(3, 3 + len);
    if (!this.okModeConfigLength(mode, serial)) return this.defaultModeConfig(mode);
    return serial;
  }
  
  defaultModeConfig(mode) {
    switch (mode) {
      case 1: return new Uint8Array([ // drums
        ]);
      case 2: return new Uint8Array([ // wave
          0,200, // wheelrange
          0x20,0x10, // atkt
          0x40,0xff, // atkv
          0x20,0x20, // dect
          0x20,0x40, // susv
          0x00,0x80, 0x01,0x00, // rlst
          0x80,0x00, 0x80,0x00, // pitchenv (noop)...
          0x80,0x00, 0x80,0x00,
          0x80,0x00, 0x80,0x00,
          0x80,0x00, 0x80,0x00,
          1, // shape (sine)
          0, // coefc
        ]);
      case 3: return new Uint8Array([ // fm
          0,200, // wheelrange
          0x20,0x10, // atkt
          0x40,0xff, // atkv
          0x20,0x20, // dect
          0x20,0x40, // susv
          0x00,0x80, 0x01,0x00, // rlst
          0x80,0x00, 0x80,0x00, // pitchenv (noop)...
          0x80,0x00, 0x80,0x00,
          0x80,0x00, 0x80,0x00,
          0x80,0x00, 0x80,0x00,
          0x01,0x00, // modrate
          0x01,0x00, // range
          0xff,0xff, 0xff,0xff, // rangeenv (noop)...
          0xff,0xff, 0xff,0xff,
          0xff,0xff, 0xff,0xff,
          0xff,0xff, 0xff,0xff,
        ]);
      case 4: return new Uint8Array([ // sub
          0x20,0x10, // atkt
          0x40,0xff, // atkv
          0x20,0x20, // dect
          0x20,0x40, // susv
          0x00,0x80, 0x01,0x00, // rlst
          0x01,0x00, // width, hz
        ]);
    }
    return [];
  }
  
  okModeConfigLength(mode, serial) {
    switch (mode) {
      case 1: return true; // DRUM: Could be validated but would have to scan all the way thru.
      case 2: {
          if (serial.length < 32) return false;
          const c = serial[31];
          return (serial.length === 32 + 2 * c);
        }
      case 3: return (serial.length === 50);
      case 4: return (serial.length === 14);
    }
    return (serial.length === 0);
  }
  
  reprHarmonics(src) {
    let dst = "";
    for (let srcp=0; srcp<src.length; srcp+=2) {
      const v = (src[srcp] << 8) | src[srcp+1];
      dst += v.toString(16).padStart(4, '0');
      dst += " "
    }
    return dst;
  }
  
  evalHarmonics(src) {
    const dst = src.split(/\s+/g).map(v => parseInt(v, 16)).filter(v => ((typeof(v) === "number") && !isNaN(v)));
    return dst;
  }
  
  encodeDrumsConfig(dst) {
    for (const card of this.element.querySelectorAll(".modeContainer .drum")) {
      const drumId = +card.getAttribute("data-drumId");
      const noteid = +card.querySelector("input[name='noteid']")?.value || 0;
      const pan = +card.querySelector("input[name='pan']")?.value || 0;
      const trimlo = +card.querySelector("input[name='trimlo']")?.value || 0;
      const trimhi = +card.querySelector("input[name='trimhi']")?.value || 0;
      const serial = this.drumSerials[drumId] || [];
      dst.push(noteid);
      dst.push(pan);
      dst.push(trimlo);
      dst.push(trimhi);
      dst.push(serial.length >> 8);
      dst.push(serial.length & 0xff);
      for (const b of serial) dst.push(b);
    }
  }
  
  encodeWaveConfig(dst) {
    const wheelrange = +this.element.querySelector("input[name='wheelrange']").value;
    dst.push(wheelrange >> 8);
    dst.push(wheelrange & 0xff);
    for (const b of this.waveEnv.encodeLevel()) dst.push(b);
    for (const b of this.waveEnv.encodePitch()) dst.push(b);
    dst.push(+this.element.querySelector("input[name='shape']").value);
    const coefv = this.evalHarmonics(this.element.querySelector("input[name='harmonics']").value);
    dst.push(coefv.length);
    for (const coef of coefv) {
      dst.push(coef >> 8);
      dst.push(coef & 0xff);
    }
  }
  
  encodeFmConfig(dst) {
    const wheelrange = +this.element.querySelector("input[name='wheelrange']").value;
    const modrate = +this.element.querySelector("input[name='modrate']").value;
    const modrange = +this.element.querySelector("input[name='modrange']").value;
    dst.push(wheelrange >> 8);
    dst.push(wheelrange & 0xff);
    for (const b of this.fmEnv.encodeLevel()) dst.push(b);
    for (const b of this.fmEnv.encodePitch()) dst.push(b);
    dst.push(modrate >> 8);
    dst.push(modrate & 0xff);
    dst.push(modrange >> 8);
    dst.push(modrange & 0xff);
    for (const b of this.fmEnv.encodeRange()) dst.push(b);
  }
  
  encodeSubConfig(dst) {
    for (const b of this.subEnv.encodeLevel()) dst.push(b);
    const subwidth = +this.element.querySelector("input[name='subwidth']").value;
    dst.push(subwidth >> 8);
    dst.push(subwidth & 0xff);
  }
  
  encodePost(dst) {
    // The order of UI elements is what we emit. Importantly, it might not match the order of (this.postCards).
    const elements = Array.from(this.element.querySelectorAll(".EgsOpCard"));
    for (const element of elements) {
      const card = this.postCards.find(c => c.element === element);
      if (!card) throw new Error(`Failed to locate controller for EGS post op`);
      dst.push(card.k);
      const lenp = dst.length;
      for (const b of card.encode()) dst.push(b);
      dst.splice(lenp, 0, dst.length - lenp);
    }
  }
  
  encode() {
    const dst = [];
    const mode = +this.element.querySelector(".radioBar[data-k='mode'] input:checked")?.value;
    if (!mode) return new Uint8Array([0,0,0,0,0]); // Either not found or noop, either way emit a noop
    dst.push(mode);
    switch (mode) {
      case 1: this.encodeDrumsConfig(dst); break;
      case 2: this.encodeWaveConfig(dst); break;
      case 3: this.encodeFmConfig(dst); break;
      case 4: this.encodeSubConfig(dst); break;
    }
    const cfglen = dst.length - 1;
    dst.splice(1, 0, cfglen >> 8, cfglen & 0xff);
    const postlenp = dst.length;
    this.encodePost(dst);
    const postlen = dst.length - postlenp;
    dst.splice(postlenp, 0, postlen >> 8, postlen & 0xff);
    return new Uint8Array(dst);
  }
  
  /* Events.
   **********************************************************************************/
   
  dirty() {
    if (!this.res) return;
    this.data.dirty(this.res.path, () => this.encode());
  }
   
  onSelectPreset(event) {
    if (!this.res) return;
    const select = this.element.querySelector("select.preset");
    const preset = EGS_CHANNEL_PRESETS.find(p => p.name === select.value);
    select.value = "";
    if (!preset) return;
    this.res = {
      ...this.res,
      serial: preset.serial,
    };
    this.buildUi();
    this.dirty();
  }
  
  onModeChanged() {
    this.rebuildModeContainer();
    this.dirty();
  }
  
  onDeleteDrum(id) {
    const row = this.element.querySelector(`.drum[data-drumId='${id}']`);
    if (!row) return;
    row.remove();
    delete this.drumSerials[id];
    this.dirty();
  }
  
  nextDrumNoteid() {
    const present = Array.from(this.element.querySelectorAll(".modeContainer .drum input[name='noteid']")).map(e => +e.value).filter(v => !isNaN(v));
    if (!present.length) return 35; // Acoustic Bass Drum; the lowest assigned GM drum.
    present.sort((a, b) => a - b);
    // First, if the highest present note is below 81 (Open Triangle; highest assigned drum), take the next after it.
    const max = present[present.length - 1];
    if (max < 81) return max + 1;
    // Next, if there's a gap in the present noteids, and below 128, take that.
    for (let i=0, noteid=present[0]; (i<present.length) && (noteid<128); i++, noteid++) {
      if (present[i] !== noteid) return noteid;
    }
    // Finally, whatever, use zero and let the user figure it out.
    return 0;
  }
  
  onAddDrum() {
    const parent = this.element.querySelector(".modeContainer");
    if (!parent) return;
    const noteid = this.nextDrumNoteid();
    const card = this.dom.spawn(null, "DIV", ["drum"]);
    this.populateDrumCard(card, noteid, 0x80, 0x80, 0xff, new Uint8Array(0));
    parent.insertBefore(card, parent.querySelector("input.add"));
    this.dirty();
  }
  
  onEditDrum(id) {
    const card = this.element.querySelector(`.drum[data-drumId='${id}']`);
    const serial = this.drumSerials[id];
    if (!card || !serial) return;
    const modal = this.dom.spawnModal(EditorModal);
    modal.setupWithSerial(MidiEditor, serial);
    modal.result.then((result) => {
      if (!result) return;
      this.drumSerials[id] = result;
      this.dirty();
    }).catch(e => this.dom.modalError(e));
  }
  
  onDrumNoteidChanged(id) {
    const focusCard = this.element.querySelector(`.drum[data-drumId='${id}']`);
    if (!focusCard) return;
    const focusInput = focusCard.querySelector("input[name='noteid']");
    if (!focusInput) return;
    const focusNoteid = +focusInput.value;
    if (isNaN(focusNoteid)) return;
    const title = focusCard.querySelector(".title");
    if (title) {
      title.innerText = `${MidiFile.reprNote(focusNoteid, true)} ${MidiFile.GM_DRUM_NAMES[focusNoteid] || ''}`;
    }
    for (const card of this.element.querySelectorAll(".drum")) {
      const drumId = +card.getAttribute("data-drumId");
      if (drumId === id) continue;
      const input = card.querySelector("input[name='noteid']");
      if (!input) continue;
      const noteid = +input.value;
      if (isNaN(noteid)) continue;
      if (noteid === focusNoteid) {
        focusInput.classList.add("invalid");
        return;
      }
    }
    focusInput.classList.remove("invalid");
  }
  
  deletePostCard(controller) {
    let p = this.postCards.indexOf(controller);
    if (p >= 0) this.postCards.splice(p, 1);
    controller.element.remove();
    this.dirty();
  }
  
  movePostCard(element, dy) {
    const peers = Array.from(element.parentNode?.children || []).filter(e => e.tagName === "DIV");
    const p = peers.indexOf(element);
    if (p < 0) return;
    let np = p + dy;
    if (dy > 0) np++;
    if ((np < 0) || (np > peers.length)) return;
    this.dom.ignoreNextRemoval(element);
    element.parentNode.insertBefore(element, peers[np]);
    this.dirty();
  }
  
  onAddPost() {
    const keys = ["waveshaper", "delay", "tremolo", "lopass", "hipass", "bpass", "notch"];
    this.dom.modalPickOne(keys).then((choice) => {
      if (!choice) return;
      const k = keys.indexOf(choice) + 1;
      const v = new Uint8Array([0,0,0,0,0,0,0,0]);//TODO Is it ok that this is not the expected length?
      const post = this.element.querySelector(".post");
      post.querySelector("input[name='add-post-step']")?.remove();
      const card = this.dom.spawnController(post, EgsOpCard);
      card.cbDelete = () => this.deletePostCard(card);
      card.cbMove = (dy) => this.movePostCard(card.element, dy);
      card.cbMute = (mute) => this.dirty(); // Mute is just "delete" but undoable.
      card.cbDirty = () => this.dirty();
      card.setup(k, v);
      this.postCards.push(card);
      this.dom.spawn(post, "INPUT", { name: "add-post-step", type: "button", value: "+", "on-click": () => this.onAddPost() });
    }).catch(() => {});
  }
}
