/* EnvUi.js
 */
 
import { Dom } from "../Dom.js";
import { EnvPointModal } from "./EnvPointModal.js";

export class EnvUi {
  static getDependencies() {
    return [HTMLElement, Dom, "nonce", Window];
  }
  constructor(element, dom, nonce, window) {
    this.element = element;
    this.dom = dom;
    this.nonce = nonce;
    this.window = window;
    
    this.hint = ""; // "", "level", "pitch", "range"
    this.flags = 0; // as encoded
    this.susp = -1; // <0 or valid
    this.loline = []; // {t:abs ms,v:0..65535}
    this.hiline = []; // parallel to loline
    this.renderTimeout = null;
    this.mouseListener = null;
    this.mouseAnchor = [0, 0]; // Event coordinates for zoom; canvas coordinates for canvas.
    this.mouseLine = "", // "lo", "hi" during canvas drag.
    this.mouseIndex = -1; // Which point in line.
    this.mouseDelta = [0, 0]; // Offset to point's location, from mouse's, in canvas pixels.
    this.vboxAnchor = null; // Copy of (this.vbox) during Zoom drag.
    this.handleRadius = 6;
    this.vbox = { // Visible bounds in model space (gets recalculated after acquiring model)
      ta: 0,      // Left edge in ms.
      tz: 1000,   // Right edge in ms.
      va: 0,      // Bottom edge in value units.
      vz: 0xffff, // Top edge in value units.
    };
    
    // Vertical guidelines in time, fine to coarse (ie rendering order).
    this.timeGuides = [
      [    1, "#800"],
      [   10, "#444"],
      [   50, "#666"],
      [  250, "#888"],
      [ 1000, "#4a4"],
      [ 4000, "#ccc"],
      [16000, "#0f0"],
    ];
    this.timeGuideSpacing = 10; // Draw time guides if at least so many pixels between.
    
    // Horizontal guidelines in value, fine to coarse.
    this.valueGuidesGeneric = [
      [0x0001, "#800"],
      [0x0010, "#444"],
      [0x0100, "#666"],
      [0x1000, "#888"],
      [0x8000, "#aaa"],
    ];
    this.valueGuidesPitch = [
      [    1, "#800"],
      [  100, "#444"],
      [ 1200, "#4a4"],
      [32768, "#0f0"],
    ];
    this.valueGuideSpacing = 10;
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
  
  /* Decode.
   ***********************************************************************************/
  
  /* Decodes from Uint8Array (src) starting at (srcp) and returns the final position.
   */
  setup(src, srcp, hint) {
  
    /* Lots to decode...
     */
    let flags=0, inivlo=0, inivhi=0, defaultFlags = false;;
    if (srcp < src.length) {
      flags = src[srcp++];
    } else if (hint === "pitch") {
      defaultFlags = true;
      inivlo = inivhi = 0x8000;
    } else if (hint === "range") {
      defaultFlags = true;
      inivlo = inivhi = 0xffff;
    }
    if (flags & 0x01) {
      inivlo = (src[srcp] << 8) | src[srcp+1];
      srcp += 2;
      if (flags & 0x02) {
        inivhi = (src[srcp] << 8) | src[srcp+1];
        srcp += 2;
      } else {
        inivhi = inivlo;
      }
    }
    let susp=-1;
    if (flags & 0x04) {
      susp = src[srcp++];
    }
    let pointc = 0;
    if (srcp < src.length) {
      pointc = src[srcp++];
    }
    const loline=[{ t: 0, v: inivlo }], hiline=[{ t: 0, v: inivhi }];
    let lot=0, hit=0;
    for (let i=pointc; i-->0; ) {
      const lo = {};
      if (src[srcp] & 0x80) {
        lot += ((src[srcp] & 0x7f) << 7) | src[srcp+1];
        srcp += 2;
      } else {
        lot += src[srcp++];
      }
      lo.t = lot;
      lo.v = (src[srcp] << 8) | src[srcp+1];
      srcp += 2;
      loline.push(lo);
      if (flags & 0x02) {
        const hi = {};
        if (src[srcp] & 0x80) {
          hit += ((src[srcp] & 0x7f) << 7) | src[srcp+1];
          srcp += 2;
        } else {
          hit += src[srcp++];
        }
        hi.t = hit;
        hi.v = (src[srcp] << 8) | src[srcp+1];
        srcp += 2;
        hiline.push(hi);
      } else {
        hiline.push({...lo});
      }
    }
    if (susp >= pointc) susp = -1;
    
    /* Apply to local model.
     */
    this.hint = hint;
    if (defaultFlags) {
      this.flags = 0x03; // init,velocity
    } else {
      this.flags = flags;
    }
    this.susp = susp;
    this.loline = loline;
    this.hiline = hiline;
    
    this.calculateInitialBounds();
    this.buildUi();
    return srcp;
  }
  
  /* Encode.
   **********************************************************************************/
  
  // Appends to Encoder (dst).
  encode(dst) {
  
    /* (this.flags) stays fresh generally, except we'll reassess 0x01 (init).
     * (velocity,sustain) get updated at the moment of change.
     */
    if (this.loline[0].v || ((this.flags & 0x02) && this.hiline[0].v)) {
      this.flags |= 0x01;
    } else {
      this.flags &= ~0x01;
    }
    dst.u8(this.flags);
  
    // Initial value.
    if (this.flags & 0x01) {
      dst.u16be(this.loline[0].v);
      if (this.flags & 0x02) {
        dst.u16be(this.hiline[0].v);
      }
    }
    
    // Sustain index.
    if (this.flags & 0x04) {
      dst.u8(this.susp);
    }
    
    // Point count. Beware that [0] is a dummy for the initial point.
    // (loline,hiline) always have the same length; that gets enforced at the moment of changes.
    if (this.loline.length >= 256) throw new Error(`Too many envelope points. ${this.loline.length}, limit 255.`);
    dst.u8(this.loline.length - 1);
    
    // Rephrase points with relative times, and eliminate the first entry.
    const loline=[], hiline=[];
    for (let now=0, i=1; i<this.loline.length; i++) {
      const pt = this.loline[i];
      let t = pt.t - now;
      if (t >= 16384) throw new Error(`Envelope leg too long. ${t}, limit 16383.`);
      loline.push({ t, v: pt.v });
    }
    if (this.flags & 0x02) for (let now=0, i=1; i<this.hiline.length; i++) {
      const pt = this.hiline[i];
      let t = pt.t - now;
      if (t >= 16384) throw new Error(`Envelope leg too long. ${t}, limit 16383.`);
      hiline.push({ t, v: pt.v });
    }
    
    // Emit points.
    for (let i=0; i<loline.length; i++) {
      if (loline[i].t >= 0x80) {
        dst.u8(0x80 | (loline[i].t >> 7));
        dst.u8(loline[i].t & 0x7f);
      } else {
        dst.u8(loline[i].t);
      }
      dst.u16be(loline[i].v);
      if (this.flags & 0x02) {
        if (hiline[i].t >= 0x80) {
          dst.u8(0x80 | (hiline[i].t >> 7));
          dst.u8(hiline[i].t & 0x7f);
        } else {
          dst.u8(hiline[i].t);
        }
        dst.u16be(hiline[i].v);
      }
    }
  }
  
  /* Layout and render.
   ***********************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "CANVAS", ["graph"], {
      "on-mousemove": e => this.populateTattle(e),
      "on-mouseleave": e => this.clearTattle(),
      "on-mousewheel": e => this.onCanvasWheel(e),
      "on-mousedown": e => this.onCanvasMouseDown(e),
      "on-contextmenu": e => e.preventDefault(),
    });
    
    const controls = this.dom.spawn(this.element, "DIV", ["controls"]);
    this.dom.spawn(controls, "SELECT", { name: "loVisibility", "on-change": () => this.renderSoon() },
      this.dom.spawn(null, "OPTION", { value: "visible" }, "Lo Visible"),
      this.dom.spawn(null, "OPTION", { value: "hidden" }, "Lo Hidden")
    );
    this.dom.spawn(controls, "SELECT", { name: "hiVisibility", "on-change": () => this.onHiVisibilityChanged() },
      this.dom.spawn(null, "OPTION", { value: "visible" }, "Hi Visible"),
      this.dom.spawn(null, "OPTION", { value: "hidden" }, "Hi Hidden"),
      this.dom.spawn(null, "OPTION", { value: "disabled" }, "Hi Disabled")
    );
    this.dom.spawn(controls, "INPUT", { type: "number", name: "susp", min: -1, max: 255, value: this.susp, "on-input": () => this.onSuspChanged() });
    this.dom.spawn(controls, "DIV", ["zoom"], { "on-mousedown": e => this.onZoomMouseDown(e) }, this.dom.spawn(null, "DIV", "Z"));
    this.dom.spawn(controls, "DIV", ["tattle"]);
    
    this.renderSoon();
  }
  
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 20);
  }
  
  renderNow() {
    const canvas = this.element.querySelector("canvas");
    if (!canvas) return;
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    
    /* Vertical guidelines in time.
     * Don't draw intervals smaller than 1ms or negative times, since those aren't encodable.
     * Make 1ms and 1s more prominent than the others; the others should follow a simple pattern.
     */
    ctx.lineWidth = 1;
    for (const [msSpacing, color] of this.timeGuides) {
      const xSpacing = this.xFromT(canvas.width, this.vbox.ta + msSpacing);
      if (xSpacing < this.timeGuideSpacing) continue;
      ctx.beginPath();
      let t = Math.floor(this.vbox.ta / msSpacing) * msSpacing;
      for (;; t+=msSpacing) {
        if (t < 0) continue;
        const x = this.xFromT(canvas.width, t) + 0.5;
        if (x >= canvas.width) break;
        ctx.moveTo(x, 0);
        ctx.lineTo(x, canvas.height);
      }
      ctx.strokeStyle = color;
      ctx.stroke();
    }
    
    /* Horizontal guidelines in value.
     * Same idea as the vertical time lines, but we use different intervals and an offset when displaying pitch.
     */
    let valueGuides = this.valueGuidesGeneric;
    let valueOffset = 0;
    if (this.hint === "pitch") {
      valueGuides = this.valueGuidesPitch;
      valueOffset = 0x8000;
    }
    for (const [vSpacing, color] of valueGuides) {
      const ySpacing = this.yFromV(canvas.height, this.vbox.vz - vSpacing);
      if (ySpacing < this.valueGuideSpacing) continue;
      ctx.beginPath();
      let vhi=valueOffset, vlo=valueOffset;
      for (;; vhi+=vSpacing, vlo-=vSpacing) {
        if ((vlo < 0) && (vhi > 0x10000)) break;
        if (vhi <= 0x10000) {
          const y = this.yFromV(canvas.height, vhi) + 0.5;
          ctx.moveTo(0, y);
          ctx.lineTo(canvas.width, y);
        }
        if (vlo >= 0) {
          const y = this.yFromV(canvas.height, vlo) + 0.5;
          ctx.moveTo(0, y);
          ctx.lineTo(canvas.width, y);
        }
      }
      ctx.strokeStyle = color;
      ctx.stroke();
    }
    
    if (this.element.querySelector("select[name='loVisibility']")?.value === "visible") {
      this.drawLine(ctx, canvas, this.loline, "#a40", "#ea0");
    }
    if (this.element.querySelector("select[name='hiVisibility']")?.value === "visible") {
      this.drawLine(ctx, canvas, this.hiline, "#e33", "#f66");
    }
  }
  
