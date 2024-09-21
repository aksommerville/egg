/* Channel.js
 * Distinct PCM stream fed by an event stream that might be talking to other Channels too.
 * Each Channel has a fixed configuration at startup.
 */
 
import { SynthFormats } from "./SynthFormats.js";
import { Env } from "./Env.js";

const SYNTH_GLOBAL_TRIM = 0.250;
 
export class Channel {
  constructor(audio, ctx, hdr, dst, soundsConstraint) {
    this.audio = audio;
    this.ctx = ctx;
    this.dst = dst || ctx.destination; // Gets replaced by head of post, if applicable.
    this.soundsConstraint = soundsConstraint;
    this.postTail = null;
    this.voiceMode = "silent"; // "silent" "drums" "sub" "wave" "fm"
    this.init(hdr);
  }
  
  /* Public API for consumption by Audio or Bus.
   *************************************************************************/
  
  isDummy() {
    return false;
  }
  
  isFinished() {
    return false;
  }
  
  install() {
  }
  
  uninstall() {
    if (this.postTail) {
      this.postTail.disconnect();
      this.postTail = null;
    }
  }
  
  // (v) in -1..1
  setWheel(v, when) {
    console.log(`TODO Channel.setWheel v=${v} when=${when}`);
  }
  
  // (noteid) in 0..127, (velocity) in 0..1, (dur,when) in s.
  playNote(noteid, velocity, dur, when) {
    //console.log(`TODO Channel.playNote noteid=${noteid} velocity=${velocity} dur=${dur} when=${when} voiceMode=${this.voiceMode}`);
    if (!when) when = this.ctx.currentTime;
    
    // Drums do their own thing.
    if (this.voiceMode === "drums") {
      return this.playNoteDrums(noteid, velocity, when);
    }
    
    /* All other voice modes take a frequency in Hz and a generic envelope.
     */
    const frequency = SynthFormats.frequencyForMidiNote(noteid);
    let osc = null;
    switch (this.voiceMode) {
      case "sub": osc = this.oscillateSub(frequency); break;
      case "wave": osc = this.oscillateWave(frequency, velocity, dur, when); break;
      case "fm": osc = this.oscillateFm(frequency, velocity, dur, when); break;
    }
    if (!osc) return;
    const tail = this.applyLevel(osc, this.levelenv, velocity, dur, when);
    tail.connect(this.dst);
  }
  
  /* Private: Voicing.
   ************************************************************************/
   
  playNoteDrums(noteid, velocity, when) {
    const index = noteid + this.drumsBias;
    if (index < 1) return;
    const trim = this.drumsLo * (1 - velocity) + this.drumsHi * velocity;
    if (trim <= 0) return;
    const snd = this.audio.acquireSound(this.drumsRid, index);
    if (!snd) return;
    if (snd instanceof Promise) snd.then(b => this.playSound(b, trim, when));
    else this.playSound(snd, trim, when);
  }
  
  playSound(buffer, trim, when) {
    const sourceNode = new AudioBufferSourceNode(this.ctx, {
      buffer,
      channelCount: 1,
    });
    let endNode = sourceNode;
    if (trim !== 1.0) {
      const gainNode = new GainNode(this.ctx, { gain: trim });
      endNode.connect(gainNode);
      endNode = gainNode;
    }
    if (this.pan !== 0.0) {
      const panNode = new StereoPannerNode(this.ctx, { pan: this.pan });
      endNode.connect(panNode);
      endNode = panNode;
    }
    endNode.connect(this.dst);
    sourceNode.start(when);
  }
  
  oscillateSub(frequency) {
    console.log(`TODO Channel.oscillateSub freq=${frequency} width=${this.subWidth}`);
    return null; // AudioNode
  }
  
  oscillateWave(frequency, velocity, dur, when) {
    const options = { frequency };
    if (this.wave instanceof PeriodicWave) {
      options.type = "custom";
      options.periodicWave = this.wave;
    } else {
      options.type = this.wave;
    }
    const osc = new OscillatorNode(this.ctx, options);
    //TODO pitchenv
    //TODO pitchlfo
    //TODO wheel
    osc.start();
    return osc;
  }
  
