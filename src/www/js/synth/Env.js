/* Env.js
 * Generic linear envelope.
 */
 
export class Env {

  /* (serial) is a Uint8Array in the format described by etc/doc/audio-format.md.
   * It may be null for a default level envelope.
   * The Env object is a template that can be used to produce multiple concrete envelopes at arbitrary velocity and duration.
   */
  constructor(serial) {
    if (serial) this.decode(serial);
    else this.setDefault();
  }
  
  /* Returns an array of {time,value}, guaranteed to begin with a valid point at time (when).
   * Values in 0..1, times in absolute seconds.
   */
  apply(velocity, dur, when) {
    let loweight, hiweight;
    if (velocity <= 0) {
      loweight = 1;
      hiweight = 0;
    } else if (velocity >= 1) {
      loweight = 0;
      hiweight = 1;
    } else {
      loweight = 1 - velocity;
      hiweight = velocity;
    }
    const startTime = when;
    const dst = [];
    dst.push({time:when, value:(this.initlo*loweight + this.inithi*hiweight)});
    let sus = this.susp - 1;
    for (const src of this.points) {
      const delay = src.tlo*loweight + src.thi*hiweight;
      when += delay;
      const value = src.vlo*loweight + src.vhi*hiweight;
      dst.push({time:when, value});
      if (!sus--) { // Sustain.
        const send = startTime + dur;
        if (when < send) {
          when = send;
          dst.push({time:when, value});
        }
      }
    }
    return dst;
  }
  
  scaleIterator(iter, mult, add) {
    for (const point of iter) point.value = point.value * mult + add;
  }
  
  setDefault() {
    this.initlo = 0;
    this.inithi = 0;
    this.susp = 1;
    this.points = [ // Times all relative.
      { tlo:0.020, thi:0.010, vlo:0.400, vhi:0.999 },
      { tlo:0.020, thi:0.025, vlo:0.100, vhi:0.250 },
      { tlo:0.100, thi:0.200, vlo:0.000, vhi:0.000 },
    ];
  }
  
  decode(serial) {
    this.susp = serial[0] || 0xff;
    this.initlo = ((serial[1] << 8) | serial[2]) / 65535.0;
    this.inithi = ((serial[3] << 8) | serial[4]) / 65535.0;
    this.points = [];
    let sp = 5;
    for (; sp<serial.length; sp+=8) {
      const tlo = ((serial[sp] << 8) | serial[sp+1]) / 1000.0;
      const thi = ((serial[sp+2] << 8) | serial[sp+3]) / 1000.0;
      const vlo = ((serial[sp+4] << 8) | serial[sp+5]) / 65535.0;
      const vhi = ((serial[sp+6] << 8) | serial[sp+7]) / 65535.0;
      this.points.push({tlo, thi, vlo, vhi});
    }
    if (this.susp >= this.points.length) this.susp = -1;
  }
}
