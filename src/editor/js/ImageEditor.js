/* ImageEditor.js
 * Don't be fooled by the name: It's read-only, not a real "Editor".
 * "Editor" is just for consistency with the other resource editors.
 * Actual editing of images is out of scope -- use your favorite external tool.
 * We could add some things here, like zoom, scroll, change background color, measure regions...
 * Maybe some support for gridded tilesheets?
 * But for now we're just a dumb image viewer.
 */
 
import { Dom } from "./Dom.js";

export class ImageEditor {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    this.url = null;
  }
  
  onRemoveFromDom() {
    if (this.url) URL.revokeObjectURL(this.url);
  }
  
  static checkResource(res) {
    if (res.type === "image") return 2;
    return 0;
  }
  
  setup(res) {
    if (this.url) URL.revokeObjectURL(this.url);
    const blob = new Blob([res.serial]);
    this.url = URL.createObjectURL(blob);
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "IMG", { src: this.url });
  }
}
