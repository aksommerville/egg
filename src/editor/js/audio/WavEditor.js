/* WavEditor.js
 * We're not going deep on this one, but do show the levels and allow simple adjustments like dropping channels and truncating silence.
 * (actually, dropping channels will happen implicitly)
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { AudioService } from "./AudioService.js";
 
export class WavEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, AudioService];
  }
  constructor(element, dom, data, audioService) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.audioService = audioService;
    
    this.res = null;
    this.file = null;
  }
  
  static isWavSignature(s) {
    return (s &&
      (s.length >= 12) &&
      (s[0] === 0x52) && // "RIFF"
      (s[1] === 0x49) &&
      (s[2] === 0x46) &&
      (s[3] === 0x46) &&
      (s[8] === 0x57) && // "WAVE"
      (s[9] === 0x41) &&
      (s[10] === 0x56) &&
      (s[11] === 0x45)
    );
  }
  
  static checkResource(res) {
    if (WavEditor.isWavSignature(res.serial)) return 2;
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.file = new WavFile(res.serial);
    this.buildUi();
  };
  
  buildUi() {
    this.element.innerHTML = "";
    if (!this.file) return;
    
    const ribbon = this.dom.spawn(this.element, "DIV", ["ribbon"]);
    this.dom.spawn(ribbon, "INPUT", { type: "button", value: `Trim lead (${this.countLeadingZeroes()})`, "on-click": () => this.onTrimLead() });
    this.dom.spawn(ribbon, "INPUT", { type: "button", value: `Trim tail (${this.countTrailingZeroes()})`, "on-click": () => this.onTrimTail() });
    this.dom.spawn(ribbon, "INPUT", { type: "button", value: `Level... (${this.reprLevel()})`, "on-click": () => this.onEditLevel() });
    this.dom.spawn(ribbon, "INPUT", { type: "button", value: ">", "on-click": () => this.onPlay() });
    this.dom.spawn(ribbon, "INPUT", { type: "button", value: "!!", "on-click": () => this.onStop() });
    
    this.dom.spawn(this.element, "CANVAS");
    this.render();
  }
  
  updateButtonLabels() {
    let button;
    if (button = this.element.querySelector("input[value^='Trim lead']")) {
      button.value = `Trim lead (${this.countLeadingZeroes()})`;
    }
    if (button = this.element.querySelector("input[value^='Trim tail']")) {
      button.value = `Trim tail (${this.countTrailingZeroes()})`;
    }
    if (button = this.element.querySelector("input[value^='Level']")) {
      button.value = `Level... (${this.reprLevel()})`;
    }
  }
  
  render() {
    const canvas = this.element.querySelector("canvas");
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    if (!this.file) return;
    
    const midy = canvas.height * 0.5;
    const calcy = (level) => {
      return Math.round((level * midy) / 32768 + midy);
    };
    
    ctx.beginPath();
    ctx.moveTo(0, calcy(0));
    ctx.lineTo(bounds.width, calcy(0));
    ctx.strokeStyle = "#666";
    ctx.stroke();
    
    if (this.file.pcm.length <= canvas.width) {
      // Uncommonly short data. Draw it exactly.
      ctx.beginPath();
      ctx.moveTo(0, calcy(this.file.pcm[0]));
      for (let i=0; i<this.file.pcm.length; i++) {
        const x = ((i + 1) * canvas.width) / this.file.pcm.length;
        ctx.lineTo(x, calcy(this.file.pcm[i]));
      }
      ctx.strokeStyle = "#fff";
      ctx.stroke();
    } else {
      // Data is wider than view (typically *much* wider).
      // Bucket the levels for every 2 pixels horizontally.
      // Trace a clockwise shape: left to right on the highs, then right to left on the lows.
      const bucketc = Math.max(1, bounds.width >> 1);
      const buckets = [];
      for (let p=0, i=0, x=0; i<bucketc; i++, x+=2) {
        const nextp = ((i + 1) * this.file.pcm.length) / bucketc;
        let lo=this.file.pcm[p], hi=this.file.pcm[p];
        for (; p<nextp; p++) {
          const v = this.file.pcm[p];
          if (v < lo) lo = v; else if (v > hi) hi = v;
        }
        buckets.push({ lo: calcy(lo), hi: calcy(hi), x });
      }
      ctx.beginPath();
      ctx.moveTo(buckets[0].x, buckets[0].hi);
      for (let i=1; i<buckets.length; i++) ctx.lineTo(buckets[i].x, buckets[i].hi);
      for (let i=buckets.length; i-->0; ) ctx.lineTo(buckets[i].x, buckets[i].lo);
      ctx.fillStyle = "#ccc";
      ctx.fill();
    }
  }
  
  countLeadingZeroes() {
    let c = 0;
    if (this.file) {
      while ((c < this.file.pcm.length) && !this.file.pcm[c]) c++;
    }
    return c;
  }
  
  countTrailingZeroes() {
    let c = 0;
    if (this.file) {
      while ((c < this.file.pcm.length) && !this.file.pcm[this.file.pcm.length - c - 1]) c++;
    }
    return c;
  }
  
  reprLevel() {
    if (!this.file?.pcm.length) return "";
    let hi=this.file.pcm[0], lo=this.file.pcm[0], sqsum=0;
    for (const v of this.file.pcm) {
      if (v < lo) lo = v; else if (v > hi) hi = v;
      sqsum += v * v;
    }
    const peak = Math.max(-lo, hi);
    const rms = Math.sqrt(sqsum / this.file.pcm.length);
    return `p=${peak}, rms=${rms}`;
  }
  
  changed() {
    this.updateButtonLabels();
    this.render();
    this.data.dirty(this.res.path, () => this.file.encode());
  }
  
  onTrimLead() {
    const rmc = this.countLeadingZeroes();
    if (rmc < 1) return;
    const nv = new Int16Array(this.file.pcm.length - rmc);
    const srcview = new Int16Array(this.file.pcm.buffer, this.file.pcm.byteOffset + (rmc << 1), nv.length);
    nv.set(srcview);
    this.file.pcm = nv;
    this.changed();
  }
  
  onTrimTail() {
    const rmc = this.countTrailingZeroes();
    if (rmc < 1) return;
    const nv = new Int16Array(this.file.pcm.length - rmc);
    const srcview = new Int16Array(this.file.pcm.buffer, this.file.pcm.byteOffset, nv.length);
    nv.set(srcview);
    this.file.pcm = nv;
    this.changed();
  }
  
  onEditLevel() {
    if (!this.file) return;
    let hi=this.file.pcm[0], lo=this.file.pcm[0];
    for (const v of this.file.pcm) {
      if (v < lo) lo = v; else if (v > hi) hi = v;
    }
    const peak = Math.min(32767, Math.max(-lo, hi));
    const recommend = 32767 / peak;
    this.dom.modalInput("Gain:", recommend).then((result) => {
      const gain = +result;
      if (!gain) return;
      if (gain === 1) return;
      for (let i=0; i<this.file.pcm.length; i++) {
        this.file.pcm[i] = Math.max(-32768, Math.min(32767, Math.round(this.file.pcm[i] * gain)));
      }
      this.changed();
    }).catch(() => {});
  }
  
  onPlay() {
    if (this.audioService.outputMode === "none") {
      if (this.audioService.serverAvailable) this.audioService.setOutputMode("server");
      else this.audioService.setOutputMode("client");
    }
    this.audioService.play(this.file.encode());
  }
  
  onStop() {
    this.audioService.stop();
  }
}

export class WavFile {
  constructor(src) {
    if (src instanceof Uint8Array) {
      this.decode(src);
    } else {
      throw new Error(`Inappropriate input to WavFile`);
    }
  }
  
  decode(src) {
    this.rate = 0; // We accept anything, even zero.
    this.pcm = []; // Int16Array if not empty.
    let chanc = 0;
    if (WavEditor.checkResource({ serial: src }) !== 2) throw new Error(`WAV signature mismatch`);
    for (let srcp=12; srcp<src.length; ) {
      const chunkid = (src[srcp] << 24) | (src[srcp+1] << 16) | (src[srcp+2] << 8) | src[srcp+3];
      const chunklen = src[srcp+4] | (src[srcp+5] << 8) | (src[srcp+6] << 16) | (src[srcp+7] << 24);
      srcp += 8;
      if ((chunklen < 0) || (srcp > src.length - chunklen)) throw new Error(`WAV framing error`);
      switch (chunkid) {
        case 0x666d7420: { // fmt
            if (chunklen < 16) throw new Error(`WAV fmt chunk invalid length ${chunklen}`);
            const format = src[srcp] | (src[srcp+1] << 8);
            chanc = src[srcp+2] | (src[srcp+3] << 8);
            this.rate = src[srcp+4] | (src[srcp+5] << 8) | (src[srcp+6] << 16) | (src[srcp+7] << 24);
            const samplesize = src[srcp+14] | (src[srcp+15] << 8);
            if (format !== 1) throw new Error(`WAV format ${format} not supported. Must be 1 (LPCM)`);
            if (chanc < 1) throw new Error(`WAV zero channel count`);
            if (samplesize !== 16) throw new Error(`WAV sample size must be 16, found ${samplesize}`);
          } break;
        case 0x64617461: { // data
            if (!chanc) throw new Error(`WAV data without fmt`);
            const stride = 2 * chanc;
            const framec = chunklen / stride;
            let dstp = this.pcm.length;
            if (this.pcm.length) {
              const nv = new Int16Array(this.pcm.length + framec);
              nv.set(this.pcm);
              this.pcm = nv;
            } else {
              this.pcm = new Int16Array(framec);
            }
            for (let rdp=0; rdp<chunklen; rdp+=stride, dstp++) {
              this.pcm[dstp] = src[srcp+rdp] | (src[srcp+rdp+1] << 8);
            }
          } break;
      }
      srcp += chunklen;
    }
  }
  
  encode() {
    const fmtlen = 16;
    const datalen = this.pcm.length * 2;
    const filelen = 8 + fmtlen + 8 + datalen;
    const rifflen = 12 + filelen;
    const dst = new Uint8Array(rifflen);
    dst[0] = 0x52; // RIFF
    dst[1] = 0x49;
    dst[2] = 0x46;
    dst[3] = 0x46;
    dst[4] = filelen;
    dst[5] = filelen >> 8;
    dst[6] = filelen >> 16;
    dst[7] = filelen >> 24;
    dst[8] = 0x57; // WAVE
    dst[9] = 0x41;
    dst[10] = 0x56;
    dst[11] = 0x45;
    dst[12] = 0x66; // fmt
    dst[13] = 0x6d;
    dst[14] = 0x74;
    dst[15] = 0x20;
    dst[16] = fmtlen;
    dst[17] = 0;
    dst[18] = 0;
    dst[19] = 0;
    // fmt...
    dst[20] = 1; dst[21] = 0; // format 1 LPCM
    dst[22] = 1; dst[23] = 0; // chanc 1
    dst[24] = this.rate;
    dst[25] = this.rate >> 8;
    dst[26] = this.rate >> 16;
    dst[27] = this.rate >> 24;
    const br = this.rate * 2;
    dst[28] = br;
    dst[29] = br >> 8;
    dst[30] = br >> 16;
    dst[31] = br >> 24;
    dst[32] = 2; dst[33] = 0; // bytes/frame
    dst[34] = 16; dst[35] = 0; // bits/sample
    dst[36] = 0x64; // data
    dst[37] = 0x61;
    dst[38] = 0x74;
    dst[39] = 0x61;
    dst[40] = datalen;
    dst[41] = datalen >> 8;
    dst[42] = datalen >> 16;
    dst[43] = datalen >> 24;
    const dataview = new Int16Array(dst.buffer, dst.byteOffset + 44, this.pcm.length);
    dataview.set(this.pcm);
    return dst;
  }
}
