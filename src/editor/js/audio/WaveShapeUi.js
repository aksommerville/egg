/* WaveShapeUi.js
 */
 
import { Dom } from "../Dom.js";

export class WaveShapeUi {
  static getDependencies() {
    return [HTMLElement, Dom, Window];
  }
  constructor(element, dom, window) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    
    this.shape = 0; // (0,1,2,3,4)=(custom,sine,square,saw,triangle)
    this.coefv = []; // 0..65535, only if (shape==0).
    this.renderTimeout = null;
    this.mouseListener = null;
    this.sine = []; // Precalculated single-period sine wave of a length amenable to output.
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
  
  /* Encode and decode: Not much to it.
   ******************************************************************************/
  
  setup(src, srcp) {
    let shape = 1;
    if (srcp < src.length) {
      shape = src[srcp++];
    }
    const coefv = [];
    if (!shape) {
      const coefc = src[srcp++] || 0;
      for (let i=coefc; i-->0; ) {
        coefv.push((src[srcp] << 8) | src[srcp+1]);
        srcp += 2;
      }
    }
    this.shape = shape;
    this.coefv = coefv;
    this.buildUi();
    return srcp;
  }
  
  encode(dst) {
    dst.u8(this.shape);
    if (!this.shape) {
      let zeroc = 0;
      while ((zeroc < this.coefv.length) && !this.coefv[this.coefv.length - zeroc - 1]) zeroc++;
      if (zeroc) this.coefv.splice(this.coefv.length - zeroc, zeroc);
      if (this.coefv.length > 0xff) throw new Error(`Too many harmonics. ${this.coefv.length}, limit 255.`);
      dst.u8(this.coefv.length);
      for (const coef of this.coefv) dst.u16be(coef);
    }
  }
  
