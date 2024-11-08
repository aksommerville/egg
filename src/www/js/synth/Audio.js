/* Audio.js
 * Top level of our synthesizer.
 */
 
import { Rom } from "../Rom.js";
import { SynthFormats } from "./SynthFormats.js";
import { Song } from "./Song.js";

const AUDIO_FUTURE_INTERVAL = 2.000;
 
export class Audio {
  constructor(rt) {
    if (!Audio.discriminator) Audio.discriminator = 1;
    this.discriminator = Audio.discriminator++;
    this.rt = rt;
    this.autoplayPolicy = navigator?.getAutoplayPolicy?.("audiocontext");
    this.ctx = null;
    this.noise = null; // AudioBuffer containing one second of random noise, created on demand.
    this.pcmLimit = 0; // Maximum length of sound effect, gets populated during start().
    this.songid = 0;
    this.song = null;
    
    /* (sounds) is populated lazy, as sound effects get asked for.
     * So an entry in (sounds) with (serial) present needs to be decoded first.
     */
    this.sounds = []; // {rid,serial?:Uint8Array,snd?:AudioBuffer}
  }
  
  /* API for Runtime.
   ***************************************************************************/
  
  start() {
    if (!window.AudioContext) {
      this.ctx = null;
      return;
    }
    if (!this.ctx) {
      this.ctx = new AudioContext({
        latencyHint: "interactive",
      });
    }
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
    if (!this.ctx) return;
    if (this.song) {
      this.song.update(this.ctx, this.ctx.currentTime + AUDIO_FUTURE_INTERVAL);
    }
  }
  
  playSong(serial, repeat) {
    this.endSong();
    if (!serial?.length) return;
    this.songid = 0x10000; // Assume it's an external song. Caller can replace this after.
    const format = SynthFormats.detectFormat(serial);
    switch (format) {
      case "egs": {
          this.song = new Song(serial, repeat, this.ctx);
        } break;
      case "wav": {
          const decoded = this.decodeSound(serial);
          if (!decoded) {
            this.songid = 0;
          } else if (decoded instanceof Promise) {
            decoded.then(buffer => this.playAudioBuffer(buffer, 0.5, 0, this.songid));
          } else if (decoded instanceof AudioBuffer) {
            this.playAudioBuffer(decoded, 0.5, 0, this.songid);
          } else {
            this.songid = 0;
          }
        } break;
      default: {
          console.log(`Audio.playSong: Unknown format ${JSON.stringify(format)}`);
        }
    }
  }
  
  endSong() {
    if (this.song) {
      this.song.cancel(this.ctx);
      this.song = null;
    }
    this.songid = 0;
  }
  
  /* Platform entry points.
   ****************************************************************************/
  
  egg_play_sound(rid) {
    if (!this.ctx) return;
    const snd = this.acquireSound(rid);
    if (!snd) return;
    if (snd instanceof Promise) snd.then(buffer => this.playAudioBuffer(buffer, 0.5, 0));
    else this.playAudioBuffer(snd, 0.5, 0);
  }
  
  egg_play_song(rid, force, repeat) {
    if (!this.ctx) return;
    
    // Acquire resource. If missing, force rid to zero, because all silence is alike.
    let serial = this.rt.rom.getResource(Rom.TID_song, rid);
    if (!serial?.length) serial = null;
    if (!serial) rid = 0;
    
    /* If we're not using the force and this one is already playing, get out and let it ride.
     */
    if (!force) {
      if (rid === this.songid) return;
    }
    
    this.playSong(serial, repeat);
    this.songid = rid;
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
  acquireSound(rid) {
    let soundsp = this.searchSounds(rid);
    if (soundsp >= 0) return this.finishSoundEntry(this.sounds[soundsp]);
    soundsp = -soundsp - 1;
    const serial = this.rt.rom.getResource(Rom.TID_sounds, rid);
    if (serial) {
      this.installSound(serial, soundsp, rid);
      soundsp = this.searchSounds(rid, p);
      if (soundsp >= 0) return this.finishSoundEntry(this.sounds[soundsp]);
      soundsp = -soundsp - 1;
    }
    this.sounds.splice(soundsp, 0, { rid, snd: null });
    return null;
  }
  
  searchSounds(rid) {
    let lo=0, hi=this.sounds.length;
    while (lo < hi) {
      const ck = (lo + hi) >> 1;
      const q = this.sounds[ck];
           if (rid < q.rid) hi = ck;
      else if (rid > q.rid) lo = ck + 1;
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
  
  installSound(serial, soundsp, rid) {
    this.sounds.splice(soundsp, 0, { rid, serial });
  }
  
  // => null | AudioBuffer | Promise<AudioBuffer>
  decodeSound(serial) {
    switch (SynthFormats.detectFormat(serial)) {
      case "egs": return this.decodeEgs(serial);
      case "wav": return SynthFormats.decodeWav(serial);
      default: console.log(`unknown sound format ${JSON.stringify(SynthFormats.detectFormat(serial))}`);
    }
    return null;
  }
  
  /* Return a Promise that resolves to an AudioBuffer containing
   * a print of the given encoded EGS song.
   */
  decodeEgs(serial) {
    const egs = SynthFormats.splitEgs(serial);
    let durs = 0;
    for (let iter=SynthFormats.iterateEgsEvents(egs.events), event; event=iter.next(); ) {
      if (event.type === "delay") durs += event.delay;
    }
    const samplec = Math.max(1, Math.min(this.pcmLimit, Math.ceil(durs * this.ctx.sampleRate)));
    const subctx = new OfflineAudioContext(1, samplec, this.ctx.sampleRate);
    const song = new Song(egs, false, subctx);
    song.update(subctx, subctx.currentTime + durs + 1.0);
    return subctx.startRendering();
  }
  
  /* Start playing this AudioBuffer on the main context.
   */
  playAudioBuffer(buffer, trim, when, songid) {
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
    endNode.connect(this.ctx.destination);
    sourceNode.start(when);
    if (songid) {
      sourceNode.addEventListener("ended", () => {
        if (this.songid === songid) {
          this.songid = 0;
        }
      });
    }
  }
  
  getNoise() {
    if (!this.noise) {
      const fv = new Float32Array(this.ctx.sampleRate);
      for (let i=fv.length; i-->0; ) fv[i] = Math.random() * 2 - 1;
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
