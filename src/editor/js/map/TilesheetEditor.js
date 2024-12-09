/* TilesheetEditor.js
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { Namespaces } from "../Namespaces.js";
import { TileModal } from "./TileModal.js";

export class TilesheetEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, Window, Namespaces, "nonce"];
  }
  constructor(element, dom, data, window, namespaces, nonce) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.window = window;
    this.namespaces = namespaces;
    this.nonce = nonce;
    
    this.res = null;
    this.image = null;
    this.model = {}; // [tablename]: Uint8Array(256)
    this.mouseListener = null;
    this.renderTimeout = null;
    this.tileidByTileid = new Uint8Array(256);
    for (let i=0; i<256; i++) this.tileidByTileid[i] = i;
    this.selp = -1;
    this.dragp = -1;
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
    this.model = TilesheetEditor.decode(this.res.serial);
    this.data.getImageAsync(this.res.rid).catch(e => null).then(image => {
      this.image = image;
      return this.namespaces.whenLoaded;
    }).then(() => this.buildUi());
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const toolbar = this.dom.spawn(this.element, "DIV", ["toolbar"]);
    
    const tables = this.dom.spawn(toolbar, "DIV", ["tables", "checkBar"], { "on-input": () => this.onVisibilityChanged() });
    for (const entry of this.namespaces.getSymbols("NS", "tilesheet")) {
      const id = `TilesheetEditor-${this.nonce}-table-${entry.name}`;
      this.dom.spawn(tables, "INPUT", { id, type: "checkbox", name: entry.name });
      this.dom.spawn(tables, "LABEL", { for: id }, entry.name);
    }
    
    const layers = this.dom.spawn(toolbar, "DIV", ["layers", "checkBar"], { "on-input": () => this.onVisibilityChanged() });
    for (const name of ["image", "blotter", "grid", "tileid", "numeric"]) {
      const id = `TilesheetEditor-${this.nonce}-layer-${name}`;
      this.dom.spawn(layers, "INPUT", { type: "checkbox", id, name });
      this.dom.spawn(layers, "LABEL", { for: id }, name);
    }
    layers.querySelector("input[name='image']").checked = true;
    layers.querySelector("input[name='grid']").checked = true;
    
    this.dom.spawn(toolbar, "DIV", ["advice"],
      this.dom.spawn(null, "DIV", ["tip"], "ctl-click to select"),
      this.dom.spawn(null, "DIV", ["tip"], "shift-drag to copy, visible tables only")
    );
    
    const canvas = this.dom.spawn(this.element, "CANVAS", ["canvas"], { "on-mousedown": e => this.onMouseDown(e) });
    
    this.renderSoon();
  }
  
  getVisibleTables() {
    return Array.from(this.element.querySelectorAll(".toolbar .tables input:checked")).map(e => e.name);
  }
  
  getVisibleLayers() {
    return Array.from(this.element.querySelectorAll(".toolbar .layers input:checked")).map(e => e.name);
  }
  
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 50);
  }
  
  /* Render.
   ****************************************************************************************/
  
  renderNow() {
    const canvas = this.element.querySelector(".canvas");
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.imageSmoothingEnabled = false;
    ctx.textBaseline = "top";
    ctx.font = "9px monospace";
    ctx.fillStyle = "#444";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    const tableNames = this.getVisibleTables();
    const layers = this.getVisibleLayers();
    const numeric = layers.includes("numeric");
    
    // Source image is always square. (if not, it's wrong and we're right). Makes the aspect management a snap.
    const dstw = Math.min(bounds.width, bounds.height);
    const dsth = dstw;
    const dstx = (bounds.width >> 1) - (dstw >> 1);
    const dsty = (bounds.height >> 1) - (dsth >> 1);
    ctx.fillStyle = "#222";
    ctx.fillRect(dstx, dsty, dstw, dsth);
    const dsttilesize = dstw / 16; // May be fractional, expect it to be so 15/16 of the time.
    let subp = 0; // Index of table, controls positioning within each cell.
    
    // Start with the related image. Weird to not want this, but it is toggleable.
    if (layers.includes("image") && this.image) {
      ctx.drawImage(this.image, 0, 0, this.image.naturalWidth, this.image.naturalHeight, dstx, dsty, dstw, dsth);
    }
    
    // Blotter if requested, to improve visibility of the overlays.
    if (layers.includes("blotter")) {
      ctx.fillStyle = "#000";
      ctx.globalAlpha = 0.75;
      ctx.fillRect(dstx, dsty, dstw, dsth);
      ctx.globalAlpha = 1;
    }
    
    // (tileid) renders like a table if enabled, but always numeric. Kind of weird to want it but you do you.
    if (layers.includes("tileid")) {
      this.renderNumericTable(canvas, ctx, dstx, dsty, dsttilesize, this.tileidByTileid, subp++);
    }
    
    // Then all the generic tables.
    if (layers.includes("numeric")) {
      for (const tableName of tableNames) {
        this.renderNumericTable(canvas, ctx, dstx, dsty, dsttilesize, this.model[tableName], subp++);
      }
    } else {
      for (const tableName of tableNames) {
        this.renderGraphicTable(canvas, ctx, dstx, dsty, dsttilesize, this.model[tableName], tableName, subp++);
      }
    }
    
    // Then grid lines if requested.
    if (layers.includes("grid")) {
      ctx.beginPath();
      for (let i=0; i<=16; i++) {
        const p = Math.round(i * dsttilesize) + 0.5;
        ctx.moveTo(dstx + p, dsty);
        ctx.lineTo(dstx + p, dsty + dsth);
        ctx.moveTo(dstx, dsty + p);
        ctx.lineTo(dstx + dstw, dsty + p);
      }
      ctx.strokeStyle = "#0f0";
      ctx.stroke();
    }
    
    // And finally the selection highlight.
    if (this.selp >= 0) {
      ctx.globalAlpha = 0.75;
      ctx.fillStyle = "#0ff";
      const col = this.selp & 15;
      const row = this.selp >> 4;
      ctx.fillRect(dstx + col * dsttilesize, dsty + row * dsttilesize, dsttilesize, dsttilesize);
      ctx.globalAlpha = 1;
    }
  }
  
  renderNumericTable(canvas, ctx, dstx, dsty, dsttilesize, table, subp) {
    if (!table) return;
    ctx.fillStyle = [
      "#fff", "#faa", "#afa", "#aaf",
      "#ffa", "#faf", "#aff", "#aaa",
    ][subp & 7];
    const boxw = dsttilesize >> 1;
    const boxh = dsttilesize >> 2;
    for (let row=0, p=0; row<16; row++) {
      for (let col=0; col<16; col++, p++) {
        const v = table[p];
        if (!v) continue; // Zeroes are always omitted.
        const x = Math.round(dstx + col * dsttilesize) + (subp >> 2) * boxw;
        const y = Math.round(dsty + row * dsttilesize) + (subp & 3) * boxh;
        ctx.fillRect(x, y, boxw, boxh);
        ctx.strokeText(v, x + 2, y + 4);
      }
    }
  }
  
  renderGraphicTable(canvas, ctx, dstx, dsty, dsttilesize, table, name, subp) {
    if (!table) return;
    const boxw = dsttilesize >> 1;
    const boxh = boxw;
    const subx = (subp & 2) ? boxw : 0;
    const suby = (subp & 1) ? boxh : 0;
    if (name === "neighbors") {
      ctx.strokeStyle = "#fff";
    } else {
      ctx.globalAlpha = 0.75;
    }
    for (let row=0, p=0; row<16; row++) {
      for (let col=0; col<16; col++, p++) {
        const v = table[p];
        if (!v) continue; // Zeroes are always omitted.
        const x = Math.round(dstx + col * dsttilesize) + subx;
        const y = Math.round(dsty + row * dsttilesize) + suby;
        if (name === "neighbors") {
          this.renderNeighborMask(ctx, x, y, boxw, boxh, v);
        } else {
          ctx.fillStyle = TilesheetEditor.colorForTableValue(v);
          ctx.fillRect(x, y, boxw, boxh);
        }
      }
    }
    ctx.globalAlpha = 1;
  }
  
  renderNeighborMask(ctx, x, y, w, h, mask) {
    ctx.beginPath();
    const midx = x + (w >> 1);
    const midy = y + (h >> 1);
    if (mask & 0x80) { ctx.moveTo(midx, midy); ctx.lineTo(x, y); }
    if (mask & 0x40) { ctx.moveTo(midx, midy); ctx.lineTo(midx, y); }
    if (mask & 0x20) { ctx.moveTo(midx, midy); ctx.lineTo(x + w, y); }
    if (mask & 0x10) { ctx.moveTo(midx, midy); ctx.lineTo(x, midy); }
    if (mask & 0x08) { ctx.moveTo(midx, midy); ctx.lineTo(x + w, midy); }
    if (mask & 0x04) { ctx.moveTo(midx, midy); ctx.lineTo(x, y + h); }
    if (mask & 0x02) { ctx.moveTo(midx, midy); ctx.lineTo(midx, y + h); }
    if (mask & 0x01) { ctx.moveTo(midx, midy); ctx.lineTo(x + w, y + h); }
    ctx.stroke();
  }
  
  /* Slice the bits of (v) a little funny to ensure that low values are well differentiated.
   *   v: x x x x  x x x x
   *   r:   5      6     7
   *   g: 5     6      7
   *   b:     6      7
   */
  static colorForTableValue(v) {
    let r=0, g=0, b=0;
    if (v & 0x01) r |= 0x80;
    if (v & 0x02) g |= 0x80;
    if (v & 0x04) b |= 0x80;
    if (v & 0x08) r |= 0x40;
    if (v & 0x10) g |= 0x40;
    if (v & 0x20) b |= 0x40;
    if (v & 0x40) r |= 0x20;
    if (v & 0x80) g |= 0x20;
    r |= r >> 3; r |= r >> 6;
    g |= g >> 3; g |= g >> 6;
    b |= b >> 2; b |= b >> 4;
    return `rgb(${r},${g},${b})`;
  }
  
  /* Events.
   *************************************************************************************/
   
  dirty() {
    this.data.dirty(this.res.path, () => TilesheetEditor.encode(this.model));
  }
   
  tileidFromEvent(event) {
  
    // Carefully repeat render's placement.
    // It's simple enough that I'm ok with the duplication.
    const canvas = this.element.querySelector(".canvas");
    const bounds = canvas.getBoundingClientRect();
    const dstw = Math.min(bounds.width, bounds.height);
    const dsth = dstw;
    const dstx = (bounds.width >> 1) - (dstw >> 1);
    const dsty = (bounds.height >> 1) - (dsth >> 1);
    const dsttilesize = dstw / 16; // May be fractional, expect it to be so 15/16 of the time.
    
    const col = Math.floor((event.x - dstx - bounds.x) / dsttilesize);
    if ((col < 0) || (col > 15)) return -1;
    const row = Math.floor((event.y - dsty - bounds.y) / dsttilesize);
    if ((row < 0) || (row > 15)) return -1;
    return (row << 4) | col;
  }
  
  onVisibilityChanged() {
    this.renderSoon();
  }
  
  manualEditTile(p) {
    if ((p < 0) || (p > 0xff)) return;
    const modal = this.dom.spawnModal(TileModal);
    modal.setup(this.res.path, this.image, this.model, p);
    modal.result.then(result => {
      this.renderSoon();
    }).catch(e => this.dom.modalError(e));
  }
  
  copySelection(dstp) {
    if (dstp === this.dragp) return;
    this.dragp = dstp;
    if ((dstp < 0) || (dstp > 0xff)) return;
    if (dstp === this.selp) return;
    const col = dstp & 15;
    const row = dstp >> 4;
    let changed = false;
    for (const tableName of this.getVisibleTables()) {
      const table = this.model[tableName];
      if (!table) continue;
      if (table[dstp] === table[this.selp]) continue;
      table[dstp] = table[this.selp];
      changed = true;
    }
    if (changed) {
      this.renderSoon();
      this.dirty();
    }
  }
  
  onMouseMoveOrUp(event) {
    if (event.type === "mouseup") {
      this.window.removeEventListener("mousemove", this.mouseListener);
      this.window.removeEventListener("mouseup", this.mouseListener);
      this.mouseListener = null;
      return;
    }
    const p = this.tileidFromEvent(event);
    this.copySelection(p);
  }
  
  onMouseDown(event) {
    const p = this.tileidFromEvent(event);

    if (event.ctrlKey) {
      if (p === this.selp) this.selp = -1;
      else this.selp = p;
      this.renderSoon();

    } else if (event.shiftKey) {
      if (p < 0) return;
      if (this.selp < 0) return;
      if (this.mouseListener) return;
      this.mouseListener = e => this.onMouseMoveOrUp(e);
      this.window.addEventListener("mousemove", this.mouseListener);
      this.window.addEventListener("mouseup", this.mouseListener);
      this.dragp = -1;
      this.copySelection(p);
      
    } else {
      if (p < 0) return;
      this.manualEditTile(p);
    }
  }
  
  /* Encoding.
   ********************************************************************/
   
  static decode(src) {
    if (src instanceof Uint8Array) src = new TextDecoder("utf8").decode(src);
    if (typeof(src) !== "string") return {};
    const dst = {};
    let table = null; // Uint8Array(256)
    let tablep = 0; // 0..240 by 16
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      if (!line || (line[0] === "#")) continue;
      
      if (!table) {
        table = new Uint8Array(256);
        dst[line] = table;
        tablep = 0;
        continue;
      }
      
      if (!line.match(/^[0-9a-fA-F]{32}$/)) {
        throw new Error(`tilesheet:${lineno}: Expected 32 hex digits`);
      }
      for (let linep=0; linep<32; linep+=2, tablep++) {
        table[tablep] = parseInt(line.substring(linep, linep + 2), 16);
      }
      if (tablep >= 0x100) {
        table = null;
        tablep = 0;
      }
    }
    if (table) throw new Error(`tilesheet: Incomplete final table`);
    return dst;
  }
  
  static encode(src) {
    let dst = "";
    for (const name of Object.keys(src)) {
      const table = src[name];
      // Tables of all zero should be omitted.
      let allZero = true;
      for (let i=0; i<256; i++) if (table[i]) { allZero = false; break; }
      if (allZero) continue;
      // But if even one tile is nonzero, emit the whole thing.
      dst += name + "\n";
      for (let y=0, p=0; y<16; y++) {
        for (let x=0; x<16; x++, p++) {
          dst += "0123456789abcdef"[table[p] >> 4];
          dst += "0123456789abcdef"[table[p] & 15];
        }
        dst += "\n";
      }
      dst += "\n";
    }
    return new TextEncoder("utf8").encode(dst);
  }
}
