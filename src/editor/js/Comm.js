/* Comm.js
 * HTTP and WebSocket interface.
 */
 
export class Comm {
  static getDependencies() {
    return [Window];
  }
  constructor(window) {
    this.window = window;
    
    this.nextPlayheadListener = 1;
    this.playheadListeners = [];
    this.playheadSocket = null;
    this.playheadConnected = false;
  }
  
  http(method, path, query, headers, body, returnType) {
    const options = { method };
    if (headers) options.headers = headers;
    if (body) options.body = body;
    return this.window.fetch(this.composeHttpUrl(path, query), options).then(rsp => {
      if (!rsp.ok) return rsp.text().then(body => { throw body || `${method} ${path} => ${rsp.status}\n${rsp.statusText}`; });
      return this.getHttpResponse(rsp, returnType);
    });
  }
  
  // ...and since returnType is all the way at the end, after the optional args, some conveniences:
  httpText(method, path, query, headers, body) { return this.http(method, path, query, headers, body, "text"); }
  httpBinary(method, path, query, headers, body) { return this.http(method, path, query, headers, body, "arrayBuffer"); }
  httpJson(method, path, query, headers, body) { return this.http(method, path, query, headers, body, "json"); }
  
  composeHttpUrl(path, query) {
    if (query) path += '?' + Object.keys(query).map(k => encodeUriComponent(k) + '=' + encodeUriComponent(query[k])).join('&');
    return path;
  }
  
  getHttpResponse(rsp, returnType) {
    switch (returnType) {
      case "arrayBuffer": return rsp.arrayBuffer();
      case "text": return rsp.text();
      case "json": return rsp.json();
    }
    return rsp.arrayBuffer();
  }
  
  /* Subscribe to playhead of song playing server-side.
  XXX This was copied from egg-202408. We don't have this feature yet, and expect it to change at least a little.
   ******************************************************************************/
   
  setPlayhead(v) {
    if (!this.playheadConnected) return false;
    v = Math.max(0, Math.min(0xffff, v * 0xffff));
    this.playheadSocket.send(new Uint8Array([v >> 8, v]));
    return true;
  }
  
  listenPlayhead(cb) {
    const id = this.nextPlayheadListener++;
    this.playheadListeners.push({ id, cb });
    if (!this.playheadSocket) this.connectPlayhead();
    return id;
  }
  
  unlistenPlayhead(id) {
    const p = this.playheadListeners.findIndex(l => l.id === id);
    if (p < 0) return;
    this.playheadListeners.splice(p, 1);
    // When we run out of playhead listeners, don't close the socket immediately.
    // User might be switching to a different song and will resubscribe right away.
    // So check after half a second or so.
    this.window.setTimeout(() => this.checkPlayheadAbandonment(), 500);
  }
  
  checkPlayheadAbandonment() {
    if (this.playheadListeners.length) return;
    if (!this.playheadSocket) return;
    this.playheadSocket.close();
    this.playheadSocket = null;
  }
  
  connectPlayhead() {
    this.playheadSocket = new WebSocket(`ws://${this.window.location.host}/ws/playhead`);
    this.playheadSocket.binaryType = "arraybuffer";
    this.playheadSocket.addEventListener("open", () => {
      this.playheadConnected = true;
    });
    this.playheadSocket.addEventListener("close", () => {
      this.playheadSocket = null;
      this.playheadConnected = false;
      for (const { cb } of this.playheadListeners) cb(0);
    });
    this.playheadSocket.addEventListener("error", () => {
      this.playheadSocket = null;
      this.playheadConnected = false;
      for (const { cb } of this.playheadListeners) cb(0);
    });
    this.playheadSocket.addEventListener("message", msg => {
      if (msg.data?.byteLength === 2) {
        const view = new Uint8Array(msg.data);
        let v = ((view[0] << 8) | view[1]) / 65535.0;
        if (v) {
          for (const { cb } of this.playheadListeners) cb(v);
        }
      }
    });
  }
}

Comm.singleton = true;
