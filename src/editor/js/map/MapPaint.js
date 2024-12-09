/* MapPaint.js
 * Manages application of MapEditor's painting tools.
 * Note that this is deliberately not a singleton, even though it might feel like one.
 * Each MapEditor instance gets a fresh MapPaint, and other parties should access it via MapEditor.
 */
 
import { MapRes } from "./MapRes.js";
 
export class MapPaint {
  static getDependencies() {
    return [];
  }
  constructor() {
    this.mapEditor = null;
    this.map = null;
    this.mousex = 0; // Most recent coords, when tracking motion.
    this.mousey = 0;
    this.toolRunning = ""; // Name of tool, if we're tracking motion.
    this.monalisaAnchor = null; // [x,y] when in use
    this.pedometer = null; // Array of [col,row] when in use.
    this.selection = null; // Array of [col,row] when in use. Even if rectangular, no sense optimizing for that.
    this.marquee = null; // [x,y,w,h] while making a selection. (the lasso commits immediately to this.selection). (w,h) never zero but may be negative.
    this.dragSelectionPrevious = null; // [x,y] when in use, of previous mouse position.
    this.float = null; // Uint8Array of map's size if (selection) set.
    this.floatOffset = null; // [dx,dy]
    this.selectMode = ""; // "add" | "remove"
    this.shiftKey = false;
    this.ctrlKey = false;
  }
  
  setup(mapEditor, map) {
    this.mapEditor = mapEditor;
    this.map = map;
  }
  
  setShiftKey(v) {
    this.shiftKey = !!v;
  }
  
  setControlKey(v) {
    this.ctrlKey = !!v;
  }
  
  /* High-level events from MapCanvas.
   **********************************************************************/
  
  // Returns true if you are expected to track further motion and eventually call mouseUp().
  // If false, we're done with this gesture.
  mouseDown(x, y) {
    if (this.toolRunning) return false;
    if ((x < 0) || (y < 0) || (x >= this.mapEditor.map.w) || (y >= this.mapEditor.map.h)) return false;
    const toolName = this.mapEditor.mapToolbar.getToolName();
    if (!toolName) return false;
    this.mousex = x;
    this.mousey = y;
    let result = false;
    switch (toolName) {
      case "verbatim": result = this.verbatimBegin(); break;
      case "rainbow": result = this.rainbowBegin(); break;
      case "monalisa": result = this.monalisaBegin(); break;
      case "pickup": result = this.pickupBegin(); break;
      case "heal": result = this.healBegin(); break;
      case "marquee": result = this.marqueeBegin(); break;
      case "lasso": result = this.lassoBegin(); break;
      case "poimove": result = this.poimoveBegin(); break;
      case "poiedit": result = this.poieditBegin(); break;
      case "door": result = this.doorBegin(); break;
      case "pedometer": result = this.pedometerBegin(); break;
    }
    if (!result) return false;
    this.toolRunning = toolName;
    return true;
  }
  
  mouseMove(x, y) {
    if (!this.toolRunning) return;
    if ((x < 0) || (y < 0) || (x >= this.mapEditor.map.w) || (y >= this.mapEditor.map.h)) return;
    if ((x === this.mousex) && (y === this.mousey)) return;
    this.mousex = x;
    this.mousey = y;
    switch (this.toolRunning) {
      case "verbatim": this.verbatimMove(); break;
      case "rainbow": this.rainbowMove(); break;
      case "monalisa": this.monalisaMove(); break;
      case "heal": this.healMove(); break;
      case "marquee": this.marqueeMove(); break;
      case "lasso": this.lassoMove(); break;
      case "poimove": this.poimoveMove(); break;
      case "pedometer": this.pedometerMove(); break;
    }
  }
  
  mouseUp(x, y) {
    switch (this.toolRunning) {
      case "marquee": this.marqueeEnd(); break;
      case "lasso": this.lassoEnd(); break;
    }
    this.toolRunning = "";
  }
  
  onToolChanged(toolName) {
    this.mapEditor.mapToolbar.setNote("");
    if (this.pedometer) {
      this.pedometer = null;
      this.mapEditor.mapCanvas.renderSoon();
    }
    if (this.selection) {
      if ((toolName !== "marquee") && (toolName !== "lasso")) {
        this.copySelectionDown();
        this.selection = null;
      }
    }
  }
  
