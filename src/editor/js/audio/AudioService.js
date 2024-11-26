/* AudioService.js
 * Interface to either the server's native synthesizer, or a WebAudio one that we run client-side.
 * Clients asking to play a sound don't need to care which.
 */
 
import { Comm } from "../Comm.js";
import { Audio } from "/rt/js/synth/Audio.js";
 
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
    this.audio = null;
    this.updateInterval = null;
    
    this.comm.httpStatusOnly("POST", "/api/sound").then(status => {
      this.serverAvailable = (status === 200);
      this.broadcast({ id: "availability" });
    }).catch(e => {
      this.serverAvailable = false;
      this.broadcast({ id: "availability" });
    });
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
    if (this.audio) {
      this.audio.stop();
      //this.audio = null;
    }
    if (this.updateInterval) {
      clearInterval(this.updateInterval);
      this.updateInterval = null;
    }
    this.stop();
    switch (mode) {
      case "none": break;
      case "server": if (!this.serverAvailable) return; break;
      case "client": {
          if (!this.audio) {
            const rt = {};
            this.audio = new Audio(rt);
          }
          this.audio.start();
          this.updateInterval = setInterval(() => this.update(), 100);
        } break;
      default: return;
    }
    this.outputMode = mode;
    this.broadcast({ id: "outputMode" });
  }
  
  stop() {
    switch (this.outputMode) {
      case "server": return this.comm.http("POST", "/api/sound").catch(e => {});
      case "client": {
          if (this.audio) {
            this.audio.stop();
            //this.audio = null;
          }
          if (this.updateInterval) {
            clearInterval(this.updateInterval);
            this.updateInterval = null;
          }
        } break;
    }
    return Promise.resolve();
  }
  
  play(serial, position, repeat) {
    if (!position) position = 0;
    if (!repeat) repeat = 0;
    switch (this.outputMode) {
      case "none": return Promise.reject("Audio output not enabled.");
      case "server": return this.comm.http("POST", "/api/sound", { position, repeat }, null, serial);
      //TODO We don't have to call /api/compile every time (and it's expensive!). EGS and well-formed WAV can pass right thru.
      case "client": return this.comm.httpBinary("POST", "/api/compile", null, null, serial).then(rsp => {
          if (!this.audio) {
            const rt = {};
            this.audio = new Audio(rt);
          }
          this.audio.start();
          if (!this.updateInterval) {
            this.updateInterval = setInterval(() => this.update(), 100);
          }
          this.audio.playSong(new Uint8Array(rsp), false, true);
        });
    }
    return Promise.reject(`Invalid output mode ${JSON.stringify(this.outputMode)}`);
  }
  
  update() {
    if (this.audio) this.audio.update();
  }
}

AudioService.singleton = true;
