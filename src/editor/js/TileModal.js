/* TileModal.js
 * Prompt user to select a tile from some tilesheet image.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";

export class TileModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    
    this.tileid = 0;
  }
  
  setup(imageOrName, tileid) {
    this.tileid = tileid;
    if (typeof(imageOrName) === "string") {
      this.data.getImageAsync(imageOrName).then(image => {
        this.image = image;
        this.buildUi();
      }).catch(e => this.dom.modalError(e));
    } else {
      this.image = image;
      this.buildUi();
    }
  }
  
  buildUi() {
    this.element.innerHTML = "";
    // Canvas is larger than the image, usually. We want grid lines finer than the underlying pixels.
    const canvas = this.dom.spawn(this.element, "CANVAS", { width: 512, height: 512, "on-click": e => this.onClick(e) });
    const ctx = canvas.getContext("2d");
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(this.image, 0, 0, this.image.naturalWidth, this.image.naturalHeight, 0, 0, canvas.width, canvas.height);
    ctx.beginPath();
    for (let i=0; i<=16; i++) {
      ctx.moveTo(0, i * 32);
      ctx.lineTo(canvas.width, i * 32);
      ctx.moveTo(i * 32, 0);
      ctx.lineTo(i * 32, canvas.height);
    }
    ctx.strokeStyle = "#0f0";
    ctx.globalAlpha = 0.500;
    ctx.lineWidth = 2;
    ctx.stroke();
    if ((this.tileid >= 0) && (this.tileid <= 0xff)) {
      ctx.fillStyle = "#0ff";
      const col = this.tileid & 15;
      const row = this.tileid >> 4;
      ctx.fillRect(col * 32, row * 32, 32, 32);
    }
    ctx.globalAlpha = 1;
  }
  
  onClick(event) {
    const canvas = this.element.querySelector("canvas");
    const bounds = canvas.getBoundingClientRect();
    const col = Math.floor(((event.x - bounds.x) * 16) / bounds.width);
    const row = Math.floor(((event.y - bounds.y) * 16) / bounds.height);
    if ((col < 0) || (row < 0) || (col >= 16) || (row >= 16)) return;
    this.resolve((row << 4) | col);
  }
}
