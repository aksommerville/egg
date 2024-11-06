/* SongEditor.js
 * Accepts MIDI and EGS formats.
 */
 
import { Dom } from "../Dom.js";
import { AudioService } from "./AudioService.js";
import { Song } from "./Song.js";
import { Data } from "../Data.js";
import { SongEventModal } from "./SongEventModal.js";
import { DrumChannelModal } from "./DrumChannelModal.js";
import { VoiceChannelModal } from "./VoiceChannelModal.js";

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
    
    // "sound" with no rid is probably an embedded drum.
    // Force the song to encode to EGS in that case, even if it's sourced from MIDI.
    if ((res.type === "sound") && !res.rid) {
      this.song.format = "egs";
    }
    
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
      ...this.listOperations().map(({ op, name }) => this.dom.spawn(null, "OPTION", { value: op }, name))
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
    
    this.dom.spawn(globals, "INPUT", { type: "button", value: "+Channel", "on-click": () => this.onAddChannel() });
    this.dom.spawn(globals, "INPUT", { type: "button", value: "+Event", "on-click": () => this.onAddEvent() });
    
    const channels = this.dom.spawn(this.element, "DIV", ["channels"]);
    for (const channel of this.song.channels) {
      if (!channel) continue;
      const card = this.dom.spawn(channels, "DIV", ["channel"], { "data-chid": channel.chid });
      this.populateChannelCard(card, channel);
    }
    
    const events = this.dom.spawn(this.element, "DIV", ["events"]);
    this.dom.spawn(events, "TABLE");
    this.rebuildEventsTable(null);
  }
  
  rebuildChannelCards() {
    const channels = this.element.querySelector(".channels");
    channels.innerHTML = "";
    for (const channel of this.song.channels) {
      if (!channel) continue;
      const card = this.dom.spawn(channels, "DIV", ["channel"], { "data-chid": channel.chid });
      this.populateChannelCard(card, channel);
    }
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
    const table = this.element.querySelector(".events > table");
  
    // (evenIfHuge) can be null to preserve the existing selection.
    if (evenIfHuge === null) {
      if (table.querySelector(".hugeTableWarning")) {
        evenIfHuge = false;
      } else if (table.querySelector("tr.event")) {
        evenIfHuge = true;
      }
    }
    
    const trackVisibility = +this.element.querySelector("select.trackVisibility")?.value;
    const channelVisibility = +this.element.querySelector("select.channelVisibility")?.value;
    const eventVisibility = this.element.querySelector("select.eventVisibility")?.value;
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
        const tr = this.dom.spawn(table, "TR", ["event"], { "data-event-id": event.id });
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
  
  // Returns array of { desc, events }. (events) never empty.
  gatherUselessEvents() {
    const control = [];
    const noteAdjust = [];
    const channelPressure = [];
    const wheel = [];
    const sysex = [];
    const meta = [];
    const egsFuture = [];
    const other = []
    if (this.song) for (const event of this.song.events) {
      switch (event.eopcode) {
        case 0x80: case 0x90: case 0xa0: continue;
        case 0xb0: case 0xc0: case 0xd0: case 0xe0: case 0xf0: {
            egsFuture.push(event);
            continue;
          }
      }
      switch (event.mopcode) {
        case 0x80: continue;
        case 0x90: continue;
        case 0xa0: noteAdjust.push(event); continue;
        case 0xb0: switch (event.k) {
            case 0x07: continue; // Volume.
            default: control.push(event); continue;
          }
        case 0xc0: continue;
        case 0xd0: channelPressure.push(event); continue;
        case 0xe0: wheel.push(event); continue;
        case 0xf0: case 0xf7: sysex.push(event); continue;
        case 0xff: switch (event.k) {
            case 0x04: continue; // Instrument Name.
            case 0x20: continue; // Channel Prefix.
            case 0x2f: continue; // End of Track.
            case 0x51: continue; // Set Tempo.
            case 0x7f: continue; // EGS Header.
            default: meta.push(event); continue;
          }
      }
      other.push(event);
    }
    const useless = [];
    if (control.length) useless.push({ desc: "Unknown Control Change", events: control });
    if (noteAdjust.length) useless.push({ desc: "Note Adjust", events: noteAdjust });
    if (channelPressure.length) useless.push({ desc: "Channel Pressure", events: channelPressure });
    if (wheel.length) useless.push({ desc: "Wheel", events: wheel });
    if (sysex.length) useless.push({ desc: "Sysex", events: sysex });
    if (meta.length) useless.push({ desc: "Unknown Meta", events: meta });
    if (egsFuture.length) useless.push({ desc: "EGS Future Events", events: egsFuture });
    if (other.length) useless.push({ desc: "Unknown", events: other });
    return useless;
  }
  
  calculateIdealEndTime() {
    if (!this.song?.events.length) return 0;
    //TODO For sound effects it's different, and important: Track the envelope of each note, and our ideal end time is just after they all finish.
    // EGS does not record tempo information, so there's nothing we can do with those.
    // I'll generalize that a little: If there's no Set Tempo event, we won't attempt any changes.
    // (of course we know that the default tempo is 500 ms/qnote, but a missing Set Tempo might mean we're EGS).
    let lastEvent = this.song.events[this.song.events.length - 1];
    const setTempo = this.song.events.find(e => ((e.mopcode === 0xff) && (e.k === 0x51) && (e.v?.length === 3)));
    if (!setTempo) return lastEvent.time;
    let lastNoteOnTime = 0;
    for (let i=this.song.events.length; i-->0; ) {
      const event = this.song.events[i];
      if (
        (event.eopcode === 0x80) ||
        (event.eopcode === 0x90) ||
        (event.eopcode === 0xa0) ||
        (event.mopcode === 0x90)
      ) {
        lastNoteOnTime = event.time;
        break;
      }
    }
    const usperqnote = (setTempo.v[0] << 16) | (setTempo.v[1] << 8) | setTempo.v[2];
    // If there's more than 16 qnotes between the last Note On and the last event, assume the EOT is broken and way too far out.
    // This addresses a specific bug in Logic Pro X (Apple says it's not a bug, and I say they have their heads up their asses).
    const finalSilenceQnotes = ((lastEvent.time - lastNoteOnTime) * 1000) / usperqnote;
    if (finalSilenceQnotes > 16) {
      lastEvent = { time: lastNoteOnTime + usperqnote / 1000 };
    }
    // We're not going to bother with Meta Time Signature events. There is such a thing, but I don't use them and don't know how they're formatted.
    // Assume 4 qnotes per measure, and that the ideal end time is on a measure boundary.
    // So there are two candidates for end time.
    // Take the lower if we are within 1 qnote of it, and there's no Note On after it.
    const mspermeasure = Math.round((usperqnote * 4) / 1000);
    const overage = lastEvent.time % mspermeasure;
    if (!overage) return lastEvent.time;
    const lo = lastEvent.time - overage;
    const hi = lo + mspermeasure;
    if ((overage <= usperqnote / 1000) && (lastNoteOnTime < lo)) return lo;
    return hi;
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
  
  onAddChannel() {
    if (!this.song) return;
    let chid = -1;
    for (let ckchid=0; ckchid<16; ckchid++) {
      if (!this.song.channels[ckchid]) {
        chid = ckchid;
        break;
      }
    }
    if (chid < 0) {
      this.dom.modalMessage(`All 16 channels already defined.`);
      return;
    }
    const channel = this.song.defineChannel(chid);
    if (!channel) return;
    this.rebuildChannelCards();
    this.dirty();
  }
  
  onAddEvent() {
    this.onEditEvent("new");
  }
  
  onDeleteChannel(chid) {
    if (!this.song) return;
    const eventc = this.song.events.reduce((a, e) => a + ((e.chid === chid) ? 1 : 0), 0);
    this.dom.modalPickOne(["Cancel", "Delete"], `Delete channel ${chid} and ${eventc} events?`).then(choice => {
      if (choice !== "Delete") return;
      this.song.channels[chid] = null;
      this.song.events = this.song.events.filter(e => e.chid !== chid);
      this.buildUi();
      this.dirty();
    }).catch(e => this.dom.modalError(e));
  }
  
  onEditChannelTitle(chid) {
    const channel = this.song?.channels[chid];
    if (!channel) return;
    this.dom.modalInput(`Name for channel ${chid}:`, channel.name || "").then(name => {
      if (name === null) return; // Exactly null means cancelled. Empty string should be taken literally.
      channel.name = name;
      const card = this.element.querySelector(`.channels .channel[data-chid='${chid}']`);
      if (card) this.populateChannelCard(card, channel);
      this.dirty();
    }).catch(e => this.dom.modalError(e));
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
    if (this.song.changeChannelMode(channel, mode)) {
      this.dirty();
    }
  }
  
  onEditChannelModeConfig(chid) {
    const channel = this.song?.channels[chid];
    if (!channel) return;
    let modal = null;
    switch (channel.mode) {
      case -1: // GM and NOOP, there's nothing else to configure.
      case 0: break;
      case 1: { // DRUM is separate from the rest.
          modal = this.dom.spawnModal(DrumChannelModal);
          modal.setup(channel, this.song);
        } break;
      case 2: case 3: case 4: { // WAVE, FM, and SUB share a modal.
          modal = this.dom.spawnModal(VoiceChannelModal);
          modal.setup(channel, this.song);
        } break;
    }
    if (!modal) return;
    modal.result.then(newChannel => {
      if (!newChannel) return;
      this.song.channels[chid] = newChannel;
      this.rebuildChannelCards();
      this.dirty();
    }).catch(e => this.dom.modalError(e));
  }
  
  onDeleteEvent(id) {
    if (!this.song) return;
    const p = this.song.events.findIndex(e => e.id === id);
    if (p < 0) return;
    this.song.events.splice(p, 1);
    // this.rebuildEventsTable(null) would be the right thing, but that's overkill for a removal.
    const tr = this.element.querySelector(`tr.event[data-event-id='${id}']`);
    if (tr) tr.remove();
    this.dirty();
  }
  
  onMoveEvent(id, d) {
    if (!this.song) return;
    if ((d !== 1) && (d !== -1)) return;
    const p = this.song.events.findIndex(e => e.id === id);
    if (p < 0) return;
    const event = this.song.events[p];
    const neighbor = this.song.events[p + d];
    if (!neighbor) return; // First or last event, moving in the outward direction.
    if (event.time !== neighbor.time) {
      this.dom.modalMessage(`Can't swap event ${event.id} at time ${event.time} with neighbor ${neighbor.id} at different time ${neighbor.time}`);
      return;
    }
    this.song.events[p] = neighbor;
    this.song.events[p + d] = event;
    // this.rebuildEventsTable(null), overkill
    const table = this.element.querySelector(".events > table");
    const eventElement = this.element.querySelector(`tr.event[data-event-id='${event.id}']`);
    const neighborElement = this.element.querySelector(`tr.event[data-event-id='${neighbor.id}']`);
    if (table && eventElement && neighborElement) {
      if (d < 0) {
        table.insertBefore(eventElement, neighborElement);
      } else {
        table.insertBefore(neighborElement, eventElement);
      }
    }
    this.dirty();
  }
  
  onEditEvent(id) {
    let event;
    if (id === "new") {
      if (!this.song) return;
      event = { id: this.song.nextEventId++ };
    } else {
      event = this.song?.events.find(e => e.id === id);
      if (!event) return;
    }
    const modal = this.dom.spawnModal(SongEventModal);
    modal.setup(event);
    modal.result.then(newEvent => {
      if (!newEvent) return;
      let p;
      if (id === "new") {
        p = -1;
      } else {
        p = this.song.events.indexOf(event);
        if (p < 0) return;
      }
      
      // Different approaches to update, depending on whether time changed.
      if (event.time === newEvent.time) {
        if (p >= 0) this.song.events[p] = newEvent;
        const tr = this.element.querySelector(`tr.event[data-event-id='${id}']`);
        if (tr) {
          if ((newEvent.chid >= 0) && (newEvent.chid < 0x10)) tr.style.backgroundColor = this.CHANNEL_COLORS[newEvent.chid];
          else delete tr.style.backgroundColor;
          tr.innerHTML = "";
          this.populateEventRow(tr, newEvent);
        }
      } else {
        if (p >= 0) this.song.events.splice(p, 1);
        this.song.events.push(newEvent);
        this.song.events.sort((a, b) => a.time - b.time);
        this.rebuildEventsTable(true);
      }
      
      this.dirty();
    }).catch(e => this.dom.modalError(e));
  }
  
  /* Global operations.
   * Any method on this class whose name starts "op_" will appear in the Operations menu automatically.
   * If the first argument to that method is "name", return the operation's display name.
   *****************************************************************************/
  
  // => {op,name}[]
  listOperations() {
    return Object.getOwnPropertyNames(this.__proto__).filter(v => v.startsWith("op_")).map(v => ({
      op: v.substring(3),
      name: this[v]("name"),
    }));
  }
  
  onOperation() {
    const select = this.element.querySelector("select.operations");
    if (!select) return;
    const op = select.value;
    select.value = "";
    this["op_" + op]?.();
  }
  
  op_trimLeadingSilence(q) {
    if (q === "name") return "Trim leading silence";
    if (!this.song) return;
    let firstTime = 0;
    for (const event of this.song.events) {
      if (this.song.isNoteEvent(event)) {
        firstTime = event.time;
        break;
      }
    }
    if (!firstTime) {
      this.dom.modalMessage("Already trimmed.");
      return;
    }
    for (const event of this.song.events) {
      event.time = Math.max(0, event.time - firstTime);
    }
    this.dom.modalMessage(`Trimmed ${firstTime} ms from start.`);
    this.rebuildEventsTable(null);
    this.dirty();
  }
  
  op_autoEndTime(q) {
    if (q === "name") return "Auto end time";
    if (!this.song) return;
    if (this.song.events.length < 1) return;
    const endTime = this.calculateIdealEndTime();
    const lastEvent = this.song.events[this.song.events.length - 1];
    if (lastEvent.time === endTime) {
      this.dom.modalMessage(`Already at the ideal end time, ${endTime} ms`);
      return;
    }
    const diff = endTime - lastEvent.time;
    // The last event should always be End of Track.
    // But if it's not EOT, add an artificial EOT event and trust the encoder to figure out what to do with it.
    // (fwiw this is exactly what Song.decodeEgs() does).
    if ((lastEvent.mopcode === 0xff) && (lastEvent.k === 0x2f)) {
      lastEvent.time = endTime;
    } else {
      this.song.events.push({ time: endTime, id: this.song.nextEventId++, mopcode: 0xff, k: 0x2f, v: new Uint8Array(0), chid: 0xff });
    }
    // Additionally, force all events to be strictly <=endTime. calculateIdealEndTime is allowed to clip eg Note Off events.
    for (const event of this.song.events) {
      if (event.time > endTime) event.time = endTime;
    }
    if (diff > 0) {
      this.dom.modalMessage(`Padded end time by ${diff} ms.`);
    } else {
      this.dom.modalMessage(`Trimmed end time by ${-diff} ms.`);
    }
    this.rebuildEventsTable(null);
    this.dirty();
  }
  
  op_reassignChannels(q) {
    if (q === "name") return "Reassign channels...";
    if (!this.song) return;
    const inUse = []; // sparse, indexed by chid
    for (const event of this.song.events) {
      if (!event.hasOwnProperty("chid")) continue;
      if (event.chid >= 0x10) continue;
      inUse[event.chid] = true;
    }
    const plan = []; // toChid, indexed by fromChid, sparse
    for (let toChid=0, fromChid=0; fromChid<16; fromChid++) {
      if (!inUse[fromChid]) continue;
      if (toChid !== fromChid) {
        plan[fromChid] = toChid;
      }
      toChid++;
    }
    if (plan.length < 1) {
      this.dom.modalMessage("Channel assignments already packed.");
      return;
    }
    let message = `Will reassign ${plan.length} channels...\n`;
    for (let fromChid=0; fromChid<16; fromChid++) {
      const toChid = plan[fromChid];
      if (typeof(toChid) === "number") {
        message += `${fromChid} => ${toChid}\n`;
      } else {
        plan[fromChid] = fromChid;
      }
    }
    this.dom.modalPickOne(["Cancel", "Reassign"], message).then(choice => {
      if (choice !== "Reassign") return;
      const nchannels = this.song.channels.map(c => null);
      for (let fromChid=0; fromChid<16; fromChid++) {
        const channel = this.song.channels[fromChid];
        if (!channel) continue;
        channel.chid = plan[fromChid];
        nchannels[channel.chid] = channel;
      }
      this.song.channels = nchannels;
      for (const event of this.song.events) {
        if ((event.chid >= 0) && (event.chid < 0x10)) {
          event.chid = plan[event.chid];
        }
        if ((event.mopcode === 0xff) && (event.k === 0x20) && (event.v?.length === 1)) { // Channel Prefix.
          event.v[0] = plan[event.v[0]];
        }
        // Luckily, no need for a messy rewrite of the EGS Header event. That will be rewritten from (song.channels).
      }
      this.buildUi(); // Importantly, must rebuild channel cards, not just events.
      this.dirty();
    }).catch(e => this.dom.modalError(e));
  }
  
  op_removeUselessEvents(q) {
    if (q === "name") return "Remove useless events...";
    if (!this.song) return;
    const useless = this.gatherUselessEvents();
    if (useless.length < 1) {
      this.dom.modalMessage("No useless events.");
      return;
    }
    let summary = `${useless.reduce((a, v) => a + v.events.length, 0)} events can be eliminated:\n`;
    for (const { desc, events } of useless) {
      summary += `- ${events.length} ${desc}\n`;
    }
    this.dom.modalPickOne(["Cancel", "Delete All"], summary).then(choice => {
      if (choice !== "Delete All") return;
      for (const bucket of useless) {
        for (const event of bucket.events) {
          const p = this.song.events.indexOf(event);
          if (p >= 0) this.song.events.splice(p, 1);
        }
      }
      this.rebuildEventsTable(null);
      this.dirty();
    }).catch(e => this.dom.modalError(e));
  }
}
