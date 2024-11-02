/* EnvUi.js XXX
 * Used by EgsChannelEditor to edit multiple envelopes graphically.
 * TODO Probably needs a tattle, and maybe extra support for aligning pitch env at 0x8000.
 */
 
import { Dom } from "./Dom.js";

export class EnvUiXXX {
  static getDependencies() {
    return [HTMLElement, Dom, Window, "nonce"];
  }
  constructor(element, dom, window, nonce) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    this.nonce = nonce;
    
    // Owner should replace:
    this.dirty = () => {};
    
    this.ctx = null;
    this.model = new EnvModel();
    this.renderTimeout = null;
    this.mouseListener = null;
    this.dragDot = null;
    this.handleRadius = 6;
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    if (this.renderTimeout) this.window.clearTimeout(this.renderTimeout);
    if (this.mouseListener) {
      this.window.removeEventListener("mousemove", this.mouseListener);
      this.window.removeEventListener("mouseup", this.mouseListener);
      this.mouseListener = null;
    }
  }
  
  /* Principal envelope. Required.
   */
  setLevel(serial) {
    if (serial.length !== 12) throw new Error(`level env must be 12 bytes`);
    this.model.setLevel(serial);
    this.enableVisibility("level");
    this.setDefaultXRange();
    this.renderSoon();
  }
  
  setPitch(serial) {
    if (serial.length !== 16) throw new Error(`pitch env must be 16 bytes`);
    this.model.setPitch(serial);
    this.enableVisibility("pitch");
    this.renderSoon();
  }
  
  setRange(serial) {
    if (serial.length !== 16) throw new Error(`range env must be 16 bytes`);
    this.model.setRange(serial);
    this.enableVisibility("range");
    this.renderSoon();
  }
  
  encodeLevel() {
    return this.model.encodeLevel();
  }
  
  encodePitch() {
    return this.model.encodePitch();
  }
  
  encodeRange() {
    return this.model.encodeRange();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "CANVAS", { "on-mousedown": e => this.onMouseDown(e) });
    const controls = this.dom.spawn(this.element, "DIV", ["controls"]);
    
    this.dom.spawn(controls, "INPUT", { type: "button", value: "!", title: "Set Y for all visible points to a default.", "on-click": () => this.onDefaultVisiblePoints() });
    
    const visibility = this.dom.spawn(controls, "DIV", ["checkBar", "visibility"], { "on-change": () => this.onVisibilityChanged() });
    let idBase = `EnvUi-${this.nonce}-visibility`;
    this.dom.spawn(visibility, "INPUT", { id: idBase + "-lo", type: "checkbox", name: "lo", checked: "checked" });
    this.dom.spawn(visibility, "LABEL", { for: idBase + "-lo" }, "Lo");
    this.dom.spawn(visibility, "INPUT", { id: idBase + "-hi", type: "checkbox", name: "hi", checked: "checked" });
    this.dom.spawn(visibility, "LABEL", { for: idBase + "-hi" }, "Hi");
    this.dom.spawn(visibility, "INPUT", { id: idBase + "-level", type: "checkbox", name: "level", checked: "checked", disabled: "disabled" });
    this.dom.spawn(visibility, "LABEL", { for: idBase + "-level" }, "Level");
    this.dom.spawn(visibility, "INPUT", { id: idBase + "-pitch", type: "checkbox", name: "pitch", checked: "checked", disabled: "disabled" });
    this.dom.spawn(visibility, "LABEL", { for: idBase + "-pitch" }, "Pitch");
    this.dom.spawn(visibility, "INPUT", { id: idBase + "-range", type: "checkbox", name: "range", checked: "checked", disabled: "disabled" });
    this.dom.spawn(visibility, "LABEL", { for: idBase + "-range" }, "Range");
    
    this.dom.spawn(controls, "INPUT", { type: "range", min: 0, max: 1, step: 0.001, name: "xrange", "on-input": () => this.onXRangeChanged() });
  }
  
  enableVisibility(name) {
    const input = this.element.querySelector(`.visibility input[name='${name}']`);
    if (input) input.disabled = false;
  }
  
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 30);
  }
  
  renderNow() {
    const canvas = this.element.querySelector("canvas");
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    this.model.setViewSize(bounds.width, bounds.height);
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#004";
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    
    /* Vertical guidelines at s/4 and s.
     */
    const pxpersec = bounds.width / (this.model.tz / 1000);
    ctx.beginPath();
    for (let x=0; x<canvas.width; x+=pxpersec/4) {
      ctx.moveTo(x, 0);
      ctx.lineTo(x, canvas.height);
    }
    ctx.strokeStyle = "#118";
    ctx.stroke();
    ctx.beginPath();
    for (let x=0; x<canvas.width; x+=pxpersec) {
      ctx.moveTo(x, 0);
      ctx.lineTo(x, canvas.height);
    }
    ctx.strokeStyle = "#33a";
    ctx.stroke();
    
    /* Draw the lines described by (this.dots).
     * Dots are grouped by line, and always proceed left to right.
     */
    for (const line of this.model.getLines()) {
      ctx.beginPath();
      ctx.moveTo(line.points[0].x, line.points[0].y);
      for (const point of line.points) {
        ctx.lineTo(point.x, point.y);
      }
      ctx.strokeStyle = this.lineColor(line.env, line.edge);
      ctx.stroke();
    }
    
    /* Draw a handle around each dot for which at least one axis is mutable.
     */
    for (const point of this.model.getMutablePoints()) {
      ctx.beginPath();
      ctx.ellipse(point.x, point.y, this.handleRadius, this.handleRadius, 0, 0, Math.PI * 2);
      ctx.fillStyle = this.lineColor(point.env, point.edge);
      ctx.fill();
    }
  }
  
  lineColor(env, edge) {
    const lo = (edge === "lo");
    switch (env) {
      case "level": return lo ? "#aa0" : "#ff0";
      case "pitch": return lo ? "#800" : "#f00";
      case "range": return lo ? "#080" : "#0f0";
    }
    return "#888";
  }
  
  setDefaultXRange() {
    const elen = this.model.getMaximumDurationMs();
    const xrangems = Math.max(250, elen) + 250;
    this.element.querySelector("input[name='xrange']").value = this.normalizeXRange(xrangems);
  }
  
  /* Normalized xrange is the UI widget's value, 0..1.
   * Denormalized is milliseconds.
   * The encodable range is 0..66045.
   * We'll clamp the low end to 250.
   * High end should clamp at or above 66045, but strongly favor values under 5000 or so.
   */
  normalizeXRange(ms) {
    if (ms <= 250) return 0;
    return Math.log(ms - 250) / Math.log(66100);
  }
  denormalizeXRange(n) {
    return 250 + 66100 ** n;
  }
  
  findDot(x, y) {
    let best = null;
    let bestDistance = this.handleRadius * 2;
    // Iterate backward, since rendering iterates forward, and we'd prefer to take the frontmost of identical points.
    const points = this.model.getMutablePoints();
    for (let i=points.length; i-->0; ) {
      const point = points[i];
      const dx = point.x - x;
      if (dx > this.handleRadius) continue;
      if (dx < -this.handleRadius) continue;
      const dy = point.y - y;
      if (dy > this.handleRadius) continue;
      if (dy < -this.handleRadius) continue;
      const distance = Math.sqrt(dx * dx + dy *dy);
      if (distance <= 1) return point;
      if (!best || (distance <= bestDistance)) {
        best = point;
        bestDistance = Math.sqrt(dx * dx + dy * dy);
      }
    }
    return best;
  }
  
  onVisibilityChanged() {
    this.model.lovisible = this.element.querySelector("input[name='lo']")?.checked;
    this.model.hivisible = this.element.querySelector("input[name='hi']")?.checked;
    this.model.lvisible = this.element.querySelector("input[name='level']")?.checked;
    this.model.pvisible = this.element.querySelector("input[name='pitch']")?.checked;
    this.model.rvisible = this.element.querySelector("input[name='range']")?.checked;
    this.renderSoon();
  }
  
  onXRangeChanged() {
    this.model.tz = this.denormalizeXRange(+this.element.querySelector("input[name='xrange']").value);
    this.renderSoon();
  }
  
  onMouseDown(event) {
    if (this.mouseListener) return;
    const canvas = this.element.querySelector("canvas");
    const bounds = canvas.getBoundingClientRect();
    const cvx = event.x - bounds.x;
    const cvy = event.y - bounds.y;
    const dot = this.findDot(cvx, cvy);
    if (!dot) return;
    
    this.mouseListener = e => this.onMouseUpOrMove(e);
    this.window.addEventListener("mouseup", this.mouseListener);
    this.window.addEventListener("mousemove", this.mouseListener);
    this.dragDot = dot;
  }
  
  onMouseUpOrMove(event) {
    if (event.type === "mouseup") {
      this.window.removeEventListener("mouseup", this.mouseListener);
      this.window.removeEventListener("mousemove", this.mouseListener);
      this.mouseListener = null;
      this.dragDot = null;
      this.dirty();
      return;
    }
    const canvas = this.element.querySelector("canvas");
    const bounds = canvas.getBoundingClientRect();
    const cvx = event.x - bounds.x;
    const cvy = event.y - bounds.y;
    this.model.movePointTo(this.dragDot, cvx, cvy);
    this.renderSoon();
  }
  
  onDefaultVisiblePoints() {
    this.model.defaultVisiblePoints();
    this.renderSoon();
    this.dirty();
  }
}

