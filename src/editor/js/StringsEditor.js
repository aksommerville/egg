/* StringsEditor.js
 * Show two strings resources side-by-side to aid translation.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";

export class StringsEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    
    this.res = null;
    this.lang = ""; // ISO 639
    this.strings = []; // Index per resource, may be sparse.
    this.compareLang = ""; // ISO 639
    this.compareStrings = []; // Parallel to this.strings.
    this.langs = this.readLangsFromMetadata();
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
  }
  
  static checkResource(res) {
    if (res.type === "strings") return 2;
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.decode();
    this.populateUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const table = this.dom.spawn(this.element, "TABLE", { "on-input": e => this.onInput(e) });
    const headRow = this.dom.spawn(table, "TR");
    this.dom.spawn(headRow, "TH", "Index");
    this.dom.spawn(headRow, "TH", ["lang"]);
    this.dom.spawn(headRow, "TH", ["compareLang"]);
    const controlRow = this.dom.spawn(table, "TR", ["control"]);
    const controlCol = this.dom.spawn(table, "TD");
    this.dom.spawn(controlCol, "INPUT", { type: "button", value: "+", "on-click": () => this.onAdd() });
  }
  
  populateUi() {
    const table = this.element.querySelector("table");
    for (const row of table.querySelectorAll("tr.string")) row.remove();
    table.querySelector("th.lang").innerText = this.lang;
    table.querySelector("th.compareLang").innerText = this.compareLang;
    const controlRow = table.querySelector("tr.control");
    //TODO Emitting rows for empty slots might be a problem for very sparse resources.
    // We discourage excessive sparseness for other reasons, and flattening here bolsters that case, so... ok?
    // Alternative would be an explicit mutable index on each row.
    for (let i=0; i<this.strings.length; i++) {
      const tr = this.dom.spawn(null, "TR", ["string"], { "data-index": i });
      this.dom.spawn(tr, "TD", ["index"], i);
      this.dom.spawn(tr, "TD", this.dom.spawn(null, "INPUT", ["string"], {
        type: "text",
        value: this.strings[i] || "",
        "data-index": i,
      }));
      this.dom.spawn(tr, "TD", this.compareStrings[i] || "");
      table.insertBefore(tr, controlRow);
    }
  }
  
  onAdd() {
    if (!this.res) return;
    const i = this.strings.length;
    this.strings.push("");
    this.compareStrings.push("");
    const table = this.element.querySelector("table");
    const controlRow = table.querySelector("tr.control");
    const tr = this.dom.spawn(null, "TR", ["string"], { "data-index": i });
    this.dom.spawn(tr, "TD", ["index"], i);
    const td = this.dom.spawn(tr, "TD");
    const input = this.dom.spawn(td, "INPUT", ["string"], {
      type: "text",
      value: "",
      "data-index": i,
    });
    this.dom.spawn(tr, "TD", "");
    table.insertBefore(tr, controlRow);
    input.focus();
    // No need to dirty just yet; an empty string at the end would be trimmed either way.
  }
  
  onInput(event) {
    const ixsrc = event?.target?.getAttribute?.("data-index");
    if (!ixsrc) return;
    const ix = +ixsrc;
    this.strings[ix] = event.target.value;
    this.data.dirty(this.res.path, () => this.encode());
  }
  
  encode() {
    let dst = "";
    for (let i=0; i<this.strings.length; i++) {
      let string = this.strings[i];
      if (!string) continue;
      if (string.startsWith('"') || (string !== string.trim())) {
        string = JSON.stringify(string);
      }
      dst += `${i} ${string}\n`;
    }
    return new TextEncoder("utf8").encode(dst);
  }
  
  decode() {
    this.lang = "";
    this.strings = [];
    this.compareLang = "";
    this.compareStrings = [];
    if (!this.res) return;
    this.lang = this.langFromRid(this.res.rid);
    this.strings = this.decodeText(new TextDecoder("utf8").decode(this.res.serial));
    const compareRes = this.chooseCompare(this.res.rid);
    if (compareRes) {
      this.compareLang = this.langFromRid(compareRes.rid);
      this.compareStrings = this.decodeText(new TextDecoder("utf8").decode(compareRes.serial));
    }
  }
  
  langFromRid(rid) {
    const a = (rid >> 11) & 0x1f;
    const b = (rid >> 6) & 0x1f;
    if ((a < 26) && (b < 26)) return String.fromCharCode(0x61 + a) + String.fromCharCode(0x61 + b);
    return "";
  }
  
  chooseCompare(rid) {
    const subrid = rid & 0x3f;
    let best = null;
    let bestLang = "";
    for (const res of this.data.resv) {
      if (res.type !== "strings") continue;
      if (res.rid === rid) continue;
      if ((res.rid & 0x3f) !== subrid) continue;
      const lang = this.langFromRid(res.rid);
      if (!best || (this.compareLang(lang, bestLang) < 0)) {
        best = res;
        bestLang = lang;
      }
    }
    return best;
  }
  
  /* We are not providing any facility for user to select his compare language.
   * Instead, if needed, they can edit metadata:1:lang temporarily. We select in that order.
   * Our assumption is that the first language in metadata:1:lang is the game's original language,
   * so that's the one it makes most sense to compare to.
   */
  compareLang(a, b) {
    if (a === b) return 0;
    for (let i=0; i<this.langs.length; i++) {
      if (this.langs[i] === a) return -1;
      if (this.langs[i] === b) return 1;
    }
    return a.localeCompare(b);
  }
  
  // Array of ISO 639 strings per metadata:1. We'll use to select the preferred language for comparison.
  readLangsFromMetadata() {
    const mres = this.data.resv.find(r => ((r.type === "metadata") && (r.rid === 1)));
    if (!mres) return [];
    const src = new TextDecoder("utf8").decode(mres.serial);
    for (let srcp=0; srcp<src.length; ) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = srcp;
      const line = src.substring(srcp, nlp).split("#")[0].trim();
      srcp = nlp + 1;
      if (!line) continue;
      const sepp = line.indexOf("=");
      if (sepp < 0) continue;
      const k = line.substring(0, sepp).trim();
      if (k !== "lang") continue;
      return line.substring(sepp + 1).split(",").map(v => v.trim()).filter(v => (v.length === 2));
    }
    return [];
  }
  
  decodeText(src) {
    const strings = [];
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = srcp;
      const line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      if (!line) continue;
      if (line.startsWith("#")) continue;
      let ixlen = 0;
      while (ixlen < line.length) {
        const ch = line.charCodeAt(ixlen);
        if (ch <= 0x20) break;
        if ((ch < 0x30) || (ch > 0x39)) {
          console.error(`strings, line ${lineno}: Malformed index: ${JSON.stringify(line)}`);
          continue;
        }
        ixlen++;
      }
      const index = +line.substring(0, ixlen);
      let text = line.substring(ixlen).trim();
      if (text.startsWith('"')) {
        try {
          text = JSON.parse(text);
        } catch (e) {
          console.error(`strings, line ${lineno}: Malformed JSON token`);
          continue;
        }
      }
      strings[index] = text;
    }
    return strings;
  }
}
