/* Song.js
 * EGS song player.
 * We don't decode the serial EGS (that's SynthFormats) and we don't interact directly with AudioContext (that's Channel).
 * Our main duty is timing, and presenting a uniform per-song interface to Audio.
 */
 
import { SynthFormats } from "./SynthFormats.js";
import { Channel } from "./Channel.js";
 
export class Song {
  constructor(src, repeat, ctx, globalTrim) {
    if (src instanceof Uint8Array) {
      src = SynthFormats.splitEgs(src);
    }
    if (!src || !src.channels || !src.events) {
      throw new Error(`Expected encoded or split EGS file`);
    }
    this.egs = src;
    this.iter = SynthFormats.iterateEgsEvents(this.egs.events);
    this.repeat = repeat;
    this.nextEventTime = 0;
    this.startTime = ctx.currentTime + 5.000; // Initially in the future, and we'll replace at the first update.
    this.finished = false;
    this.node = new GainNode(ctx, { gain: 1.0 });
    this.node.connect(ctx.destination);
    this.channels = this.egs.channels.map(ch => {
      if (!ch) return null;
      return new Channel(ch, ctx, this.node, globalTrim);
    });
  }
  
  cancel(ctx) {
    this.finished = true;
    this.node.gain.setValueAtTime(1.0, ctx.currentTime);
    this.node.gain.linearRampToValueAtTime(0.0, ctx.currentTime + 0.250);
    window.setTimeout(() => this.node.disconnect(), 250);
    for (const ch of this.channels) {
      if (!ch) continue;
      ch.cancel(ctx);
    }
  }
  
  setPlayhead(ctx, time) {
    if (time < 0) time = 0;
    
    // Fade out my master node and replace it.
    const oldNode = this.node;
    oldNode.gain.setValueAtTime(1.0, ctx.currentTime);
    oldNode.gain.linearRampToValueAtTime(0.0, ctx.currentTime + 0.250);
    window.setTimeout(() => oldNode.disconnect(), 250);
    this.node = new GainNode(ctx, { gain: 1.0 });
    this.node.connect(ctx.destination);
    for (const ch of this.channels) {
      if (!ch) continue;
      ch.cancel(ctx);
      ch.dst = this.node;
    }
    
    // Make a new iterator and discard events until we reach (time) or the end.
    this.iter = SynthFormats.iterateEgsEvents(this.egs.events);
    let elapsed = 0;
    while (elapsed < time) {
      const event = this.iter.next();
      if (!event) break;
      if (event.type === "delay") {
        elapsed += event.delay;
      }
    }
    const now = ctx.currentTime;
    this.startTime = now - time;
    this.nextEventTime = now + elapsed - time;
    delete this.nextStartTime;
    this.finished = false;
  }
  
  /* Returns true to proceed or false if finished.
   * A song with (repeat) can still finish, if there's an error.
   */
  update(ctx, thruTime) {
    //console.log(`Song.update`, { ctx, thruTime });
    if (this.finished) return false;
    if (!this.nextEventTime) {
      this.nextEventTime = ctx.currentTime;
      if (this.startTime > this.nextEventTime) this.startTime = this.nextEventTime;
    }
    if (this.nextStartTime && (this.nextStartTime < ctx.currentTime)) {
      this.startTime = this.nextStartTime;
      delete this.nextStartTime;
    }
    while (this.nextEventTime <= thruTime) {
      const event = this.iter.next();
      if (!event) {
        if (this.repeat) {
          this.iter = SynthFormats.iterateEgsEvents(this.egs.events);
          this.nextEventTime += 0.001; // Force some amount of delay at repeat as a safety measure.
          this.nextStartTime = this.nextEventTime;
          return true;
        } else {
          this.finished = true;
          return false;
        }
      }
      switch (event.type) {
        case "delay": {
            this.nextEventTime += event.delay;
          } break;
        case "note": {
            const channel = this.channels[event.chid];
            if (channel) {
              channel.playNote(ctx, this.nextEventTime, event.noteid, event.velocity, event.dur);
            }
          } break;
      }
    }
    return true;
  }
}