  /* Tool specifics.
   * All "Begin" functions must return true to receive "Move" or "End".
   * (this.mousex,y) are set before every call, and are always in bounds.
   **************************************************************************/
   
  verbatimBegin() {
    this.verbatimMove();
    return true;
  }
  
  verbatimMove() {
    this.mapEditor.map.v[this.mousey * this.mapEditor.map.w + this.mousex] = this.mapEditor.mapToolbar.selectedTileid;
    this.mapEditor.dirty();
    this.mapEditor.mapCanvas.renderSoon();
  }
  
  // Rainbow pencil is just regular pencil followed by heal, easy.
  rainbowBegin() {
    this.verbatimMove();
    this.healMove();
    return true;
  }
  
  rainbowMove() {
    this.verbatimMove();
    this.healMove();
  }
  
  monalisaBegin() {
    this.monalisaAnchor = [this.mousex, this.mousey];
    this.mapEditor.map.v[this.mousey * this.mapEditor.map.w + this.mousex] = this.mapEditor.mapToolbar.selectedTileid;
    this.mapEditor.dirty();
    this.mapEditor.mapCanvas.renderSoon();
    return true;
  }
  
  monalisaMove() {
    const dx = this.mousex - this.monalisaAnchor[0];
    const dy = this.mousey - this.monalisaAnchor[1];
    const col = (this.mapEditor.mapToolbar.selectedTileid & 15) + dx;
    const row = (this.mapEditor.mapToolbar.selectedTileid >> 4) + dy;
    if ((col < 0) || (row < 0) || (col >= 16) || (row >= 16)) return;
    this.mapEditor.map.v[this.mousey * this.mapEditor.map.w + this.mousex] = (row << 4) | col;
    this.mapEditor.dirty();
    this.mapEditor.mapCanvas.renderSoon();
  }
  
  pickupBegin() {
    this.mapEditor.mapToolbar.selectTile(this.mapEditor.map.v[this.mousey * this.mapEditor.map.w + this.mousex]);
  }
  
  healBegin() {
    this.healMove();
    return true;
  }
  
  healMove() {
    let changed = false;
    for (let dx=-1; dx<=1; dx++) {
      for (let dy=-1; dy<=1; dy++) {
        if (this.healCell(this.mousex + dx, this.mousey + dy)) changed = true;
      }
    }
    if (changed) {
      this.mapEditor.dirty();
      this.mapEditor.mapCanvas.renderSoon();
    }
  }
  
  poimoveBegin() {
    //TODO POI. Needs some kind of coordination with MapCanvas, because we don't have fractional coords here.
    return false;
  }
  
  poimoveMove() {
  }
  
  poieditBegin() {
    //TODO POI. Needs some kind of coordination with MapCanvas, because we don't have fractional coords here.
    return false;
  }
  
  doorBegin() {
    //TODO POI. Needs some kind of coordination with MapCanvas, because we don't have fractional coords here.
    return false;
  }
  
  /* During Spelling Bee, I had a need here and there to measure arbitrary paths in a map.
   * Did it the hard way then and I don't recommend it.
   * So we have this simple tool that you can draw a trail on the map and the tattle will show how many cells you've covered.
   */
  pedometerBegin() {
    // If they start on the final entry, we're extending it. Otherwise starting a new trail.
    if (this.pedometer?.length && (this.mousex == this.pedometer[this.pedometer.length - 1][0]) && (this.mousey === this.pedometer[this.pedometer.length - 1][1])) {
      // OK, doing it.
    } else {
      this.pedometer = [];
      this.pedometerMove();
    }
    return true;
  }
  
  pedometerMove() {
    this.pedometer.push([this.mousex, this.mousey]);
    this.mapEditor.mapCanvas.renderSoon();
    this.mapEditor.mapToolbar.setNote(this.pedometer.length);
  }
  
  /* Selection tools (marquee, lasso).
   * These are very similar to each other and very dissimilar to other tools.
   * If either tool clicks in the selection:
   *  - If SHIFT, drop a copy of the selection.
   *  - Begin moving selection.
   * Either tool clicks outside the selection:
   *  - If SHIFT, add to current selection.
   *  - If CTRL, remove from current selection.
   *  - If neither, drop the current selection and start a fresh one.
   * Note that it is not possible to clear the selection under these rules.
   * You have to press ESC or select a different tool.
   *************************************************************************************/
   
