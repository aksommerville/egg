/* TileModal.js
 * Spawned by TilesheetEditor for examining one tile in detail.
 * We modify the provided model directly and coordinate with Data.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { TilesheetEditor } from "./TilesheetEditor.js";
import { Namespaces } from "../Namespaces.js";

export class TileModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, Data, Window, Namespaces];
  }
  constructor(element, dom, data, window, namespaces) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.window = window;
    this.namespaces = namespaces;
    
    this.path = null;
    this.image = null;
    this.model = null;
    this.tileid = 0;
    this.dragBit = 0; // Last bit visited while dragging.
    this.dragMode = false; // Set or clear.
    this.mouseListener = null;
  }
  
  onRemoveFromDom() {
    if (this.mouseListener) {
      this.window.removeEventListener("mousemove", this.mouseListener);
      this.window.removeEventListener("mouseup", this.mouseListener);
      this.mouseListener = null;
    }
  }
  
  setup(path, image, model, tileid) {
    this.path = path;
    this.image = image;
    this.model = model;
    this.tileid = tileid;
    this.namespaces.whenLoaded.then(() => this.buildUi());
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const top = this.dom.spawn(this.element, "DIV", ["top"]);
    
    this.thumbnailSize = 64;
    this.neighborWidth = 20;
    this.neighborMargin = 5;
    const neighbors = this.dom.spawn(top, "CANVAS", ["neighbors"], {
      width: this.neighborWidth * 2 + this.neighborMargin * 2 + this.thumbnailSize,
      height: this.neighborWidth * 2 + this.neighborMargin * 2 + this.thumbnailSize,
      "on-mousedown": e => this.onNeighborsMouseDown(e),
    });
    
    const nav = this.dom.spawn(top, "TABLE", ["nav"]);
    for (let dy=-1; dy<=1; dy++) {
      const tr = this.dom.spawn(nav, "TR");
      for (let dx=-1; dx<=1; dx++) {
        const td = this.dom.spawn(tr, "TD");
        if (!dx) {
          if (dy < 0) this.dom.spawn(td, "INPUT", { type: "button", value: "^", "on-click": () => this.onNav(0, -1) });
          else if (dy > 0) this.dom.spawn(td, "INPUT", { type: "button", value : "v", "on-click": () => this.onNav(0, 1) });
          else td.classList.add("tileid");
        } else if (!dy) {
          if (dx < 0) this.dom.spawn(td, "INPUT", { type: "button", value: "<", "on-click": () => this.onNav(-1, 0) });
          else if (dx > 0) this.dom.spawn(td, "INPUT", { type: "button", value: ">", "on-click": () => this.onNav(1, 0) });
        }
      }
    }
    
    const fields = this.dom.spawn(this.element, "DIV", ["fields"]);
    for (const sym of this.namespaces.getSymbols("NS", "tilesheet")) {
      const row = this.dom.spawn(fields, "DIV", ["field"]);
      this.dom.spawn(row, "DIV", ["k"], sym.name);
      this.dom.spawn(row, "INPUT", { type: "number", min: 0, max: 255, name: sym.name, "on-input": () => this.onLooseInput(sym.name) });
      if (sym.name === "family") {
        // family is an opaque enum, no need for a hint. But it is very helpful to have a "New" button to assign any unused value.
        this.dom.spawn(row, "INPUT", { type: "button", value: "New", "on-click": () => this.onNewFamily() });
      } else {
        // Everything else gets a "hint" field, whether they need it or not.
        this.dom.spawn(row, "DIV", ["hint"], { "data-k": sym.name });
      }
    }
    
    const tilesize = this.namespaces.idFromName("NS", "sys", "tilesize") || (this.image?.naturalWidth >> 4) || 16;
    const macros = this.dom.spawn(this.element, "DIV", ["macros"]);
    for (const k of Object.keys(TileModal.MACROS)) {
      const macro = TileModal.MACROS[k];
      const button = this.dom.spawn(macros, "BUTTON", { "data-k": k, "on-click": () => this.onApplyMacro(k) });
      this.dom.spawn(button, "CANVAS", ["preview"], { width: macro.w * tilesize, height: macro.h * tilesize });
      this.dom.spawn(button, "DIV", k);
    }
    
    this.populateUi();
  }
  
  populateUi() {
    this.renderNeighbors();
    this.element.querySelector("td.tileid").innerText = this.tileid.toString(16).padStart(2, '0');
    for (const k of this.namespaces.getSymbols("NS", "tilesheet").map(v => v.name)) {
      const v = this.model[k]?.[this.tileid] || 0;
      const input = this.element.querySelector(`.fields input[name='${k}']`);
      if (input) input.value = v;
      const hint = this.element.querySelector(`.fields .hint[data-k='${k}']`);
      if (hint) hint.innerText = this.hintForValue(k, v);
    }
    for (const k of Object.keys(TileModal.MACROS)) {
      const preview = this.element.querySelector(`button[data-k='${k}'] canvas.preview`);
      if (preview) this.renderMacroPreview(preview, k);
    }
  }
  
  renderMacroPreview(canvas, k) {
    const macro = TileModal.MACROS[k];
    const ctx = canvas.getContext("2d");
    const col = this.tileid & 15;
    const row = this.tileid >> 4;
    
    // Ugly cross-out if it's not available.
    if (!macro || (col + macro.w > 16) || (row + macro.h > 16)) {
      ctx.fillStyle = "#888";
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      ctx.beginPath();
      ctx.moveTo(0, 0);
      ctx.lineTo(canvas.width, canvas.height);
      ctx.moveTo(canvas.width, 0);
      ctx.lineTo(0, canvas.height);
      ctx.strokeStyle = "#800";
      ctx.stroke();
      return;
    }
    
    // Image might have alpha, so clear to white first.
    ctx.fillStyle = "#fff";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    if (this.image) {
      const tilesize = this.image.naturalWidth >> 4;
      const cpw = tilesize * macro.w;
      const cph = tilesize * macro.h;
      ctx.drawImage(this.image, col * tilesize, row * tilesize, cpw, cph, 0, 0, cpw, cph);
    }
  }
  
  onApplyMacro(k) {
    const macro = TileModal.MACROS[k];
    if (!macro) return;
    const col = this.tileid & 15;
    const row = this.tileid >> 4;
    if (col + macro.w > 16) return;
    if (row + macro.h > 16) return;
    
    // Create all the tables we will use, if they don't exist yet.
    // Neighbors and weight get explicit values from the macro, so those are real.
    // Family and physics copy from the reference tile.
    // So if they're not actually being used, we harmlessly copy a bunch of zeroes that will drop off at encode.
    if (!this.model.family) this.model.family = new Uint8Array(256);
    if (!this.model.physics) this.model.physics = new Uint8Array(256);
    if (!this.model.neighbors) this.model.neighbors = new Uint8Array(256);
    if (!this.model.weight) this.model.weight = new Uint8Array(256);
    
    for (let dy=0, mp=0; dy<macro.h; dy++) {
      for (let dx=0; dx<macro.w; dx++, mp++) {
        const dsttileid = (row + dy) * 16 + (col + dx);
        this.model.family[dsttileid] = this.model.family[this.tileid];
        this.model.physics[dsttileid] = this.model.physics[this.tileid];
        if (macro.weight) this.model.weight[dsttileid] = macro.weight[mp];
        if (macro.neighbors) this.model.neighbors[dsttileid] = macro.neighbors[mp];
      }
    }
    this.dirty();
    this.populateUi();
  }
  
  onLooseInput(k) {
    if (k === "neighbors") this.renderNeighbors();
    const v = +this.element.querySelector(`.fields input[name='${k}']`)?.value || 0;
    if (!this.model[k]) this.model[k] = new Uint8Array(256);
    this.model[k][this.tileid] = v;
    const hint = this.element.querySelector(`.fields .hint[data-k='${k}']`);
    if (hint) hint.innerText = this.hintForValue(k, v);
    this.dirty();
  }
  
  hintForValue(k, v) {
    const perNamespaces = this.namespaces.nameFromId("NS", k, v);
    if (perNamespaces) return perNamespaces;
    if (k === "weight") {
      switch (v) {
        case 0: return "likeliest";
        case 254: return "unlikeliest";
        case 255: return "appt_only";
      }
    }
    return "";
  }
  
  renderNeighbors() {
    const canvas = this.element.querySelector("canvas.neighbors");
    const ctx = canvas.getContext("2d");
    ctx.imageSmoothingEnabled = false;
    ctx.fillStyle = "#fff";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    if (this.image) {
      const tilesize = this.image.naturalWidth >> 4;
      const srcx = (this.tileid & 15) * tilesize;
      const srcy = (this.tileid >> 4) * tilesize;
      const dstx = this.neighborWidth + this.neighborMargin;
      const dsty = dstx;
      ctx.drawImage(this.image, srcx, srcy, tilesize, tilesize, dstx, dsty, this.thumbnailSize, this.thumbnailSize);
    }
    const neighbors = this.model.neighbors?.[this.tileid] || 0;
    const nbit = (bit, x, y, w, h) => {
      if (neighbors & bit) ctx.fillStyle = "#080";
      else ctx.fillStyle = "#888";
      ctx.fillRect(x, y, w, h);
    };
    const a=this.neighborWidth, b=this.neighborMargin, c=this.thumbnailSize;
    nbit(0x80, 0, 0, a, a);
    nbit(0x40, a + b, 0, c, a);
    nbit(0x20, a + b + c + b, 0, a, a);
    nbit(0x10, 0, a + b, a, c);
    nbit(0x08, a + b + c + b, a + b, a, c);
    nbit(0x04, 0, a + b + c + b, a, a);
    nbit(0x02, a + b, a + b + c + b, c, a);
    nbit(0x01, a + b + c + b, a + b + c + b, a, a);
  }
  
  dirty() {
    this.data.dirty(this.path, () => TilesheetEditor.encode(this.model));
  }
  
  neighborBitFromEvent(event) {
    const canvas = this.element.querySelector("canvas.neighbors");
    const bounds = canvas.getBoundingClientRect();
    const x = event.x - bounds.x;
    const y = event.y - bounds.y;
    const col = (x < this.neighborWidth + this.neighborMargin) ? 0 : (x > bounds.width - this.neighborWidth - this.neighborMargin) ? 2 : 1;
    const row = (y < this.neighborWidth + this.neighborMargin) ? 0 : (y > bounds.height - this.neighborWidth - this.neighborMargin) ? 2 : 1;
    switch (row) {
      case 0: switch (col) {
          case 0: return 0x80;
          case 1: return 0x40;
          case 2: return 0x20;
        } break;
      case 1: switch (col) {
          case 0: return 0x10;
          case 2: return 0x08;
        } break;
      case 2: switch (col) {
          case 0: return 0x04;
          case 1: return 0x02;
          case 2: return 0x01;
        } break;
    }
    return 0;
  }
  
  onMouseMoveOrUp(event) {
    if (event.type === "mouseup") {
      this.window.removeEventListener("mousemove", this.mouseListener);
      this.window.removeEventListener("mouseup", this.mouseListener);
      this.mouseListener = null;
      return;
    }
    const bit = this.neighborBitFromEvent(event);
    if (bit === this.dragBit) return;
    this.dragBit = bit;
    if (this.dragMode) {
      this.model.neighbors[this.tileid] |= bit;
    } else {
      this.model.neighbors[this.tileid] &= ~bit;
    }
    const field = this.element.querySelector(".fields input[name='neighbors']");
    if (field) field.value = this.model.neighbors[this.tileid];
    this.dirty();
    this.renderNeighbors();
  }
  
  onNeighborsMouseDown(event) {
    if (this.mouseListener) return;
    const bit = this.neighborBitFromEvent(event);
    if (!bit) return;
    if (!this.model.neighbors) this.model.neighbors = new Uint8Array(256);
    this.dragBit = bit;
    if (this.model.neighbors[this.tileid] & bit) {
      this.dragMode = false;
      this.model.neighbors[this.tileid] &= ~bit;
    } else {
      this.dragMode = true;
      this.model.neighbors[this.tileid] |= bit;
    }
    const field = this.element.querySelector(".fields input[name='neighbors']");
    if (field) field.value = this.model.neighbors[this.tileid];
    this.mouseListener = e => this.onMouseMoveOrUp(e);
    this.window.addEventListener("mousemove", this.mouseListener);
    this.window.addEventListener("mouseup", this.mouseListener);
    this.dirty();
    this.renderNeighbors();
  }
  
  onNav(dx, dy) {
    const col = (this.tileid & 15) + dx;
    const row = (this.tileid >> 4) + dy;
    if ((col < 0) || (col > 15) || (row < 0) || (row > 15)) return;
    this.tileid = (row << 4) | col;
    this.populateUi();
  }
  
  onNewFamily() {
    if (!this.model.family) this.model.family = new Uint8Array(256);
    const inUse = new Set(this.model.family);
    for (let i=1; i<256; i++) {
      if (inUse.has(i)) continue;
      this.model.family[this.tileid] = i;
      this.element.querySelector(".fields input[name='family']").value = i;
      this.dirty();
      return;
    }
  }
}

TileModal.MACROS = {
  fat5x3: {
    desc: "For features at least 2 tiles wide everywhere.",
    w: 5,
    h: 3,
    neighbors: [
      0x0b, 0x1f, 0x16, 0xfe, 0xfb,
      0x6b, 0xff, 0xd6, 0xdf, 0x7f,
      0x68, 0xf8, 0xd0, 0x00, 0x00,
    ],
  },
  fat6x3: {
    desc: "Like fat5x3, but isthmus corners also provided (below the inner-corners 2x2, with notch pointing down). Three options for the single.",
    w: 6,
    h: 3,
    neighbors: [
      0x0b, 0x1f, 0x16, 0xfe, 0xfb, 0x00,
      0x6b, 0xff, 0xd6, 0xdf, 0x7f, 0x00,
      0x68, 0xf8, 0xd0, 0x7e, 0xdb, 0x00,
    ],
  },
  skinny4x4: {
    desc: "For single-tile-width features like a trail.",
    w: 4,
    h: 4,
    neighbors: [
      0x0a, 0x1a, 0x12, 0x02,
      0x4a, 0x5a, 0x52, 0x42,
      0x48, 0x58, 0x50, 0x40,
      0x08, 0x18, 0x10, 0x00,
    ],
  },
  square3x3: {
    desc: "No concave corners.",
    w: 3,
    h: 3,
    neighbors: [
      0x0b, 0x1f, 0x16,
      0x6b, 0xff, 0xd6,
      0x68, 0xf8, 0xd0,
    ],
  },
  exp4: {
    desc: "Four options, likeliest on the left.",
    w: 4,
    h: 1,
    weight: [0, 128, 192, 224],
  },
  exp8: {
    desc: "Eight options, likeliest on the left.",
    w: 8,
    h: 1,
    weight: [0, 128, 192, 224, 240, 248, 252, 254],
  },
};
