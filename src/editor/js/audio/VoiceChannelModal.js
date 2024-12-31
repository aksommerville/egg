/* VoiceChannelModal.js
 * For WAVE, FM, and SUB channels.
 */
 
import { Dom } from "../Dom.js";
import { AudioService } from "./AudioService.js";
import { Song } from "./Song.js";
import { EnvUi } from "./EnvUi.js";
import { WaveShapeUi } from "./WaveShapeUi.js";
import { Encoder } from "../Encoder.js";

export class VoiceChannelModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, AudioService, "nonce"];
  }
  constructor(element, dom, audioService, nonce) {
    this.element = element;
    this.dom = dom;
    this.audioService = audioService;
    this.nonce = nonce;
    
    this.channel = null;
    this.song = null;
    this.levelenv = null; // EnvUi
    this.pitchenv = null;
    this.rangeenv = null;
    this.shape = null; // WaveShapeUi
    this.dirty = false;
    
    this.audioService.setOutputMode("client");
    this.midiListener = this.audioService.listenMidi((serial, devid) => this.onMidiIn(serial, devid));
  }
  
  onRemoveFromDom() {
    this.audioService.unlistenMidi(this.midiListener);
    this.midiListener = null;
  }
  
  setup(channel, song) {
    this.channel = { ...channel };
    this.song = song;
    this.buildUi();
    if (this.audioService.audio) {
      this.audioService.audio.setLiveChannel({ ...this.channel, chid: 0 });
      this.dirty = false;
    }
  }
  
  /* Build UI.
   ******************************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", ["title"], this.channel.name ? `${this.channel.chid}: ${this.channel.name}` : `Channel ${this.channel.chid}`);
    
    // Only (WAVE,FM,SUB) are available here. User can select other modes from SongEditor.
    // It's safe to add other values here, but there won't be any further UI around them.
    // Likewise, it's safe to use this editor with an OOB mode.
    const modeBarName = `VoiceChannelModal-${this.nonce}-mode`;
    const modeBar = this.dom.spawn(this.element, "DIV", ["radioBar", "mode"], { "on-change": () => this.onModeChanged() });
    for (const [value, label] of [
      [2, "Wave"],
      [3, "FM"],
      [4, "Sub"],
    ]) {
      this.dom.spawn(modeBar, "INPUT", { type: "radio", name: modeBarName, value, id: modeBarName + "-" + value });
      this.dom.spawn(modeBar, "LABEL", { for: modeBarName + "-" + value }, label);
    }
    const modeRadio = modeBar.querySelector(`input[value='${this.channel.mode}']`);
    if (modeRadio) modeRadio.checked = true;
    
    this.dom.spawn(this.element, "DIV", ["modeSpecific"]);
    this.buildModeSpecific();

    const controls = this.dom.spawn(this.element, "DIV", ["controls"]);
    this.dom.spawn(controls, "INPUT", { type: "button", value: "OK", "on-click": () => this.onSubmit() });
  }
  
  buildModeSpecific() {
    const parent = this.element.querySelector(".modeSpecific");
    parent.innerHTML = "";
    this.levelenv = this.pitchenv = this.rangeenv = null;
    this.shape = null;
    const mode = +this.element.querySelector(".radioBar.mode input:checked")?.value;
    switch (mode) {
      case 2: this.buildUiWave(parent); break;
      case 3: this.buildUiFm(parent); break;
      case 4: this.buildUiSub(parent); break;
    }
    const tzmax = Math.max(this.levelenv?.vbox.tz || 0, this.pitchenv?.vbox.tz || 0, this.rangeenv?.vbox.tz || 0);
    const initEnv = (e, others) => {
      if (!e) return;
      e.ondirty = () => { this.dirty = true; };
      e.setTimeRange(0, tzmax);
      e.listenTimeRange((ta, tz) => {
        for (const o of others) if (o) o.setTimeRange(ta, tz);
      });
    };
    initEnv(this.levelenv, [this.pitchenv, this.rangeenv]);
    initEnv(this.pitchenv, [this.levelenv, this.rangeenv]);
    initEnv(this.rangeenv, [this.levelenv, this.pitchenv]);
  }
  
  buildUiWave(parent) {
    let vp = 0;
    this.levelenv = this.dom.spawnController(parent, EnvUi);
    vp = this.levelenv.setup(this.channel.v, vp, "level");
    this.shape = this.dom.spawnController(parent, WaveShapeUi);
    vp = this.shape.setup(this.channel.v, vp);
    this.shape.ondirty = () => { this.dirty = true; }
    this.pitchenv = this.dom.spawnController(parent, EnvUi);
    vp = this.pitchenv.setup(this.channel.v, vp, "pitch");
    if (vp < this.channel.v.length) console.log(`VoiceChannelModal.buildUiWave, decode ended at ${vp}/${this.channel.v.length}`);
  }
  
  buildUiFm(parent) {
    let vp = 0;
    this.levelenv = this.dom.spawnController(parent, EnvUi);
    vp = this.levelenv.setup(this.channel.v, vp, "level");
    
    let rate=0, range=0;
    if (vp <= this.channel.v.length - 2) {
      rate = this.channel.v[vp] + this.channel.v[vp+1] / 256.0;
      vp += 2;
    }
    if (vp <= this.channel.v.length - 2) {
      range = this.channel.v[vp] + this.channel.v[vp+1] / 256.0;
      vp += 2;
    }
    const table = this.dom.spawn(parent, "TABLE", { "on-change": () => { this.dirty = true; }});
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", "Rate"),
      this.dom.spawn(null, "TD",
        this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 256, step: 1/256, name: "rate", value: rate })
      )
    );
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", "Range"),
      this.dom.spawn(null, "TD",
        this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 256, step: 1/256, name: "range", value: range })
      )
    );
    
    this.pitchenv = this.dom.spawnController(parent, EnvUi);
    vp = this.pitchenv.setup(this.channel.v, vp, "pitch");
    this.rangeenv = this.dom.spawnController(parent, EnvUi);
    vp = this.rangeenv.setup(this.channel.v, vp, "range");
    if (vp < this.channel.v.length) console.log(`VoiceChannelModal.buildUiFm, decode ended at ${vp}/${this.channel.v.length}`);
  }
  
  buildUiSub(parent) {
    let vp = 0;
    this.levelenv = this.dom.spawnController(parent, EnvUi);
    vp = this.levelenv.setup(this.channel.v, vp, "level");
    
    let width=0;
    if (vp <= this.channel.v.length - 2) {
      width = (this.channel.v[vp] << 8) | this.channel.v[vp+1];
      vp += 2;
    }
    const table = this.dom.spawn(parent, "TABLE", { "on-change": () => { this.dirty = true; }});
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", "Width, hz"),
      this.dom.spawn(null, "TD",
        this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 65535, name: "width", value: width })
      )
    );
    
    if (vp < this.channel.v.length) console.log(`VoiceChannelModal.buildUiSub, decode ended at ${vp}/${this.channel.v.length}`);
  }
  
  /* Model.
   ****************************************************************************************/
  
  // Returns a copy of (this.channel) with (v) rebuilt from our UI.
  encode() {
    const dst = new Encoder();
    const mode = +this.element.querySelector(".radioBar.mode input:checked")?.value;
    switch (mode) {
    
      case 2: {
          this.levelenv.encode(dst);
          this.shape.encode(dst);
          this.pitchenv.encode(dst);
        } break;
        
      case 3: {
          this.levelenv.encode(dst);
          dst.u16be(this.element.querySelector("input[name='rate']")?.value * 256);
          dst.u16be(this.element.querySelector("input[name='range']")?.value * 256);
          this.pitchenv.encode(dst);
          this.rangeenv.encode(dst);
        } break;
        
      case 4: {
          this.levelenv.encode(dst);
          dst.u16be(this.element.querySelector("input[name='width']")?.value);
        } break;
    }
    return {
      ...this.channel,
      v: dst.finish(),
    };
  }
  
  /* Events.
   ********************************************************************************************/
   
  onModeChanged() {
    const mode = +this.element.querySelector(".radioBar.mode input:checked")?.value;
    switch (mode) {
      case 2: this.channel.v = new Uint8Array(Song.DEFAULT_WAVE_CONFIG); break;
      case 3: this.channel.v = new Uint8Array(Song.DEFAULT_FM_CONFIG); break;
      case 4: this.channel.v = new Uint8Array(Song.DEFAULT_SUB_CONFIG); break;
      default: this.channel.v = new Uint8Array(0);
    }
    this.dirty = true;
    this.buildModeSpecific();
  }
  
  onSubmit() {
    this.resolve(this.encode());
  }
  
  onMidiIn(serial, devid) {
    if (!this.audioService.audio) return;
    if (serial.length < 1) return;
    if ((serial[0] >= 0x80) && (serial[0] < 0xf0)) {
      const chid = 0; // (serial[0]&0x0f) if we want to respect the bus. But zero is the channel we configured.
      if (this.dirty) {
        const chcfg = { ...this.encode(), chid };
        this.audioService.audio.setLiveChannel(chcfg);
        this.dirty = false;
      }
      this.audioService.audio.egg_audio_event(chid, serial[0] & 0xf0, serial[1] || 0, serial[2] || 0, 0);
    } else switch (serial[0]) {
      case 0xff: this.audioService.audio.egg_audio_event(0xff, 0xff, 0, 0, 0); break;
      //TODO Other Realtime events?
    }
  }
}