  marqueeBegin() {
    if (!this.ctrlKey && this.pointInSelection(this.mousex, this.mousey)) {
      if (this.shiftKey) this.copySelectionDown();
      this.dragSelectionPrevious = [this.mousex, this.mousey];
      return true;
    }
    this.marquee = [this.mousex, this.mousey, 1, 1];
    if (this.shiftKey | this.ctrlKey) {
      if (!this.selection) this.selection = [];
      if (!this.float) this.float = new Uint8Array(this.mapEditor.map.w * this.mapEditor.map.h);
      if (!this.floatOffset) this.floatOffset = [0, 0];
      this.selectMode = this.ctrlKey ? "remove": "add";
    } else {
      this.copySelectionDown();
      this.selection = [];
      this.float = new Uint8Array(this.mapEditor.map.w * this.mapEditor.map.h);
      this.floatOffset = [0, 0];
      this.selectMode = "add";
    }
    this.mapEditor.mapCanvas.renderSoon();
    return true;
  }
  
  lassoBegin() {
    if (!this.ctrlKey && this.pointInSelection(this.mousex, this.mousey)) {
      if (this.shiftKey) this.copySelectionDown();
      this.dragSelectionPrevious = [this.mousex, this.mousey];
      return true;
    }
    if (this.shiftKey | this.ctrlKey) {
      if (!this.selection) this.selection = [];
      if (!this.float) this.float = new Uint8Array(this.mapEditor.map.w * this.mapEditor.map.h);
      if (!this.floatOffset) this.floatOffset = [0, 0];
      this.selectMode = this.ctrlKey ? "remove" : "add";
    } else {
      this.copySelectionDown();
      this.selection = [];
      this.float = new Uint8Array(this.mapEditor.map.w * this.mapEditor.map.h);
      this.floatOffset = [0, 0];
      this.selectMode = "add";
    }
    this.lassoMove();
    return true;
  }
  
  marqueeMove() {
    if (this.dragSelectionPrevious) {
      this.dragSelectionMove();
    } else {
      if ((this.marquee[2] = this.mousex - this.marquee[0]) >= 0) this.marquee[2]++;
      if ((this.marquee[3] = this.mousey - this.marquee[1]) >= 0) this.marquee[3]++;
      this.mapEditor.mapCanvas.renderSoon();
    }
  }
  
  lassoMove() {
    if (this.dragSelectionPrevious) {
      this.dragSelectionMove();
    } else {
      switch (this.selectMode) {
        case "add": this.selectionAdd(this.mousex, this.mousey); break;
        case "remove": this.selectionRemove(this.mousex, this.mousey); break;
      }
    }
  }
  
  marqueeEnd() {
    if (this.dragSelectionPrevious) {
      this.dragSelectionEnd();
    } else {
      if (this.marquee[2] < 0) { this.marquee[0] += this.marquee[2]; this.marquee[2] = -this.marquee[2] + 1; }
      if (this.marquee[3] < 0) { this.marquee[1] += this.marquee[3]; this.marquee[3] = -this.marquee[3] + 1; }
      this.marquee[2] += this.marquee[0];
      this.marquee[3] += this.marquee[1];
      for (let y=this.marquee[1]; y<this.marquee[3]; y++) {
        for (let x=this.marquee[0]; x<this.marquee[2]; x++) {
          if (this.selectMode === "add") this.selectionAdd(x, y);
          else this.selectionRemove(x, y);
        }
      }
      this.marquee = null;
      this.mapEditor.mapCanvas.renderSoon();
    }
  }
  
  lassoEnd() {
    if (this.dragSelectionPrevious) {
      this.dragSelectionEnd();
    } else {
    }
  }
  
  dragSelectionMove() {
    const dx = this.mousex - this.dragSelectionPrevious[0];
    const dy = this.mousey - this.dragSelectionPrevious[1];
    for (const cell of this.selection) {
      cell[0] += dx;
      cell[1] += dy;
    }
    this.floatOffset[0] += dx;
    this.floatOffset[1] += dy;
    this.dragSelectionPrevious = [this.mousex, this.mousey];
    this.mapEditor.mapCanvas.renderSoon();
  }
  
  dragSelectionEnd() {
    this.dragSelectionPrevious = null;
  }
  
  pointInSelection(x, y) {
    if (!this.selection) return false;
    return this.selection.find(s => ((s[0] === x) && (s[1] === y)));
  }
  
