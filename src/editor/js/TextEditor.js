/* TextEditor.js
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";

export class TextEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Window, Data];
  }
  constructor(element, dom, window, data) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    this.data = data;
    
    this.res = null;
    this.encoder = new window.TextEncoder("utf8");
    this.decoder = new window.TextDecoder("utf8");
    
    this.buildUi();
  }
  
  static checkResource(res) {
    // We can open empty resources, no problemo, but better to let HexEditor field them.
    if (!res?.serial?.length) return 0;
    // If the first kilobyte is UTF-8, and only selected codepoints below G0, allow it.
    // Binary files of any format will usually contain a zero somewhere early and fail fast.
    // Egg's own binary formats are careful to include UTF-8-illegal constructions in their signature.
    const src = res.serial;
    const srcc = Math.min(1024, src.length);
    for (let srcp=0; srcp<srcc; ) {
      const lead = src[srcp++];
      if (!(lead & 0x80)) { // Single byte codepoint.
        if (lead >= 0x20) ;
        else if (lead === 0x09) ;
        else if (lead === 0x0a) ;
        else if (lead === 0x0d) ;
        else return 0;
      } else if (!(lead & 0x40)) { // Misencoded.
        return 0;
      } else if (!(lead & 0x20)) { // 2
        if (srcp > srcc - 1) break; // Assume EOF is due to our truncation, allow it.
        if ((src[srcp++] & 0xc0) !== 0x80) return 0;
      } else if (!(lead & 0x10)) { // 3
        if (srcp > srcc - 2) break;
        if ((src[srcp++] & 0xc0) !== 0x80) return 0;
        if ((src[srcp++] & 0xc0) !== 0x80) return 0;
      } else if (!(lead & 0x08)) { // 4
        if (srcp > srcc - 3) break;
        if ((src[srcp++] & 0xc0) !== 0x80) return 0;
        if ((src[srcp++] & 0xc0) !== 0x80) return 0;
        if ((src[srcp++] & 0xc0) !== 0x80) return 0;
      } else { // Misencoded.
        return 0;
      }
    }
    return 1;
  }
  
  setup(res) {
    this.res = res;
    this.populateUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "TEXTAREA", { "on-input": () => this.onInput() });
  }
  
  populateUi() {
    const textarea = this.element.querySelector("textarea");
    if (this.res?.serial) {
      textarea.value = this.decoder.decode(this.res.serial);
    } else {
      textarea.value = "";
    }
  }
  
  onInput() {
    if (!this.res) return;
    const text = this.element.querySelector("textarea").value;
    this.data.dirty(this.res.path, () => this.encoder.encode(text));
  }
}
