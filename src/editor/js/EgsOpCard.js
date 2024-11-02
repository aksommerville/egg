/* EgsOpCard.js XXX
 * Single operation for the EGS post pipe.
 */
 
import { Dom } from "./Dom.js";

export class EgsOpCardXXX {
  static getDependencies() {
    return [HTMLElement, Dom, "nonce"];
  }
  constructor(element, dom, nonce) {
    this.element = element;
    this.dom = dom;
    this.nonce = nonce;
    
    // Owner should replace:
    this.cbDelete = () => {}; // we confirm first
    this.cbMove = (dy) => {};
    this.cbMute = (mute) => {};
    this.cbDirty = () => {};
    
    this.k = 0;
    this.v = []; // Empty or Uint8Array
    
    this.element.addEventListener("input", e => this.onInput(e));
  }
  
  setup(k, v) {
    this.k = k;
    this.v = v;
    this.buildUi();
  }
  
  // Returns just (v). Caller is responsible for emitting key and length.
  encode() {
    switch (this.k) {
      case 1: return this.evalWaveshaper(this.element.querySelector("input[name='waveshaper']").value);
      case 2: {
          const dst = new Uint8Array(5);
          const period = +this.element.querySelector("input[name='period']").value;
          dst[0] = period >> 8;
          dst[1] = period;
          dst[2] = +this.element.querySelector("input[name='mix']").value;
          dst[3] = +this.element.querySelector("input[name='store']").value;
          dst[4] = +this.element.querySelector("input[name='feedback']").value;
          return dst;
        }
      case 3: {
          const dst = new Uint8Array(4);
          const period = +this.element.querySelector("input[name='period']").value;
          dst[0] = period >> 8;
          dst[1] = period;
          dst[2] = +this.element.querySelector("input[name='depth']").value;
          dst[3] = +this.element.querySelector("input[name='phase']").value;
          return dst;
        }
      case 4: {
          const dst = new Uint8Array(2);
          const cutoff = +this.element.querySelector("input[name='lopass']").value;
          dst[0] = cutoff >> 8;
          dst[1] = cutoff;
          return dst;
        }
      case 5: {
          const dst = new Uint8Array(2);
          const cutoff = +this.element.querySelector("input[name='hipass']").value;
          dst[0] = cutoff >> 8;
          dst[1] = cutoff;
          return dst;
        }
      case 6: {
          const dst = new Uint8Array(4);
          const mid = +this.element.querySelector("input[name='bpass.mid']").value;
          const wid = +this.element.querySelector("input[name='bpass.wid']").value;
          dst[0] = mid >> 8;
          dst[1] = mid;
          dst[2] = wid >> 8;
          dst[3] = wid;
          return dst;
        }
      case 7: {
          const dst = new Uint8Array(4);
          const mid = +this.element.querySelector("input[name='notch.mid']").value;
          const wid = +this.element.querySelector("input[name='notch.wid']").value;
          dst[0] = mid >> 8;
          dst[1] = mid;
          dst[2] = wid >> 8;
          dst[3] = wid;
          return dst;
        }
    }
    return this.v;
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const idBase = `EgsOpCard-${this.nonce}`;
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "X", tabindex: -1, "on-click": () => this.onDelete() });
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "^", tabindex: -1, "on-click": () => this.cbMove(-1) });
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "v", tabindex: -1, "on-click": () => this.cbMove(1) });
    this.dom.spawn(this.element, "INPUT", { type: "checkbox", value: "mute", tabindex: -1, id: `${idBase}-mute`, "on-change": () => this.onMuteChanged() });
    this.dom.spawn(this.element, "LABEL", { for: `${idBase}-mute` }, "M");
    this.dom.spawn(this.element, "DIV", ["title"], this.reprKey());
    this.dom.spawn(this.element, "DIV", ["spacer"]);
    switch (this.k) {
      case 0: break;
      case 1: this.dom.spawn(this.element, "INPUT", { name: "waveshaper", value: this.reprWaveshaper() }); break;
      case 2: {
          this.dom.spawn(this.element, "INPUT", { name: "period", type: "number", min: 0, max: 65535, value: (this.v[0] << 8) | this.v[1] });
          this.dom.spawn(this.element, "INPUT", { name: "mix", type: "number", min: 0, max: 255, value: this.v[2] });
          this.dom.spawn(this.element, "INPUT", { name: "store", type: "number", min: 0, max: 255, value: this.v[3] });
          this.dom.spawn(this.element, "INPUT", { name: "feedback", type: "number", min: 0, max: 255, value: this.v[4] });
        } break;
      case 3: {
          this.dom.spawn(this.element, "INPUT", { name: "period", type: "number", min: 0, max: 65535, value: (this.v[0] <<  8) | this.v[1] });
          this.dom.spawn(this.element, "INPUT", { name: "depth", type: "number", min: 0, max: 255, value: this.v[2] });
          this.dom.spawn(this.element, "INPUT", { name: "phase", type: "number", min: 0, max: 255, value: this.v[3] });
        } break;
      case 4: this.dom.spawn(this.element, "INPUT", { name: "lopass", type: "number", min: 0, max: 65535, value: (this.v[0] << 8) | this.v[1] }); break;
      case 5: this.dom.spawn(this.element, "INPUT", { name: "hipass", type: "number", min: 0, max: 65535, value: (this.v[0] << 8) | this.v[1] }); break;
      case 6: {
          this.dom.spawn(this.element, "INPUT", { name: "bpass.mid", type: "number", min: 0, max: 65535, value: (this.v[0] << 8) | this.v[1] });
          this.dom.spawn(this.element, "INPUT", { name: "bpass.wid", type: "number", min: 0, max: 65535, value: (this.v[2] << 8) | this.v[3] });
        } break;
      case 7: {
          this.dom.spawn(this.element, "INPUT", { name: "notch.mid", type: "number", min: 0, max: 65535, value: (this.v[0] << 8) | this.v[1] });
          this.dom.spawn(this.element, "INPUT", { name: "notch.wid", type: "number", min: 0, max: 65535, value: (this.v[2] << 8) | this.v[3] });
        } break;
      default: this.dom.spawn(this.element, "INPUT", { name: "hexdump", value: this.reprHexdump() }); break;
    }
  }
  
  reprKey() {
    switch (this.k) {
      case 0: return "noop";
      case 1: return "waveshaper";
      case 2: return "delay";
      case 3: return "tremolo";
      case 4: return "lopass";
      case 5: return "hipass";
      case 6: return "bpass";
      case 7: return "notch";
    }
    return this.k.toString();
  }
  
  reprWaveshaper() {
    let dst = "";
    for (let p=0; p<this.v.length; p+=2) {
      const v = (this.v[p] << 8) | this.v[p + 1];
      dst += v.toString(16).padStart(4, '0') + " ";
    }
    return dst;
  }
  
  evalWaveshaper(src) {
    src = src.replace(/\s/g, "");
    const dst = new Uint8Array(src.length >> 1);
    for (let dstp=0, srcp=0; dstp<dst.length; dstp++, srcp+=2) {
      dst[dstp] = parseInt(src.substring(srcp, srcp + 2), 16);
    }
    return dst;
  }
  
  reprHexdump() {
    let dst = "";
    for (let p=0; p<this.v.length; p++) {
      dst += this.v[p].toString(16).padStart(2, '0');
    }
    return dst;
  }
  
  onDelete() {
    this.dom.modalPickOne(["Delete", "Cancel"], `Can't undo. Delete op ${JSON.stringify(this.reprKey())}`).then((result) => {
      if (result === "Delete") this.cbDelete();
    }).catch(e => {} );
  }
  
  onMuteChanged() {
    const mute = this.element.querySelector("input[value='mute']").checked;
    this.cbMute(mute);
  }
  
  onInput(event) {
    if (event.target.name === "mute") return;
    this.cbDirty();
  }
}