/* Model.
 */
 
class EnvModel {
  constructor() {
    this.vw = 100; // Will be replaced each render.
    this.vh = 100; // But don't let them be zero.
    this.tz = 1000; // Time axis range in ms.
    this.pa = 0x7000; // Pitch env zoom.
    this.pz = 0x9000;
    this.lpresent = false;
    this.ppresent = false;
    this.rpresent = false;
    this.lvisible = true;
    this.pvisible = true;
    this.rvisible = true;
    this.lovisible = true;
    this.hivisible = true;
    // Don't bother generalizing the envelopes, I'd just confuse myself.
    // The three envelopes consist of 26 distinct data points.
    // We'll record all of them individually, in the encoded scale.
    // Timing from level envelope. Attack and decay are u8, release is u16:
    this.atktlo = 0;
    this.atkthi = 0;
    this.dectlo = 0;
    this.decthi = 0;
    this.rlstlo = 0;
    this.rlsthi = 0;
    // Values from level. All u8:
    this.latklo = 0;
    this.latkhi = 0;
    this.lsuslo = 0;
    this.lsushi = 0;
    // Values from pitch. All u16, in cents biased to 0x8000:
    this.pinilo = 0x8000;
    this.pinihi = 0x8000;
    this.patklo = 0x8000;
    this.patkhi = 0x8000;
    this.psuslo = 0x8000;
    this.psushi = 0x8000;
    this.prlslo = 0x8000;
    this.prlshi = 0x8000;
    // Values from range. All u16:
    this.rinilo = 0xffff;
    this.rinihi = 0xffff;
    this.ratklo = 0xffff;
    this.ratkhi = 0xffff;
    this.rsuslo = 0xffff;
    this.rsushi = 0xffff;
    this.rrlslo = 0xffff;
    this.rrlshi = 0xffff;
  }
  
