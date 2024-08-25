/* HexEditor.js
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";
 
export class HexEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    
    this.res = null;
    
    this.buildUi();
  }
  
  static checkResource(res) {
    return 1; // Everything is valid and nothing is preferred.
  }
  
  setup(res) {
    this.res = res;
    this.populateUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "TEXTAREA", { "on-input": () => this.dirty() });
  }
  
  populateUi() {
    const textarea = this.element.querySelector("textarea");
    textarea.value = this.encodeForDisplay(this.res?.serial || []);
  }
  
  dirty() {
    if (!this.res) return;
    this.data.dirty(this.res.path, () => this.encodeForStorage());
  }
  
  encodeForDisplay(src) {
    let dst = "";
    for (let i=0; i<src.length; i++) {
      const b = src[i];
      dst += (b >> 4).toString(16);
      dst += (b & 15).toString(16);
      dst += ((i & 15) === 15) ? "\n" : " ";
    }
    return dst;
  }
  
  encodeForStorage() {
    // Split entire input on whitespace; each token is a single hex byte.
    const src = this.element.querySelector("textarea").value;
    const re = /[^\s]+/g;
    let dst = new Uint8Array((src.length + 2) / 3);
    let dstc = 0;
    let match;
    while (match = re.exec(src)) {
      const token = match[0];
      let v = 0;
      for (let i=0; i<token.length; i++) {
        v <<= 4;
        const ch = token.charCodeAt(i);
             if ((ch >= 0x30) && (ch <= 0x39)) v |= ch - 0x30;
        else if ((ch >= 0x61) && (ch <= 0x66)) v |= ch - 0x61 + 10;
        else if ((ch >= 0x41) && (ch <= 0x46)) v |= ch - 0x41 + 10;
        else throw new Error(`Illegal character '${token[i]}'`);
      }
      if (v > 0xff) throw new Error(`Illegal byte ${v} (${JSON.stringify(token)})`);
      if (dstc >= dst.length) {
        const nv = new Uint8Array(dst.length << 1);
        nv.set(dst);
        dst = nv;
      }
      dst[dstc++] = v;
    }
    if (dstc < dst.length) {
      const nv = new Uint8Array(dstc);
      const srcview = new Uint8Array(dst.buffer, 0, dstc);
      nv.set(srcview);
      dst = nv;
    }
    return dst;
  }
}