  oscillateFm(frequency, velocity, dur, when) {
    const options = { frequency };
    if (this.wave instanceof PeriodicWave) {
      options.type = "custom";
      options.periodicWave = this.wave;
    } else {
      options.type = this.wave;
    }
    const osc = new OscillatorNode(this.ctx, options);
    
    const modulator = new OscillatorNode(this.ctx, {
      frequency: frequency * this.fmRate,
      type: "sine",
    });
    modulator.start();
    const modgain = new GainNode(this.ctx, { gain: frequency * this.fmRange });
    modulator.connect(modgain);
    let modtail = modgain;
    
    if (this.fmenv) {
      const peak = frequency * this.fmRange;
      const iter = this.fmenv.apply(velocity, dur, when);
      modgain.gain.setValueAtTime(iter[0].value * peak, 0);
      modgain.gain.setValueAtTime(iter[0].value * peak, iter[0].time);
      for (let i=1; i<iter.length; i++) {
        modgain.gain.linearRampToValueAtTime(iter[i].value * peak, iter[i].time);
      }
    }

    //TODO fmlfo
    //TODO pitchenv
    //TODO pitchlfo
    //TODO wheel
    
    modtail.connect(osc.frequency);
    osc.start();
    return osc;
  }
  
  applyLevel(osc, env, velocity, dur, when) {
    const gainNode = new GainNode(this.ctx);
    const points = env.apply(velocity, dur, when);
    gainNode.gain.setValueAtTime(points[0].value, 0);
    gainNode.gain.setValueAtTime(points[0].value, points[0].time);
    for (let i=1; i<points.length; i++) {
      const pt = points[i];
      gainNode.gain.linearRampToValueAtTime(pt.value, pt.time);
    }
    osc.connect(gainNode);
    const endTime = points[points.length - 1].time;
    const durationFromNow = endTime - this.ctx.currentTime;
    if (1) setTimeout(() => {
      gainNode.disconnect();
      osc.stop();
    }, durationFromNow * 1000 + 100);
    return gainNode;
  }
  
  /* Private: Configure.
   *********************************************************************/
   
