/* WorldMapModal.js
 * Shows the entire collection of map resources arranged in planes.
 */
 
import { Dom } from "../Dom.js";
import { Namespaces } from "../Namespaces.js";
import { Data } from "../Data.js";
import { MapStore } from "./MapStore.js";

export class WorldMapModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, MapStore, Namespaces, Data, Window];
  }
  constructor(element, dom, mapStore, namespaces, data, window) {
    this.element = element;
    this.dom = dom;
    this.mapStore = mapStore;
    this.namespaces = namespaces;
    this.data = data;
    this.window = window;
    
    this.maps = []; // Record of canvas position of each displayed map.
    
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    
    switch (this.mapStore.neighborRegime) {
      case "neighbors":
      case "location":
        break; // ok, proceed.
      default: {
          this.dom.spawn(this.element, "DIV", `Current neighbor regime ${JSON.stringify(this.mapStore.neighborRegime)} does not support world map.`);
          return;
        }
    }
    
    const planeSelect = this.dom.spawn(this.element, "SELECT", ["planeSelect"], { "on-change": () => this.onChange() });
    for (let i=0; i<this.mapStore.planes.length; i++) {
      const plane = this.mapStore.planes[i];
      this.dom.spawn(planeSelect, "OPTION", { value: i }, `Plane ${i} (${plane.w}x${plane.h})`);
    }
    
    this.dom.spawn(this.element, "CANVAS", ["display"], { "on-click": e => this.onCanvasClick(e) });
    
    this.window.setTimeout(() => { // Wait for DOM to sort it self out...
      this.onChange(); // load the default selection
    }, 10);
  }
  
  onChange() {
    this.maps = [];
    const canvas = this.element.querySelector("canvas.display");
    const ctx = canvas.getContext("2d");
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    const p = +this.element.querySelector("select.planeSelect")?.value;
    if (isNaN(p) || (p < 0) || (p >= this.mapStore.planes.length)) return;
    const plane = this.mapStore.planes[p];
    
    // Determine the minimal bounds. (plane.x,y) are not helpful, ignore them.
    let x=0, y=0, w=plane.w, h=plane.h;
    while (w > 0) {
      if (!this.rectContainsSomething(plane, x, y, 1, h)) { x++; w--; }
      else if (!this.rectContainsSomething(plane, x + w - 1, y, 1, h)) w--;
      else break;
    }
    while (h > 0) {
      if (!this.rectContainsSomething(plane, x, y, w, 1)) { y++; h--; }
      else if (!this.rectContainsSomething(plane, x, y + h - 1, w, 1)) h--;
      else break;
    }
    if ((w < 1) || (h < 1)) return;
    
    // Determine the world's size in pixels. If we can't find the tile size or fixed map size, assume 16x16 and 20x15.
    const tilesize = this.namespaces.idFromName("NS", "sys", "tilesize") || 16;
    const mapw = this.namespaces.idFromName("NS", "sys", "mapw") || 20;
    const maph = this.namespaces.idFromName("NS", "sys", "maph") || 15;
    const wpx = tilesize * mapw * w;
    const hpx = tilesize * maph * h;
    
    // Reduce tilesize so the full image is no larger than the canvas.
    const bounds = canvas.getBoundingClientRect();
    const excess = Math.max(wpx / bounds.width, hpx / bounds.height);
    let dsttilesize = tilesize;
    if (excess > 1.0) {
      dsttilesize = Math.max(1, Math.floor(tilesize / excess));
    }
    const dstcolw = mapw * dsttilesize;
    const dstrowh = maph * dsttilesize;
    const dstfullw = dstcolw * w;
    const dstfullh = dstrowh * h;
    
    // Render all and build up (this.maps).
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const x0 = (bounds.width >> 1) - (dstfullw >> 1);
    const y0 = (bounds.height >> 1) - (dstfullh >> 1);
    for (let col=0; col<w; col++) {
      for (let row=0; row<h; row++) {
        const planep = (y + row) * plane.w + (x + col);
        const map = plane.v[planep];
        if (!map) continue;
        const dstx = x0 + col * dstcolw;
        const dsty = y0 + row * dstrowh;
        this.maps.push({
          map,
          x: dstx,
          y: dsty,
          w: dstcolw,
          h: dstrowh,
        });
        this.drawMap(ctx, dstx, dsty, dsttilesize, map);
      }
    }
  }
  
  drawMap(ctx, x0, y0, dsttilesize, map) {
    const imageName = map.getFirstCommand("image");
    if (!imageName) return;
    this.data.getImageAsync(imageName).then(image => {
      const srctilesize = image.naturalWidth >> 4;
      for (let row=0,dsty=y0,cellp=0; row<map.h; row++,dsty+=dsttilesize) {
        for (let col=0,dstx=x0; col<map.w; col++,dstx+=dsttilesize,cellp++) {
          const srcx = (map.v[cellp] & 0x0f) * srctilesize;
          const srcy = (map.v[cellp] >> 4) * srctilesize;
          ctx.drawImage(image, srcx, srcy, srctilesize, srctilesize, dstx, dsty, dsttilesize, dsttilesize);
        }
      }
    }).catch(() => {});
  }
  
  rectContainsSomething(plane, x, y, w, h) {
    for (let yi=h; yi-->0; ) {
      for (let xi=w; xi-->0; ) {
        if (plane.v[(y + yi) * plane.w + (x + xi)]) return true;
      }
    }
    return false;
  }
  
  onCanvasClick(event) {
    const canvas = event.target;
    const bounds = canvas.getBoundingClientRect();
    const x = event.x - bounds.x;
    const y = event.y - bounds.y;
    // We may assume that the canvas's framebuffer size matches its element size, because it does.
    for (const map of this.maps) {
      if (x < map.x) continue;
      if (y < map.y) continue;
      if (x >= map.x + map.w) continue;
      if (y >= map.y + map.h) continue;
      this.element.remove();
      this.window.location = "#" + map.map.path;
      return;
    }
  }
}
