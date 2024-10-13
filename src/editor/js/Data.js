/* Data.js
 * The global data repository.
 * We get the entire contents of `src/data` at app load.
 * All creation and deletion of resources should go through us.
 */
 
import { Comm } from "./Comm.js";
import { Dom } from "./Dom.js";

const DIRTY_TIMEOUT = 1000;
 
export class Data {
  static getDependencies() {
    return [Comm, Window, Dom];
  }
  constructor(comm, window, dom) {
    this.comm = comm;
    this.window = window;
    this.dom = dom;
    
    this.resv = []; // {path,serial:Uint8Array,type,rid,name}
    this.types = []; // string; all types present in resv
    this.nextTocListener = 1;
    this.tocListeners = [];
    this.nextDirtyListener = 1;
    this.dirtyListeners = [];
    this.dirties = {}; // {[path]:cb}
    this.dirtyTimeout = null;
  }
  
  /* The initial load. Only bootstrap should call this.
   */
  load() {
    return this.comm.httpJson("GET", "/api/resources/data").then(rsp => {
      this.replaceAll(rsp);
    });
  }
  
  /* Call whenever a resource changes.
   * It's safe to spam this, we debounce.
   * (path) must be in my TOC already.
   * (cb) must return a Uint8Array of the new content.
   */
  dirty(path, cb) {
    this.dirties[path] = cb;
    if (this.dirtyTimeout) this.window.clearTimeout(this.dirtyTimeout);
    else this.broadcastDirty("dirty");
    this.dirtyTimeout = this.window.setTimeout(() => this.flushDirties(), DIRTY_TIMEOUT);
  }
  
  flushDirties() {
    this.dirtyTimeout = null;
    const dirties = this.dirties;
    this.dirties = {};
    this.broadcastDirty("saving");
    Promise.all(Object.keys(dirties).map((path) => {
      const cb = dirties[path];
      try {
        const res = this.resv.find(r => r.path === path);
        if (!res) return null;
        const serial = cb();
        res.serial = serial;
        return this.comm.httpBinary("PUT", "/data/" + path, null, null, serial).then(() => {
          //console.log(`saved ${path}, ${serial.length} bytes`);
        });
      } catch (e) {
        this.dom.modalError(e);
        return null;
      }
    }).filter(v => v)).catch((error) => {
      this.dom.modalError(error);
    }).then(() => {
      if (this.dirtyTimeout) this.broadcastDirty("dirty");
      else this.broadcastDirty("clean");
    });
  }
  
  /* TOC listeners are signalled whenever a resource is added, removed, or renamed.
   */
  listenToc(cb) {
    const id = this.nextTocListener++;
    this.tocListeners.push({ id, cb });
    return id;
  }
  unlistenToc(id) {
    const p = this.tocListeners.findIndex(l => l.id === id);
    if (p >= 0) this.tocListeners.splice(p, 1);
  }
  
  /* Dirty listeners are signalled when our dirty state changes, with one of:
   *  - "clean"
   *  - "dirty": Waiting for debounce timer.
   *  - "saving": Calls in flight.
   * There isn't an "error" state; instead we report errors directly thru Dom.
   */
  listenDirty(cb) {
    const id = this.nextDirtyListener++;
    this.dirtyListeners.push({ id, cb });
    return id;
  }
  unlistenDirty(id) {
    const p = this.dirtyListeners.findIndex(l => l.id === id);
    if (p >= 0) this.dirtyListeners.splice(p, 1);
  }
  broadcastDirty(state) {
    for (const { cb } of this.dirtyListeners) cb(state);
  }
  
  /* Replace entire resource set with a response from `GET /api/resources/data`.
   */
  replaceAll(rsp) {
    this.resv = rsp.map((res) => ({
      ...this.parsePath(res.path),
      serial: this.decodeBase64(res.serial || ""),
    }));
    this.types = Array.from(new Set(this.resv.map(r => r.type))).sort();
    for (const { cb } of this.tocListeners) cb();
  }
  
  parsePath(path) {
    if (!path) return { path: "", type: "", rid: 0 };
    const words = path.split('/');
    const base = words[words.length - 1] || "";
    let type = words[words.length - 2] || "";
    if (base === "metadata") type = "metadata";
    else if (base === "manifest") type = "manifest";
    else if (base === "instruments") type = "sounds";
    let rid=0, name="";
    let match = base.match(/^([a-z]{2}-)?(\d*)/);
    if (match) {
      rid = +match[2] || 0;
      if (match[1] && (rid < 64)) {
        const hi = match[1].charCodeAt(0) - 0x61;
        const lo = match[1].charCodeAt(1) - 0x61;
        rid |= (hi << 11) | (lo << 6);
      }
    }
    if (match = base.match(/^([a-z]{2}-)?\d+-([^.]+)/)) {
      name = match[2];
    }
    return { path, type, rid, name };
  }
  
