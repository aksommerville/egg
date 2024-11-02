/* SongEditor.js
 * Accepts MIDI and EGS formats.
 */
 
import { Dom } from "../Dom.js";
import { AudioService } from "./AudioService.js";
import { Song } from "./Song.js";
import { Data } from "../Data.js";

export class SongEditor {
  static getDependencies() {
    return [HTMLElement, Dom, AudioService, Data, "nonce"];
  }
  constructor(element, dom, audioService, data, nonce) {
    this.element = element;
    this.dom = dom;
    this.audioService = audioService;
    this.data = data;
    this.nonce = nonce;
    
    this.res = null;
    this.song = null;
    
    this.CHANNEL_COLORS = [
      "#1e2e62", "#1e6258", "#5e4c08", "#5e0822",
      "#3c4357", "#3c5753", "#4f482c", "#4f2c36",
      "#235171", "#237149", "#71360a", "#710a4e",
      "#3f4c55", "#3f554a", "#4c3b2f", "#4c2f42",
    ];
  }
  
  static checkResource(res) {
    const b = res.serial;
    if (b.length >= 4) {
      if ((b[0] === 0x00) && (b[1] === 0x45) && (b[2] === 0x47) && (b[3] === 0x53)) return 2;
      if ((b[0] === 0x4d) && (b[1] === 0x54) && (b[2] === 0x68) && (b[3] === 0x64)) return 2;
    }
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.song = new Song(res.serial);
    this.buildUi();
  }
  
