/* CommandListEditor.js
 * Generic editor for command lists.
 * We can be used as a top-level resource editor, and also piecemeal within other editors.
 * Also we export CommandListModal to wrap the loose case in a modal for convenience.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { MapCommand } from "./MapRes.js";

export class CommandListEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    
    this.clientValidate = null; // (words) => errorMessage | null
    this.clientAssist = null; // (words) => void. Pop up a modal or something, it's all yours.
    this.onDirty = null; // Notify of dirty, only with setupLoose(). You can call encodeFromUi() to get a Uint8Array of the new content.
    
    this.res = null;
  }
  
  static checkResource(res) {
    return 1;
  }
  
  setup(res) {
    this.res = res;
    this.buildUi();
  }
  
  /* (src) may be:
   * - string
   * - Uint8Array
   * - string[]
   * - string[][]
   * - MapCommand[]
   * We will call (onDirty) if set, when anything changes.
   */
  setupLoose(src) {
    this.res = null;
    this.buildUi();
    this.populateUi(this.forceString(src));
  }
  
  forceString(src) {
    if (!src) return "";
    if (typeof(src) === "string") return src;
    if (src instanceof Uint8Array) return new TextDecoder("utf8").decode(src);
    if (src instanceof Array) {
      if (!src.length) return "";
      if (src[0] instanceof MapCommand) return src.map(s => s.encode()).join("\n");
      if (typeof(src[0]) === "string") return src.join("\n");
      if (src[0] instanceof Array) return src.map(s => s.join(" ")).join("\n");
    }
    throw new Error(`Inappropriate input to CommandListEditor`);
  }
  
  /* Returns an array of arrays of strings.
   */
  getCommands() {
    const dst = [];
    for (const input of this.element.querySelectorAll("input.command")) {
      dst.push(CommandListEditor.splitCommand(input.value || ""));
    }
    return dst;
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", ["fields"]);
    this.dom.spawn(this.element, "DIV", ["controls"],
      this.dom.spawn(null, "INPUT", { type: "button", value: "+", "on-click": () => this.onAddField() })
    );
    if (this.res) this.populateUi(this.res.serial);
  }
  
  populateUi(src) {
    if (src instanceof Uint8Array) src = new TextDecoder("utf8").decode(src);
    const fields = this.element.querySelector(".fields");
    fields.innerHTML = "";
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      if (!line || (line[0] === '#')) continue;
      this.addFieldUi(fields, line);
    }
  }
  
  addFieldUi(parent, src) {
    const row = this.dom.spawn(parent, "DIV", ["row"]);
    this.dom.spawn(row, "INPUT", { type: "button", value: "x", "on-click": () => this.onDeleteRow(row) });
    this.dom.spawn(row, "INPUT", { type: "button", value: "^", "on-click": () => this.onMoveRow(row, -1) });
    this.dom.spawn(row, "INPUT", { type: "button", value: "v", "on-click": () => this.onMoveRow(row, 1) });
    this.dom.spawn(row, "INPUT", { type: "button", value: "?", "on-click": () => this.onAssist(row) });
    this.dom.spawn(row, "DIV", ["validation"]);
    const input = this.dom.spawn(row, "INPUT", ["command"], { type: "text", value: src, "on-input": () => this.onChange(row) });
    this.validateRow(row);
    return input;
  }
  
  validateRow(row) {
    const text = row.querySelector("input.command")?.value || "";
    const result = this.validateText(text);
    const validation = row.querySelector(".validation");
    if (result) {
      validation.classList.remove("valid");
      validation.classList.add("invalid");
      validation.setAttribute("title", result);
    } else {
      validation.classList.add("valid");
      validation.classList.remove("invalid");
      validation.removeAttribute("title");
    }
  }
  
  encodeFromUi() {
    let dst = "";
    for (const element of this.element.querySelectorAll("input.command")) {
      dst += element.value + "\n";
    }
    return new TextEncoder("utf8").encode(dst);
  }
  
  dirty() {
    if (this.res) this.data.dirty(this.res.path, () => this.encodeFromUi());
    else if (this.onDirty) this.onDirty();
  }
  
  onDeleteRow(row) {
    row.remove();
    this.dirty();
  }
  
  onMoveRow(row, d) {
    const parent = row?.parentNode;
    if (!parent?.children) return;
    for (let i=0; i<parent.children.length; i++) {
      if (parent.children[i] === row) {
        let np = i + d;
        if (np < 0) np = 0;
        if (np >= parent.children.length) np = parent.children.length - 1;
        if (np > i) np ++;
        if (np === i) return;
        parent.insertBefore(row, parent.children[np]);
        this.dirty();
        return;
      }
    }
  }
  
  onChange(row) {
    this.validateRow(row);
    this.dirty();
  }
  
  onAddField() {
    const parent = this.element.querySelector(".fields");
    const input = this.addFieldUi(parent, "");
    input.focus();
    input.select(0, 1);
  }
  
  onAssist(row) {
    if (!this.clientAssist) return;
    const text = row.querySelector("input.command")?.value || "";
    const words = CommandListEditor.splitCommand(text);
    this.clientAssist(words);
  }
  
  validateText(src) {
    // Report tokenization errors, and call empty invalid.
    let words;
    try {
      words = CommandListEditor.splitCommand(src || "");
    } catch (e) {
      return e.toString();
    }
    if (!words.length) return "Empty command will be discarded.";
    
    /* TODO There's an opportunity for generic processing here.
     * Have the server deliver us schema data (we're going to need that eventually anyway).
     * Eagerly compile the command and report any problems.
     * Sounds like a lot of work so I'm putting it off.
     */
    
    // Let our owner have a look, and otherwise call it valid.
    if (this.clientValidate) return this.clientValidate(words);
    return "";
  }
  
  // (src) is a single-line string.
  static splitCommand(src) {
    const words = [];
    for (let srcp=0; srcp<src.length; ) {
      const ch = src.charCodeAt(srcp);
      if (ch <= 0x20) {
        srcp++;
        continue;
      }
      if (ch === 0x22) { // quote
        let closep = srcp;
        for (;;) {
          closep = src.indexOf('"', closep + 1);
          if (closep < 0) throw new Error("Unclosed string");
          if (src[closep - 1] !== "\\") break;
        }
        closep++;
        words.push(src.substring(srcp, closep));
        srcp = closep;
        continue;
      }
      let spp = src.indexOf(" ", srcp);
      if (spp < 0) spp = src.length;
      words.push(src.substring(srcp, spp));
      srcp = spp + 1;
    }
    return words;
  }
  
  // (src) may be string or Uint8Array. Calls (cb) with an array of strings, once per line.
  // Return true from (cb) to stop iteration.
  static forEachCommand(src, cb) {
    if (!src) return;
    if (src instanceof Uint8Array) src = new TextDecoder("utf8").decode(src);
    if (typeof(src) !== "string") return;
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      if (!line || (line[0] === '#')) continue;
      if (cb(this.splitCommand(line))) break;
    }
  }
}

export class CommandListModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    this.editor = null;
  }
  
  setup(src) {
    this.element.innerHTML = "";
    this.editor = this.dom.spawnController(this.element, CommandListEditor);
    this.editor.setupLoose(src);
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "OK", "on-click": () => this.onSubmit() });
  }
  
  onSubmit() {
    this.resolve(this.editor.encodeFromUi());
  }
}