  setLevel(serial) {
    this.lpresent = true;
    this.atktlo = serial[0] || 0;
    this.atkthi = serial[1] || 0;
    this.latklo = serial[2] || 0;
    this.latkhi = serial[3] || 0;
    this.dectlo = serial[4] || 0;
    this.decthi = serial[5] || 0;
    this.lsuslo = serial[6] || 0;
    this.lsushi = serial[7] || 0;
    this.rlstlo = (serial[8] << 8) | serial[9];
    this.rlsthi = (serial[10] << 8) | serial[11];
  }
  
  setPitch(serial) {
    this.ppresent = true;
    this.pinilo = (serial[0] << 8) | serial[1];
    this.pinihi = (serial[2] << 8) | serial[3];
    this.patklo = (serial[4] << 8) | serial[5];
    this.patkhi = (serial[6] << 8) | serial[7];
    this.psuslo = (serial[8] << 8) | serial[9];
    this.psushi = (serial[10] << 8) | serial[11];
    this.prlslo = (serial[12] << 8) | serial[13];
    this.prlshi = (serial[14] << 8) | serial[15];
  }
  
  setRange(serial) {
    this.rpresent = true;
    this.rinilo = (serial[0] << 8) | serial[1];
    this.rinihi = (serial[2] << 8) | serial[3];
    this.ratklo = (serial[4] << 8) | serial[5];
    this.ratkhi = (serial[6] << 8) | serial[7];
    this.rsuslo = (serial[8] << 8) | serial[9];
    this.rsushi = (serial[10] << 8) | serial[11];
    this.rrlslo = (serial[12] << 8) | serial[13];
    this.rrlshi = (serial[14] << 8) | serial[15];
  }
  
