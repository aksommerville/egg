/* MapEditor.js
 * TODO This is the big kahuna, the main entry point and also coordinator for a bunch of other map-related classes.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";

export class MapEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
  }
  
  static checkResource(res) {
    if (res.type === "map") return 2;
    return 0;
  }
  
  setup(res) {
    console.log(`TODO MapEditor.setup`, { res });
  }
}