  init(hdr) {
    console.log(`TODO new Channel with header`, hdr);
    
    /* First, read the Channel Header.
     * Collect all the state fields in (config), and build up the final post pipe.
     */
    const config = this.defaultConfig();
    SynthFormats.forEachChannelHeaderField(hdr, (opcode, body) => {
      switch (opcode) {

        case 0x01: this.setCMaster(config, body); break;
        case 0x02: this.setCPan(config, body); break;
        case 0x03: this.setCDrums(config, body); break;
        case 0x04: this.setCWheel(config, body); break;
        case 0x05: this.setCSub(config, body); break;
        case 0x06: this.setCShape(config, body); break;
        case 0x07: this.setCHarmonics(config, body); break;
        case 0x08: this.setCFm(config, body); break;
        case 0x09: this.setCFmenv(config, body); break;
        case 0x0a: this.setCFmlfo(config, body); break;
        case 0x0b: this.setCPitchenv(config, body); break;
        case 0x0c: this.setCPitchlfo(config, body); break;
        case 0x0d: this.setCLevel(config, body); break;
        
        case 0x80: this.addPGain(config, body); break;
        case 0x81: this.addPWaveshaper(config, body); break;
        case 0x82: this.addPDelay(config, body); break;
        case 0x84: this.addPTremolo(config, body); break;
        case 0x85: this.addPLopass(config, body); break;
        case 0x86: this.addPHipass(config, body); break;
        case 0x87: this.addPBpass(config, body); break;
        case 0x88: this.addPNotch(config, body); break;
        
        default: {
            //console.log(`Channel.init, unknown opcode ${opcode} with ${body.length}-byte payload.`);
            if (((opcode >= 0x60) && (opcode <= 0x7f)) || ((opcode >= 0xe0) && (opcode <= 0xff))) {
              throw new Error(`Unknown 'critical' channel op ${opcode}`);
            }
          }
      }
    });
    
    /* If master and pan are applicable, apply them to the post pipe.
     * The native synthesizer that we mimic does this differently, in some cases it bakes master and pan into the individual voices.
     * That's a win for performance on the native side, but in WebAudio I'm not so sure. So I'm doing the simpler thing.
     */
    if (!this.soundsConstraint) {
      config.master *= SYNTH_GLOBAL_TRIM;
    }
    if (config.master !== 1.0) {
      this.addPMaster(config);
    }
    if (config.pan && (this.ctx.destination.channelCount >= 2)) {
      this.addPPan(config);
    }
    
    /* Attach post pipe if present. This applies to all voice modes, and it's perfectly orthogonal.
     */
    if (config.postHead && config.postTail) {
      config.postTail.connect(this.dst);
      this.dst = config.postHead;
      this.postTail = config.postTail;
    }
    
    /* Drum channels are their own thing, and ignore most of the config.
     */
    if (config.drumsRid) {
      this.voiceMode = "drums";
      this.drumsRid = config.drumsRid;
      this.drumsBias = config.drumsBias;
      this.drumsLo = config.drumsLo;
      this.drumsHi = config.drumsHi;
      return;
    }
    
    /* All other voice modes accept "wheel" and "level".
     */
    this.wheelRange = config.wheelRange;
    this.levelenv = config.levelenv || new Env();
    
    /* Subtractive voices, that's it for config.
     */
    if (config.subWidth) {
      this.voiceMode = "sub";
      this.subWidth = config.subWidth;
      return;
    }
    
    /* FM and Wave voices, far and away the likeliest two, share a bunch of voicing properties.
     */
    this.wave = this.generateWave(config.shape, config.harmonics);
    if (config.pitchlfoPeriod) {
      //TODO Pitch LFO.
    }
    if (config.pitchenv) this.pitchenv = config.pitchenv;
    this.voiceMode = "wave";
    
    /* And finally, if FM rate and range are nonzero, separate FM from Wave.
     */
    if (!config.fmRate && !config.fmRange) return;
    this.voiceMode = "fm";
    this.fmRate = config.fmRate;
    this.fmRange = config.fmRange;
    if (config.fmlfoPeriod) {
      //TODO FM LFO.
    }
    if (config.fmenv) this.fmenv = config.fmenv;
  }
  
  defaultConfig() {
    return {
      postHead: null, // AudioNode
      postTail: null, // AudioNode
      master: 0.5, // 0..1
      pan: 0, // -1..1
      wheelRange: 200, // cents
      shape: 0, // 0..3 = sine,square,saw,triangle
      // Unset by default:
      //drumsRid: u16
      //drumsBias: s8
      //drumsLo: 0..1
      //drumsHi: 0..1
      //subWidth: hz
      //harmonics: (0..1)[]
      //fmRate
      //fmRange
      //fmlfoPeriod: s
      //fmlfoDepth: 0..1
      //fmlfoPhase: 0..1
      //pitchlfoPeriod: s
      //pitchlfoDepth: cents
      //pitchlfoPhase: 0..1
      //fmenv: Env
      //pitchenv: Env
      //levelenv: Env
    };
  }
  
  setCMaster(config, s) { // master (u0.8)
    config.master = (s[0] || 0) / 255.0;
  }
  
  setCPan(config, s) { // pan (s0.8)
    config.pan = ((s[0] || 0) - 0x80) / 128.0;
  }
  
  setCDrums(config, s) { // drums (u16 rid,s8 bias,u0.8 trimlo,u0.8 trimhi)
    config.drumsRid = (s[0] << 8) | s[1];
    config.drumsBias = s[2] || 0;
    if (config.drumsBias & 0x80) config.drumsBias |= ~0xff;
    config.drumsLo = (s[3] || 0) / 255.0;
    config.drumsHi = (s[4] || 0) / 255.0;
  }
  
  setCWheel(config, s) { // wheel (u16 cents)
    config.wheelRange = (s[0] << 8) | s[1];
  }
  
  setCSub(config, s) { // sub (u16 width(hz))
    config.subWidth = (s[0] << 8) | s[1];
  }
  
  setCShape(config, s) { // shape (u8: 0,1,2,3 = sine,square,saw,triangle)
    config.shape = s[0] || 0;
  }
  
