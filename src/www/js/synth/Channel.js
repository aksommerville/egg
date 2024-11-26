/* Channel.js
 * One channel of an EGS song, same idea as MIDI channels.
 * Each Channel has a fixed configuration at startup.
 */
 
import { SynthFormats } from "./SynthFormats.js";
import { Song } from "./Song.js";
import { Env } from "./Env.js";
 
export class Channel {

  /* (src) must be from SynthFormats.splitEgs().channels, and not null.
   */
  constructor(src, ctx, globalTrim) {
    this.chid = src.chid;
    this.trim = src.trim / 255.0;
    this.trim *= globalTrim;
    this.mode = src.mode;
    this.decode(src.v, ctx);
    this.inflight = []; // {time,tail}
  }
  
  decode(src, ctx) {
    switch (this.mode) {
      case 0: break; // NOOP
      case 1: this.decodeDrum(src); break;
      case 2: this.decodeWave(src, ctx); break;
      case 3: this.decodeFm(src, ctx); break;
      case 4: this.decodeSub(src, ctx); break;
      default: {
          console.log(`Channel ${this.chid}, unknown mode ${this.mode}. Will ignore channel.`);
        }
    }
  }
  
  cancel(ctx) {
    if (ctx) {
      for (const voice of this.inflight) {
        if (voice.time >= ctx.currentTime) {
          voice.tail.disconnect();
        } else {
          if (voice.tail.gain) {
            voice.tail.gain.setValueAtTime(voice.tail.gain.value, ctx.currentTime);
            voice.tail.gain.cancelScheduledValues(ctx.currentTime);
            voice.tail.gain.linearRampToValueAtTime(0, ctx.currentTime + 0.200);
          }
        }
      }
    }
    this.inflight = [];
  }
  
  /* (when) in seconds of context time.
   * (noteid) in 0..127.
   * (velocity) in 0..1.
   * (dur) in seconds.
   */
  playNote(ctx, when, noteid, velocity, dur) {
    switch (this.mode) {
      case 1: this.playNoteDrum(ctx, when, noteid, velocity); break;
      case 2: // WAVE, FM, SUB share some bits.
      case 3:
      case 4: {
          const frequency = SynthFormats.frequencyForMidiNote(noteid);
          let oscAndTail = null;
          switch (this.mode) {
            case 2: oscAndTail = this.oscillateWave(ctx, frequency, when, velocity, dur); break;
            case 3: oscAndTail = this.oscillateFm(ctx, frequency, when, velocity, dur); break;
            case 4: oscAndTail = this.oscillateSub(ctx, frequency, when, velocity, dur); break;
          }
          if (!oscAndTail) return;
          this.finishTunedNote(ctx, oscAndTail[0], oscAndTail[1], when, velocity, dur);
        } break;
    }
  }
  
  /* Apply level envelope and connect to output.
   * For WAVE, FM, SUB.
   */
  finishTunedNote(ctx, osc, tail, when, velocity, dur) {
    const env = new GainNode(ctx);
    const plan = this.levelenv.apply(velocity, when, dur);
    env.gain.setValueAtTime(plan[0].v, 0);
    for (const pt of plan) {
      env.gain.linearRampToValueAtTime(pt.v, pt.t);
    }
    
    tail.connect(env);
    env.connect(ctx.destination);
    if (osc) {
      osc.start(when);
      osc.stop(plan[plan.length - 1].t);
      osc.addEventListener("ended", () => {
        tail.disconnect();
        const p = this.inflight.findIndex(v => v.tail === env);
        if (p >= 0) this.inflight.splice(p, 1);
      });
      this.inflight.push({ time: when, tail: env });
    }
  }
  
  decodeDrum(src) {
    this.drums = []; // Sparse. {trimlo,trimhi,v,buffer} indexed by noteid.
    for (let srcp=0; srcp<src.length; ) {
      const noteid = src[srcp++];
      const trimlo = src[srcp++] / 255.0;
      const trimhi = src[srcp++] / 255.0;
      const len = (src[srcp] << 8) | src[srcp+1];
      srcp += 2;
      if (srcp > src.length - len) break;
      if ((noteid < 0x80) && (trimlo || trimhi)) { // Don't bother capturing the note if trims are zero, or noteid invalid.
        this.drums[noteid] = {
          trimlo, trimhi,
          v: src.slice(srcp, srcp + len),
          buffer: null, // lazy
        };
      }
      srcp += len;
    }
  }
  