  drawLine(ctx, canvas, line, lineColor, pointColor) {
    if (line.length < 1) return;
    line = line.map(p => ({
      x: this.xFromT(canvas.width, p.t),
      y: this.yFromV(canvas.height, p.v),
    }));
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(line[0].x, line[0].y);
    for (const point of line) {
      ctx.lineTo(point.x, point.y);
    }
    ctx.strokeStyle = lineColor;
    ctx.stroke();
    ctx.fillStyle = pointColor;
    for (const point of line) {
      ctx.beginPath();
      ctx.ellipse(point.x, point.y, this.handleRadius, this.handleRadius, 0, 0, Math.PI * 2, false);
      ctx.fill();
    }
    // Sustain point. Beware our lines have an extra zero point, which (susp) is not aware of.
    if ((this.susp >= 0) && (this.susp < line.length - 1)) {
      const point = line[this.susp + 1];
      ctx.beginPath();
      ctx.ellipse(point.x, point.y, this.handleRadius + 2, this.handleRadius + 2, 0, 0, Math.PI * 2, false);
      ctx.strokeStyle = "#fff";
      ctx.stroke();
    }
  }
  
  /* Projection.
   * (x,y) are always in the graphics context space, which is exactly the size of the canvas's visible space but zeroed.
   * (t,v) are in model space as encoded -- milliseconds and u16.
   * Both sides get rounded to integers during each conversion.
   * Our converters do not enforce bounds. (eg (t,v) can be negative here, but must clamp to zero before encoding).
   * Note that (y) runs top-to-bottom while (v) runs bottom-to-top; but you shouldn't need to worry about that.
   *************************************************************************************/
  
