/* SoundsEditor.js
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";

export class SoundsEditor {
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
  
  onRemoveFromDom() {
  }
  
  static checkResource(res) {
    if (res.type === "sounds") return 2;
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.populateUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    //TODO
  }
  
  populateUi() {
    //TODO
  }
}