  playNoteDrum(ctx, when, noteid, velocity) {
    const drum = this.drums[noteid];
    if (!drum) return;
    let trim = drum.trimlo * (1 - velocity) + drum.trimhi * velocity;
    trim *= this.trim;
    if (drum.buffer) {
      this.playPcm(ctx, when, drum.buffer, trim);
    } else {
      this.printDrum(drum.v, ctx.sampleRate).then(buffer => {
        drum.buffer = buffer;
        this.playPcm(ctx, when, drum.buffer, trim);
      });
    }
  }
  
  playPcm(ctx, when, buffer, trim) {
    const sourceNode = new AudioBufferSourceNode(ctx, {
      buffer,
      channelCount: 1,
    });
    let endNode = sourceNode;
    if (trim === 1.0) {
      sourceNode.connect(ctx.destination);
    } else {
      const gainNode = new GainNode(ctx, { gain: trim });
      sourceNode.connect(gainNode);
      gainNode.connect(ctx.destination);
      endNode = gainNode;
    }
    this.inflight.push({ time: when, tail: endNode });
    sourceNode.start(when);
    sourceNode.addEventListener("ended", () => {
      const p = this.inflight.findIndex(v => v.tail === endNode);
      if (p >= 0) this.inflight.splice(p, 1);
    });
  }
  
  printDrum(serial, sampleRate) {
    const format = SynthFormats.detectFormat(serial);
    if (format === "wav") {
      return Promise.resolve(SynthFormats.decodeWav(serial));
    }
    if (format === "egs") {
      const egs = SynthFormats.splitEgs(serial);
      let durs = 0;
      for (let iter=SynthFormats.iterateEgsEvents(egs.events), event; event=iter.next(); ) {
        if (event.type === "delay") durs += event.delay;
      }
      const samplec = Math.max(1, Math.ceil(durs * sampleRate));
      const subctx = new OfflineAudioContext(1, samplec, sampleRate);
      const song = new Song(egs, false, subctx, 1.0);
      song.update(subctx, subctx.currentTime + durs + 1.0);
      return subctx.startRendering();
    }
    return Promise.resolve(new AudioBuffer({ length: 1, numberOfChannels: 1, sampleRate, channelCount: 1 }));
  }
  
  decodeWave(src, ctx) {
    this.shape = "sine";
    this.pitchenv = null;
    this.levelenv = new Env();
    let srcp = this.levelenv.decode(src, 0);
    this.levelenv.scale(this.trim);
    if (srcp >= src.length) return;
    srcp = this.decodeWaveShape(src, srcp, ctx);
    if (srcp >= src.length) return;
    this.pitchenv = new Env();
    srcp = this.pitchenv.decode(src, srcp);
    if (!this.pitchenv.pointc && (this.pitchenv.inivlo === 0.5) && (this.pitchenv.inivhi === 0.5)) {
      this.pitchenv = null;
    } else {
      this.pitchenv.bias(-0.5);
      this.pitchenv.scale(32768.0);
    }
  }
  
  decodeWaveShape(src, srcp, ctx) {
    this.shape = src[srcp++];
    if (!this.shape) {
      const coefc = src[srcp++] || 0;
      const real = new Float32Array(1 + coefc);
      const imag = new Float32Array(1 + coefc);
      for (let i=1; i<=coefc; i++, srcp+=2) {
        real[i] = ((src[srcp] << 8) | src[srcp+1]) / 65535.0;
      }
      this.periodicWave = new PeriodicWave(ctx, { real, imag, disableNormalization: true });
    }
    switch (this.shape) {
      case 0: this.shape = "custom"; break;
      case 1: this.shape = "sine"; break;
      case 2: this.shape = "square"; break;
      case 3: this.shape = "sawtooth"; break;
      case 4: this.shape = "triangle"; break;
      default: this.shape = "sine";
    }
    return srcp;
  }
  
