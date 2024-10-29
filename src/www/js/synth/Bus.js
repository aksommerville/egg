/* Bus.js
 * Self-contained event and signal graphs, equivalent to one song.
 */
 
import { SynthFormats } from "./SynthFormats.js";
import { Channel } from "./Channel.js";

const BUS_FORWARD_TIME = 2.000; // How far ahead we can schedule events.

export class Bus {
  constructor(audio, serial, rid, repeat) {
    this.audio = audio;
    this.serial = serial;
    this.rid = rid;
    this.repeat = repeat;
    this.init();
  }
  
  init() {
    this.master = new GainNode(this.audio.ctx, { gain: 1.0 });
    const egs = SynthFormats.splitEgs(this.serial);
    this.channels = egs.channels.filter(v => v).map(chhdr => {
      const channel = new Channel(this.audio, this.audio.ctx, chhdr, this.master);
      if (channel.isDummy()) return null;
      channel.install();
      return channel;
    });
    this.events = SynthFormats.iterateEgsEvents(egs.events);
    this.connected = false;
    this.nextEventTime = 0;
    this.finishedEvents = false;
    this.cancelled = false;
  }
  
  /* Public API, for consumption by Audio.
   *************************************************************************/
  
  start(delay) {
    if (this.connected) return;
    this.master.connect(this.audio.ctx.destination);
    this.connected = true;
    this.nextEventTime = this.audio.ctx.currentTime + delay;
  }
  
  stop() {
    if (this.connected) {
      this.master.disconnect();
      this.connected = false;
    }
    // It's probably not necessary to uninstall Channels, since they're attached to our master, but let's be sure:
    for (const channel of this.channels) channel.uninstall();
  }
  
  /* A cancelled Bus can still be playing for arbitrary time afterward.
   */
  isCancelled() {
    return this.cancelled;
  }
  
  cancel(fadeTime) {
    if (!this.connected) return;
    const now = this.audio.ctx.currentTime;
    this.master.gain.setValueAtTime(this.master.gain.value, now);
    this.master.gain.linearRampToValueAtTime(0, now + fadeTime);
    this.cancelled = true;
  }
  
  uncancel() {
    if (!this.connected) return;
    if (!this.cancelled) return;
    const now = this.audio.ctx.currentTime;
    this.master.gain.setValueAtTime(this.master.gain.value, now);
    this.master.gain.linearRampToValueAtTime(1, now + 0.500);
    this.cancelled = false;
  }
  
  /* Schedule upcoming events if warranted.
   * Returns true if we're still playing, or false if it's safe to delete.
   */
  update() {
    //console.log(`Bus(${this.rid}).update connected=${this.connected} finishedEvents=${this.finishedEvents} nextEventTime=${this.nextEventTime}`);
    if (!this.connected) return false;
    if (this.finishedEvents) return this.signalPending();
    const now = this.audio.ctx.currentTime;
    
    // If we get an unusually long update period, assume we suspended and resumed. Skip notes.
    if (!this.lastUpdateTime) this.lastUpdateTime = now;
    const elapsed = now - this.lastUpdateTime;
    const suppress = (elapsed > 1.000);
    this.lastUpdateTime = now;
    
    const later = now + BUS_FORWARD_TIME;
    while (this.nextEventTime < later) {
      const event = this.events.next();
      //console.log(`Bus(${this.rid}).update, now=${now} later=${later} nextEventTime=${this.nextEventTime} event=${JSON.stringify(event)}`);
      
      if (!event) {
        if (!this.repeat) {
          this.finishedEvents = true;
          break;
        }
        this.events.reset();
        // When repeating, advance time by a small amount, just in case we have a zero-length song.
        this.nextEventTime += 0.010;
        continue;
      }
      
      switch (event.type) {
        case "delay": this.nextEventTime += event.delay; break;
        case "note": {
            if (suppress) continue;
            const channel = this.channels[event.chid];
            if (channel) channel.playNote(event.noteid, event.velocity, event.dur, this.nextEventTime);
          } break;
        case "wheel": {
            const channel = this.channels[event.chid];
            if (channel) channel.setWheel(event.wheel, this.nextEventTime);
          } break;
      }
    }
    return this.signalPending();
  }
  
  /* Poll channels for activity.
   * Drop any defunct ones, then return true if we are still capable of producing a signal.
   */
  signalPending() {
    if (this.cancelled && (this.master.gain.value <= 0)) {
      return false;
    }
    let result = false;
    for (let i=this.channels.length; i-->0; ) {
      const channel = this.channels[i];
      if (!channel) continue;
      if (channel.isFinished()) {
        channel.uninstall();
        this.channels[i] = null;
      } else {
        result = true;
      }
    }
    return result;
  }
}