  setCHarmonics(config, s) { // harmonics (u0.16...)
    const coefc = s.length >> 1;
    config.harmonics = [];
    for (let sp=0, i=coefc; i-->0; sp+=2) {
      config.harmonics.push(((s[sp] << 8) | s[sp+1]) / 65535.0);
    }
  }
  
  setCFm(config, s) { // fm (u8.8 rate,u8.8 range)
    config.fmRate = ((s[0] << 8) | s[1]) / 256.0;
    config.fmRange = ((s[2] << 8) | s[3]) / 256.0;
  }
  
  setCFmenv(config, s) { // fmenv (env...)
    config.fmenv = new Env(s);
  }
  
  setCFmlfo(config, s) { // fmlfo (u16 ms,u0.16 depth,u0.8 phase)
    config.fmlfoPeriod = ((s[0] << 8) | s[1]) / 1000.0;
    config.fmlfoDepth = ((s[2] << 8) | s[3]) / 65535.0;
    config.fmlfoPhase = (s[4] || 0) / 256.0;
  }
  
  setCPitchenv(config, s) { // pitchenv (env...)
    config.pitchenv = new Env(s);
  }
  
  setCPitchlfo(config, s) { // pitchlfo (u16 ms,u16 cents,u0.8 phase)
    config.pitchlfoPeriod = ((s[0] << 8) | s[1]) / 1000.0;
    config.pitchlfoDepth = (s[2] << 8) | s[3];
    config.pitchlfoPhase = (s[4] | 0) / 256.0;
  }
  
  setCLevel(config, s) { // level (env...)
    config.levelenv = new Env(s);
  }
  
  addPMaster(config) {
    const gainNode = new GainNode(this.ctx, { gain: config.master });
    if (config.postHead) gainNode.connect(config.postHead);
    else config.postTail = gainNode;
    config.postHead = gainNode;
  }
  
  addPPan(config) {
    const panNode = new StereoPannerNode(this.ctx, { pan: config.pan });
    if (config.postHead) panNode.connect(config.postHead);
    else config.postTail = panNode;
    config.postHead = panNode;
  }
  
  addPGain(config, s) { // gain (u8.8 gain,u0.8 clip,u0.8 gate)
    //TODO
  }
  
  addPWaveshaper(config, s) { // waveshaper (s0.16...)
    //TODO
  }
  
  addPDelay(config, s) { // delay (u16 ms,u0.8 dry,u0.8 wet,u0.8 store,u0.8 feedback)
    //TODO
  }
  
  addPTremolo(config, s) { // tremolo (u16 ms,u0.16 depth,u0.8 phase)
    //TODO
  }
  
  addPLopass(config, s) { // lopass (u16 hz)
    //TODO
  }
  
  addPHipass(config, s) { // hipass (u16 hz)
    //TODO
  }
  
  addPBpass(config, s) { // bpass (u16 mid,u16 wid)
    //TODO
  }
  
  addPNotch(config, s) { // notch (u16 mid,u16 wid)
    //TODO
  }
  
  /* Private: Generate wave.
   ***************************************************************/
   
  generateWave(shape, harmonics) {
    console.log(`TODO Channel.generateWave`, { shape, harmonics });
    if (harmonics) {
      let hc = harmonics.length;
      while (hc && (harmonics[hc - 1] <= 0)) hc--;
      if ((hc !== 1) || (harmonics[0] !== 1)) {
        if (shape) {
          console.warning(`Channel.generateWave: Harmonics with non-sine primitive not yet supported. Ignoring 'shape' ${shape}`);//TODO
        }
        const real = new Float32Array(1 + hc); // Params to PeriodWave include a DC term; inputs do not.
        const imag = new Float32Array(1 + hc);
        for (let i=0; i<hc; i++) real[1 + i] = harmonics[i];
        return new PeriodicWave(this.ctx, { real, imag, disableNormalization: true });
      }
    }
    switch (shape) {
      case 0: return "sine";
      case 1: return "square";
      case 2: return "sawtooth";
      case 3: return "triangle";
    }
    return "sine";
  }
}