  /* Determine viewable area from scratch, based on the local model.
   */
  calculateInitialBounds() {
    this.vbox.ta = 0; // Left edge always starts at time zero.
    
    /* Right edge is 1.25 of the furthest (t).
     * Lines must be sorted in time, we don't have to iterate.
     * If all times are zero, set it at 1 second.
     */
    this.vbox.tz = Math.max(this.loline[this.loline.length - 1]?.t || 0, this.hiline[this.hiline.length - 1]?.t);
    if (this.vbox.tz <= 0) this.vbox.tz = 1000;
    else this.vbox.tz = Math.ceil(this.vbox.tz * 1.25);
    
    /* For pitch envelopes, start with one octave in each direction,
     * then expand to contain all present values.
     * The full range for a pitch envelope is ridiculous, 27 octaves each way.
     */
    if (this.hint === "pitch") {
      this.vbox.va = 0x8000 - 1200;
      this.vbox.vz = 0x8000 + 1200;
      for (const { v } of this.loline) {
        if (v < this.vbox.va ) this.vbox.va = v;
        else if (v > this.vbox.vz) this.vbox.vz = v;
      }
      for (const { v } of this.hiline) {
        if (v < this.vbox.va ) this.vbox.va = v;
        else if (v > this.vbox.vz) this.vbox.vz = v;
      }
    
    /* Level and range envelopes are semantically 0..1, and it makes sense to show the full range always.
     */
    } else {
      this.vbox.va = 0;
      this.vbox.vz = 0x10000;
    }
  }
  
