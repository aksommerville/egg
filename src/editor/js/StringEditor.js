/* StringEditor.js
 * Show two string files side by side.
 */
 
import { Dom } from "./Dom.js";
import { Resmgr } from "./Resmgr.js";

export class StringEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Resmgr, Window];
  }
  constructor(element, dom, resmgr, window) {
    this.element = element;
    this.dom = dom;
    this.resmgr = resmgr;
    this.window = window;
    
    this.pathPrefix = "";
    this.files = [];
    this.names = [null, "(l)", "(r)"]; // indexed by column id, so 1 and 2.
    this.rows = []; // [id,left,right], strings are decoded if they were stored JSON.
    this.buildUi();
    this.element.addEventListener("input", () => this.onDirty());
  }
  
  setup(serial, path) {
    this.pathPrefix = path.replace(/[^\/]*$/, "");
    this.files = this.resmgr.bus.toc.filter(res => res.type === "string");
    if (this.files.length < 1) return;
    if (this.files.length === 1) {
      this.chooseFiles(this.files[0], null);
    } else {
      const main = this.files.find(f => path === f.path);
      if (!main) return;
      let ref = null;
      const lang = this.resmgr.getMetadata("language").trim().substring(0, 2) || "en";
      if (main.qual !== lang) ref = this.files.find(f => ((f.qual === lang) && (f.name === main.name)));
      this.chooseFiles(ref, main);
    }
  }
  
  chooseFiles(l, r) {
    this.element.querySelector("th.left").innerText = this.names[1] = l?.path.replace(this.pathPrefix, "") || "";
    this.element.querySelector("th.right").innerText = this.names[2] = r?.path.replace(this.pathPrefix, "") || "";
    for (const row of this.element.querySelectorAll("tr.res")) row.remove();
    this.rows = [];
    if (l) this.decodeFile(1, l.serial);
    if (r) this.decodeFile(2, r.serial);
    // It's tempting to sort (this.rows) but don't. There might be some value in preserving the original order.
    const table = this.element.querySelector("table");
    for (const [id, lstr, rstr] of this.rows) {
      const tr = this.dom.spawn(table, "TR", ["res"]);
      this.dom.spawn(tr, "TD", ["id"], this.dom.spawn(null, "INPUT", { type: "text", value: id }));
      this.dom.spawn(tr, "TD", ["left"], this.dom.spawn(null, "INPUT", { type: "text", value: lstr }));
      this.dom.spawn(tr, "TD", ["right"], this.dom.spawn(null, "INPUT", { type: "text", value: rstr }));
    }
  }
  
  decodeFile(col, serial) {
    const src = new this.window.TextDecoder("utf8").decode(serial);
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      if (!line) continue;
      if (line.startsWith("#")) continue;
      const match = line.match(/^([^\s]*)\s+(.*)$/);
      if (!match) continue;
      const id = match[1];
      const v = match[2].match(/^".*"$/) ? JSON.parse(match[2]) : match[2];
      const p = this.rows.findIndex(r => r[0] === id);
      if (p >= 0) this.rows[p][col] = v;
      else {
        const row = [id, "", ""];
        row[col] = v;
        this.rows.push(row);
      }
    }
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const table = this.dom.spawn(this.element, "TABLE");
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TH", ["id"], "ID");
    this.dom.spawn(tr, "TH", ["left"], this.names[1], { "on-click": () => this.chooseLanguage(1) });
    this.dom.spawn(tr, "TH", ["right"], this.names[2], { "on-click": () => this.chooseLanguage(2) });
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "+", "on-click": () => this.onAdd() });
  }
  
  chooseLanguage(col) {
    this.dom.modalPickOne(this.files.map(f => f.path), `Currently '${this.names[col]}'`).then(lang => {
      this.names[col] = lang;
      const file = this.files.find(f => f.path === lang);
      if (col === 1) {
        const other = this.files.find(f => f.path.endsWith(this.names[2]));
        this.chooseFiles(file, other);
      } else {
        const other = this.files.find(f => f.path.endsWith(this.names[1]));
        this.chooseFiles(other, file);
      }
    }).catch(() => {});
  }
  
  onAdd() {
    this.dom.modalInput("ID or name:", this.unusedId()).catch(() => {}).then(rsp => {
      if (!rsp) return;
      const table = this.element.querySelector("table");
      const tr = this.dom.spawn(table, "TR", ["res"]);
      this.dom.spawn(tr, "TD", ["id"], this.dom.spawn(null, "INPUT", { type: "text", value: rsp }));
      this.dom.spawn(tr, "TD", ["left"], this.dom.spawn(null, "INPUT", { type: "text", value: "" }));
      this.dom.spawn(tr, "TD", ["right"], this.dom.spawn(null, "INPUT", { type: "text", value: "" }));
      this.onDirty();
    });
  }
  
  onDirty() {
    const lfile = this.files.find(f => f.path.endsWith(this.names[1]));
    const rfile = this.files.find(f => f.path.endsWith(this.names[2]));
    if (lfile) this.resmgr.dirty(lfile.path, () => this.encode("left", lfile.serial));
    if (rfile) this.resmgr.dirty(rfile.path, () => this.encode("right", rfile.serial));
  }
  
  unusedId() {
    const used = [];
    for (const input of this.element.querySelectorAll("tr.res td.id input")) {
      const rid = +input.value;
      if (isNaN(rid)) continue;
      used[rid] = true;
    }
    for (let rid=1; ; rid++) if (!used[rid]) return rid;
  }
  
  encode(colname, oldserial) {
  
    /* Pull new content off the UI.
     */
    const content = {}; // key:id, value: text, not fully encoded
    for (const row of this.element.querySelectorAll("tr.res")) {
      const id = row.querySelector("td.id input").value.trim();
      if (!id) continue;
      const v = row.querySelector("td." + colname + " input").value.trim();
      content[id] = v;
    }
    
    /* Decode old content. Keep comments and blank lines.
     * Where an old ID is present in new content, emit the new content and drop it.
     * Old ID absent from new content, skip it.
     */
    let dst = "";
    const src = new this.window.TextDecoder("utf8").decode(oldserial);
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      if (!line || line.startsWith("#")) {
        dst += line + "\n";
        continue;
      }
      const match = line.match(/^([^\s]*)\s+(.*)$/);
      if (!match) continue; // This shouldn't happen. Drop the invalid line if it does.
      const id = match[1];
      if (id in content) {
        dst += id + " ";
        const v = content[id];
        if (v.startsWith('"') || (v.indexOf("\n") >= 0)) {
          dst += JSON.stringify(v);
        } else {
          dst += v;
        }
        dst += "\n";
        delete content[id];
      } else {
        // String was deleted or renamed. Skip it.
      }
    }
    
    /* Any strings remaining in content were not in the old text. Emit them all.
     */
    for (const id of Object.keys(content)) {
      dst += id + " ";
      const v = content[id];
      if (v.startsWith('"') || (v.indexOf("\n") >= 0)) {
        dst += JSON.stringify(v);
      } else {
        dst += v;
      }
      dst += "\n";
    }
    
    return new this.window.TextEncoder("utf8").encode(dst);
  }
}