  /* Layout.
   ************************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    const left = this.dom.spawn(this.element, "DIV", ["left"]);
    const shapeMenu = this.dom.spawn(left, "SELECT", { name: "shape", "on-change": () => this.onShapeChanged() },
      this.dom.spawn(null, "OPTION", { value: "0" }, "Harmonics"),
      this.dom.spawn(null, "OPTION", { value: "1" }, "Sine"),
      this.dom.spawn(null, "OPTION", { value: "2" }, "Square"),
      this.dom.spawn(null, "OPTION", { value: "3" }, "Saw"),
      this.dom.spawn(null, "OPTION", { value: "4" }, "Triangle"),
    );
    shapeMenu.value = this.shape;
    this.dom.spawn(left, "CANVAS", ["harmonics"], {
      "on-mousedown": e => this.onMouseDown(e),
      "on-contextmenu": e => e.preventDefault(),
    });
    this.dom.spawn(this.element, "CANVAS", ["preview"]);
    this.renderSoon();
  }
  
  /* Render.
   **************************************************************************************/
   
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderHarmonics();
      this.renderPreview();
    }, 50);
  }
  
  renderHarmonics() {
    const canvas = this.element.querySelector("canvas.harmonics");
    if (!canvas) return;
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    
    // Only relevant to shape 0.
    if (this.shape) {
      ctx.fillStyle = "#000";
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      return;
    }
    
    const colc = Math.max(32, this.coefv.length);
    const colw = canvas.width / colc;
    
    // Faintly color background of odd columns.
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    ctx.fillStyle = "#111";
    for (let col=1; col<colc; col+=2) {
      const x = col * colw;
      ctx.fillRect(x, 0, colw, canvas.height);
    }
    
    // Brightly color columns of nonzero harmonics.
    ctx.fillStyle = "#f80";
    for (let col=0, x=0; col<this.coefv.length; col++, x+=colw) {
      if (!this.coefv[col]) continue;
      const h = (this.coefv[col] * canvas.height) / 65535;
      ctx.fillRect(x, canvas.height - h, colw, h);
    }
  }
  
  renderPreview() {
    const canvas = this.element.querySelector("canvas.preview");
    if (!canvas) return;
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#204";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    ctx.beginPath();
    ctx.moveTo(0, canvas.height / 2);
    ctx.lineTo(canvas.width, canvas.height / 2);
    ctx.strokeStyle = "#315";
    ctx.lineWidth = 1;
    ctx.stroke();
    ctx.strokeStyle = "#ff0";
    ctx.lineWidth = 3;
    switch (this.shape) {
      case 0: this.renderPreviewHarmonics(canvas, ctx); break;
      case 1: this.renderSine(canvas, ctx); break;
      // Square, saw, and triangle are simple enough, just do them inline:
      case 2: {
          ctx.beginPath();
          ctx.moveTo(2, canvas.height / 2);
          ctx.lineTo(2, 2);
          ctx.lineTo(canvas.width / 2, 2);
          ctx.lineTo(canvas.width / 2, canvas.height - 2);
          ctx.lineTo(canvas.width - 2, canvas.height - 2);
          ctx.lineTo(canvas.width - 2, canvas.height / 2);
          ctx.stroke();
        } break;
      case 3: {
          ctx.beginPath();
          ctx.moveTo(0, 0);
          ctx.lineTo(canvas.width, canvas.height);
          ctx.stroke();
        } break;
      case 4: {
          ctx.beginPath();
          ctx.moveTo(0, canvas.height);
          ctx.lineTo(canvas.width / 2, 0);
          ctx.lineTo(canvas.width, canvas.height);
          ctx.stroke();
        } break;
    }
  }
  
  renderPreviewHarmonics(canvas, ctx) {
    /* Usually when printing a wave, we build it up one harmonic at a time.
     * Since this is a one-and-done kind of deal, I want to try generating it samplewise instead.
     */
    this.requireSine(canvas.width);
    const halfh = canvas.height / 2;
    const voices = [];
    for (let i=0; i<this.coefv.length; i++) {
      const coef = this.coefv[i];
      if (!coef) continue;
      voices.push({ step: i+1, level: (coef*halfh)/65535, p:0 });
    }
    ctx.beginPath();
    ctx.moveTo(0, halfh);
    if (voices.length < 1) {
      ctx.lineTo(canvas.width, halfh);
    } else {
      for (let x=0; x<canvas.width; x++) {
        let y = halfh;
        for (const voice of voices) {
          y -= this.sine[voice.p] * voice.level;
          if ((voice.p += voice.step) >= this.sine.length) voice.p -= this.sine.length;
        }
        ctx.lineTo(x, y);
      }
    }
    ctx.stroke();
  }
  
  renderSine(canvas, ctx) {
    this.requireSine(canvas.width);
    const halfh = canvas.height / 2;
    ctx.beginPath();
    ctx.moveTo(0, halfh);
    for (let x=0; x<this.sine.length; x++) {
      const y = halfh - this.sine[x] * halfh;
      ctx.lineTo(x, y);
    }
    ctx.stroke();
  }
  
  requireSine(len) {
    if (len === this.sine.length) return;
    this.sine = [];
    for (let i=0, t=0, dt=(Math.PI*2)/len; i<len; i++, t+=dt) {
      this.sine.push(Math.sin(t));
    }
  }
  
  /* Events.
   ********************************************************************************/
  
  onShapeChanged() {
    this.shape = +this.element.querySelector("select[name='shape']").value;
    this.renderSoon();
  }
  
  onMouseDown(event) {
    if (this.mouseListener) return;
    if (this.shape) return; // Only relevant for "Custom" shape.
    const canvas = event.target;
    if (canvas.tagName !== "CANVAS") return;
    const bounds = canvas.getBoundingClientRect();
    
    const colc = Math.max(32, this.coefv.length);
    const colw = canvas.width / colc;
    
    const col = Math.floor((event.x - bounds.x) / colw);
    if ((col < 0) || (col >= 256)) return;
    
    // Right click: Zero the harmonic.
    if (event.button === 2) {
      console.log(`Right click in column ${col}`);
      if (col >= this.coefv.length) return;
      this.coefv[col] = 0;
      this.renderSoon();
      return;
    }
    
    // Left click: Pad (coefv) until we reach this column, then set this column.
    // I'm undecided whether to allow free dragging after that. TODO decide.
    const v = Math.max(0, Math.min(0xffff, Math.round(((canvas.height - (event.y - bounds.y)) * 65535) / canvas.height)));
    while (this.coefv.length <= col) this.coefv.push(0);
    this.coefv[col] = v;
    this.renderSoon();
  }
}