  xFromT(vieww, t) {
    return Math.round(((t - this.vbox.ta) * vieww) / (this.vbox.tz - this.vbox.ta));
  }
  
  tFromX(vieww, x) {
    return Math.round(this.vbox.ta + (x * (this.vbox.tz - this.vbox.ta)) / vieww);
  }
  
  yFromV(viewh, v) {
    return Math.round(viewh - ((v - this.vbox.va) * viewh) / (this.vbox.vz - this.vbox.va));
  }
  
  vFromY(viewh, y) {
    y = viewh - y;
    return Math.round(this.vbox.va + (y * (this.vbox.vz - this.vbox.va)) / viewh);
  }
  
  /* Events.
   **********************************************************************************/
   
  onZoomMouse(event) {
    if (event.type === "mouseup") {
      this.window.removeEventListener("mousemove", this.mouseListener);
      this.window.removeEventListener("mouseup", this.mouseListener);
      this.mouseListener = null;
      return;
    }
    const dx = event.x - this.mouseAnchor[0];
    const dy = event.y - this.mouseAnchor[1];
    const tScale = Math.pow(2, dx / -100); // Double scale every 100 pixels.
    const vScale = Math.pow(2, dy / 100); // Double scale every 100 pixels.
    const midt = (this.vboxAnchor.ta + this.vboxAnchor.tz) / 2;
    const midv = (this.vboxAnchor.va + this.vboxAnchor.vz) / 2;
    const tRadius = Math.max(1, Math.round((this.vboxAnchor.tz - this.vboxAnchor.ta) * tScale) / 2);
    const vRadius = Math.max(1, Math.round((this.vboxAnchor.vz - this.vboxAnchor.va) * vScale) / 2);
    if (this.vbox.ta) {
      this.vbox = {
        ta: midt - tRadius,
        tz: midt + tRadius,
        va: midv - vRadius,
        vz: midv + vRadius,
      };
    } else { // When current view touches time zero, we anchor to that instead of view's center.
      this.vbox = {
        ta: 0,
        tz: tRadius * 2,
        va: midv - vRadius,
        vz: midv + vRadius,
      };
    }
    if (this.vbox.ta < 0) this.vbox.ta = 0;
    if (this.vbox.va < 0) this.vbox.va = 0;
    if (this.vbox.vz > 0x10000) this.vbox.vz = 0x10000;
    this.renderSoon();
  }
  
