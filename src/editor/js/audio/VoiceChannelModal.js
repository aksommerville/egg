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
  }
  
  setup(channel, song) {
    this.channel = { ...channel };
    this.song = song;
    this.buildUi();
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
  }
  
  buildUiWave(parent) {
    let vp = 0;
    this.levelenv = this.dom.spawnController(parent, EnvUi);
    vp = this.levelenv.setup(this.channel.v, vp, "level");
    this.shape = this.dom.spawnController(parent, WaveShapeUi);
    vp = this.shape.setup(this.channel.v, vp);
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
    const table = this.dom.spawn(parent, "TABLE");
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
    const table = this.dom.spawn(parent, "TABLE");
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
    this.buildModeSpecific();
  }
  
  onSubmit() {
    this.resolve(this.encode());
  }
}
