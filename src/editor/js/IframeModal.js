/* IframeModal.js
 */
 
import { Dom } from "./Dom.js";

export class IframeModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.iframe = this.dom.spawn(this.element, "IFRAME");
  }
  
  setUrl(url) {
    this.iframe.src = url;
  }
  
  setSize(w, h) {
    console.log(`IframeModal.setSize ${w},${h}`);
    this.iframe.width = w;
    this.iframe.height = h;
  }
}