  onZoomMouseDown(event) {
    if (this.mouseListener) return;
    this.mouseAnchor = [event.x, event.y];
    this.vboxAnchor = {...this.vbox};
    this.mouseListener = e => this.onZoomMouse(e);
    this.window.addEventListener("mousemove", this.mouseListener);
    this.window.addEventListener("mouseup", this.mouseListener);
  }
  
  onCanvasMouse(event) {
    if (event.type === "mouseup") {
      this.window.removeEventListener("mousemove", this.mouseListener);
      this.window.removeEventListener("mouseup", this.mouseListener);
      this.mouseListener = null;
      return;
    }
    const line = (this.mouseLine === "lo") ? this.loline : this.hiline;
    const point = line[this.mouseIndex];
    if (!point) return;
    const canvas = this.element.querySelector("canvas");
    const bounds = canvas.getBoundingClientRect();
    const x = (event.x - bounds.x) + this.mouseDelta[0];
    const y = (event.y - bounds.y) + this.mouseDelta[1];
    let t = this.tFromX(bounds.width, x);
    let v = this.vFromY(bounds.height, y);
    if (this.mouseIndex > 0) {
      // (t) must not be lower than the point before us, nor further than 16384 ms from it.
      const pv = line[this.mouseIndex - 1];
      if (t < pv.t) t = pv.t;
      else if (t >= pv.t + 16384) t = pv.t + 16383;
    } else {
      // The very first point must have (t) zero.
      t = 0;
    }
    // For the "level" envelope only, the first and last points must have value zero.
    if (this.hint === "level") {
      if (!this.mouseIndex || (this.mouseIndex === line.length - 1)) {
        v = 0;
      }
    }
    if (v < 0) v = 0;
    else if (v > 0xffff) v = 0xffff;
    const dt = t - point.t;
    for (let i=this.mouseIndex+1; i<line.length; i++) line[i].t += dt;
    point.t = t;
    point.v = v;
    this.renderSoon();
  }
  
  onCanvasMouseDown(event) {
    if (this.mouseListener) return;
    const canvas = this.element.querySelector("canvas");
    if (!canvas) return;
    const bounds = canvas.getBoundingClientRect();
    const x = event.x - bounds.x;
    const y = event.y - bounds.y;
    
    event.preventDefault();
    event.stopPropagation();
    
    /* Check the high line first if visible, since it is rendered second.
     * Also do handles before lines, otherwise the loline handles are hard to reach.
     */
    const hivis = (this.element.querySelector("select[name='hiVisibility']")?.value === "visible");
    const lovis = (this.element.querySelector("select[name='loVisibility']")?.value === "visible");
    if (hivis) {
      if (this.checkMouseDownPoint(x, y, bounds.width, bounds.height, this.hiline, event)) return;
    }
    if (lovis) {
      if (this.checkMouseDownPoint(x, y, bounds.width, bounds.height, this.loline, event)) return;
    }
    if (hivis) {
      if (this.checkMouseDownLine(x, y, bounds.width, bounds.height, this.hiline, event)) return;
    }
    if (lovis) {
      if (this.checkMouseDownLine(x, y, bounds.width, bounds.height, this.loline, event)) return;
    }
  }
  
  checkMouseDownPoint(x, y, vieww, viewh, line, event) {
    if (line.length < 1) return false;
    let whichLine;
    if (line === this.loline) whichLine = "lo";
    else if (line === this.hiline) whichLine = "hi";
    else return false;
    line = line.map(p => ({
      x: this.xFromT(vieww, p.t),
      y: this.yFromV(viewh, p.v),
    }));
    
    const r2 = this.handleRadius * this.handleRadius;
    for (let i=line.length; i-->0; ) {
      const point = line[i];
      const dx = point.x - x;
      const dy = point.y - y;
      if (dx * dx + dy * dy > r2) continue;
      if (event.button === 2) {
        /* Right click on handle. */
        this.editPointManually(whichLine, i);
        return true;
      } else {
        /* Left click on handle. */
        this.mouseListener = e => this.onCanvasMouse(e);
        this.window.addEventListener("mousemove", this.mouseListener);
        this.window.addEventListener("mouseup", this.mouseListener);
        this.mouseAnchor = [x, y];
        this.mouseLine = whichLine;
        this.mouseIndex = i;
        this.mouseDelta = [point.x - x, point.y - y];
        return true;
      }
    }
    
    return false;
  }
  