  copySelectionDown() {
    if (!this.selection) return;
    for (const [dstx, dsty] of this.selection) {
      if ((dstx < 0) || (dsty < 0) || (dstx >= this.mapEditor.map.w) || (dsty >= this.mapEditor.map.h)) continue;
      const srcx = dstx - this.floatOffset[0];
      const srcy = dsty - this.floatOffset[1];
      if ((srcx >= 0) && (srcy >= 0) && (srcx < this.mapEditor.map.w) && (srcy < this.mapEditor.map.h)) {
        this.mapEditor.map.v[dsty * this.mapEditor.map.w + dstx] = this.float[srcy * this.mapEditor.map.w + srcx];
      }
    }
    this.mapEditor.dirty();
    this.mapEditor.mapCanvas.renderSoon();
  }
  
  selectionAdd(x, y) {
    if (this.pointInSelection(x, y)) return;
    this.selection.push([x, y]);
    const dstx = x + this.floatOffset[0];
    const dsty = y + this.floatOffset[1];
    if ((dstx >= 0) && (dsty >= 0) && (dstx < this.mapEditor.map.w) && (dsty < this.mapEditor.map.h)) {
      this.float[dsty * this.mapEditor.map.w + dstx] = this.mapEditor.map.v[y * this.mapEditor.map.w + x];
      this.mapEditor.map.v[y * this.mapEditor.map.w + x] = 0;
    }
    this.mapEditor.mapCanvas.renderSoon();
  }
  
  selectionRemove(x, y) {
    const p = this.selection.findIndex(s => ((s[0] === x) && (s[1] === y)));
    if (p < 0) return;
    this.selection.splice(p, 1);
    const dstx = x + this.floatOffset[0];
    const dsty = y + this.floatOffset[1];
    if ((dstx >= 0) && (dsty >= 0) && (dstx < this.mapEditor.map.w) && (dsty < this.mapEditor.map.h)) {
      this.mapEditor.map.v[y * this.mapEditor.map.w + x] = this.float[dsty * this.mapEditor.map.w + dstx];
    }
    this.mapEditor.mapCanvas.renderSoon();
  }
  
  // Public. MapEditor calls this on ESC.
  dropSelection() {
    if (this.toolInProgress) return;
    if (!this.selection) return;
    this.copySelectionDown();
    this.selection = null;
  }
  
  deleteSelection() {
    if (this.toolInProgress) return;
    if (!this.selection) return;
    this.selection = null;
    this.mapEditor.dirty();
    this.mapEditor.mapCanvas.renderSoon();
  }
  
  // Return [[x,y]...] and drop selection.
  captureSelection() {
    const pv = this.selection || [];
    this.dropSelection();
    return pv;
  }
  
  // Re-float a previous selection acquired from captureSelection().
  restoreSelection(pv) {
    this.dropSelection();
    this.selection = [];
    for (const [x, y] of pv) this.selectionAdd(x, y);
  }
  
  /* Heal.
   * This is its own section because it's complex and sensitive.
   * healCell(x, y) does not call out at all. Instead it returns true if anything changed.
   *******************************************************************/
   
  healCell(x, y) {
  
    // Get out fast if it's OOB, family zero, or appointment-only.
    if (!this.mapEditor.tilesheet.family) return false;
    if ((x < 0) || (y < 0) || (x >= this.mapEditor.map.w) || (y >= this.mapEditor.map.h)) return false;
    const tileid = this.mapEditor.map.v[y * this.mapEditor.map.w + x];
    if (this.mapEditor.tilesheet.weight?.[tileid] === 0xff) return false;
    const family = this.mapEditor.tilesheet.family[tileid];
    if (!family) return false;
    
    // Which of my 8 neighbors are in the same family?
    const neighbors = this.collectNeighbors(x, y, family);
    
    // Assemble a set of candidate tiles.
    const candidates = this.collectCandidateTiles(family, neighbors);
    
    // It's possible to discover no candidates. Whatever, just get out.
    if (!candidates.length) return false;
    
    // Select one of the candidates according to weight.
    const ntileid = this.pickCandidateTile(candidates);
    
    // And if it's different, commit it.
    if (ntileid === tileid) return false;
    this.mapEditor.map.v[y * this.mapEditor.map.w + x] = ntileid;
    return true;
  }
  