  /* Presentation.
   *****************************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    if (!this.song) return;
    
    const globals = this.dom.spawn(this.element, "DIV", ["globals"]);
    this.dom.spawn(globals, "SELECT", ["operations"], { "on-change": () => this.onOperation() },
      this.dom.spawn(null, "OPTION", { value: "", disabled: "disabled", selected: "selected" }, "Operations..."),
      ...this.listOperations().map(op => this.dom.spawn(null, "OPTION", { value: op }, op))
    );
    
    const player = this.dom.spawn(globals, "DIV", ["player"]);
    this.dom.spawn(player, "INPUT", { type: "button", value: ">", "on-click": () => this.onPlay() });
    this.dom.spawn(player, "INPUT", { type: "button", value: "!!", "on-click": () => this.onStop() });
    
    const visibility = this.dom.spawn(globals, "DIV", ["visibility"]);
    this.dom.spawn(visibility, "SELECT", ["trackVisibility"], { "on-change": () => this.onVisibilityChanged() },
      this.dom.spawn(null, "OPTION", { value: "-1" }, "All tracks"),
      ...this.song.getTrackIds().map(trackid => this.dom.spawn(null, "OPTION", { value: trackid }, `Track ${trackid}`))
    );
    this.dom.spawn(visibility, "SELECT", ["channelVisibility"], { "on-change": () => this.onVisibilityChanged() },
      this.dom.spawn(null, "OPTION", { value: "-1" }, "All channels"),
      ...this.song.getChannelIds().map(chid => this.dom.spawn(null, "OPTION", { value: chid }, `Channel ${chid}`))
    );
    this.dom.spawn(visibility, "SELECT", ["eventVisibility"], { "on-change": () => this.onVisibilityChanged() },
      this.dom.spawn(null, "OPTION", { value: "all" }, "All events"),
      this.dom.spawn(null, "OPTION", { value: "note" }, "Notes"),
      this.dom.spawn(null, "OPTION", { value: "config" }, "Config"),
      this.dom.spawn(null, "OPTION", { value: "adjust" }, "Adjustment"),
    );
    
    const channels = this.dom.spawn(this.element, "DIV", ["channels"]);
    for (const channel of this.song.channels) {
      if (!channel) continue;
      const card = this.dom.spawn(channels, "DIV", ["channel"], { "data-chid": channel.chid });
      this.populateChannelCard(card, channel);
    }
    
    const events = this.dom.spawn(this.element, "DIV", ["events"]);
    this.dom.spawn(events, "TABLE");
    this.rebuildEventsTable(false);
  }
  
  populateChannelCard(card, channel) {
    card.innerHTML = "";
    if ((channel.chid >= 0) && (channel.chid < 0x10)) card.style.backgroundColor = this.CHANNEL_COLORS[channel.chid];
    
    const idBase = `SongEditor-${this.nonce}-ch${channel.chid}`;
    const controls = this.dom.spawn(card, "DIV", ["controls"]);
    this.dom.spawn(controls, "INPUT", ["pushButton"], { type: "checkbox", name: "mute", id: idBase + "-mute", "data-chid": channel.chid });
    this.dom.spawn(controls, "LABEL", { for: idBase + "-mute" }, "M");
    this.dom.spawn(controls, "INPUT", ["pushButton"], { type: "checkbox", name: "solo", id: idBase + "-solo", "data-chid": channel.chid });
    this.dom.spawn(controls, "LABEL", { for: idBase + "-solo" }, "S");
    this.dom.spawn(controls, "DIV", ["spacer"]);
    this.dom.spawn(controls, "INPUT", { type: "button", value: "X", "on-click": () => this.onDeleteChannel(channel.chid) });
    
    this.dom.spawn(card, "DIV", ["title"], { "on-click": () => this.onEditChannelTitle(channel.chid) }, channel.name ? `${channel.chid}: ${channel.name}` : `Channel ${channel.chid}`);
    
    const trim = channel.hasOwnProperty("trim") ? channel.trim : 128;
    this.dom.spawn(card, "DIV", ["trimRow"],
      this.dom.spawn(null, "INPUT", { type: "range", min: 0, max: 255, value: trim, name: `trim-${channel.chid}`, "on-input": () => this.onTrimChanged(channel.chid) }),
      this.dom.spawn(null, "DIV", ["trimTattle"], { "data-chid": channel.chid }, trim)
    );
    
    const pid = channel.pid || 0;
    const pidElementId = `SongEditor-${this.nonce}-ch${channel.chid}-pid`;
    this.dom.spawn(card, "DIV", ["pidRow"],
      this.dom.spawn(null, "INPUT", { id: pidElementId, name: "pid", type: "number", min: 0, max: 127, value: pid, name: `pid-${channel.chid}`, "on-input": () => this.onPidChanged(channel.chid) }),
      this.dom.spawn(null, "DIV", ["pidTattle"], { "data-chid": channel.chid }, Song.GM_PROGRAM_NAMES[pid] || ""),
    );
    
    let modeSelect;
    this.dom.spawn(card, "DIV", ["modeRow"],
      modeSelect = this.dom.spawn(null, "SELECT", { name: `mode-${channel.chid}`, "on-change": () => this.onModeChanged(channel.chid) },
        this.dom.spawn(null, "OPTION", { value: "-1" }, "GM"),
        this.dom.spawn(null, "OPTION", { value: "0" }, "Noop"),
        this.dom.spawn(null, "OPTION", { value: "1" }, "Drum"),
        this.dom.spawn(null, "OPTION", { value: "2" }, "Wave"),
        this.dom.spawn(null, "OPTION", { value: "3" }, "FM"),
        this.dom.spawn(null, "OPTION", { value: "4" }, "Sub"),
      ),
      this.dom.spawn(null, "INPUT", { type: "button", value: "Edit...", "on-click": () => this.onEditChannelModeConfig(channel.chid) }),
    );
    modeSelect.value = channel.hasOwnProperty("mode") ? channel.mode : -1;
  }
  
  rebuildEventsTable(evenIfHuge) {
    const trackVisibility = +this.element.querySelector("select.trackVisibility")?.value;
    const channelVisibility = +this.element.querySelector("select.channelVisibility")?.value;
    const eventVisibility = this.element.querySelector("select.eventVisibility")?.value;
    const table = this.element.querySelector(".events > table");
    table.innerHTML = "";
    
    const events = this.song.events.filter(event => {
      if ((trackVisibility >= 0) && (event.trackid !== trackVisibility)) return false;
      if ((channelVisibility >= 0) && (event.chid !== channelVisibility)) return false;
      switch (eventVisibility) {
        case "all": break;
        case "note": if (!this.song.isNoteEvent(event)) return false; break;
        case "config": if (!this.song.isConfigEvent(event)) return false; break;
        case "adjust": if (!this.song.isAdjustEvent(event)) return false; break;
      }
      return true;
    });
    
    if (evenIfHuge || (events.length < 1000)) {
      for (const event of events) {
        const tr = this.dom.spawn(table, "TR", ["event"]);
        if ((event.chid >= 0) && (event.chid < 0x10)) tr.style.backgroundColor = this.CHANNEL_COLORS[event.chid];
        this.populateEventRow(tr, event);
      }
      
    } else {
      this.dom.spawn(table, "TR",
        this.dom.spawn(null, "TD", ["hugeTableWarning"],
          this.dom.spawn(null, "INPUT", { type: "button", value: `Click to show ${events.length} events`, "on-click": () => this.rebuildEventsTable(true) })
        )
      );
    }
  }
  
  populateEventRow(tr, event) {
    this.dom.spawn(tr, "TD", ["controls"],
      this.dom.spawn(null, "INPUT", { type: "button", value: "X", "on-click": () => this.onDeleteEvent(event.id) }),
      this.dom.spawn(null, "INPUT", { type: "button", value: "^", "on-click": () => this.onMoveEvent(event.id, -1) }),
      this.dom.spawn(null, "INPUT", { type: "button", value: "v", "on-click": () => this.onMoveEvent(event.id, 1) }),
    );
    this.dom.spawn(tr, "TD", ["time"], event.time);
    this.dom.spawn(tr, "TD", ["desc"], this.reprEvent(event), { "on-click": () => this.onEditEvent(event.id) });
  }
  
  reprEvent(event) {
    let dst = "";
    if ((event.chid >= 0) && (event.chid < 0x10)) {
      if (event.chid < 10) dst += " ";
      else dst += "1";
      dst += (event.chid % 10).toString();
    } else {
      dst += "--";
    }
    let addSerial = false;
    if (event.eopcode) switch (event.eopcode) {
      case 0x80: case 0x90: case 0xa0: dst += ` NTE ${event.noteid.toString(16).padStart(2, '0')} v=${event.velocity.toString(16).padStart(2, '0')}`; break;
      default: dst += " FUT"; addSerial = true; break;
    } else switch (event.mopcode) {
      case 0x80: dst += ` OFF ${event.noteid.toString(16).padStart(2, '0')} v=${event.velocity.toString(16).padStart(2, '0')}`; break;
      case 0x90: dst += `  ON ${event.noteid.toString(16).padStart(2, '0')} v=${event.velocity.toString(16).padStart(2, '0')}`; break;
      case 0xa0: dst += ` ADJ ${event.noteid.toString(16).padStart(2, '0')} v=${event.velocity.toString(16).padStart(2, '0')}`; break;
      case 0xb0: dst += ` CTL ${event.k.toString(16).padStart(2, '0')}=${event.v.toString(16).padStart(2, '0')}`; break;
      case 0xc0: dst += ` PGM ${event.pid.toString(16).padStart(2, '0')}`; break;
      case 0xd0: dst += ` PRS ${event.velocity.toString(16).padStart(2, '0')}`; break;
      case 0xe0: dst += ` WHL ${event.wheel}`; break;
      case 0xf0: case 0xf7: dst += " SSX"; addSerial = true; break;
      case 0xff: dst += ` MET ${event.k.toString(16).padStart(2, '0')} =`; addSerial = true; break;
      default: dst += " ???"; addSerial = true; break;
    }
    if (addSerial && event.v?.length) {
      let src = event.v;
      const limit = 12;
      let elided = false;
      if (src.length > limit) {
        src = src.slice(0, limit);
        elided = true;
      }
      for (let i=0; i<src.length; i++) {
        dst += " " + src[i].toString(16).padStart(2, '0');
      }
      if (elided) dst += "...";
    }
    return dst;
  }
  
  /* Model and communication.
   ********************************************************************************/
   