  /* Look for places where (x,y) intersects this (line).
   * If there is a sensible intersection, take the appropriate action and return true.
   * Return false for caller to consider other options.
   */
  checkMouseDownLine(x, y, vieww, viewh, line, event) {
    if (line.length < 1) return false;
    let whichLine;
    if (line === this.loline) whichLine = "lo";
    else if (line === this.hiline) whichLine = "hi";
    else return false;
    line = line.map(p => ({
      x: this.xFromT(vieww, p.t),
      y: this.yFromV(viewh, p.v),
    }));
    
    /* Scalar rejection must be within (handleRadius), and projection in 0..1.
     * We don't need to fuzz the projection edges; anything up there would be a point click anyway.
     */
    for (let i=1; i<line.length; i++) {
      const pta = line[i - 1];
      const ptb = line[i];
      const len = Math.sqrt((pta.x - ptb.x) ** 2 + (pta.y - ptb.y) ** 2);
      if (len < 1) continue;
      const rej = ((ptb.x - pta.x) * (y - pta.y) - (x - pta.x) * (ptb.y - pta.y)) / len;
      if ((rej < -this.handleRadius) || (rej > this.handleRadius)) continue;
      const proj = ((ptb.x - pta.x) * (x - pta.x) + (ptb.y - pta.y) * (y - pta.y)) / len;
      if ((proj < 0) || (proj >= len)) continue;
      
      // Create a point here, and a noop point at proportional time on the other line.
      const tnorm = proj/len;
      const newPoint = {
        t: this.tFromX(vieww, x),
        v: this.vFromY(viewh, y),
      };
      if (whichLine === "lo") {
        this.loline.splice(i, 0, newPoint);
        this.hiline.splice(i, 0, {
          t: this.hiline[i - 1].t * (1.0 - tnorm) + this.hiline[i].t * tnorm,
          v: this.hiline[i - 1].v * (1.0 - tnorm) + this.hiline[i].v * tnorm,
        });
      } else {
        this.hiline.splice(i, 0, newPoint);
        this.loline.splice(i, 0, {
          t: this.loline[i - 1].t * (1.0 - tnorm) + this.loline[i].t * tnorm,
          v: this.loline[i - 1].v * (1.0 - tnorm) + this.loline[i].v * tnorm,
        });
      }
      
      // Increment susp if we're below it.
      if (i <= this.susp + 1) {
        this.susp++;
        const suspElement = this.element.querySelector("input[name='susp']");
        if (suspElement) suspElement.value = this.susp;
      }
      
      // Begin dragging.
      this.mouseListener = e => this.onCanvasMouse(e);
      this.window.addEventListener("mousemove", this.mouseListener);
      this.window.addEventListener("mouseup", this.mouseListener);
      this.mouseAnchor = [x, y];
      this.mouseLine = whichLine;
      this.mouseIndex = i;
      this.mouseDelta = [0, 0];
      
      this.renderSoon();
      return true;
    }
    
    // Finally, if the click was beyond the end time, add a new point here and start dragging it.
    // I don't want to deal with the case where it's beyond *this* line's time, but not the other's, so require greater than both.
    if (
      (x > this.xFromT(vieww, this.loline[this.loline.length - 1].t)) &&
      (x > this.xFromT(vieww, this.hiline[this.hiline.length - 1].t))
    ) {
      const newPoint = {
        t: this.tFromX(vieww, x),
        v: this.vFromY(viewh, y),
      };
      if (this.hint === "level") newPoint.v = 0;
      this.loline.push(newPoint);
      this.hiline.push({...newPoint});
      this.mouseListener = e => this.onCanvasMouse(e);
      this.window.addEventListener("mousemove", this.mouseListener);
      this.window.addEventListener("mouseup", this.mouseListener);
      this.mouseAnchor = [x, y];
      this.mouseLine = whichLine;
      this.mouseIndex = line.length;
      this.mouseDelta = [0, 0];
      this.renderSoon();
      return true;
    }
    
    return false;
  }
  
