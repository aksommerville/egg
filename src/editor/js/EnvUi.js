/* EnvUi.js
 * Used by EgsChannelEditor to edit multiple envelopes graphically.
 * TODO Probably needs a tattle, and maybe extra support for aligning pitch env at 0x8000.
 */
 
import { Dom } from "./Dom.js";

export class EnvUi {
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
    this.level = null;
    this.pitch = null;
    this.range = null;
    this.renderTimeout = null;
    this.mouseListener = null;
    this.dots = []; // {x,y,xmutable,ymutable,env:"level"|"pitch"|"range",field:"init"|"atk"|"sus"|"rls",edge:"lo"|"hi"}
    this.dotsDirty = false; // True to rebuild during the next render.
    this.handleRadius = 6;
    this.dragDot = null;
    
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
    this.level = new Uint8Array(serial);
    this.enableVisibility("level");
    this.setDefaultXRange();
    this.renderSoon(true);
  }
  
  setPitch(serial) {
    if (serial.length !== 16) throw new Error(`pitch env must be 16 bytes`);
    this.pitch = new Uint8Array(serial);
    this.enableVisibility("pitch");
    this.renderSoon(true);
  }
  
  setRange(serial) {
    if (serial.length !== 16) throw new Error(`range env must be 16 bytes`);
    this.range = new Uint8Array(serial);
    this.enableVisibility("range");
    this.renderSoon(true);
  }
  
  encodeLevel() {
    return this.level;
  }
  
  encodePitch() {
    return this.pitch;
  }
  
  encodeRange() {
    return this.range;
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "CANVAS", { "on-mousedown": e => this.onMouseDown(e) });
    const controls = this.dom.spawn(this.element, "DIV", ["controls"]);
    
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
    
    this.dom.spawn(controls, "INPUT", { type: "range", min: 0.250, max: 5, step: 0.010, name: "xrange", "on-input": () => this.onXRangeChanged() });
  }
  
  enableVisibility(name) {
    const input = this.element.querySelector(`.visibility input[name='${name}']`);
    if (input) input.disabled = false;
  }
  
  renderSoon(andRebuildDots) {
    if (andRebuildDots) this.dotsDirty = true;
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 50);
  }
  
  renderNow() {
    const canvas = this.element.querySelector("canvas");
    const bounds = canvas.getBoundingClientRect();
    if (this.dotsDirty) {
      this.dotsDirty = false;
      this.rebuildDots(bounds.width, bounds.height);
    }
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#004";
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    
    /* Vertical guidelines at s/4 and s.
     */
    const pxpersec = bounds.width / +this.element.querySelector("input[name='xrange']").value;
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
    let doti = 0;
    while (doti < this.dots.length) {
      let c = 1;
      let dot = this.dots[doti];
      while (
        (doti + c < this.dots.length) &&
        (dot.env === this.dots[doti + c].env) &&
        (dot.edge === this.dots[doti + c].edge)
      ) c++;
      ctx.beginPath();
      ctx.moveTo(dot.x, dot.y);
      for (let i=0; i<c; i++) {
        dot = this.dots[doti + i];
        ctx.lineTo(dot.x, dot.y);
      }
      ctx.strokeStyle = this.lineColor(dot.env, dot.edge);
      ctx.stroke();
      doti += c;
    }
    
    /* Draw a handle around each dot for which at least one axis is mutable.
     */
    for (const dot of this.dots) {
      if (!dot.xmutable && !dot.ymutable) continue;
      ctx.beginPath();
      ctx.ellipse(dot.x, dot.y, this.handleRadius, this.handleRadius, 0, 0, Math.PI * 2);
      ctx.fillStyle = this.lineColor(dot.env, dot.edge);
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
  
  rebuildDots(w, h) {
    // {x,y,xmutable,ymutable,env:"level"|"pitch"|"range",field:"init"|"atk"|"sus"|"rls",edge:"lo"|"hi"}
    this.dots = [];
    const showLo = this.element.querySelector(".visibility input[name='lo']").checked;
    const showHi = this.element.querySelector(".visibility input[name='hi']").checked;
    if (!showLo && !showHi) return;
    
    const xrange = +this.element.querySelector("input[name='xrange']").value * 1000; // ms, like the encoded envs
    const xscale = w / xrange;
    const yscale = h / 0xff;
    const xv = (t) => (t || 0) * xscale;
    const yv = (v) => h - (v || 0) * yscale;
    
    /* (initv,rlsv) may be null for immutable.
     */
    const addLine = (env, edge, initv, atkt, atkv, dect, susv, rlst, rlsv) => {
      this.dots.push({ x: 0, y: yv(initv || 0), xmutable: false, ymutable: (initv !== null), env, edge, field: "init" });
      this.dots.push({ x: xv(atkt), y: yv(atkv), xmutable: true, ymutable: true, env, edge, field: "atk" });
      this.dots.push({ x: xv(atkt + dect), y: yv(susv), xmutable: true, ymutable: true, env, edge, field: "sus" });
      this.dots.push({ x: xv(atkt + dect + rlst), y: yv(rlsv || 0), xmutable: true, ymutable: (rlsv !== null), env, edge, field: "rls" });
    };
    
    if (this.level && this.element.querySelector(".visibility input[name='level']").checked) {
      if (showLo) {
        addLine("level", "lo",
          null,
          this.level[0], this.level[2],
          this.level[4], this.level[6],
          (this.level[8] << 8) | this.level[9], null
        );
      }
      if (showHi) {
        addLine("level", "hi",
          null,
          this.level[1], this.level[3],
          this.level[5], this.level[7],
          (this.level[10] << 8) | this.level[11], null
        );
      }
    }
    
    const aux = (e, name) => {
      if (e && this.element.querySelector(`.visibility input[name='${name}']`).checked) {
        if (showLo) {
          addLine(name, "lo",
            ((e[0] << 8) | e[1]) / 256,
            this.level[0], ((e[4] << 8) | e[5]) / 256,
            this.level[4], ((e[8] << 8) | e[9]) / 256,
            (this.level[8] << 8) | this.level[9], ((e[12] << 8) | e[13]) / 256
          );
        }
        if (showHi) {
          addLine(name, "hi",
            ((e[2] << 8) | e[3]) / 256,
            this.level[1], ((e[6] << 8) | e[7]) / 256,
            this.level[5], ((e[10] << 8) | e[11]) / 256,
            (this.level[10] << 8) | this.level[11], ((e[14] << 8) | e[15]) / 256
          );
        }
      }
    };
    aux(this.pitch, "pitch");
    aux(this.range, "range");
  }
  
  setDefaultXRange() {
    if (this.level?.length !== 12) return;
    let lo=0, hi=0;
    lo += this.level[0];
    hi += this.level[1];
    lo += this.level[4];
    hi += this.level[5];
    lo += this.level[8] << 8;
    lo += this.level[9];
    hi += this.level[10] << 8;
    hi += this.level[11];
    // Use the longer of the two, and no less than 250 ms, and add 250 ms of headroom.
    const xrange = (Math.max(250, lo, hi) + 250) / 1000;
    this.element.querySelector("input[name='xrange']").value = xrange;
  }
  
  findDot(x, y) {
    let best = null;
    let bestDistance = this.handleRadius * 2;
    for (const dot of this.dots) {
      if (!dot.xmutable && !dot.ymutable) continue;
      const dx = dot.x - x;
      if (dx > this.handleRadius) continue;
      if (dx < -this.handleRadius) continue;
      const dy = dot.y - y;
      if (dy > this.handleRadius) continue;
      if (dy < -this.handleRadius) continue;
      const distance = Math.sqrt(dx * dx + dy *dy);
      if (distance <= 1) return dot;
      if (!best || (distance <= bestDistance)) {
        best = dot;
        bestDistance = Math.sqrt(dx * dx + dy * dy);
      }
    }
    return best;
  }
  
  /* Move other dots to agree with (ref).
   * (ref) itself can not change here, presumably it was just moved by the user.
   * Except we do enforce an x minimum of 1 for non-init fields.
   */
  forceDotRelations(ref) {
    if (ref.field !== "init") {
      if (ref.x < 1) ref.x = 1;
    }
    for (const dot of this.dots) {
      if (dot === ref) continue;
      // Where (field) and (edge) are the same, the other gets my (x).
      if ((dot.field === ref.field) && (dot.edge === ref.edge)) {
        dot.x = ref.x;
      // Force order constraints if we're on the same edge.
      } else if (dot.edge === ref.edge) switch (ref.field) {
        case "atk": switch (dot.field) {
            case "sus": dot.x = Math.max(dot.x, ref.x + 1); break;
            case "rls": dot.x = Math.max(dot.x, ref.x + 2); break;
          } break;
        case "sus": switch (dot.field) {
            case "atk": dot.x = Math.min(dot.x, ref.x - 1); break;
            case "rls": dot.x = Math.max(dot.x, ref.x + 1); break;
          } break;
        case "rls": switch (dot.field) {
            case "atk": dot.x = Math.min(dot.x, ref.x - 2); break;
            case "sus": dot.x = Math.min(dot.x, ref.x - 2); break;
          } break;
      }
    }
  }
  
  rewriteSerialsFromDots() {
    const canvas = this.element.querySelector("canvas");
    const xrange = +this.element.querySelector("input[name='xrange']").value;
    const xscale = (xrange * 1000) / canvas.width;
    const yscale = 0xffff / canvas.height;
    let dst = null;
    const t8 = (dstp, x, edge, ...priors) => {
      if (edge === "hi") dstp++;
      let v = Math.floor(x * xscale);
      for (const p of priors) {
        v -= dst[p + ((edge === "hi") ? 1 : 0)];
      }
      if (v < 0) v = 0; else if (v > 0xff) v = 0xff;
      dst[dstp] = v;
    };
    const t16 = (dstp, x, edge, ...priors) => {
      if (edge === "hi") dstp += 2;
      let v = Math.floor(x * xscale);
      for (const p of priors) {
        let dp = p + ((edge === "hi") ? 1 : 0); // NB priors are all 8-bit, not 16.
        v -= dst[dp];
      }
      if (v < 0) v = 0; else if (v > 0xffff) v = 0xffff;
      dst[dstp] = v >> 8;
      dst[dstp + 1] = v;
    };
    const v8 = (dstp, y, edge) => {
      let v = ((canvas.height - y) * yscale) >> 8;
      if (v < 0) v = 0; else if (v > 0xff) v = 0xff;
      dst[dstp + ((edge === "hi") ? 1 : 0)] = v;
    };
    const v16 = (dstp, y, edge) => {
      let v = Math.floor((canvas.height - y) * yscale);
      if (v < 0) v = 0; else if (v > 0xffff) v = 0xffff;
      if (edge === "hi") dstp += 2;
      dst[dstp] = v >> 8;
      dst[dstp + 1] = v;
    };
    for (const dot of this.dots) {
      let aux = null;
      dst = this.level;
      switch (dot.env) {
        case "level": switch (dot.field) {
            case "atk": {
                t8(0, dot.x, dot.edge);
                v8(2, dot.y, dot.edge);
              } break;
            case "sus": {
                t8(4, dot.x, dot.edge, 0);
                v8(6, dot.y, dot.edge);
              } break;
            case "rls": {
                t16(8, dot.x, dot.edge, 0, 4);
              } break;
          } break;
        case "pitch": aux = this.pitch; break;
        case "range": aux = this.range; break;
      }
      if (aux) {
        dst = aux;
        switch (dot.field) {
          case "init": v16(0, dot.y, dot.edge); break;
          case "atk": v16(4, dot.y, dot.edge); break;
          case "sus": v16(8, dot.y, dot.edge); break;
          case "rls": v16(12, dot.y, dot.edge); break;
        }
      }
    }
  }
  
  onVisibilityChanged() {
    this.renderSoon(true);
  }
  
  onXRangeChanged() {
    this.renderSoon(true);
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
    if (this.dragDot.xmutable) this.dragDot.x = cvx;
    if (this.dragDot.ymutable) this.dragDot.y = cvy;
    this.forceDotRelations(this.dragDot);
    this.rewriteSerialsFromDots();
    this.renderSoon();
  }
}