  oscillateWave(ctx, frequency, when, velocity, dur) {
    const options = {
      frequency,
      type: this.shape,
    };
    if (this.shape === "custom") {
      options.periodicWave = this.periodicWave;
    }
    const osc = new OscillatorNode(ctx, options);
    
    if (this.pitchenv) {
      const plan = this.pitchenv.apply(velocity, when, dur);
      osc.detune.setValueAtTime(plan[0].v, 0);
      for (const pt of plan) {
        osc.detune.linearRampToValueAtTime(pt.v, pt.t);
      }
    }

    return [osc, osc];
  }
  
  decodeFm(src, ctx) {
    this.rate = 1;
    let rangePeak = 1;
    this.pitchenv = null;
    this.rangeenv = new Env();
    this.levelenv = new Env();
    let srcp = this.levelenv.decode(src, 0);
    this.levelenv.scale(this.trim);
    if (srcp < src.length) {
      this.rate = src[srcp] + src[srcp+1] / 256.0;
      srcp += 2;
    }
    if (srcp < src.length) {
      rangePeak = src[srcp] + src[srcp+1] / 256.0;
      srcp += 2;
    }
    if (srcp < src.length) {
      this.pitchenv = new Env();
      srcp = this.pitchenv.decode(src, srcp);
      if (!this.pitchenv.points.length && (this.pitchenv.inivlo === 0.5) && (this.pitchenv.inivhi === 0.5)) {
        this.pitchenv = null;
      } else {
        this.pitchenv.bias(-0.5);
        this.pitchenv.scale(32768.0);
      }
    }
    if (srcp < src.length) {
      srcp = this.rangeenv.decode(src, srcp);
      this.rangeenv.scale(rangePeak);
    } else {
      this.rangeenv.setConstant(rangePeak);
    }
  }
  
  oscillateFm(ctx, frequency, when, velocity, dur) {
    const options = {
      frequency,
      type: "sine",
    };
    const osc = new OscillatorNode(ctx, options);
    
    const modulator = new OscillatorNode(ctx, {
      frequency: frequency * this.rate,
      type: "sine",
    });
    modulator.start();//TODO Confirm it's ok that we're not stopping or disconnecting this.
    
    const modgain = new GainNode(ctx, { gain: frequency });
    modgain.gain.setValueAtTime(frequency, 0);
    modulator.connect(modgain);
    const iter = this.rangeenv.apply(velocity, when, dur);
    modgain.gain.setValueAtTime(iter[0].v * frequency, 0);
    modgain.gain.setValueAtTime(iter[0].v * frequency, iter[0].t);
    for (let i=1; i<iter.length; i++) {
      modgain.gain.linearRampToValueAtTime(iter[i].v * frequency, iter[i].t);
    }
    modgain.connect(osc.frequency);
    
    if (this.pitchenv) {
      const plan = this.pitchenv.apply(velocity, when, dur);
      osc.detune.setValueAtTime(plan[0].v, 0);
      modulator.detune.setValueAtTime(plan[0].v, 0);
      for (const pt of plan) {
        osc.detune.linearRampToValueAtTime(pt.v, pt.t);
        modulator.detune.linearRampToValueAtTime(pt.v, pt.t);
      }
    }
    
    return [osc, osc];
  }
  
  decodeSub(src, ctx) {
    this.subWidth = 100;
    this.levelenv = new Env();
    let srcp = this.levelenv.decode(src, 0);
    this.levelenv.scale(this.trim);
    if (srcp < src.length) {
      this.subWidth = (src[srcp] << 8) | src[srcp+1];
      srcp += 2;
    }
    const samplec = 16000;
    const samplev = new Float32Array(samplec);
    for (let i=samplec; i-->0; ) samplev[i] = Math.random() * 2 - 1;
    this.noise = new AudioBuffer({
      length: samplec,
      numberOfChannels: 1,
      sampleRate: ctx.sampleRate,
      channelCount: 1,
    });
    this.noise.copyToChannel(samplev, 0);
  }
  
  oscillateSub(ctx, frequency, when, velocity, dur) {
    const noiseNode = new AudioBufferSourceNode(ctx, { buffer: this.noise, channelCount: 1, loop: true });
    const Q = 1;//((65536 - this.subWidth) * 2) / ctx.sampleRate; //TODO Sane calculation for Q.
    const filter = new BiquadFilterNode(ctx, { frequency, Q, type: "bandpass" });
    noiseNode.connect(filter);
    return [noiseNode, filter];
  }
}
