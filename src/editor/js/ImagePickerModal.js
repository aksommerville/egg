/* ImagePickerModal.js
 * Shows all image resources and invites the user to pick one.
 * Resolves with the image's name if present, or its rid. Either way you can prefix "image:" for use in command lists.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";

export class ImagePickerModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const promises = [];
    for (const res of this.data.resv) {
      if (res.type !== "image") continue;
      if (!res.image) promises.push(this.data.getImageAsync(res.rid));
      this.addImageRow(this.element, res);
    }
    Promise.all(promises).then(() => this.renderThumbnails());
  }
  
  addImageRow(parent, res) {
    const button = this.dom.spawn(parent, "BUTTON", { "on-click": () => this.resolve(res.name || res.rid) });
    this.dom.spawn(button, "CANVAS", { "data-rid": res.rid });
    this.dom.spawn(button, "DIV", ["path"], res.path);
  }
  
  renderThumbnails() {
    const widthLimit = 300;
    const heightLimit = 150;
    for (const res of this.data.resv) {
      if (res.type !== "image") continue;
      const canvas = this.element.querySelector(`canvas[data-rid='${res.rid}']`);
      if (!canvas) continue;
      const xscale = Math.max(1, Math.floor(res.image.naturalWidth / widthLimit));
      const yscale = Math.max(1, Math.floor(res.image.naturalHeight / heightLimit));
      const scale = Math.max(xscale, yscale);
      canvas.width = Math.floor(res.image.naturalWidth / scale);
      canvas.height = Math.floor(res.image.naturalHeight / scale);
      const ctx = canvas.getContext("2d");
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      ctx.drawImage(res.image, 0, 0, res.image.naturalWidth, res.image.naturalHeight, 0, 0, canvas.width, canvas.height);
    }
  }
}
