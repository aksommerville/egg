/* Env.js
 */
 
/* Don't instantiate me. Use LevelEnv or AuxEnv.
 */
export class Env {
  /* Returns an array of {t,v}, with (t) in absolute seconds, always ascending.
   * (dur,when) in seconds.
   */
  apply(velocity, dur, when) {
    const pointv = [];
    if (velocity < 0) velocity = 0;
    else if (velocity > 1) velocity = 1;
    const aweight = 1.0 - velocity;
    pointv.push({ t: when, v: aweight * this.inivlo + velocity * this.inivhi });
    pointv.push({ t: pointv[0].t + aweight * this.atktlo + velocity * this.atkthi, v: aweight * this.atkvlo + velocity * this.atkvhi });
    pointv.push({ t: pointv[1].t + aweight * this.dectlo + velocity * this.decthi, v: aweight * this.susvlo + velocity * this.susvhi });
    const sust = Math.max(0, when + dur - pointv[2].t);
    pointv.push({ t: pointv[2].t + sust, v: pointv[2].v });
    pointv.push({ t: pointv[3].t + aweight * this.rlstlo + velocity * this.rlsthi, v: aweight * this.rlsvlo + velocity * this.rlsvhi });
    return pointv;
  }
  
  bias(dv) {
    this.inivlo += dv;
    this.inivhi += dv;
    this.atkvlo += dv;
    this.atkvhi += dv;
    this.susvlo += dv;
    this.susvhi += dv;
    this.rlsvlo += dv;
    this.rlsvhi += dv;
  }
  
  scale(sv) {
    this.inivlo *= sv;
    this.inivhi *= sv;
    this.atkvlo *= sv;
    this.atkvhi *= sv;
    this.susvlo *= sv;
    this.susvhi *= sv;
    this.rlsvlo *= sv;
    this.rlsvhi *= sv;
  }
}
 
export class LevelEnv extends Env {
  /* 12 bytes in. See etc/doc/audio-format.md
   */
  constructor(serial) {
    super();
    this.inivlo = 0;
    this.inivhi = 0;
    this.atktlo = (serial[0] || 0) / 1000.0;
    this.atkthi = (serial[1] || 0) / 1000.0;
    this.atkvlo = (serial[2] || 0) / 255.0;
    this.atkvhi = (serial[3] || 0) / 255.0;
    this.dectlo = (serial[4] || 0) / 1000.0;
    this.decthi = (serial[5] || 0) / 1000.0;
    this.susvlo = (serial[6] || 0) / 255.0;
    this.susvhi = (serial[7] || 0) / 255.0;
    this.rlstlo = ((serial[8] << 8) | serial[9]) / 1000.0;
    this.rlsthi = ((serial[10] << 8) | serial[11]) / 1000.0;
    this.rlsvlo = 0;
    this.rlsvhi = 0;
  }
}

export class AuxEnv extends Env {
  /* 16 bytes in. See etc/doc/audio-format.md
   * You must also provide a LevelEnv -- AuxEnv does not contain its own timing.
   * Values are initially just as encoded: 0..0xffff.
   */
  constructor(serial, level) {
    super();
    this.inivlo = (serial[0] << 8) | serial[1];
    this.inivhi = (serial[2] << 8) | serial[3];
    this.atkvlo = (serial[4] << 8) | serial[5];
    this.atkvhi = (serial[6] << 8) | serial[7];
    this.susvlo = (serial[8] << 8) | serial[9];
    this.susvhi = (serial[10] << 8) | serial[11];
    this.rlsvlo = (serial[12] << 8) | serial[13];
    this.rlsvhi = (serial[14] << 8) | serial[15];
    this.atktlo = level.atktlo;
    this.atkthi = level.atkthi;
    this.dectlo = level.dectlo;
    this.decthi = level.decthi;
    this.rlstlo = level.rlstlo;
    this.rlsthi = level.rlsthi;
  }
}

//XXX old version
export class EnvXXX {

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
