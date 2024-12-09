/* MapCanvas.js
 * The main workspace under MapEditor.
 * We're a full-size canvas with hacks for scrolling and zooming.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { MapEditor } from "./MapEditor.js";
import { TilesheetEditor } from "./TilesheetEditor.js";

export class MapCanvas {
  static getDependencies() {
    return [HTMLElement, Dom, MapEditor, Window, Data];
  }
  constructor(element, dom, mapEditor, window, data) {
    this.element = element;
    this.dom = dom;
    this.mapEditor = mapEditor;
    this.window = window;
    this.data = data;
    
    this.renderTimeout = null;
    this.mouseListener = null;
    this.originx = 0; // Where the top-left corner of cell (0,0) goes, in canvas coordinates. Often negative, and can be positive for small maps.
    this.originy = 0;
    this.zoom = 4; // Larger means bigger tiles.
    this.tilesize = Math.round(this.mapEditor.tilesize * this.zoom); // For display. Natural tilesize in the source image is (this.mapEditor.tilesize) always.
    this.image = null;
    this.loadImage();
    
    // Placement of the mousedown listener is fragile: Can't use canvas because it's pointer-events:none, and can't use sizer because it might be short.
    const scroller = this.dom.spawn(this.element, "DIV", ["scroller"], {
      "on-scroll": () => { this.mapEditor.mapToolbar.clearTattle(); this.renderSoon(); },
      "on-mousedown": e => this.onMouseDown(e),
      "on-mousemove": e => this.onMouseMove(e),
      "on-mouseenter": e => this.onMouseMove(e),
      "on-mouseleave": e => this.onMouseMove(e),
      "on-mousewheel": e => this.onMouseWheel(e),
    });
    const sizer = this.dom.spawn(scroller, "DIV", ["sizer"]);
    const canvas = this.dom.spawn(this.element, "CANVAS");
    this.recalculateSize();
  }
  
  onRemoveFromDom() {
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
    if (this.mouseListener) {
      this.window.removeEventListener("mousemove", this.mouseListener);
      this.window.removeEventListener("mouseup", this.mouseListener);
      this.mouseListener = null;
    }
  }
  
  recalculateSize() {
    const sizer = this.element.querySelector(".sizer");
    sizer.style.width = this.mapEditor.map.w * this.tilesize + "px";
    sizer.style.height = this.mapEditor.map.h * this.tilesize + "px";
    this.renderSoon();
  }
  
  loadImage() {
    const name = this.mapEditor.map.getFirstCommand("image");
    this.data.getImageAsync(name).then(img => {
      this.image = img;
      this.renderSoon();
    }).catch(e => {
      console.log(`MapCanvas failed to acquire image ${JSON.stringify(name)}`, e);
    });
  }
  
  /* Populates (originx,originy) based on inputs, (this.tilesize), and sizer bounds (ie visible map size).
   * This happens at the start of each render.
   * Mouse events should treat our instance fields as gospel, so they transform against what's currently rendered.
   */
  refreshOutputBounds(vieww, viewh, scrollx, scrolly) {
    const sizer = this.element.querySelector(".sizer");
    const totalw = parseInt(sizer.style.width);
    const totalh = parseInt(sizer.style.height);
    if (totalw <= vieww) {
      this.originx = (vieww >> 1) - (totalw >> 1);
    } else {
      this.originx = -scrollx;
    }
    if (totalh <= viewh) {
      this.originy = (viewh >> 1) - (totalh >> 1);
    } else {
      this.originy = -scrolly;
    }
  }
  
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 20);
  }
  
  renderNow() {
  
    /* Gather some things, recalculate the output bounds, and clear the canvas.
     */
    const scroller = this.element.querySelector(".scroller");
    const scrollx = scroller.scrollLeft;
    const scrolly = scroller.scrollTop;
    const canvas = this.element.querySelector("canvas");
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    this.refreshOutputBounds(bounds.width, bounds.height, scrollx, scrolly);
    const ctx = canvas.getContext("2d");
    ctx.imageSmoothingEnabled = false;
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    const visibility = this.mapEditor.mapToolbar.getVisibility();
    
    /* Which cells are in view?
     * If none, we're done.
     */
    let [cola, rowa] = this.mapPositionFromCanvas(0, 0);
    let [colz, rowz] = this.mapPositionFromCanvas(canvas.width - 1, canvas.height - 1);
    if (cola < 0) cola = 0;
    if (colz >= this.mapEditor.map.w) colz = this.mapEditor.map.w - 1;
    if (cola > colz) return;
    if (rowa < 0) rowa = 0;
    if (rowz >= this.mapEditor.map.h) rowz = this.mapEditor.map.h - 1;
    if (rowa > rowz) return;
    
    /* Copy the tile for each visible cell.
     */
    if (this.image && visibility.includes("image")) {
      for (let dsty=this.originy+rowa*this.tilesize, row=rowa; row<=rowz; row++, dsty+=this.tilesize) {
        for (let dstx=this.originx+cola*this.tilesize, col=cola; col<=colz; col++, dstx+=this.tilesize) {
          const tileid = this.mapEditor.map.v[row * this.mapEditor.map.w + col];
          const srcx = (tileid & 15) * this.mapEditor.tilesize;
          const srcy = (tileid >> 4) * this.mapEditor.tilesize;
          ctx.drawImage(this.image, srcx, srcy, this.mapEditor.tilesize, this.mapEditor.tilesize, dstx, dsty, this.tilesize, this.tilesize);
        }
      }
    } else {
      for (let dsty=this.originy+rowa*this.tilesize, row=rowa; row<=rowz; row++, dsty+=this.tilesize) {
        for (let dstx=this.originx+cola*this.tilesize, col=cola; col<=colz; col++, dstx+=this.tilesize) {
          const tileid = this.mapEditor.map.v[row * this.mapEditor.map.w + col];
          ctx.fillStyle = this.colorForTileid(tileid);
          ctx.fillRect(dstx, dsty, this.tilesize, this.tilesize);
        }
      }
    }
    
    /* Highlight cells based on physics, if requested.
     */
    if (visibility.includes("physics")) {
      ctx.globalAlpha = 0.75;
      const table = this.mapEditor.tilesheet.physics || [];
      const margin = 1;
      const size = this.tilesize - margin * 2;
      for (let dsty=this.originy+rowa*this.tilesize+margin, row=rowa; row<=rowz; row++, dsty+=this.tilesize) {
        for (let dstx=this.originx+cola*this.tilesize+margin, col=cola; col<=colz; col++, dstx+=this.tilesize) {
          const tileid = this.mapEditor.map.v[row * this.mapEditor.map.w + col];
          const physics = table[tileid] || 0;
          // Our own colorForTileid() puts similar tileid close together. We want the opposite.
          // And there's some value in using the same colors that TilesheetEditor uses.
          ctx.fillStyle = TilesheetEditor.colorForTableValue(physics);
          ctx.fillRect(dstx, dsty, size, size);
        }
      }
      ctx.globalAlpha = 1;
    }
    
    /* Highlight regions of interest, from the commands.
     */
    if (visibility.includes("regions")) {
      //TODO ROI
    }
    
    /* Highlight points of interest, from the commands.
     */
    if (visibility.includes("poi")) {
      //TODO POI
    }
    
    /* Selection if present.
     * No visibility flag for this, since if you don't want it just don't use it.
     */
    let trail;
    if (trail = this.mapEditor.mapPaint.selection) {
      for (const [x,y] of trail) {
        const col = x - this.mapEditor.mapPaint.floatOffset[0];
        const row = y - this.mapEditor.mapPaint.floatOffset[1];
        const tileid = this.mapEditor.mapPaint.float[row * this.mapEditor.map.w + col];
        const [dstx, dsty] = this.canvasPositionFromMap(x, y);
        if (this.image) {
          const srcx = (tileid & 15) * this.mapEditor.tilesize;
          const srcy = (tileid >> 4) * this.mapEditor.tilesize;
          ctx.drawImage(this.image, srcx, srcy, this.mapEditor.tilesize, this.mapEditor.tilesize, dstx, dsty, this.tilesize, this.tilesize);
        } else {
          ctx.fillStyle = this.colorForTileid(tileid);
          ctx.fillRect(dstx, dsty, this.tilesize, this.tilesize);
        }
      }
      ctx.fillStyle = "#0f0";
      ctx.globalAlpha = 0.500;
      for (const [x, y] of trail) {
        const [dstx, dsty] = this.canvasPositionFromMap(x, y);
        ctx.fillRect(dstx, dsty, this.tilesize, this.tilesize);
      }
      ctx.globalAlpha = 1;
    }
    
    /* If the marquee is in progress, ie actively making a selection but not committed yet, draw that special.
     */
    if (trail = this.mapEditor.mapPaint.marquee) {
      let [x, y, w, h] = trail;
      if (w < 0) { x += w; w = -w; w++; }
      if (h < 0) { y += h; h = -h; h++; }
      const [dstx, dsty] = this.canvasPositionFromMap(x, y);
      ctx.fillStyle = "#0ff";
      ctx.globalAlpha = 0.750;
      ctx.fillRect(dstx, dsty, w * this.tilesize, h * this.tilesize);
      ctx.globalAlpha = 1;
    }
    
    /* If a pedometer trail exists, draw it.
     * No visibility flag for this, since if you don't want it just don't use it.
     */
    if (trail = this.mapEditor.mapPaint.pedometer) {
      ctx.fillStyle = "#f00";
      ctx.globalAlpha = 0.500;
      for (const [x, y] of trail) {
        const [dstx, dsty] = this.canvasPositionFromMap(x, y);
        ctx.fillRect(dstx, dsty, this.tilesize, this.tilesize);
      }
      // Highlight the last cell special.
      ctx.fillStyle = "#ff0";
      const [dstx, dsty] = this.canvasPositionFromMap(trail[trail.length-1][0], trail[trail.length-1][1]);
      ctx.fillRect(dstx, dsty, this.tilesize, this.tilesize);
      ctx.globalAlpha = 1;
    }
    
    /* Grid lines, if requested.
     */
    if (visibility.includes("grid")) {
      const xa = this.originx + cola * this.tilesize + 0.5;
      const xz = xa + (colz - cola + 1) * this.tilesize;
      const ya = this.originy + rowa * this.tilesize + 0.5;
      const yz = ya + (rowz - rowa + 1) * this.tilesize;
      ctx.beginPath();
      for (let dsty=ya; dsty<=yz; dsty+=this.tilesize) {
        ctx.moveTo(xa, dsty);
        ctx.lineTo(xz, dsty);
      }
      for (let dstx=xa; dstx<=xz; dstx+=this.tilesize) {
        ctx.moveTo(dstx, ya);
        ctx.lineTo(dstx, yz);
      }
      ctx.strokeStyle = "#0f0";
      ctx.stroke();
    }
  }
  
  /* Tile colors, when image is not available.
   * I don't expect this to come up often, but we have to do something.
   */
  colorForTileid(tileid) {
    const col = tileid & 15;
    const row = tileid >> 4;
    return "#" + "0123456789abcdef"[col] + "0123456789abcdef"[row] + "0123456789abcdef"[Math.max(col, row)];
  }
  
  // Returns [x,y], the map cell containing (x,y) in canvas pixels.
  // Can be OOB.
  mapPositionFromCanvas(x, y) {
    return [
      Math.floor((x - this.originx) / this.tilesize),
      Math.floor((y - this.originy) / this.tilesize),
    ];
  }
  
  // Returns [x,y], the canvas position of cell (x,y)'s top left corner.
  canvasPositionFromMap(x, y) {
    return [
      x * this.tilesize + this.originx,
      y * this.tilesize + this.originy,
    ];
  }
  
  // Like mapPositionFromCanvas, but input is a mouse event, and output can be fractional.
  mapPositionFromEvent(event) {
    const bounds = this.element.querySelector("canvas").getBoundingClientRect();
    // These events from the scroller, not the canvas, and we're stuck with that.
    // If it's outside the canvas bounds, report OOB coordinates, to ensure that clicking the scroll bar doesn't also edit the map.
    if ((event.x > bounds.x + bounds.width) || (event.y > bounds.y + bounds.height)) {
      return [this.mapEditor.map.w, this.mapEditor.map.h];
    }
    return [
      (event.x - bounds.x - this.originx) / this.tilesize,
      (event.y - bounds.y - this.originy) / this.tilesize,
    ];
  }
  
  onMouseDown(event) {
    if (this.mouseListener) return;
    const [col, row] = this.mapPositionFromEvent(event); // NB (col,row) are fractional
    if ((col < 0) || (row < 0) || (col >= this.mapEditor.map.w) || (row >= this.mapEditor.map.h)) return;
    
    //TODO Search for POI handles or other overlay widgets.
    
    if (this.mapEditor.mapPaint.mouseDown(Math.floor(col), Math.floor(row))) {
      this.mouseListener = e => this.onMouseUpOrMove(e);
      this.window.addEventListener("mousemove", this.mouseListener);
      this.window.addEventListener("mouseup", this.mouseListener);
    }
  }
  
  onMouseUpOrMove(event) {
    const [col, row] = this.mapPositionFromEvent(event); // NB (col,row) are fractional
    if ((col < 0) || (row < 0) || (col >= this.mapEditor.map.w) || (row >= this.mapEditor.map.h)) return;
    if (event.type === "mouseup") {
      this.window.removeEventListener("mousemove", this.mouseListener);
      this.window.removeEventListener("mouseup", this.mouseListener);
      this.mouseListener = null;
      this.mapEditor.mapPaint.mouseUp(Math.floor(col), Math.floor(row));
      return;
    }
    this.mapEditor.mapPaint.mouseMove(Math.floor(col), Math.floor(row));
  }
  
  onMouseMove(event) {
    if (event.type === "mouseleave") {
      this.mapEditor.mapToolbar.clearTattle();
    } else {
      const [x, y] = this.mapPositionFromEvent(event);
      this.mapEditor.mapToolbar.setTattle(Math.floor(x), Math.floor(y));
    }
  }
  
  onMouseWheel(event) {
    // By and large, the mouse wheel is used for scrolling, and we get that for free.
    // Just look for control key, and if that's pressed, prevent the usual behavior and zoom instead.
    if (event.ctrlKey) {
      const focus = this.mapPositionFromEvent(event);
      const prevCanvas = this.canvasPositionFromMap(focus[0], focus[1]); // just (event-canvasOrigin) but do it this way for consistency
      const base = 1.25;
      if ((event.deltaY > 0) && (this.zoom > 0.25)) this.zoom /= base;
      else if ((event.deltaY < 0) && (this.zoom < 32)) this.zoom *= base;
      this.tilesize = Math.max(1, Math.round(this.mapEditor.tilesize * this.zoom));
      this.recalculateSize();
      // Now scroll to try to keep the cursor pointing at the same map position. It's not perfect, hopefully close enough to be useful.
      const focusCanvas = this.canvasPositionFromMap(focus[0], focus[1]);
      const scroller = this.element.querySelector(".scroller");
      scroller.scrollLeft += focusCanvas[0] - prevCanvas[0];
      scroller.scrollTop += focusCanvas[1] - prevCanvas[1];
      event.preventDefault();
      event.stopPropagation();
    }
  }
}
