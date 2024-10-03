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
    this.wheelRaw = 0; // -1..1, straight off the event stream. (which is normalized before it reaches us)
    this.wheelRange = 0; // cents, established at config
    this.wheel = 1; // Multiplier, updates on wheel events.
    this.pitchLfoOsc = null;
    this.pitchLfoTail = null; // Emits cents if not null.
    this.fmLfoOsc = null;
    this.fmLfoTail = null; // Emits 0..1 if not null.
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
    if (this.pitchLfoOsc) {
      this.pitchLfoOsc.stop();
      this.pitchLfoTail.disconnect();
      this.pitchLfoOsc = null;
      this.pitchLfoTail = null;
    }
    if (this.fmLfoOsc) {
      this.fmLfoOsc.stop();
      this.fmLfoTail.disconnect();
      this.fmLfoOsc = null;
      this.fmLfoTail = null;
    }
  }
  
  // (v) in -1..1
  setWheel(v, when) {
    console.log(`TODO Channel.setWheel v=${v} when=${when}`);
    v = Math.min(1.0, Math.max(-1.0, v));
    if (v === this.wheelRaw) return;
    this.wheelRaw = v;
    if (!this.wheelRange) return;
    this.wheel = Math.pow(2, (v * this.wheelRange) / 1200.0);
    //TODO Update running voices.
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
    const stoppables = [];
    switch (this.voiceMode) {
      case "sub": osc = this.oscillateSub(frequency, stoppables); break;
      case "wave": osc = this.oscillateWave(frequency, velocity, dur, when, stoppables); break;
      case "fm": osc = this.oscillateFm(frequency, velocity, dur, when, stoppables); break;
    }
    if (!osc) return;
    const tail = this.applyLevel(osc, this.levelenv, velocity, dur, when, stoppables);
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
  
  oscillateSub(frequency, stoppables) {
    frequency *= this.wheel;
    const noise = this.audio.getNoise();
    const noiseNode = new AudioBufferSourceNode(this.ctx, { buffer: noise, channelCount: 1, loop: true });
    const Q = ((65536 - this.subWidth) * 10) / this.ctx.sampleRate; //TODO Sane calculation for Q.
    const filter = new BiquadFilterNode(this.ctx, { frequency, Q, type: "bandpass" });
    noiseNode.connect(filter);
    noiseNode.start();
    stoppables.push([noiseNode, filter]);
    return filter;
  }
  
  oscillateWave(frequency, velocity, dur, when, stoppables) {
    frequency *= this.wheel;
    const options = { frequency };
    if (this.wave instanceof PeriodicWave) {
      options.type = "custom";
      options.periodicWave = this.wave;
    } else {
      options.type = this.wave;
    }
    const osc = new OscillatorNode(this.ctx, options);
    
    if (this.pitchenv) {
      const iter = this.pitchenv.apply(velocity, dur, when);
      this.pitchenv.scaleIterator(iter, 65535.0, -32768.0);
      osc.detune.setValueAtTime(iter[0].value, 0);
      osc.detune.setValueAtTime(iter[0].value, iter[0].time);
      for (let i=1; i<iter.length; i++) {
        osc.detune.linearRampToValueAtTime(iter[i].value, iter[i].time);
      }
    }
    
    if (this.pitchLfoTail) {
      this.pitchLfoTail.connect(osc.detune);
    }
    
    osc.start();
    return osc;
  }
  
  oscillateFm(frequency, velocity, dur, when, stoppables) {
    frequency *= this.wheel;
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
    stoppables.push(modulator);
    
    let modtail = modulator;
    const modgain = new GainNode(this.ctx, { gain: frequency * this.fmRange });
    modgain.gain.setValueAtTime(frequency * this.fmRange, 0);
    if (this.fmLfoTail) {
      const lfoGain = new GainNode(this.ctx, { gain: frequency * this.fmRange });
      this.fmLfoTail.connect(lfoGain);
      lfoGain.connect(modgain.gain);
      modgain.gain.setValueAtTime(0, 0);
    }
    modtail.connect(modgain);
    modtail = modgain;
    
    if (this.fmenv) {
      const peak = frequency * this.fmRange;
      const iter = this.fmenv.apply(velocity, dur, when);
      modgain.gain.setValueAtTime(iter[0].value * peak, 0);
      modgain.gain.setValueAtTime(iter[0].value * peak, iter[0].time);
      for (let i=1; i<iter.length; i++) {
        modgain.gain.linearRampToValueAtTime(iter[i].value * peak, iter[i].time);
      }
    }
    
    if (this.pitchenv) {
      const iter = this.pitchenv.apply(velocity, dur, when);
      this.pitchenv.scaleIterator(iter, 65535.0, -32768.0);
      osc.detune.setValueAtTime(iter[0].value, 0);
      osc.detune.setValueAtTime(iter[0].value, iter[0].time);
      for (let i=1; i<iter.length; i++) {
        osc.detune.linearRampToValueAtTime(iter[i].value, iter[i].time);
      }
    }
    
    if (this.pitchLfoTail) {
      this.pitchLfoTail.connect(osc.detune);
    }
    
    modtail.connect(osc.frequency);
    osc.start();
    return osc;
  }
  
  /* We arrange to stop and remove (osc) at the appropriate time.
   * And we do the same for anything in (stoppables), eg LFOs.
   * (stoppables) can be AudioNode to call stop(), or [Source, Destination] to call disconnect().
   */
  applyLevel(osc, env, velocity, dur, when, stoppables) {
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
    setTimeout(() => { //XXX This shouldn't be necessary -- we can specify the time at AudioNode.stop(), do it where we start().
      for (const node of stoppables) {
        if (node instanceof Array) node[0].disconnect(node[1]);
        else node.stop();
      }
      gainNode.disconnect();
      if (osc.stop) osc.stop();
    }, durationFromNow * 1000 + 100);
    return gainNode;
  }
  
  /* Private: Configure.
   *********************************************************************/
   
  init(hdr) {
    
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
      const { osc, tail } = this.prepareLfo(config.pitchlfoPeriod, -config.pitchlfoDepth, config.pitchlfoDepth, config.pitchlfoPhase);
      this.pitchLfoOsc = osc;
      this.pitchLfoTail = tail;
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
      const { osc, tail } = this.prepareLfo(config.fmlfoPeriod, 1 - config.fmlfoDepth, 1, config.fmlfoPhase);
      this.fmLfoOsc = osc;
      this.fmLfoTail = tail;
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
    if (config.postTail) config.postTail.connect(gainNode);
    else config.postHead = gainNode;
    config.postTail = gainNode;
  }
  
  addPPan(config) {
    const panNode = new StereoPannerNode(this.ctx, { pan: config.pan });
    if (config.postTail) config.postTail.connect(panNode);
    else config.postHead = panNode;
    config.postTail = panNode;
  }
  
  addPGain(config, s) { // gain (u8.8 gain,u0.8 clip,u0.8 gate)
    const gain = ((s[0] << 8) | s[1]) / 256.0;
    const clip = (s[2] || 0) / 255.0;
    const gate = (s[3] || 0) / 255.0;
    const resolution = 20;
    const coefv = new Float32Array(resolution * 2 + 1);
    const step = gain / resolution;
    for (let lop=resolution-1, hip=resolution+1, inv=step; lop>=0; lop--, hip++, inv+=step) {
      const v = (inv < gate) ? 0 : (inv >= clip) ? clip : inv;
      coefv[lop] = -v;
      coefv[hip] = v;
    }
    const ws = new WaveShaperNode(this.ctx, { curve: coefv });
    if (config.postTail) config.postTail.connect(ws);
    else config.postHead = ws;
    config.postTail = ws;
  }
  
  addPWaveshaper(config, s) { // waveshaper (s0.16...)
    const coefc = s.length >> 1;
    if (coefc < 1) return;
    const coefv = new Float32Array(coefc);
    for (let i=0, sp=0; i<coefc; i++, sp+=2) {
      coefv[i] = (((s[sp] << 8) | s[sp+1]) - 0x8000) / 32768.0;
    }
    const ws = new WaveShaperNode(this.ctx, { curve: coefv });
    if (config.postTail) config.postTail.connect(ws);
    else config.postHead = ws;
    config.postTail = ws;
  }
  
  addPDelay(config, s) { // delay (u16 ms,u0.8 dry,u0.8 wet,u0.8 store,u0.8 feedback)
    const ms = (s[0] << 8) | s[1];
    const dry = (s[2] || 0) / 255.0;
    const wet = (s[3] || 0) / 255.0;
    const sto = (s[4] || 0) / 255.0;
    const fbk = (s[5] || 0) / 255.0;
    if (ms < 1) return;
    const delayNode = new DelayNode(this.ctx, { delayTime: ms / 1000 });
    const splitNode = new GainNode(this.ctx, { gain: 1 });
    const dryNode = new GainNode(this.ctx, { gain: dry });
    const wetNode = new GainNode(this.ctx, { gain: wet });
    const stoNode = new GainNode(this.ctx, { gain: sto });
    const fbkNode = new GainNode(this.ctx, { gain: fbk });
    const combineNode = new GainNode(this.ctx, { gain: 1 });
    splitNode.connect(dryNode);
    dryNode.connect(combineNode);
    splitNode.connect(stoNode);
    stoNode.connect(delayNode);
    delayNode.connect(fbkNode);
    fbkNode.connect(delayNode);
    delayNode.connect(wetNode);
    wetNode.connect(combineNode);
    if (config.postTail) config.postTail.connect(splitNode);
    else config.postHead = splitNode;
    config.postTail = combineNode;
  }
  
  addPTremolo(config, s) { // tremolo (u16 ms,u0.16 depth,u0.8 phase)
    const ms = (s[0] << 8) | s[1];
    const depth = ((s[2] << 8) | s[3]) / 65535.0;
    const phase = s[4] / 256.0;
    const { osc, tail } = this.prepareLfo(ms / 1000, 1 - depth, 1, phase);
    const gainNode = new GainNode(this.ctx);
    tail.connect(gainNode.gain);
    if (config.postTail) config.postTail.connect(gainNode);
    else config.postHead = gainNode;
    config.postTail = gainNode;
    //TODO Does (osc) need stopped when we uninstall?
  }
  
  addPLopass(config, s) { // lopass (u16 hz)
    const frequency = (s[0] << 8) | s[1];
    const filter = new BiquadFilterNode(this.ctx, { frequency, Q: 1, type: "lowpass" });
    if (config.postTail) config.postTail.connect(filter);
    else config.postHead = filter;
    config.postTail = filter;
  }
  
  addPHipass(config, s) { // hipass (u16 hz)
    const frequency = (s[0] << 8) | s[1];
    const filter = new BiquadFilterNode(this.ctx, { frequency, Q: 1, type: "highpass" });
    if (config.postTail) config.postTail.connect(filter);
    else config.postHead = filter;
    config.postTail = filter;
  }
  
  addPBpass(config, s) { // bpass (u16 mid,u16 wid)
    const frequency = (s[0] << 8) | s[1];
    const width = (s[2] << 8) | s[3];
    const Q = ((65536 - width) * 10) / this.ctx.sampleRate; //TODO Sane calculation for Q.
    const filter = new BiquadFilterNode(this.ctx, { frequency, Q, type: "bandpass" });
    if (config.postTail) config.postTail.connect(filter);
    else config.postHead = filter;
    config.postTail = filter;
  }
  
  addPNotch(config, s) { // notch (u16 mid,u16 wid)
    const frequency = (s[0] << 8) | s[1];
    const width = (s[2] << 8) | s[3];
    const Q = ((65536 - width) * 10) / this.ctx.sampleRate; //TODO Sane calculation for Q.
    const filter = new BiquadFilterNode(this.ctx, { frequency, Q, type: "notch" });
    if (config.postTail) config.postTail.connect(filter);
    else config.postHead = filter;
    config.postTail = filter;
  }
  
  /* Private: Generate LFO.
   * Returns { osc, tail }, both AudioNodes, installed against the current context and running, but not outputting anywhere.
   *******************************************************************/
   
  prepareLfo(period, lo, hi, phase) {
    phase = 1 - phase;
    const imix =  Math.cos(phase * 2 * Math.PI);
    const rmix = -Math.sin(phase * 2 * Math.PI);
    const real = new Float32Array([(lo + hi) / 2, rmix * (hi - lo) / 2]);
    const imag = new Float32Array([(lo + hi) / 2, imix * (hi - lo) / 2]);
    //console.log(`prepareLfo period=${period} range=${lo}..${hi} phase=${phase} real=[${real[0]},${real[1]}] imag=[${imag[0]},${imag[1]}]`);
    const periodicWave = new PeriodicWave(this.ctx, { real, imag, disableNormalization: true });
    const frequency = 1 / period;
    const osc = new OscillatorNode(this.ctx, { frequency, type: "custom", periodicWave });
    osc.start();
    return { osc, tail: osc };
  }
  
  /* Private: Generate wave.
   ***************************************************************/
   
  generateWave(shape, harmonics) {
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
