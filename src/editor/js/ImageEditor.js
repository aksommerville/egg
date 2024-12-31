/* ImageEditor.js
 * We're not an actual image editor. More "viewer with bells and whistles".
 */
 
import { Dom } from "./Dom.js";

export class ImageEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Window, "nonce"];
  }
  constructor(element, dom, window, nonce) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    this.nonce = nonce;
    
    this.path = "";
    this.image = null;
    this.renderFullTimeout = null;
    this.fullView = null; // {srcx,srcy,srcw,srch,dsx,dsty,dstw,dsth} Created at first render.
    // Different games that happen to have the same rid and name for an image will unintentionally share their animation config.
    // I don't think it's worth worrying about; this storage isn't meant to be super permanent, just a convenience during modify-refresh cycles.
    this.db = {}; // [path]:{selected:string,faces:[{name,delay,xform,v:[{tileid,delay?,xform?}]}]}
    this.faces = { selected: "", faces: [] };
    this.dirtyTimeout = null;
    this.animationTimeout = null;
  }
  
  onRemoveFromDom() {
    if (this.renderFullTimeout) {
      this.window.clearTimeout(this.renderFullTimeout);
      this.renderFullTimeout = null;
    }
    if (this.dirtyTimeout) {
      this.window.clearTimeout(this.dirtyTimeout);
      this.dirtyTimeout = null;
      this.saveFaces();
    }
    if (this.animationTimeout) {
      this.window.clearTimeout(this.animationTimeout);
      this.animationTimeout = null;
    }
  }
  
  static checkResource(res) {
    if (res.type === "image") return 2;
    return 0;
  }
  
  setup(res) {
    this.path = res.path;
    const blob = new Blob([res.serial]);
    const url = URL.createObjectURL(blob);
    const image = document.createElement("IMG");
    image.addEventListener("load", () => { URL.revokeObjectURL(url); this.image = image; this.onImageChanged(); }, { once: true });
    image.addEventListener("error", () => { URL.revokeObjectURL(url); this.image = null; this.onImageChanged() }, { once: true });
    image.src = url;
    
    this.element.innerHTML = "";
    const imageRow = this.dom.spawn(this.element, "DIV", ["imageRow"]);
    this.dom.spawn(imageRow, "CANVAS", ["full"], {
      "on-mousewheel": e => this.onFullWheel(e),
      "on-mousedown": e => this.onFullMouseDown(e),
    });
    const gridLinesId = `ImageEditor-${this.nonce}-gridLines`;
    this.dom.spawn(imageRow, "DIV", ["details"],
      this.dom.spawn(null, "DIV", ["path"], this.path),
      this.dom.spawn(null, "DIV", ["dimensions"], "loading..."),
      this.dom.spawn(null, "DIV", ["controls"],
        this.dom.spawn(null, "INPUT", { type: "button", value: "Reset Zoom", "on-click": () => this.onResetZoom() }),
        this.dom.spawn(null, "INPUT", ["toggle"], { name: "gridLines", type: "checkbox", checked: "checked", id: gridLinesId, "on-change": () => this.renderFullSoon() }),
        this.dom.spawn(null, "LABEL", { for: gridLinesId }, "Grid"),
      ),
      this.dom.spawn(null, "DIV", ["tip"], "wheel to scroll, ctl-wheel to zoom, click to add tile"),
    );
    const animationRow = this.dom.spawn(this.element, "DIV", ["animationRow"]);
    this.dom.spawn(animationRow, "CANVAS", ["preview"]);
    this.dom.spawn(animationRow, "DIV", ["faceList"], { "on-change": () => this.onFaceListSelection() });
    this.dom.spawn(animationRow, "DIV", ["workbench"]);
    
    this.loadFaces();
  }
  
  loadFaces() {
    try {
      this.db = JSON.parse(this.window.localStorage.getItem("eggImageEditor.faces"));
      if (!this.db || (typeof(this.db) !== "object")) throw null;
    } catch (e) {
      this.db = {};
    }
    if (!(this.faces = this.db[this.path])) {
      this.faces = this.db[this.path] = { selected: "", faces: [] };
    }
    if (!this.faces.faces) this.faces.faces = [];
    if (!this.faces.faces.length) {
      this.faces.faces.push({ name: "face1", delay: 250, xform: 0, v: [] });
    }
    if (!this.faces.selected) this.faces.selected = this.faces.faces[0].name;
    this.rebuildFaceList();
    this.selectFace(this.faces.selected);
  }
  
  saveFaces() {
    // (this.faces) is always a member of (this.db). So just reencode (this.db).
    this.window.localStorage.setItem("eggImageEditor.faces", JSON.stringify(this.db));
  }
  
  dirty() {
    if (this.dirtyTimeout) return;
    this.dirtyTimeout = this.window.setTimeout(() => {
      this.dirtyTimeout = null;
      this.saveFaces();
    }, 500);
  }
  
  /* Face list and workbench.
   ***********************************************************************/
  
  rebuildFaceList() {
    const faceList = this.element.querySelector(".faceList");
    faceList.innerHTML = "";
    for (const face of this.faces.faces) {
      const row = this.dom.spawn(faceList, "DIV", ["row"]);
      const id = `ImageEditor-${this.nonce}-face-${face.name}`;
      this.dom.spawn(row, "INPUT", { type: "radio", name: "face", id, "data-name": face.name });
      this.dom.spawn(row, "LABEL", { for: id }, face.name);
    }
    this.dom.spawn(faceList, "INPUT", { type: "button", value: "+", "on-click": () => this.onAddFace() });
  }
  
  populateWorkbench(face) {
    const workbench = this.element.querySelector(".workbench");
    workbench.innerHTML = "";
    if (!face) return;
    this.dom.spawn(workbench, "INPUT", { type: "button", value: "X", "on-click": () => this.onDeleteFace(face.name) });
    const table = this.dom.spawn(workbench, "TABLE");
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", ["key"], "Delay if unset"),
      this.dom.spawn(null, "TD", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 9999, name: "delay", value: face.delay, "on-change": e => this.onChangeDefaultDelay(face, e.target.value) }))
    );
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", ["key"], "Xform if unset"),
      this.dom.spawn(null, "TD", this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 7, name: "xform", value: face.xform, "on-change": e => this.onChangeDefaultXform(face, e.target.value) }))
    );
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TH", ""),
      this.dom.spawn(null, "TH", "Tile"),
      this.dom.spawn(null, "TH", "Delay"),
      this.dom.spawn(null, "TH", "Xform"),
    );
    for (const frame of face.v) {
      this.addFrameRow(table, frame, face);
    }
    this.dom.spawn(workbench, "INPUT", { type: "button", value: "+", "on-click": () => this.onAddFrame(face, table) });
  }
  
  onAddFrame(face, table) {
    const frame = { tileid: "0x00", delay: "", xform: "" };
    face.v.push(frame);
    this.dirty();
    this.addFrameRow(table, frame, face);
    this.restartAnimation();
  }
  
  addFrameRow(table, frame, face) {
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", this.dom.spawn(null, "INPUT", { type: "button", value: "X", "on-click": () => this.onDeleteFrame(frame, face) })),
      this.dom.spawn(null, "TD", this.dom.spawn(null, "INPUT", { type: "text", value: frame.tileid, "on-change": e => this.onChangeTileid(frame, e.target.value) })),
      this.dom.spawn(null, "TD", this.dom.spawn(null, "INPUT", { type: "text", value: frame.delay, "on-change": e => this.onChangeDelay(frame, e.target.value) })),
      this.dom.spawn(null, "TD", this.dom.spawn(null, "INPUT", { type: "text", value: frame.xform, "on-change": e => this.onChangeXform(frame, e.targte.value) })),
    );
  }
  
  onDeleteFrame(frame, face) {
    const p = face.v.indexOf(frame);
    if (p < 0) return;
    face.v.splice(p, 1);
    this.dirty();
    this.populateWorkbench(face);
    this.restartAnimation();
  }
  
  onChangeDefaultDelay(face, value) {
    face.delay = +value;
    this.dirty();
    this.restartAnimation();
  }
  
  onChangeDefaultXform(face, value) {
    face.xform = +value;
    this.dirty();
    this.restartAnimation();
  }
  
  onChangeTileid(frame, value) {
    frame.tileid = value;
    this.dirty();
    this.restartAnimation();
  }
  
  onChangeDelay(frame, value) {
    frame.delay = value;
    this.dirty();
    this.restartAnimation();
  }
  
  onChangeXform(frame, value) {
    frame.xform = value;
    this.dirty();
    this.restartAnimation();
  }
  
  selectFace(name) {
    const input = this.element.querySelector(`.faceList input[data-name='${name}']`);
    if (!input) return;
    input.click();
  }
  
  unusedFaceName() {
    for (let i=1; ; i++) {
      const name = "face" + i;
      if (!this.faces.faces.find(f => f.name === name)) return name;
    }
  }
  
  onAddFace() {
    this.dom.modalInput("Face name (letters, digits, underscore, dash):", this.unusedFaceName()).then(name => {
      if (!name) return;
      if (!name.match(/^[a-zA-Z0-9_-]+$/)) throw `Invalid name ${JSON.stringify(name)}`;
      if (this.faces.faces.find(f => f.name === name)) throw `Face ${JSON.stringify(name)} already exists`;
      this.faces.faces.push({ name, delay: 250, xform: 0, v: [] });
      this.rebuildFaceList();
      this.selectFace(name);
      this.dirty();
    }).catch(e => this.dom.modalError(e));
  }
  
  onFaceListSelection() {
    const input = this.element.querySelector(`.faceList input:checked`);
    const name = input?.getAttribute("data-name") || "";
    this.populateWorkbench(this.faces.faces.find(f => f.name === name));
    this.faces.selected = name;
    this.dirty();
    this.restartAnimation();
  }
  
  onImageChanged() {
    this.element.querySelector(".details .dimensions").innerText = this.image ? `${this.image.naturalWidth}x${this.image.naturalHeight}` : "---";
    this.fullView = null;
    this.renderFullSoon();
  }
  
  onDeleteFace(name) {
    const p = this.faces.faces.findIndex(f => f.name === name);
    if (p >= 0) this.faces.faces.splice(p, 1);
    if (this.faces.selected === name) this.faces.selected = "";
    this.rebuildFaceList();
    if (this.faces.faces.length > 0) {
      this.selectFace(this.faces.faces[0].name);
    } else {
      this.populateWorkbench(null, "");
      this.restartAnimation();
    }
    this.dirty();
  }
  
  onClickTile(tileid) {
    const face = this.faces.faces.find(f => f.name === this.faces.selected);
    if (!face) return;
    face.v.push({ tileid: "0x" + tileid.toString(16).padStart(2, '0'), delay: "", xform: "" });
    this.dirty();
    this.populateWorkbench(face);
    this.restartAnimation();
  }
  
  /* Animated preview.
   **********************************************************************/
   
  restartAnimation() {
    if (this.animationTimeout) {
      this.window.clearTimeout(this.animationTimeout);
      this.animationTimeout = null;
    }
    const face = this.faces.faces.find(f => f.name === this.faces.selected);
    if (face && face.v.length) {
      this.animate(face, 0);
    } else {
      this.renderNoPreview();
    }
  }
  
  renderNoPreview() {
    const canvas = this.element.querySelector("canvas.preview");
    canvas.width = 16;
    canvas.height = 16;
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
  }
  
  /* Draw frame (p) immediately, and schedule the next one.
   * Call only when this.animationTimeout is unset.
   */
  animate(face, p) {
    if (this.image) {
      const canvas = this.element.querySelector("canvas.preview");
      canvas.width = this.image.naturalWidth >> 4;
      canvas.height = this.image.naturalHeight >> 4;
      const ctx = canvas.getContext("2d");
      ctx.clearRect(0, 0, canvas.width, canvas.height);
    
      if ((p < 0) || (p >= face.v.length)) return;
      const frame = face.v[p];
      if (++p >= face.v.length) p = 0;
    
      const tileid = +frame.tileid;
      const xform = +(frame.xform || face.xform);
      const srcx = (tileid & 0x0f) * canvas.width;
      const srcy = (tileid >> 4) * canvas.height;
      ctx.save();
      const tilesize = canvas.width;
      switch (xform & 7) {
        case 0: ctx.setTransform(1, 0, 0, 1, 0, 0); break; // No xform.
        case 1: ctx.setTransform(-1, 0, 0, 1, tilesize, 0); break; // XREV
        case 2: ctx.setTransform(1, 0, 0, -1, 0, tilesize); break; // YREV
        case 3: ctx.setTransform(-1, 0, 0, -1, tilesize, tilesize); break; // XREV|YREV ie 180
        case 4: ctx.setTransform(0, 1, 1, 0, 0, 0); break; // SWAP
        case 5: ctx.setTransform(0, -1, 1, 0, 0, tilesize); break; // SWAP|XREV
        case 6: ctx.setTransform(0, 1, -1, 0, tilesize, 0); break; // SWAP|YREV
        case 7: ctx.setTransform(0, -1, -1, 0, tilesize, tilesize); break; // SWAP|XREV|YREV
      }
      ctx.drawImage(this.image, srcx, srcy, canvas.width, canvas.height, 0, 0, canvas.width, canvas.height);
      ctx.restore();
    
      this.animationTimeout = this.window.setTimeout(() => {
        this.animationTimeout = null;
        this.animate(face, p);
      }, +(frame.delay || face.delay));
    } else {
      // No image. This will definitely happen on the first attempt. Delay a little and try again.
      this.renderNoPreview();
      this.animationTimeout = this.window.setTimeout(() => {
        this.animationTimeout = null;
        this.animate(face, p);
      }, 500);
    }
  }
  
  /* "full" canvas: Show the whole image, with optional grid lines, zoom, and scroll.
   ************************************************************************/
   
  onResetZoom() {
    this.fullView = null;
    this.renderFullSoon();
  }
   
  renderFullSoon() {
    if (this.renderFullTimeout) return;
    this.renderFullTimeout = this.window.setTimeout(() => {
      this.renderFullTimeout = null;
      this.renderFullNow();
    }, 50);
  }
  
  renderFullNow() {
    const canvas = this.element.querySelector(".full");
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.imageSmoothingEnabled = false;
    ctx.fillStyle = "#888";
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    if (this.image) {
      this.requireFullView(canvas.width, canvas.height, this.image.naturalWidth, this.image.naturalHeight);
      const { dstx, dsty, dstw, dsth, srcx, srcy, srcw, srch } = this.fullView;
      ctx.drawImage(this.image, srcx, srcy, srcw, srch, dstx, dsty, dstw, dsth);
      if (this.element.querySelector("input[name='gridLines']")?.checked) {
        this.drawGrid(ctx);
      }
    }
  }
  
  drawGrid(ctx) {
    // We're going to draw all 34 lines even if they are way offscreen.
    // Allow fractional cell sizes in the natural image: We can draw a grid even for things that aren't tilesheets.
    const srccolw = this.image.naturalWidth / 16;
    const srcrowh = this.image.naturalHeight / 16;
    const dstcolw = (srccolw * this.fullView.dstw) / this.fullView.srcw;
    const dstrowh = (srcrowh * this.fullView.dsth) / this.fullView.srch;
    let dstx = this.fullView.dstx - (this.fullView.srcx * this.fullView.dstw) / this.fullView.srcw;
    let dsty = this.fullView.dsty - (this.fullView.srcy * this.fullView.dsth) / this.fullView.srch;
    const xa=dstx, ya=dsty;
    const xz = xa + dstcolw * 16;
    const yz = ya + dstrowh * 16;
    ctx.beginPath();
    for (let i=17; i-->0; dstx+=dstcolw, dsty+=dstrowh) {
      ctx.moveTo(xa, dsty);
      ctx.lineTo(xz, dsty);
      ctx.moveTo(dstx, ya);
      ctx.lineTo(dstx, yz);
    }
    ctx.strokeStyle = "#0f0";
    ctx.globalAlpha = 0.75;
    ctx.stroke();
    ctx.globalAlpha = 1;
  }
  
  requireFullView(cvw, cvh, imw, imh) {
    if (this.fullView) return;
    
    /* Select an initial zoom level that leaves at least (margin) on each side.
     * Will be either an integer or 1/integer.
     * We've CSS'd the canvas size to 520x520, which fits a typical 256x256 tilesheet at exactly 2.0 zoom.
     */
    const margin = 4;
    const xscale = (cvw - margin * 2) / imw;
    const yscale = (cvh - margin * 2) / imh;
    let scale;
    if ((xscale >= 1) && (yscale >= 1)) {
      scale = Math.floor(Math.min(xscale, yscale));
    } else {
      for (let i=2; i<40; i++) { // "i<40" as a sanity check
        scale = 1 / i;
        if ((scale <= xscale) && (scale <= yscale)) break;
      }
    }
    const dstw = Math.max(1, Math.floor(imw * scale));
    const dsth = Math.max(1, Math.floor(imh * scale));
    const dstx = (cvw >> 1) - (dstw >> 1);
    const dsty = (cvh >> 1) - (dsth >> 1);
    
    this.fullView = { dstx, dsty, dstw, dsth, srcx: 0, srcy: 0, srcw: imw, srch: imh };
  }
  
  fullZoom(d, mx, my) {
  
    // Project mouse position into source image.
    const canvas = this.element.querySelector(".full");
    const bounds = canvas.getBoundingClientRect();
    const cx=mx-bounds.x, cy=my-bounds.y;
    const srcmx = this.fullView.srcx + ((cx - this.fullView.dstx) * this.fullView.srcw) / this.fullView.dstw;
    const srcmy = this.fullView.srcy + ((cy - this.fullView.dsty) * this.fullView.srch) / this.fullView.dsth;
    
    // Turn current zoom level into a scalar and multiply that by a constant per (d).
    const zoomBase = 1.500;
    let zoom = this.fullView.dstw / this.fullView.srcw;
    if (d > 0) zoom *= zoomBase;
    else zoom /= zoomBase;
    
    // Select bounds in src that would fill dst at the given zoom.
    const srcsize = bounds.width / zoom;
    let srcx = srcmx - ((cx * srcsize) / bounds.width);
    let srcy = srcmy - ((cy * srcsize) / bounds.height);
    
    // We don't actually need to clip and calculate proper dst bounds. Just let src run oob.
    this.fullView = { srcx, srcy, srcw: srcsize, srch: srcsize, dstx: 0, dsty: 0, dstw: bounds.width, dsth: bounds.height };
    
    this.renderFullNow();
  }
  
  fullScroll(dx, dy) {
    const canvas = this.element.querySelector(".full");
    const ddist = canvas.width / 8; // Regardless of zoom, shift view by a fixed proportion of the output size.
    dx = (dx < 0) ? -1 : (dx > 0) ? 1 : 0;
    dy = (dy < 0) ? -1 : (dy > 0) ? 1 : 0;
    this.fullView.srcx += dx * (ddist * this.fullView.srcw) / this.fullView.dstw;
    this.fullView.srcy += dy * (ddist * this.fullView.srch) / this.fullView.dsth;
    this.renderFullNow();
  }
  
  onFullWheel(event) {
    if (!this.image) return;
    if (!this.fullView) return;
    const d = event.deltaX || event.deltaY;
    if (!d) return;
    if (event.ctrlKey) this.fullZoom(-d, event.x, event.y);
    else if (event.shiftKey) this.fullScroll(d, 0);
    else this.fullScroll(0, d);
    event.stopPropagation();
    event.preventDefault();
  }
  
  onFullMouseDown(event) {
    if (!this.image) return;
    if (!this.fullView) return;
    const canvas = this.element.querySelector(".full");
    const bounds = canvas.getBoundingClientRect();
    const cx=event.x-bounds.x, cy=event.y-bounds.y;
    const srcmx = this.fullView.srcx + ((cx - this.fullView.dstx) * this.fullView.srcw) / this.fullView.dstw;
    const srcmy = this.fullView.srcy + ((cy - this.fullView.dsty) * this.fullView.srch) / this.fullView.dsth;
    const col = Math.floor((srcmx * 16) / this.image.naturalWidth);
    const row = Math.floor((srcmy * 16) / this.image.naturalHeight);
    if ((col < 0) || (row < 0) || (col >= 0x10) || (row >= 0x10)) return;
    this.onClickTile((row << 4) | col);
  }
}