  dirty() {
    if (!this.res || !this.song) return;
    this.data.dirty(this.res.path, () => this.song.encode());
  }
  
  /* Modify (this.song) to accord with our Mute and Solo buttons.
   * Returns the original (this.song.channels) -- caller must restore those after encoding.
   */
  applyTransientChannelFocus() {
    const oldChannels = this.song.channels;
    const mute = Array.from(this.element.querySelectorAll(".channel input[name='mute']:checked")).map(e => +e.getAttribute("data-chid")).filter(v => !isNaN(v));
    const solo = Array.from(this.element.querySelectorAll(".channel input[name='solo']:checked")).map(e => +e.getAttribute("data-chid")).filter(v => !isNaN(v));
    const newChannels = this.song.channels.map(c => (c ? {...c} : null));
    
    /* The basic idea:
     *   1: If at least one channel has Solo, we only play channels that have it.
     *   2: Any channel that has Mute is eliminated.
     * So if you select *both* Solo and Mute on one channel, and nothing else, all playback is muted.
     */
    if (solo.length) { // Eliminate any channels not in (solo)...
      for (const channel of newChannels) {
        if (!channel) continue;
        if (solo.includes(channel.chid)) continue;
        channel.mode = 0;
      }
    }
    for (const chid of mute) { // ...then eliminate channels in (mute).
      const channel = newChannels[chid];
      if (!channel) continue;
      channel.mode = 0;
    }
    
    this.song.channels = newChannels;
    return oldChannels;
  }
  
