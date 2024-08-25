/* ManifestEditor.js
 * It's not an actual resource type, doesn't get stored in the ROM, but for our purposes it's no different.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";

// Types that we know we can ignore.
const STANDARD_TYPES = [
  "manifest", "metadata", "image", "song", "sound", "strings", "code",
];

export class ManifestEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, Window];
  }
  constructor(element, dom, data, window) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.window = window;
    
    this.res = null;
    this.types = []; // {name,tid}, decoded from (this.res.serial).
    this.encoder = new this.window.TextEncoder("utf8");
    this.decoder = new this.window.TextDecoder("utf8");
    this.buildUi();
    this.tocListener = this.data.listenToc(() => this.onTocChanged());
    this.onTocChanged();
  }
  
  onRemoveFromDom() {
    this.data.unlistenToc(this.tocListener);
  }
  
  static checkResource(res) {
    if (res.type === "manifest") return 2;
    return 0;
  }
  
  setup(res) {
    this.res = res;
    if (res) this.types = this.decode(res.serial);
    else this.types = [];
    this.populateUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const table = this.dom.spawn(this.element, "TABLE", { "on-input": () => this.onInput() });
    const addRow = this.dom.spawn(table, "TR", ["add"]);
    const addCol = this.dom.spawn(addRow, "TD");
    this.dom.spawn(addCol, "INPUT", { type: "button", value: "+", "on-click": () => this.onAdd() });
  }
  
  populateUi() {
    const table = this.element.querySelector("table");
    for (const row of table.querySelectorAll("tr.type")) row.remove();
    const addRow = table.querySelector("tr.add");
    for (const { name, tid } of this.types) {
      const row = this.dom.spawn(null, "TR", ["type", "defined"]);
      this.dom.spawn(row, "TD", this.dom.spawn(null, "INPUT", { type: "text", name: "name", value: name }));
      this.dom.spawn(row, "TD", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 255, name: "tid", value: tid }));
      table.insertBefore(row, addRow);
    }
    for (const name of this.data.types) {
      if (STANDARD_TYPES.includes(name)) continue;
      if (this.types.find(t => t.name === name)) continue;
      const row = this.dom.spawn(null, "TR", ["type", "discovered"]);
      this.dom.spawn(row, "TD", this.dom.spawn(null, "INPUT", { type: "text", name: "name", value: name }));
      this.dom.spawn(row, "TD", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 255, name: "tid", value: "0" }));
      table.insertBefore(row, addRow);
    }
  }
  
  decode(serial) {
    const types = [];
    const src = this.decoder.decode(serial);
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).split("#")[0].trim();
      srcp = nlp + 1;
      if (!line) continue;
      const words = line.split(/\s+/g);
      if (words[0] !== "type") continue;
      types.push({
        name: words[1] || "",
        tid: +words[2] || 0,
      });
    }
    return types;
  }
  
  encode() {
    let dst = "";
    for (const row of this.element.querySelectorAll("tr.type")) {
      const name = row.querySelector("input[name='name']")?.value || "";
      if (!name) continue;
      const tid = +row.querySelector("input[name='tid']")?.value || 0;
      if (!tid) continue;
      dst += `type ${name} ${tid}\n`;
    }
    return this.encoder.encode(dst);
  }
  
  onTocChanged() {
    this.populateUi();
  }
  
  onAdd() {
    this.types.push({ name: "", tid: 0 });
    this.populateUi();
    const rows = Array.from(this.element.querySelectorAll("tr.type.defined"));
    if (rows.length > 0) {
      const row = rows[rows.length - 1];
      const input = row.querySelector("input[name='name']");
      input.focus();
    }
  }
  
  onInput() {
    if (!this.res) return;
    this.data.dirty(this.res.path, () => this.encode());
  }
}
