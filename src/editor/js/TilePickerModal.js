/* TilePickerModal.js
 * Shows a 16x16-tile tilesheet and invites the user to select a tile.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";

export class TilePickerModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
  }
  
  setup(imageName) {
    this.data.getImageAsync(imageName).then(src => this.buildUi(src));
  }
  
  buildUi(src) {
    this.element.innerHTML = "";
    const limit = 700;
    const xscale = Math.max(1, Math.floor(limit / src.naturalWidth));
    const yscale = Math.max(1, Math.floor(limit / src.naturalHeight));
    const scale = Math.min(xscale, yscale);
    const canvas = this.dom.spawn(this.element, "CANVAS", { "on-click": e => this.onClick(e) });
    canvas.width = src.naturalWidth * scale;
    canvas.height = src.naturalHeight * scale;
    const ctx = canvas.getContext("2d");
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(src, 0, 0, src.naturalWidth, src.naturalHeight, 0, 0, canvas.width, canvas.height);
    ctx.beginPath();
    const tilesize = (src.naturalWidth * scale) >> 4;
    for (let i=0; i<=16; i++) {
      ctx.moveTo(i * tilesize, 0);
      ctx.lineTo(i * tilesize, canvas.height);
      ctx.moveTo(0, i * tilesize);
      ctx.lineTo(canvas.width, i * tilesize);
    }
    ctx.strokeStyle = "#0f08";
    ctx.stroke();
  }
  
  onClick(event) {
    const canvas = this.element.querySelector("canvas");
    const bounds = canvas.getBoundingClientRect();
    const col = Math.floor((event.x - bounds.x) * 16 / bounds.width);
    const row = Math.floor((event.y - bounds.y) * 16 / bounds.height);
    if ((col < 0) || (row < 0) || (col > 15) || (row > 15)) return;
    const tileid = row * 16 + col;
    this.resolve(tileid);
  }
}
