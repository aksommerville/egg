/* EgsEditor.js
 * For single EGS-text sounds or songs. May be from standalone files, or segments of an MSF file.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";
import { EgsFile } from "./EgsFile.js";
 
export class EgsEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, "nonce"];
  }
  constructor(element, dom, data, nonce) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.nonce = nonce;
    
    this.res = null;
    this.egs = null;
  }
  
  static checkResource(res) {
    if (!res.serial.length) return 1;
    // Read to the first non-empty line, be mindful of comments. If it starts "song" or "channel", it's EGS.
    const decoder = new TextDecoder("utf8");
    for (let srcp=0; srcp<res.serial.length; ) {
      if (res.serial[srcp] <= 0x20) { srcp++; continue; }
      let nlp = res.serial.indexOf(0x0a, srcp);
      if (nlp < 0) nlp = res.serial.length;
      const line = decoder.decode(res.serial.slice(srcp, nlp)).split('#')[0].trim();
      srcp = nlp + 1;
      if (!line) continue;
      if (line.startsWith("song")) return 2;
      if (line.startsWith("channel")) return 2;
      return 0;
    }
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.egs = new EgsFile(this.res.serial);
    console.log(`EgsEditor.setup`, { res, egs: this.egs });
    this.buildUi();
  };
  
  /* Build UI.
   * Plain buildUi discards all transient state and rebuilds from (this.egs).
   * buildUiMode() can be called at any time, and will use the mode from our radio buttons.
   ************************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    const channels = this.dom.spawn(this.element, "DIV", ["channels"]);
    const events = this.dom.spawn(this.element, "DIV", ["events"]);
    if (!this.egs) return;
    
    for (const channel of this.egs.channels) {
      const mode = channel.getMode();
      const card = this.dom.spawn(channels, "DIV", ["channel"], { "data-chid": channel.chid });
      
      this.dom.spawn(card, "DIV", ["title"], `Channel ${channel.chid}`);
      this.dom.spawn(card, "INPUT", { type: "range", name: "master", min: 0, max: 0xff, value: channel.getMaster() });
      this.dom.spawn(card, "INPUT", { type: "range", name: "pan", min: 0, max: 0xff, value: channel.getPan() });
      
      const drumsId = `EgsEditor-${this.nonce}-${channel.chid}-mode-drums`;
      const subId = `EgsEditor-${this.nonce}-${channel.chid}-mode-sub`;
      const fmId = `EgsEditor-${this.nonce}-${channel.chid}-mode-fm`;
      let name = `EgsEditor-${this.nonce}-${channel.chid}-mode`;
      this.dom.spawn(card, "DIV", ["radioBar"], { "on-change": () => this.onModeChanged(channel.chid) },
        this.dom.spawn(null, "INPUT", { id: drumsId, type: "radio", name, value: "drums" }),
        this.dom.spawn(null, "LABEL", { for: drumsId }, "Drums"),
        this.dom.spawn(null, "INPUT", { id: subId, type: "radio", name, value: "sub" }),
        this.dom.spawn(null, "LABEL", { for: subId }, "Sub"),
        this.dom.spawn(null, "INPUT", { id: fmId, type: "radio", name, value: "fm" }),
        this.dom.spawn(null, "LABEL", { for: fmId }, "FM"),
      );
      const modeElement = card.querySelector(`.radioBar input[value='${mode}']`);
      if (modeElement) modeElement.checked = true;
      
      this.dom.spawn(card, "DIV", ["modeConfig"]);
      this.buildUiMode(card, channel);
      
      this.dom.spawn(card, "DIV", ["post"]);
      this.buildUiPost(card, channel);
    }
    
    for (const event of this.egs.events) {
      this.dom.spawn(events, "DIV", ["event"], JSON.stringify(event));
    }
  }
  
  buildUiMode(card, channel) {
    if (!card || !channel) return;
    switch (card.querySelector(`.radioBar input:checked`)?.value) {
      case "drums": this.buildUiDrums(card, channel); break;
      case "sub": this.buildUiSub(card, channel); break;
      case "fm": this.buildUiFm(card, channel); break;
    }
  }
  
  buildUiDrums(card, channel) {
    const parent = card.querySelector(".modeConfig");
    parent.innerHTML = "";
    const src = channel.drums || [];
    const table = this.dom.spawn(parent, "TABLE");
    this.spawnIntRow(table, "rid", (src[0] << 8) | src[1]);
    this.spawnIntRow(table, "bias", ((src[2] & 0x80) ? (~0xff | src[2]) : src[2]) || 0);
    this.spawnIntRow(table, "trimlo", src[3] || 0);
    this.spawnIntRow(table, "trimhi", src[4] || 0);
  }
  
  buildUiSub(card, channel) {
    const parent = card.querySelector(".modeConfig");
    parent.innerHTML = "";
    const table = this.dom.spawn(parent, "TABLE");
    this.spawnIntRow(table, "wheel", this.param(channel.wheel, 0, 2));
    this.spawnIntRow(table, "sub", this.param(channel.sub, 0, 2));
    this.spawnEnvRow(table, "level", channel.level, channel.chid);
  }
  
  buildUiFm(card, channel) {
    const parent = card.querySelector(".modeConfig");
    parent.innerHTML = "";
    const table = this.dom.spawn(parent, "TABLE");
    this.spawnIntRow(table, "wheel", this.param(channel.wheel, 0, 2));
    this.spawnIntRow(table, "shape", this.param(channel.shape, 0, 1)); // TODO enum
    this.spawnHarmonicsRow(table, "harmonics", channel.harmonics, channel.chid);
    this.spawnIntRow(table, "fmrate", this.param(channel.fm, 0, 2, 0, false, 8));
    this.spawnIntRow(table, "fmrange", this.param(channel.fm, 2, 2, 0, false, 8));
    this.spawnEnvRow(table, "fmenv", channel.fmenv, channel.chid);
    this.spawnLfoRow(table, "fmlfo", channel.fmlfo);
    this.spawnEnvRow(table, "pitchenv", channel.pitchenv, channel.chid);
    this.spawnLfoRow(table, "pitchlfo", channel.pitchlfo);
    this.spawnEnvRow(table, "level", channel.level, channel.chid);
  }
  
  buildUiPost(card, channel) {
    const parent = card.querySelector(".post");
    parent.innerHTML = "";
    this.dom.spawn(parent, "DIV", "TODO: Post");
  }
  
  spawnIntRow(table, label, v) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", label);
    this.dom.spawn(tr, "TD",
      this.dom.spawn(null, "INPUT", { type: "number", value: v, name: label })
    );
  }
  
  spawnEnvRow(table, label, serial, chid) {
    if (!serial) serial = [];
    const value = `env, ${serial.length} bytes`;
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", label);
    this.dom.spawn(tr, "TD",
      this.dom.spawn(null, "INPUT", { type: "button", value, "on-click": () => this.onEditEnv(chid, label) })
    );
  }
  
  spawnLfoRow(table, label, serial) {
    if (!serial) serial = [];
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", label);
    const td = this.dom.spawn(tr, "TD", ["lfo"]);
    this.dom.spawn(td, "DIV", 
      this.dom.spawn(null, "INPUT", { type: "number", name: `${label}.period`, value: this.param(serial, 0, 2) }),
      this.dom.spawn(null, "INPUT", { type: "number", name: `${label}.depth`, value: this.param(serial, 2, 2) })
    );
    this.dom.spawn(td, "INPUT", { type: "range", min: 0, max: 0xff, name: `${label}.phase`, value: this.param(serial, 4, 1) });
  }
  
  spawnHarmonicsRow(table, label, serial, chid) {
    if (!serial) serial = [];
    const value = `${serial.length >> 1} coefficients`;
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", label);
    this.dom.spawn(tr, "TD",
      this.dom.spawn(null, "INPUT", { type: "button", value, "on-click": () => this.onEditHarmonics(chid, label) })
    );
  }
  
  /* Reencode.
   ***************************************************************************/
  
  // Read an integer from some serial field in channel.
  param(src, p, c, bias, signed, fractBitCount) {
    if (!src) src = [];
    let v = src[p] || 0;
    let top = 0x80;
    while (c-- > 1) {
      p++;
      v <<= 8;
      v |= src[p] || 0;
      top <<= 8;
    }
    if (signed && (v & top)) v |= ~(top - 1);
    if (bias) v += bias;
    if (fractBitCount) v /= (1 << fractBitCount);
    return v;
  }
  
  encode() {
    return this.res.serial;//TODO
  }
  
  /* Events.
   ***************************************************************************/
   
  onModeChanged(chid) {
    this.buildUiMode(
      this.element.querySelector(`.channel[data-chid='${chid}']`),
      this.egs?.channels.find(c => c.chid === chid)
    );
  }
  
  onEditEnv(chid, label) {
    console.log(`EgsEditor.onEditEnv(${chid},${label})`);
  }
  
  onEditHarmonics(chid, label) {
    console.log(`EgsEditor.onEditHarmonics(${chid},${label})`);
  }
}