  encodeLevel() {
    const dst = new Uint8Array(12);
    dst[0] = this.atktlo;
    dst[1] = this.atkthi;
    dst[2] = this.latklo;
    dst[3] = this.latkhi;
    dst[4] = this.dectlo;
    dst[5] = this.decthi;
    dst[6] = this.lsuslo;
    dst[7] = this.lsushi;
    dst[8] = this.rlstlo >> 8;
    dst[9] = this.rlstlo;
    dst[10] = this.rlsthi >> 8;
    dst[11] = this.rlsthi;
    return dst;
  }
  
  encodePitch() {
    const dst = new Uint8Array(16);
    dst[0] = this.pinilo >> 8;
    dst[1] = this.pinilo;
    dst[2] = this.pinihi >> 8;
    dst[3] = this.pinihi;
    dst[4] = this.patklo >> 8;
    dst[5] = this.patklo;
    dst[6] = this.patkhi >> 8;
    dst[7] = this.patkhi;
    dst[8] = this.psuslo >> 8;
    dst[9] = this.psuslo;
    dst[10] = this.psushi >> 8;
    dst[11] = this.psushi;
    dst[12] = this.prlslo >> 8;
    dst[13] = this.prlslo;
    dst[14] = this.prlshi >> 8;
    dst[15] = this.prlshi;
    return dst;
  }
  
  encodeRange() {
    const dst = new Uint8Array(16);
    dst[0] = this.rinilo >> 8;
    dst[1] = this.rinilo;
    dst[2] = this.rinihi >> 8;
    dst[3] = this.rinihi;
    dst[4] = this.ratklo >> 8;
    dst[5] = this.ratklo;
    dst[6] = this.ratkhi >> 8;
    dst[7] = this.ratkhi;
    dst[8] = this.rsuslo >> 8;
    dst[9] = this.rsuslo;
    dst[10] = this.rsushi >> 8;
    dst[11] = this.rsushi;
    dst[12] = this.rrlslo >> 8;
    dst[13] = this.rrlslo;
    dst[14] = this.rrlshi >> 8;
    dst[15] = this.rrlshi;
    return dst;
  }
  
  getMaximumDurationMs() {
    return Math.max(
      this.atktlo + this.dectlo + this.rlstlo,
      this.atkthi + this.decthi + this.rlsthi
    );
  }
  
  xFromT(t) {
    return (t * this.vw) / this.tz;
  }
  
  tFromX(x) {
    return (x * this.tz) / this.vw;
  }
  
  yFromV8(v) {
    return this.vh - (v * this.vh) / 256;
  }
  
  yFromV16(v, lo, hi) {
    if ((lo !== 0) || (hi !== 0xffff)) v = ((v - lo) * 0xffff) / (hi - lo);
    if (v <= 0) return this.vh;
    if (v >= 0xffff) return 0;
    return this.vh - (v * this.vh) / 65536;
  }
  
