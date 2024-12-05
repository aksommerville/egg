/* LaunchService.js
 */
 
import { Data } from "./Data.js";
import { Comm } from "./Comm.js";
import { Dom } from "./Dom.js";
import { IframeModal } from "./IframeModal.js";

export class LaunchService {
  static getDependencies() {
    return [Data, Comm, Dom, Window];
  }
  constructor(data, comm, dom, window) {
    this.data = data;
    this.comm = comm;
    this.dom = dom;
    this.window = window;
    
    this.gamehtml = null;
  }
  
  launch() {
    this.requireGamehtml()
      .then(() => this.comm.httpText("GET", this.gamehtml))
      .then(rsp => this.launchHtml(rsp))
      .catch(e => this.dom.modalError(e));
  }
  
  requireGamehtml() {
    const errmsg = "Launch eggdev with '--gamehtml=PATH' for web launch capability.";
    if (this.gamehtml) return Promise.resolve(this.gamehtml);
    if (this.gamehtml === "") return Promise.reject(errmsg);
    return this.comm.httpText("GET", "/api/gamehtml").then(rsp => {
      this.gamehtml = rsp || "";
      if (!this.gamehtml) throw errmsg;
    });
  }
  
  launchHtml(html) {
    const blob = new Blob([html], { type: "text/html" });
    const url = URL.createObjectURL(blob);
    const modal = this.dom.spawnModal(IframeModal);
    this.setModalSizePerRom(modal);
    modal.setUrl(url);
    URL.revokeObjectURL(url);
    //TODO Would be great if the modal would dismiss itself after the embedded runtime halts. What are our options?
  }
  
  setModalSizePerRom(modal) {
    const res = this.data.resv.find(r => r.path === "metadata");
    if (res) {
      const size = this.readSizeFromMetadata(res.serial);
      if (size) {
        const xscale = Math.max(1, Math.floor(this.window.document.body.clientWidth / size[0]));
        const yscale = Math.max(1, Math.floor(this.window.document.body.clientHeight / size[1]));
        const scale = Math.min(xscale, yscale);
        modal.setSize(size[0] * scale, size[1] * scale);
        return;
      }
    }
    console.log(`LaunchService: Unable to determine size for runtime modal. Leaving unset.`);
  }
  
  // [w,h] or null
  readSizeFromMetadata(src) {
    if (src instanceof Uint8Array) src = new TextDecoder("utf8").decode(src);
    if (typeof(src) !== "string") return null;
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      if (!line || line.startsWith("#")) continue;
      const sepp = line.indexOf("=");
      if (sepp < 0) continue;
      const k = line.substring(0, sepp).trim();
      if (k !== "fb") continue;
      const v = line.substring(sepp + 1).trim();
      const wh = v.split('x');
      if (!wh || (wh.length !== 2)) return null;
      return wh.map(v => +v);
    }
    return null;
  }
}

LaunchService.singleton = true;