  editPointManually(whichLine, index) {
  
    /* I don't understand why, but if we launch the modal during this event cycle, the right-click event gets delivered as contextmenu to the modal's blotter.
     * Not going to bugger around looking for a clean fix, just bump the launch off to the next cycle.
     */
    this.window.setTimeout(() => {
  
    const line = (whichLine === "lo") ? this.loline : (whichLine === "hi") ? this.hiline : null;
    if (!line) return;
    const point = line[index];
    if (!point) return;
    const modal = this.dom.spawnModal(EnvPointModal);
    modal.setup(whichLine, index, point.t, point.v, this.hint);
    modal.result.then(result => {
      if (!result) return;
      
      if (result === "delete") {
        this.loline.splice(index, 1);
        this.hiline.splice(index, 1);
        if (index - 1 === this.susp) this.susp = -1;
        else if (index - 1 < this.susp) this.susp--;
        const suspElement = this.element.querySelector("input[name='susp']");
        if (suspElement) suspElement.value = this.susp;
        
      } else {
        if (index > 0) {
          const pv = line[index - 1];
          if (result.t < pv.t) result.t = pv.t;
          else if (result.t >= pv.t + 16384) result.t = pv.t + 16384;
        } else {
          result.t = 0;
        }
        if (result.v < 0) result.v = 0;
        else if (result.v > 0xffff) result.v = 0xffff;
        line[index] = result;
      }
      this.renderSoon();
    }).catch(e => this.dom.modalError(e));
    
    }, 0);
  }
  
  onCanvasWheel(event) {
    let d = event.deltaX || event.deltaY || event.deltaZ;
    if (!d) return;
    // Chrome on Linux gives me units of 120, with (deltaMode==DOM_DELTA_PIXEL), always in deltaY regardless of Shift.
    // I have no idea why 120 is the default, and there's doesn't appear to be any way to determine how many clicks of the wheel it was.
    // So we'll assume one click always.
    if (d < 0) d = -1;
    else if (d > 0) d = 1;
    // Then scale up to pixels. This is entirely arbitrary, anything >0 is fine.
    d *= 16;
    // Don't use xFromY or yFromV; they round to integers.
    const canvas = this.element.querySelector("canvas");
    if (event.shiftKey) {
      const msperpx = (this.vbox.tz - this.vbox.ta) / canvas.width;
      const dt = Math.round(msperpx * d);
      this.vbox.ta += dt;
      this.vbox.tz += dt;
      if (this.vbox.ta < 0) {
        this.vbox.tz -= this.vbox.ta;
        this.vbox.ta = 0;
      }
    } else {
      const vperpx = (this.vbox.vz - this.vbox.va) / canvas.height;
      const dv = Math.round(vperpx * d);
      this.vbox.va -= dv;
      this.vbox.vz -= dv;
      if (this.vbox.va < 0) {
        this.vbox.vz -= this.vbox.va;
        this.vbox.va = 0;
      } else if (this.vbox.vz > 0x10000) {
        this.vbox.va -= this.vbox.vz - 0x10000;
        this.vbox.vz = 0x10000;
      }
    }
    this.renderSoon();
  }
  
  populateTattle(event) {
    const tattle = this.element.querySelector(".tattle");
    if (!tattle) return;
    const canvas = this.element.querySelector("canvas");
    if (!canvas) return;
    const bounds = canvas.getBoundingClientRect();
    const t = this.tFromX(canvas.width, event.x - bounds.x);
    let v = this.vFromY(canvas.height, event.y - bounds.y);
    if (this.hint === "pitch") {
      v -= 0x8000;
      if (v > 0) v = `+${v} c`;
      else v = `${v} c`;
    } else {
      v = v.toString(16).padStart(4, '0');
    }
    tattle.innerText = `${v} @ ${t}ms`;
  }
  
  clearTattle() {
    const tattle = this.element.querySelector(".tattle");
    if (!tattle) return;
    tattle.innerText = "";
  }
  
  onSuspChanged() {
    const susp = +this.element.querySelector("input[name='susp']")?.value;
    if (isNaN(susp)) return;
    this.susp = susp;
    if (this.susp >= 0) {
      this.flags |= 0x04;
    } else {
      this.flags &= ~0x04;
    }
    this.renderSoon();
  }
  
  onHiVisibilityChanged() {
    const value = this.element.querySelector("select[name='hiVisibility']").value;
    if (value === "disabled") {
      this.flags &= ~0x02;
    } else {
      this.flags |= 0x02;
    }
    this.renderSoon();
  }
}
