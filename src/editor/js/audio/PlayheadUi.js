/* PlayheadUi.js
 * Simple horizontal bar with a thumb that moves with our assumption of the current playhead.
 * It would be complicated and bandwidthy to read the actual playhead, and we can guess it close enough.
 */
 
import { AudioService } from "./AudioService.js";
import { SynthFormats } from "/rt/js/synth/SynthFormats.js";
 
export class PlayheadUi {
  static getDependencies() {
    return [HTMLCanvasElement, AudioService, Window];
  }
  constructor(element, audioService, window) {
    this.element = element;
    this.audioService = audioService;
    this.window = window;
    
    onplay = (p) => {};
    
    this.startTime = 0;
    this.p = 0;
    this.dur = 1;
    this.renderTimeout = null;
    this.renderInterval = null;
    this.audioListener = this.audioService.listen(e => this.onAudioEvent(e));
    this.element.width = 200;
    this.element.height = 20;
    this.thumbWidth = 10;
    this.element.addEventListener("mousedown", e => this.onMouseDown(e));
    this.renderSoon();
  }
  
  onRemoveFromDom() {
    this.endUpdates();
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
    if (this.audioListener) {
      this.audioService.unlisten(this.audioListener);
      this.audioListener = null;
    }
  }
  
  setDuration(s) {
    this.dur = s;
    this.renderSoon();
  }
  
  renderSoon() {
    if (this.renderTimeout) return;
    if (this.renderInterval) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 50);
  }
  
  renderNow() {
    const ctx = this.element.getContext("2d");
    ctx.fillStyle = "#400";
    ctx.fillRect(0, 0, this.element.width, this.element.height);
    const thumbx = Math.floor((this.p * (this.element.width - this.thumbWidth)) / this.dur);
    ctx.fillStyle = "#ff0";
    ctx.fillRect(thumbx, 0, this.thumbWidth, this.element.height);
    // Guidelines:
    ctx.strokeStyle = "#fff";
    ctx.globalAlpha = 0.333;
    ctx.beginPath();
    for (let division=8; division>=1; division>>=1) {
      for (let t=0; t<=division; t++) {
        let x = Math.floor((t * this.element.width) / division);
        if (x >= this.element.width) x = this.element.width - 1;
        x += 0.5;
        ctx.moveTo(x, 0);
        ctx.lineTo(x, this.element.height);
      }
    }
    ctx.stroke();
    ctx.globalAlpha = 1;
    // Then draw the guidelines again in black, only the lower part, so they're visible above the thumb:
    ctx.strokeStyle = "#000";
    ctx.beginPath();
    for (let t=0; t<=8; t++) {
      let x = Math.floor((t * this.element.width) / 8);
      if (x >= this.element.width) x = this.element.width - 1;
      x += 0.5;
      ctx.moveTo(x, this.element.height >> 1);
      ctx.lineTo(x, this.element.height);
    }
    ctx.stroke();
  }
  
  beginUpdates() {
    if (this.renderInterval) return;
    this.renderInterval = this.window.setInterval(() => {
      if ((this.p = (Date.now() / 1000) - this.startTime) >= this.dur) {
        // Time expires, assume it's done playing.
        this.p = this.dur;
        this.endUpdates();
      }
      this.renderNow();
    }, 50);
  }
  
  endUpdates() {
    if (this.renderInterval) {
      this.window.clearInterval(this.renderInterval);
      this.renderInterval = null;
    }
  }
  
  onAudioEvent(event) {
    switch (event.id) {
      case "play": {
          this.startTime = (Date.now() / 1000) - event.position;
          this.p = event.position;
          this.dur = SynthFormats.measureDuration(event.serial);
          this.beginUpdates();
        } break;
      case "stop": {
          this.endUpdates();
        } break;
    }
  }
  
  onMouseDown(event) {
    const bounds = this.element.getBoundingClientRect();
    const nx = Math.max(0, Math.min(1, (event.x - bounds.x - this.thumbWidth / 2) / (bounds.width - this.thumbWidth)));
    this.onplay(this.dur * nx);
  }
}
