/* TilesheetEditor.js
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";

export class TilesheetEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, Window];
  }
  constructor(element, dom, data, window) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.window = window;
    
    this.res = null;
    this.mouseListener = null;
    this.renderTimeout = null;
  }
  
  onRemoveFromDom() {
    if (this.mouseListener) {
      this.window.removeEventListener("mousemove", this.mouseListener);
      this.window.removeEventListener("mouseup", this.mouseListener);
      this.mouseListener = null;
    }
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
  }
  
  static checkResource(res) {
    if (res.type === "tilesheet") return 2;
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const toolbar = this.dom.spawn(this.element, "DIV", ["toolbar"]);
    
    const tables = this.dom.spawn(toolbar, "DIV", ["tables"]);
    const layers = this.dom.spawn(toolbar, "DIV", ["layers"]);
    
    const canvas = this.dom.spawn(this.element, "CANVAS", ["canvas"]);
    //TODO
  }
}
