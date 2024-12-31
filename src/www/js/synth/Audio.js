/* Audio.js
 * Top level of our synthesizer.
 */
 
import { Rom } from "../Rom.js";
import { SynthFormats } from "./SynthFormats.js";
import { Song } from "./Song.js";
import { Channel } from "./Channel.js";

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
    this.songNode = null; // GainNode, if current song is PCM.
    this.globalTrim = 0.333;
    this.liveChannels = [];
    
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
    for (const channel of this.liveChannels) channel?.cancel(this.ctx);
    this.liveChannels = [];
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
      if (this.song.finished && (this.song.nextEventTime <= this.ctx.currentTime)) {
        this.song.cancel(this.ctx);
        this.song = null;
        this.songid = 0;
      }
    }
  }
  
  playSong(serial, repeat, fullTrim) {
    this.endSong();
    if (!serial?.length) return;
    this.songid = 0x10000; // Assume it's an external song. Caller can replace this after.
    const format = SynthFormats.detectFormat(serial);
    switch (format) {
      case "egs": {
          this.song = new Song(serial, repeat, this.ctx, fullTrim ? 1.0 : this.globalTrim);
        } break;
      case "wav": {
          const decoded = this.decodeSound(serial);
          if (!decoded) {
            this.songid = 0;
          } else if (decoded instanceof Promise) {
            decoded.then(buffer => {
              const nodes = this.playAudioBuffer(buffer, 1, 0, this.songid);
              if (repeat) nodes.sourceNode.loop = true;
              this.songBuffer = buffer;
              this.songBufferNode = nodes.sourceNode;
              this.songNode = nodes.endNode;
              this.songBufferStartTime = this.ctx.currentTime;
              this.songBufferLength = decoded.duration;
            });
          } else if (decoded instanceof AudioBuffer) {
            const nodes = this.playAudioBuffer(decoded, 1, 0, this.songid);
            if (repeat) nodes.sourceNode.loop = true;
            this.songBuffer = decoded;
            this.songBufferNode = nodes.sourceNode;
            this.songNode = nodes.endNode;
            this.songBufferStartTime = this.ctx.currentTime;
            this.songBufferLength = decoded.duration;
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
    if (this.songNode) {
      const oldNode = this.songNode;
      oldNode.gain.setValueAtTime(this.songNode.gain.value, this.ctx.currentTime);
      oldNode.gain.linearRampToValueAtTime(0, this.ctx.currentTime + 0.250);
      window.setTimeout(() => oldNode.disconnect(), 250);
      this.songNode = null;
    }
    this.songBufferNode = null;
    this.songid = 0;
  }
  
  setLiveChannel(chcfg) {
    if (!chcfg) return;
    const chid = chcfg.chid;
    if ((typeof(chid) !== "number") || (chid < 0) || (chid >= 0x10)) return;
    while (chid >= this.liveChannels.length) this.liveChannels.push(null);
    let channel = this.liveChannels[chid];
    if (channel) {
      channel.cancelHard(this.ctx);
    }
    channel = new Channel(chcfg, this.ctx, this.ctx.destination, 1.0/*this.globalTrim*/);
    this.liveChannels[chid] = channel;
  }
  
  /* Platform entry points.
   ****************************************************************************/
  
  egg_play_sound(rid) {
    if (!this.ctx) return;
    const snd = this.acquireSound(rid);
    if (!snd) return;
    if (snd instanceof Promise) snd.then(buffer => this.playAudioBuffer(buffer, 1, 0));
    else this.playAudioBuffer(snd, 1, 0);
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
    if (!this.ctx) return;
    if ((chid >= 0) && (chid < 0x10)) {
      while (chid >= this.liveChannels.length) this.liveChannels.push(null);
      let channel = this.liveChannels[chid];
      if (!channel) {
        const cfg = this.configureLiveChannel(chid);
        if (!cfg) return;
        channel = this.liveChannels[chid] = new Channel(cfg, this.ctx, this.ctx.destination, this.globalTrim);
      }
      switch (opcode) {
        case 0x89: channel.playNote(this.ctx, this.ctx.currentTime, a, b / 127.0, durms); break;
        case 0x80: channel.endNote(this.ctx, this.ctx.currentTime, a, b / 127.0); break;
        case 0x90: channel.beginNote(this.ctx, this.ctx.currentTime, a, b / 127.0); break;
      }
    } else switch (opcode) {
      case 0xff: {
          for (const channel of this.liveChannels) channel?.cancelHard(this.ctx);
        } break;
    }
  }
  
  egg_audio_get_playhead() {
    if (!this.ctx) return 0.0;
    if (this.song) {
      const t = this.ctx.currentTime - this.song.startTime;
      if (t <= 0.0) return 0.001;
      return t;
    }
    if (this.songBufferNode) {
      let t = this.ctx.currentTime - this.songBufferStartTime;
      t %= this.songBufferLength;
      if (t <= 0.0) return 0.001;
      return t;
    }
    return 0.0;
  }
  
  egg_audio_set_playhead(s) {
    if (!this.ctx) return;
    if (s < 0) s = 0;
    if (this.song) {
      this.song.setPlayhead(this.ctx, s);
    }
    if (this.songBufferNode) {
      const repeat = this.songBufferNode.loop;
      const oldNode = this.songNode;
      oldNode.gain.setValueAtTime(this.songNode.gain.value, this.ctx.currentTime);
      oldNode.gain.linearRampToValueAtTime(0, this.ctx.currentTime + 0.250);
      window.setTimeout(() => oldNode.disconnect(), 250);
      const nodes = this.playAudioBuffer(this.songBuffer, 0.5, 0, this.songid, s);
      if (repeat) nodes.sourceNode.loop = true;
      this.songBufferNode = nodes.sourceNode;
      this.songNode = nodes.endNode;
      this.songBufferStartTime = this.ctx.currentTime - s;
    }
  }
  
  /* Private.
   **************************************************************************/
   
  configureLiveChannel(chid) {
    // This shouldn't actually happen. The source should call setLiveChannel() before sending events.
    const trim = 0x80;
    const mode = 2; // WAVE
    const v = [
      0x06, // levelenv (velocity+sustain)
        1, // sustain index
        3, // pointc
        0x20,0x40,0x00, 0x10,0xf0,0x00,
        0x30,0x10,0x00, 0x30,0x30,0x00,
        0x60,0x00,0x00, 0x81,0x00,0x00,0x00,
      3, // shape (3=saw)
    ];
    return { chid, trim, mode, v };
  }
   
  // AudioBuffer, null, or Promise<AudioBuffer>
  acquireSound(rid) {
    let soundsp = this.searchSounds(rid);
    if (soundsp >= 0) return this.finishSoundEntry(this.sounds[soundsp]);
    soundsp = -soundsp - 1;
    const serial = this.rt.rom.getResource(Rom.TID_sounds, rid);
    if (serial) {
      this.installSound(serial, soundsp, rid);
      soundsp = this.searchSounds(rid);
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
    durs += 0.500; // Add half a second to allow for tails beyond the last delay.
    const samplec = Math.max(1, Math.min(this.pcmLimit, Math.ceil(durs * this.ctx.sampleRate)));
    const subctx = new OfflineAudioContext(1, samplec, this.ctx.sampleRate);
    const song = new Song(egs, false, subctx, 1.0);
    song.update(subctx, subctx.currentTime + durs + 1.0);
    return subctx.startRendering();
  }
  
  /* Start playing this AudioBuffer on the main context.
   */
  playAudioBuffer(buffer, trim, when, songid, skip) {
    const sourceNode = new AudioBufferSourceNode(this.ctx, {
      buffer,
      channelCount: 1,
    });
    const gainNode = new GainNode(this.ctx, { gain: trim });
    sourceNode.connect(gainNode);
    gainNode.connect(this.ctx.destination);
    sourceNode.start(when, skip);
    if (songid) {
      sourceNode.addEventListener("ended", () => {
        if (this.songid === songid) {
          this.songid = 0;
        }
      });
    }
    return { sourceNode, endNode: gainNode };
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