  // 8-bit mask of neighbors for a given cell. You tell us the family.
  collectNeighbors(x, y, family) {
    let neighbors = 0;
    for (let dy=-1, mask=0x80; dy<=1; dy++) {
      const ny = y + dy;
      for (let dx=-1; dx<=1; dx++) {
        if (!dx && !dy) continue; // important to continue without advancing (mask).
        const nx = x + dx;
        if ((nx >= 0) && (ny >= 0) && (nx < this.mapEditor.map.w) && (ny < this.mapEditor.map.h)) {
          const otherFamily = this.mapEditor.tilesheet.family[this.mapEditor.map.v[ny * this.mapEditor.map.w + nx]];
          if (otherFamily === family) {
            neighbors |= mask;
          }
        }
        mask >>= 1;
      }
    }
    return neighbors;
  }
  
  // Array of tileid in the given family, whose neighbor masks contain only bits in (neighbors), filtered to the highest bit count.
  collectCandidateTiles(family, neighbors) {
    let candidates = [];
    let popc = 0;
    const popcnt8 = (v) => {
      let c = 0;
      v &= 0xff;
      for (; v; v>>=1) if (v & 1) c++;
      return c;
    };
    const forbitten = ~neighbors;
    const famv = this.mapEditor.tilesheet.family;
    const neiv = this.mapEditor.tilesheet.neighbors;
    for (let tileid=0; tileid<0x100; tileid++) {
      if (famv[tileid] !== family) continue;
      const mask = neiv ? neiv[tileid] : 0;
      if (mask & forbitten) continue;
      const mpopc = popcnt8(mask);
      if (mpopc < popc) continue;
      if (mpopc > popc) {
        candidates = [];
        popc = mpopc;
      }
      candidates.push(tileid);
    }
    return candidates;
  }
  
  pickCandidateTile(candidates) {
    if (candidates.length < 2) return candidates[0];
    const wv = this.mapEditor.tilesheet.weight;
    if (wv) {
      // 0..254,255=likely..unlikely,appt_only
      // If they're all appointment-only, fall thru and pick balanced-random.
      // Otherwise, remove the appointment-only ones.
      let aoc = 0;
      for (const candidate of candidates) {
        if (wv[candidate] === 0xff) aoc++;
      }
      if (aoc < candidates.length) {
        if (aoc) candidates = candidates.filter(t => (wv[t] !== 0xff));
        const wsum = candidates.reduce((a, v) => a + (0x100 - wv[v]), 0);
        let choice = Math.random() * wsum;
        for (const candidate of candidates) {
          choice -= 0x100 - wv[candidate];
          if (choice <= 0) return candidate;
        }
      }
    }
    return candidates[Math.floor(Math.random() * candidates.length)];
  }
  
  /* Resize map.
   ***************************************************************/
   
  resize(w, h) {
    if ((w < 1) || (w > 0xffff) || (h < 1) || (h > 0xffff)) throw new Error(`Invalid dimensions ${w} x ${h}`);
    if ((w === this.map.w) && (h === this.map.h)) return false;
    const nv = new Uint8Array(w * h);
    // We always anchor top left. I figure since we have a marquee and layer-dragging, the user can prep it himself.
    this.copyCells(nv, w, h, this.map.v, this.map.w, this.map.h);
    this.map.v = nv;
    this.map.w = w;
    this.map.h = h;
    this.mapEditor.mapCanvas.recalculateSize();
    this.mapEditor.dirty();
    return true;
  }
  
  copyCells(dst, dstw, dsth, src, srcw, srch) {
    const cpw = Math.min(dstw, srcw);
    const cph = Math.min(dsth, srch);
    for (let dstrowp=0, srcrowp=0, yi=cph; yi-->0; dstrowp+=dstw, srcrowp+=srcw) {
      for (let dstp=dstrowp, srcp=srcrowp, xi=cpw; xi-->0; dstp++, srcp++) {
        dst[dstp] = src[srcp];
      }
    }
  }
}

MapPaint.singleton = false; // sic

MapPaint.TOOLS = [
  { name: "verbatim",  icon:  0 },
  { name: "rainbow",   icon:  1 },
  { name: "monalisa",  icon:  2 },
  { name: "pickup",    icon:  3 },
  { name: "heal",      icon:  4 },
  { name: "marquee",   icon:  5 },
  { name: "lasso",     icon:  6 },
  { name: "poimove",   icon:  7 },
  { name: "poiedit",   icon:  8 },
  { name: "door",      icon:  9 },
  { name: "pedometer", icon: 10 },
];