  v16FromY(y, lo, hi) {
    let v = ((this.vh - y) * 0xffff) / this.vh;
    if ((lo !== 0) || (hi !== 0xffff)) v = lo + ((v * (hi - lo)) / 0xffff);
    if (v < 0) return 0;
    if (v > 0xffff) return 0xffff;
    return v;
  }
  
  pt8(t, v, env, fld, edge) {
    return {
      x: this.xFromT(t),
      y: this.yFromV8(v),
      xmut: (fld !== "ini"),
      ymut: ((env !== "level") || (fld !== "rls")),
      env, fld, edge,
    };
  }
  
  pt16(t, v, env, fld, edge, lo, hi) {
    return {
      x: this.xFromT(t),
      y: this.yFromV16(v, lo, hi),
      xmut: (fld !== "ini"),
      ymut: ((env !== "level") || (fld !== "rls")),
      env, fld, edge,
    };
  }
  
  getMutablePoints() {
    const points = [];
    if (this.lpresent && this.lvisible) {
      if (this.lovisible) {
        points.push(this.pt8(this.atktlo,                             this.latklo, "level", "atk", "lo"));
        points.push(this.pt8(this.atktlo + this.dectlo,               this.lsuslo, "level", "sus", "lo"));
        points.push(this.pt8(this.atktlo + this.dectlo + this.rlstlo,           0, "level", "rls", "lo"));
      }
      if (this.hivisible) {
        points.push(this.pt8(this.atkthi,                             this.latkhi, "level", "atk", "hi"));
        points.push(this.pt8(this.atkthi + this.decthi,               this.lsushi, "level", "sus", "hi"));
        points.push(this.pt8(this.atkthi + this.decthi + this.rlsthi,           0, "level", "rls", "hi"));
      }
    }
    if (this.ppresent && this.pvisible) {
      if (this.lovisible) {
        points.push(this.pt16(0,                                       this.pinilo, "pitch", "ini", "lo", this.pa, this.pz));
        points.push(this.pt16(this.atktlo,                             this.patklo, "pitch", "atk", "lo", this.pa, this.pz));
        points.push(this.pt16(this.atktlo + this.dectlo,               this.psuslo, "pitch", "sus", "lo", this.pa, this.pz));
        points.push(this.pt16(this.atktlo + this.dectlo + this.rlstlo, this.prlslo, "pitch", "rls", "lo", this.pa, this.pz));
      }
      if (this.hivisible) {
        points.push(this.pt16(0,                                       this.pinihi, "pitch", "ini", "hi", this.pa, this.pz));
        points.push(this.pt16(this.atkthi,                             this.patkhi, "pitch", "atk", "hi", this.pa, this.pz));
        points.push(this.pt16(this.atkthi + this.decthi,               this.psushi, "pitch", "sus", "hi", this.pa, this.pz));
        points.push(this.pt16(this.atkthi + this.decthi + this.rlsthi, this.prlshi, "pitch", "rls", "hi", this.pa, this.pz));
      }
    }
    if (this.rpresent && this.rvisible) {
      if (this.lovisible) {
        points.push(this.pt16(0,                                       this.rinilo, "range", "ini", "lo", 0, 0xffff));
        points.push(this.pt16(this.atktlo,                             this.ratklo, "range", "atk", "lo", 0, 0xffff));
        points.push(this.pt16(this.atktlo + this.dectlo,               this.rsuslo, "range", "sus", "lo", 0, 0xffff));
        points.push(this.pt16(this.atktlo + this.dectlo + this.rlstlo, this.rrlslo, "range", "rls", "lo", 0, 0xffff));
      }
      if (this.hivisible) {
        points.push(this.pt16(0,                                       this.rinihi, "range", "ini", "hi", 0, 0xffff));
        points.push(this.pt16(this.atkthi,                             this.ratkhi, "range", "atk", "hi", 0, 0xffff));
        points.push(this.pt16(this.atkthi + this.decthi,               this.rsushi, "range", "sus", "hi", 0, 0xffff));
        points.push(this.pt16(this.atkthi + this.decthi + this.rlsthi, this.rrlshi, "range", "rls", "hi", 0, 0xffff));
      }
    }
    return points;
  }
  