  /* UI events.
   ****************************************************************************/
   
  onPlay() {
    if (!this.song) return;
    if (this.audioService.outputMode === "none") {
      if (this.audioService.serverAvailable) this.audioService.setOutputMode("server");
      else this.audioService.setOutputMode("client");
    }
    const oldChannels = this.applyTransientChannelFocus();
    const serial = this.song.encode();
    this.song.channels = oldChannels;
    this.audioService.play(serial, 0/*position*/, 0/*repeat*/).then(() => {
    }).catch(e => this.dom.modalError(e));
  }
  
  onStop() {
    this.audioService.stop();
  }
  
  onVisibilityChanged() {
    this.rebuildEventsTable(false);
  }
  
  onDeleteChannel(chid) {
    console.log(`SongEditor.onDeleteChannel ${chid}`);
  }
  
  onEditChannelTitle(chid) {
    console.log(`SongEditor.onEditChannelTitle ${chid}`);
  }
  
  onTrimChanged(chid) {
    const tattle = this.element.querySelector(`.trimTattle[data-chid='${chid}']`);
    if (!tattle) return;
    const trim = +this.element.querySelector(`input[name='trim-${chid}']`)?.value;
    if (isNaN(trim)) {
      tattle.innerText = "";
    } else {
      tattle.innerText = trim;
      const channel = this.song?.channels[chid];
      if (channel) {
        channel.trim = trim;
        this.dirty();
      }
    }
  }
  
  onPidChanged(chid) {
    const tattle = this.element.querySelector(`.pidTattle[data-chid='${chid}']`);
    if (!tattle) return;
    const pid = +this.element.querySelector(`input[name='pid-${chid}']`)?.value;
    if (isNaN(pid)) {
      tattle.innerText = "";
    } else {
      tattle.innerText = Song.GM_PROGRAM_NAMES[pid] || "";
      const channel = this.song?.channels[chid];
      if (channel) {
        channel.pid = pid;
        this.dirty();
      }
    }
  }
  
  onModeChanged(chid) {
    const mode = +this.element.querySelector(`select[name='mode-${chid}']`)?.value;
    if (isNaN(mode)) return;
    const channel = this.song?.channels[chid];
    if (!channel) return;
    if (channel.mode === mode) return;
    channel.mode = mode;
    channel.v = new Uint8Array(0);//TODO Don't just chuck it
    this.dirty();
  }
  
  onEditChannelModeConfig(chid) {
    console.log(`SongEditor.onEditChannelModeConfig ${chid}`);
  }
  
  onDeleteEvent(id) {
    console.log(`SongEditor.onDeleteEvent ${id}`);
  }
  
  onMoveEvent(id, d) {
    console.log(`SongEditor.onMoveEvent ${id} ${d}`);
  }
  
  onEditEvent(id) {
    console.log(`SongEditor.onEditEvent ${id}`);
  }
  
  /* Global operations.
   * Any method on this class whose name starts "op_" will appear in the Operations menu automatically.
   *****************************************************************************/
   
  listOperations() {
    return Object.getOwnPropertyNames(this.__proto__).filter(v => v.startsWith("op_")).map(v => v.substring(3));
  }
  
  onOperation() {
    const select = this.element.querySelector("select.operations");
    if (!select) return;
    const op = select.value;
    select.value = "";
    const fn = this["op_" + op];
    if (!fn) return;
    fn();
  }
  
  op_trimLeadingSilence() {
    console.log(`SongEditor.op_trimLeadingSilence`);
  }
  
  op_autoEndTime() {
    console.log(`SongEditor.op_autoEndTime`);
  }
  
  op_reassignChannels() {
    console.log(`SongEditor.op_reassignChannels`);
  }
  
  op_removeUselessEvents() {
    console.log(`SongEditor.op_removeUselessEvents`);
  }
}
