/* MsfEditor.js
 * For multi-sound text files, and the special "instruments" file.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";
import { MsfFile } from "./MsfFile.js";
import { MsfTitleModal } from "./MsfTitleModal.js";
import { EditorModal } from "./EditorModal.js";
import { EgsEditor } from "./EgsEditor.js";
 
export class MsfEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    
    this.res = null;
    this.msf = null;
  }
  
  static checkResource(res) {
    if (!res.serial.length) return 1;
    // Read to the first non-empty line, be mindful of comments. If it starts "sound" or "instrument", it's MSF.
    const decoder = new TextDecoder("utf8");
    for (let srcp=0; srcp<res.serial.length; ) {
      if (res.serial[srcp] <= 0x20) { srcp++; continue; }
      let nlp = res.serial.indexOf(0x0a, srcp);
      if (nlp < 0) nlp = res.serial.length;
      const line = decoder.decode(res.serial.slice(srcp, nlp)).split('#')[0].trim();
      srcp = nlp + 1;
      if (!line) continue;
      if (line.startsWith("sound")) return 2;
      if (line.startsWith("instrument")) return 2;
      return 0;
    }
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.msf = new MsfFile(res.serial);
    this.buildUi();
  };
  
  buildUi() {
    this.element.innerHTML = "";
    if (!this.msf) return;
    const list = this.dom.spawn(this.element, "DIV", ["list"]);
    for (const sound of this.msf.sounds) {
      const card = this.dom.spawn(list, "DIV", ["sound"]);
      this.dom.spawn(card, "DIV", ["title"], `${sound.id} ${sound.name}`, { "on-click": () => this.onEditTitle(sound.id, card) });
      this.dom.spawn(card, "DIV", ["spacer"]);
      const buttonsRow = this.dom.spawn(card, "DIV", ["buttonsRow"]);
      this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: ">", "on-click": () => this.onPlay(sound.id) });
      this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: "Edit", "on-click": () => this.onEdit(sound.id) });
      this.dom.spawn(buttonsRow, "DIV", ["spacer"]);
      this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: "X", "on-click": () => this.onDelete(sound.id, card) });
    }
  }
  
  onEditTitle(id, card) {
    const sound = this.msf?.sounds.find(s => s.id === id);
    if (!sound) return;
    const modal = this.dom.spawnModal(MsfTitleModal);
    modal.setup(sound.id, sound.name);
    modal.result.then((result) => {
      if (!result) return;
      sound.id = result.id;
      sound.name = result.name;
      this.data.dirty(this.res.path, () => this.msf.encode());
      const element = card?.querySelector(".title");
      if (element) element.innerText = `${sound.id} ${sound.name}`;
    }).catch(e => this.dom.modalError(e));
  }
  
  onPlay(id) {
    console.log(`MsfEditor.onPlay ${id}`);
  }
  
  onEdit(id) {
    const sound = this.msf?.sounds.find(s => s.id === id);
    if (!sound) return;
    const modal = this.dom.spawnModal(EditorModal);
    const editor = modal.setupWithSerial(EgsEditor, new TextEncoder("utf8").encode(sound.encode()));
    console.log(`MsfEditor.onEdit ${id}`, sound);
    modal.result.then(result => {
      console.log(`EditorModal(EgsEditor) resolved`, result);
    }).catch(e => this.dom.modalError(e));
  }
  
  onDelete(id, card) {
    if (!this.msf) return;
    const p = this.msf.sounds.findIndex(s => s.id === id);
    if (p < 0) return;
    this.msf.sounds.splice(p, 1);
    this.data.dirty(this.res.path, () => this.msf.encode());
    if (card) card.remove();
  }
}
