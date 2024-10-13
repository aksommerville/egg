/* EgsEditor.js
 * For single EGS-text sounds or songs. May be from standalone files, or segments of an MSF file.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";
 
export class EgsEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
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
    console.log(`EgsEditor.setup`, res);
  };
}