  getLines() {
    const lines = [];
    if (this.lpresent && this.lvisible) {
      if (this.lovisible) {
        const line = { env: "level", edge: "lo", points: [] };
        lines.push(line);
        line.points.push(this.pt8(0,                                                 0, "level", "ini", "lo"));
        line.points.push(this.pt8(this.atktlo,                             this.latklo, "level", "atk", "lo"));
        line.points.push(this.pt8(this.atktlo + this.dectlo,               this.lsuslo, "level", "sus", "lo"));
        line.points.push(this.pt8(this.atktlo + this.dectlo + this.rlstlo,           0, "level", "rls", "lo"));
      }
      if (this.hivisible) {
        const line = { env: "level", edge: "hi", points: [] };
        lines.push(line);
        line.points.push(this.pt8(0,                                                 0, "level", "ini", "hi"));
        line.points.push(this.pt8(this.atkthi,                             this.latkhi, "level", "atk", "hi"));
        line.points.push(this.pt8(this.atkthi + this.decthi,               this.lsushi, "level", "sus", "hi"));
        line.points.push(this.pt8(this.atkthi + this.decthi + this.rlsthi,           0, "level", "rls", "hi"));
      }
    }
    if (this.ppresent && this.pvisible) {
      if (this.lovisible) {
        const line = { env: "pitch", edge: "lo", points: [] };
        lines.push(line);
        line.points.push(this.pt16(0,                                       this.pinilo, "pitch", "ini", "lo", this.pa, this.pz));
        line.points.push(this.pt16(this.atktlo,                             this.patklo, "pitch", "atk", "lo", this.pa, this.pz));
        line.points.push(this.pt16(this.atktlo + this.dectlo,               this.psuslo, "pitch", "sus", "lo", this.pa, this.pz));
        line.points.push(this.pt16(this.atktlo + this.dectlo + this.rlstlo, this.prlslo, "pitch", "rls", "lo", this.pa, this.pz));
      }
      if (this.hivisible) {
        const line = { env: "pitch", edge: "hi", points: [] };
        lines.push(line);
        line.points.push(this.pt16(0,                                       this.pinihi, "pitch", "ini", "hi", this.pa, this.pz));
        line.points.push(this.pt16(this.atkthi,                             this.patkhi, "pitch", "atk", "hi", this.pa, this.pz));
        line.points.push(this.pt16(this.atkthi + this.decthi,               this.psushi, "pitch", "sus", "hi", this.pa, this.pz));
        line.points.push(this.pt16(this.atkthi + this.decthi + this.rlsthi, this.prlshi, "pitch", "rls", "hi", this.pa, this.pz));
      }
    }
    if (this.rpresent && this.rvisible) {
      if (this.lovisible) {
        const line = { env: "range", edge: "lo", points: [] };
        lines.push(line);
        line.points.push(this.pt16(0,                                       this.rinilo, "range", "ini", "lo", 0, 0xffff));
        line.points.push(this.pt16(this.atktlo,                             this.ratklo, "range", "atk", "lo", 0, 0xffff));
        line.points.push(this.pt16(this.atktlo + this.dectlo,               this.rsuslo, "range", "sus", "lo", 0, 0xffff));
        line.points.push(this.pt16(this.atktlo + this.dectlo + this.rlstlo, this.rrlslo, "range", "rls", "lo", 0, 0xffff));
      }
      if (this.hivisible) {
        const line = { env: "range", edge: "hi", points: [] };
        lines.push(line);
        line.points.push(this.pt16(0,                                       this.rinihi, "range", "ini", "hi", 0, 0xffff));
        line.points.push(this.pt16(this.atkthi,                             this.ratkhi, "range", "atk", "hi", 0, 0xffff));
        line.points.push(this.pt16(this.atkthi + this.decthi,               this.rsushi, "range", "sus", "hi", 0, 0xffff));
        line.points.push(this.pt16(this.atkthi + this.decthi + this.rlsthi, this.rrlshi, "range", "rls", "hi", 0, 0xffff));
      }
    }
    return lines;
  }
  
