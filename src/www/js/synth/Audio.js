/* Audio.js
 * Top level of our synthesizer.
 * Most of the magic happens in Channel.js.
 */
 
import { Rom } from "../Rom.js";
import { SynthFormats } from "./SynthFormats.js";
import { Channel } from "./Channel.js";
import { Bus } from "./Bus.js";
 
export class Audio {
  constructor(rt) {
    this.rt = rt;
    this.autoplayPolicy = navigator?.getAutoplayPolicy?.("audiocontext");
    this.ctx = null;
    this.songs = []; // Bus. There can be multiple; one song can fade out while the next one begins.
    this.newSongDelay = 0.500;
    this.fadeTimeNormal = 1.000;
    this.fadeTimeQuick = 0.250;
    this.noise = null; // AudioBuffer containing one second of random noise, created on demand.
    
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
    this.pcmLimit = this.ctx.sampleRate * 5; // Printed PCM dumps are restricted to 5 seconds play time.
  }
  
  stop() {
    if (!this.ctx) return;
    if (this.ctx.state === "running") {
      this.ctx.suspend();
    }
  }
  
  /* Signal playback runs without intervention.
   * Songs, we schedule events a few seconds in advance.
   * Runtime should update us at least once per second.
   * Once per video frame is overkill but no problem (and that's what we do).
   */
  update() {
    for (let i=this.songs.length; i-->0; ) {
      const song = this.songs[i];
      if (!song.update()) {
        this.songs.splice(i, 1);
        song.stop();
      }
    }
  }
  
  /* Platform entry points.
   ****************************************************************************/
  
  egg_play_sound(rid, p) {
    if (!this.ctx) return;
    const snd = this.acquireSound(rid, p);
    if (!snd) return;
    if (snd instanceof Promise) snd.then(buffer => this.playAudioBuffer(buffer, 0.5, 0.0, 0));
    else this.playAudioBuffer(snd, 0.5, 0.0, 0);
  }
  
  egg_play_song(rid, force, repeat) {
    if (!this.ctx) return;
    
    // Acquire resource. If missing, force rid to zero, because all silence is alike.
    let serial = this.rt.rom.getResource(Rom.TID_song, rid);
    if (!serial?.length) serial = null;
    if (!serial) rid = 0;
    
    /* If we're not using the force, check for already-playing songs.
     * If it's already playing in the foreground, great, we're done.
     * If it's playing but cancelled, uncancel it and cancel whatever's in the foreground.
     */
    if (!force) {
      for (const song of this.songs) {
        if (song.rid !== rid) continue;
        if (song.isCancelled()) {
          for (let cancel of this.songs) cancel.cancel(this.fadeTimeQuick);
          song.uncancel();
          return;
        } else {
          return;
        }
      }
    }
    
    /* Cancel everything currently playing.
     * And if the incoming song is silence, we're done.
     */
    for (const song of this.songs) song.cancel(this.fadeTimeNormal);
    if (!serial) return;
    
    /* Create, install, and start a new Bus for this song.
     * If any other songs are still running, defer startup by a tasteful interval.
     */
    const song = new Bus(this, serial, rid, repeat);
    song.start(this.songs.length ? this.newSongDelay : 0);
    this.songs.push(song);
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
   
  // AudioBuffer, null, or Promise<AudioBuffer>
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
      const result = this.decodeSound(entry.serial);
      delete entry.serial;
      if (result instanceof Promise) {
        return result.then((buffer) => {
          return entry.snd = buffer;
        });
      } else {
        entry.snd = result;
      }
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
    let samplec = 0;
    for (let iter=SynthFormats.iterateEgsEvents(egs.events), event; event=iter.next(); ) {
      if (event.type === "delay") samplec += event.delay;
    }
    samplec = Math.max(1, Math.min(this.pcmLimit, Math.ceil(samplec * this.ctx.sampleRate)));
    const subctx = new OfflineAudioContext(1, samplec, this.ctx.sampleRate);
    this.playEgs(subctx, egs);
    return subctx.startRendering();
  }
  
  /* Start playing this AudioBuffer on the main context.
   */
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
  
  /* Given a split EGS resource (see SynthFormats.splitEgs), start playing it in the provided context.
   * This can be the main context, or an OfflineAudioContext for recording.
   * Bus is not involved, because it pays out playback over time and we want to commit every event immediately.
   */
  playEgs(ctx, egs) {
    const channels = egs.channels.map(chhdr => {
      const channel = new Channel(this, ctx, chhdr, ctx.destination, true);
      if (channel.isDummy()) return;
      channel.install();
      return channel;
    });
    let when = 0;
    for (let iter=SynthFormats.iterateEgsEvents(egs.events), event; event=iter.next(); ) {
      switch (event.type) {
        case "delay": when += event.delay; break;
        case "wheel": channels[event.chid].setWheel(event.wheel, when); break;
        case "note": channels[event.chid].playNote(event.noteid, event.velocity, event.dur, when); break;
      }
    }
  }
  
  getNoise() {
    if (!this.noise) {
      const fv = new Float32Array(this.ctx.sampleRate);
      for (let i=fv.length; i-->0; ) fv[i] = Math.random() * 2 - 1;
      console.log(`fv`, fv);
      this.noise = new AudioBuffer({
        length: fv.length,
        numberOfChannels: 1,
        sampleRate: this.ctx.sampleRate,
        channelCount: 1,
      });
      this.noise.copyToChannel(fv, 0);
    }
    return this.noise;
  }
}