  /* Kind of silly how this doesn't exist yet in standard web APIs.
   */
  decodeBase64(src) {
    let dst = new Uint8Array(src.length);
    let dstc = 0;
    const tmp = [0, 0, 0, 0];
    let tmpc = 0;
    for (let srcp=0; srcp<src.length; srcp++) {
      let ch = src.charCodeAt(srcp);
           if ((ch >= 0x41) && (ch <= 0x5a)) ch = ch - 0x41;
      else if ((ch >= 0x61) && (ch <= 0x7a)) ch = ch - 0x61 + 26;
      else if ((ch >= 0x30) && (ch <= 0x39)) ch = ch - 0x30 + 52;
      else if (ch === 0x2b) ch = 62;
      else if (ch === 0x2f) ch = 63;
      else if (ch === 0x3d) continue; // '=', ignore
      else if (ch <= 0x20) continue;
      else throw new Error(`Unexpected character 0x${ch.toString(16).padStart(2, '0')} in base64.`);
      tmp[tmpc++] = ch;
      if (tmpc >= 4) {
        dst[dstc++] = (tmp[0] << 2) | (tmp[1] >> 4);
        dst[dstc++] = (tmp[1] << 4) | (tmp[2] >> 2);
        dst[dstc++] = (tmp[2] << 6) | tmp[3];
        tmpc = 0;
      }
    }
    switch (tmpc) {
      case 1: dst[dstc++] = tmp[0] << 2; break; // shouldn't happen
      case 2: dst[dstc++] = (tmp[0] << 2) | (tmp[1] >> 4); break;
      case 3: dst[dstc++] = (tmp[0] << 2) | (tmp[1] >> 4); dst[dstc++] = (tmp[1] << 4) | (tmp[2] >> 2); break;
    }
    return new Uint8Array(dst.buffer, 0, dstc);
  }
  
  newResource(path, serial) {
    if (!path || this.resv.find(r => r.path === path)) return Promise.reject("Invalid path");
    if (!serial) serial = new Uint8Array(0);
    return this.comm.httpBinary("PUT", "/data/" + path, null, null, serial).then(() => {
      this.resv.push({
        ...this.parsePath(path),
        serial,
      });
      this.types = Array.from(new Set(this.resv.map(r => r.type))).sort();
      for (const { cb } of this.tocListeners) cb();
    });
  }
  
  // Syncs to backend but assumes that will succeed. Returns resv entry.
  newResourceSync(path, serial) {
    if (!path || this.resv.find(r => r.path === path)) return Promise.reject("Invalid path");
    if (!serial) serial = new Uint8Array(0);
    const res = { ...this.parsePath(path), serial };
    this.resv.push(res);
    this.types = Array.from(new Set(this.resv.map(r => r.type))).sort();
    for (const { cb } of this.tocListeners) cb();
    this.comm.httpBinary("PUT", "/data/" + path, null, null, serial).catch(e => this.dom.modalError(e));
    return res;
  }
  
  deleteResource(path) {
    if (!this.resv.find(r => r.path === path)) return Promise.reject("Resource not found");
    delete this.dirties[path];
    return this.comm.httpBinary("DELETE", "/data/" + path).then(() => {
      // Repeat the search. TOC might have changed while we were in flight.
      const p = this.resv.findIndex(r => r.path === path);
      if (p >= 0) {
        this.resv.splice(p, 1);
        this.types = Array.from(new Set(this.resv.map(r => r.type))).sort();
        for (const { cb } of this.tocListeners) cb();
      }
    });
  }
  
  resByString(src, type) {
    const rid = +src;
    for (const res of this.resv) {
      if (res.type !== type) continue;
      if (res.rid === rid) return res;
      if (res.name === src) return res;
    }
    return null;
  }
  
  resolveId(src) {
    const split = src.split(':');
    if (split.length === 2) {
      const type = split[0];
      const name = split[1];
      const res = this.resByString(name, type);
      if (res) return res.rid;
    }
    return 0;
  }
  
  getImage(name) {
    const res = this.resByString(name, "image");
    if (!res) return null;
    if (!res.image) {
      const blob = new Blob([res.serial]);
      const url = URL.createObjectURL(blob);
      res.image = new Image();
      res.image.addEventListener("load", () => {
        URL.revokeObjectURL(url);
      }, { once: true });
      res.image.addEventListener("error", (e) => {
        URL.revokeObjectURL(url);
      }, { once: true });
      res.image.src = url;
    }
    return res.image;
  }
  
  getImageAsync(name) {
    const image = this.getImage(name);
    if (!image) return Promise.reject();
    if (image.complete) return Promise.resolve(image);
    return new Promise((resolve, reject) => {
      image.addEventListener("load", () => resolve(image), { once: true });
      image.addEventListener("error", e => reject(e), { once: true });
    });
  }
}

Data.singleton = true;