  setViewSize(w, h) {
    if ((w < 1) || (h < 1)) return;
    this.vw = w;
    this.vh = h;
  }
  
  movePointTo(point, x, y) {

    if (point.xmut) {
      let t = Math.round(this.tFromX(x));
      let max = 0xff;
      if (point.edge === "lo") switch (point.fld) {
        case "sus": t -= this.atktlo; break;
        case "rls": t -= this.atktlo + this.dectlo; max = 0xffff; break;
      } else switch (point.fld) {
        case "sus": t -= this.atkthi; break;
        case "rls": t -= this.atkthi + this.decthi; max = 0xffff; break;
      }
      if (t < 0) t = 0;
      else if (t > max) t = max;
      if (point.edge === "lo") switch (point.fld) {
        case "atk": this.atktlo = t; break;
        case "sus": this.dectlo = t; break;
        case "rls": this.rlstlo = t; break;
      } else switch (point.fld) {
        case "atk": this.atkthi = t; break;
        case "sus": this.decthi = t; break;
        case "rls": this.rlsthi = t; break;
      }
    }
    
    if (point.ymut) {
      let v;
      if (point.env === "pitch") v = this.v16FromY(y, this.pa, this.pz);
      else v = this.v16FromY(y, 0, 0xffff);
      switch (point.env) {
        case "level": switch (point.fld) {
            case "atk": if (point.edge === "lo") this.latklo = v >> 8; else this.latkhi = v >> 8; break;
            case "sus": if (point.edge === "lo") this.lsuslo = v >> 8; else this.lsushi = v >> 8; break;
          } break;
        case "pitch": switch (point.fld) {
            case "ini": if (point.edge === "lo") this.pinilo = v; else this.pinihi = v; break;
            case "atk": if (point.edge === "lo") this.patklo = v; else this.patkhi = v; break;
            case "sus": if (point.edge === "lo") this.psuslo = v; else this.psushi = v; break;
            case "rls": if (point.edge === "lo") this.prlslo = v; else this.prlshi = v; break;
          } break;
        case "range": switch (point.fld) {
            case "ini": if (point.edge === "lo") this.rinilo = v; else this.rinihi = v; break;
            case "atk": if (point.edge === "lo") this.ratklo = v; else this.ratkhi = v; break;
            case "sus": if (point.edge === "lo") this.rsuslo = v; else this.rsushi = v; break;
            case "rls": if (point.edge === "lo") this.rrlslo = v; else this.rrlshi = v; break;
          } break;
      }
    }
  }
  
  defaultVisiblePoints() {
    if (this.lpresent && this.lvisible) {
      // It's a little weird to request this for level, but we can make up something sane.
      if (this.lovisible) {
        this.latklo = 0x60;
        this.lsuslo = 0x30;
      }
      if (this.hivisible) {
        this.latkhi = 0xff;
        this.lsushi = 0x40;
      }
    }
    if (this.ppresent && this.pvisible) {
      // For pitch, this is important. Set everything to 0x8000 ie noop.
      if (this.lovisible) {
        this.pinilo = 0x8000;
        this.patklo = 0x8000;
        this.psuslo = 0x8000;
        this.prlslo = 0x8000;
      }
      if (this.hivisible) {
        this.pinihi = 0x8000;
        this.patkhi = 0x8000;
        this.psushi = 0x8000;
        this.prlshi = 0x8000;
      }
    }
    if (this.rpresent && this.rvisible) {
      // Range, the most sensible default is straight ones.
      if (this.lovisible) {
        this.rinilo = 0xffff;
        this.ratklo = 0xffff;
        this.rsuslo = 0xffff;
        this.rrlslo = 0xffff;
      }
      if (this.hivisible) {
        this.rinihi = 0xffff;
        this.ratkhi = 0xffff;
        this.rsushi = 0xffff;
        this.rrlshi = 0xffff;
      }
    }
  }
}
