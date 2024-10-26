/* AudioService.js
 * Interface to either the server's native synthesizer, or a WebAudio one that we run client-side.
 * Clients asking to play a sound don't need to care which.
 */
 
import { Comm } from "./Comm.js";
 
export class AudioService {
  static getDependencies() {
    return [Comm];
  }
  constructor(comm) {
    this.comm = comm;
    
    this.outputMode = "none"; // "none","server","client"
    this.serverAvailable = null; // boolean after we've confirmed.
    this.nextListenerId = 1;
    this.listeners = [];
    
    this.comm.httpStatusOnly("POST", "/api/sound").then(status => {
      this.serverAvailable = (status === 200);
      this.broadcast({ id: "availability" });
    }).catch(e => this.serverAvailable = false);
  }
  
  listen(cb) {
    const id = this.nextListenerId++;
    this.listeners.push({ cb, id });
    return id;
  }
  
  unlisten(id) {
    const p = this.listeners.findIndex(l => l.id === id);
    if (p >= 0) this.listeners.splice(p, 1);
  }
  
  broadcast(event) {
    for (const { cb } of this.listeners) cb(event);
  }
  
  setOutputMode(mode) {
    if (mode === this.outputMode) return;
    switch (mode) {
      case "none": break;
      case "server": if (!this.serverAvailable) return; break;
      case "client": break;
      default: return;
    }
    this.stop();
    this.outputMode = mode;
  }
  
  stop() {
    switch (this.outputMode) {
      case "server": return this.comm.http("POST", "/api/sound").catch(e => {}); break;
      case "client": break; //TODO Stop playback, WebAudio
    }
    return Promise.resolve();
  }
  
  play(serial, position, repeat) {
    if (!position) position = 0;
    if (!repeat) repeat = 0;
    switch (this.outputMode) {
      case "none": return Promise.reject("Audio output not enabled.");
      case "server": return this.comm.http("POST", "/api/sound", { position, repeat }, null, serial);
      case "client": return this.comm.httpBinary("POST", "/api/compile", null, null, serial).then(rsp => {
          console.log(`Compile ok`, { serial, rsp });
          //TODO Forward (rsp) to synthesizer.
        }).catch(e => {
          console.log(`Compile failed`, e);
          throw e;
        });
    }
    return Promise.reject(`Invalid output mode ${JSON.stringify(this.outputMode)}`);
  }
}

AudioService.singleton = true;
