/* Audio.js
 */
 
import { Rom } from "../Rom.js";
import { SynthFormats } from "./SynthFormats.js";
 
export class Audio {
  constructor(rt) {
    this.rt = rt;
    this.autoplayPolicy = navigator?.getAutoplayPolicy?.("audiocontext");
    this.ctx = null;
    
    /* (sounds) is populated lazy, as sound effects get asked for.
     * Since the resources all contain multiple sounds, we install entire resources at once,
     * but don't decode the individual members.
     * So an entry in (sounds) with (serial) present needs to be decoded first.
     */
    this.sounds = []; // {rid,p,serial?:Uint8Array,snd?:AudioBuffer}
  }
  
  /* API for Runtime.
   ***************************************************************************/
  
  start() {
    this.ctx = null;
    if (!window.AudioContext) return;
    this.ctx = new AudioContext({
      latencyHint: "interactive",
    });
    if (this.ctx.state === "suspended") {
      this.ctx.resume();
    }
  }
  
  stop() {
    if (!this.ctx) return;
    if (this.ctx.state === "running") {
      this.ctx.suspend();
    }
  }
  
  /* Platform entry points.
   ****************************************************************************/
  
  egg_play_sound(rid, p) {
    if (!this.ctx) return;
    const snd = this.acquireSound(rid, p);
    if (!snd) return;
    this.playAudioBuffer(snd, 0.5, 0.0, 0);
  }
  
  egg_play_song(rid, force, repeat) {
    console.log(`TODO Audio.egg_play_song rid=${rid} force=${force} repeat=${repeat}`);
    if (!this.ctx) return;
  }
  
  egg_audio_event(chid, opcode, a, b, durms) {
    console.log(`TODO Audio.egg_audio_event(${chid}, ${opcode}, ${a}, ${b}, ${durms})`);
    if (!this.ctx) return;
  }
  
  egg_audio_get_playhead() {
    console.log(`TODO Audio.egg_audio_get_playhead`);
    if (!this.ctx) return 0.0;
    return 0.0;
  }
  
  egg_audio_set_playhead(s) {
    console.log(`TODO Audio.egg_audio_set_playhead(${s})`);
    if (!this.ctx) return;
  }
  
  /* Private.
   **************************************************************************/
   
  acquireSound(rid, p) {
    let soundsp = this.searchSounds(rid, p);
    if (soundsp >= 0) return this.finishSoundEntry(this.sounds[soundsp]);
    soundsp = -soundsp - 1;
    const serial = this.rt.rom.getResource(Rom.TID_sounds, rid);
    if (serial) {
      this.installSounds(serial, soundsp, rid);
      soundsp = this.searchSounds(rid, p);
      if (soundsp >= 0) return this.finishSoundEntry(this.sounds[soundsp]);
      soundsp = -soundsp - 1;
    }
    this.sounds.splice(soundsp, 0, { rid, p, snd: null });
    return null;
  }
  
  searchSounds(rid, p) {
    let lo=0, hi=this.sounds.length;
    while (lo < hi) {
      const ck = (lo + hi) >> 1;
      const q = this.sounds[ck];
           if (rid < q.rid) hi = ck;
      else if (rid > q.rid) lo = ck + 1;
      else if (p < q.p) hi = ck;
      else if (p > q.p) lo = ck + 1;
      else return ck;
    }
    return -lo - 1;
  }
  
  finishSoundEntry(entry) {
    if (entry.serial) {
      entry.snd = this.decodeSound(entry.serial);
      delete entry.serial;
    }
    return entry.snd;
  }
  
  installSounds(serial, soundsp, rid) {
    SynthFormats.forEach(serial, (subid, subserial) => {
      this.sounds.splice(soundsp++, 0, { rid, p: subid, serial: subserial });
    });
  }
  
  decodeSound(serial, p) {
    switch (SynthFormats.detectFormat(serial)) {
      case "egs": return this.decodeEgs(serial);
      case "msf": return null; // MSF embedded in MSF is illegal.
      case "wav": return SynthFormats.decodeWav(serial);
      //default: console.log(`unknown sound format ${JSON.stringify(SynthFormats.detectFormat(serial))}`);
    }
    return null;
  }
  
  decodeEgs(serial) {
    const egs = SynthFormats.splitEgs(serial);
    console.log(`TODO Audio.decodeEgs`, egs);
    // return AudioBuffer
    return null;
  }
  
  playAudioBuffer(buffer, trim, pan, when) {
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
    if (pan !== 0.0) {
      const panNode = new StereoPannerNode(this.ctx, { pan });
      endNode.connect(panNode);
      endNode = panNode;
    }
    endNode.connect(this.ctx.destination);
    sourceNode.start(when);
  }
}
