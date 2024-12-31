/* AudioService.js
 * Interface to either the server's native synthesizer, or a WebAudio one that we run client-side.
 * Clients asking to play a sound don't need to care which.
 * We are also responsible for MIDI-In.
 */
 
import { Comm } from "../Comm.js";
import { Audio } from "/rt/js/synth/Audio.js";
import { SynthFormats } from "/rt/js/synth/SynthFormats.js";
 
export class AudioService {
  static getDependencies() {
    return [Comm, Window];
  }
  constructor(comm, window) {
    this.comm = comm;
    this.window = window;
    
    this.outputMode = "none"; // "none","server","client"
    this.serverAvailable = null; // boolean after we've confirmed.
    this.nextListenerId = 1;
    this.listeners = [];
    this.audio = null;
    this.updateInterval = null;
    this.midiAccess = null;
    this.nextMidiListenerId = 1;
    this.midiListeners = [];
    
    if (!this.window.navigator?.requestMIDIAccess) {
      console.log(`AudioService: MIDI not available`);
    } else this.window.navigator.requestMIDIAccess().then(access => {
      this.midiAccess = access;
      // Don't use addEventListener here: We might see the same object more than once.
      for (const [id, device] of access.inputs) {
        device.onmidimessage = mm => this.onMidiMessage(mm);
      }
      this.midiAccess.addEventListener("statechange", e => {
        if (e.port?.state === "connected") {
          e.port.onmidimessage = mm => this.onMidiMessage(mm);
        }
      });
    }).catch(e => {
      console.log(`AudioService: MIDI error`, e);
    });
    
    this.comm.httpStatusOnly("POST", "/api/sound?unavailableStatus=299").then(status => {
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
  
  // cb(serial:Uint8Array, deviceId:string). Per MDN, serial will be a single event.
  listenMidi(cb) {
    const id = this.nextMidiListenerId++;
    this.midiListeners.push({ cb, id });
    return id;
  }
  
  unlistenMidi(id) {
    const p = this.midiListeners.findIndex(l => l.id === id);
    if (p >= 0) this.midiListeners.splice(p, 1);
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
      case "server": this.outputMode = "none"; return this.comm.http("POST", "/api/sound").catch(e => {});
      case "client":  {
          if (this.audio) {
            this.audio.stop();
            //this.audio = null;
          }
          if (this.updateInterval) {
            clearInterval(this.updateInterval);
            this.updateInterval = null;
          }
          this.outputMode = "none";
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
      case "client": {
          let compile; // Promise
          switch (SynthFormats.detectFormat(serial)) {
            case "egs": case "wav": compile = Promise.resolve(serial); break;
            default: compile = this.comm.httpBinary("POST", "/api/compile", null, null, serial); break;
          }
          return compile.then(rsp => {
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
    }
    return Promise.reject(`Invalid output mode ${JSON.stringify(this.outputMode)}`);
  }
  
  update() {
    if (this.audio) this.audio.update();
  }
  
  onMidiMessage(event) {
    if (!event.data) return;
    for (const { cb } of this.midiListeners) cb(event.data, event.target.id);
  }
}

AudioService.singleton = true;
