/* Env.js
 */
 
export class Env {
  constructor() {
    this.flags = 0;
    this.inivlo = 0;
    this.inivhi = 0;
    this.susp = -1;
    this.points = []; // {tlo,thi,vlo,vhi}, (t) in relative seconds
  }
  
  /* Returns new (srcp).
   */
  decode(src, srcp) {
  
    if (srcp < src.length) {
      this.flags = src[srcp++];
    } else {
      this.flags = 0;
    }
    
    if (this.flags & 0x01) {
      this.inivlo = ((src[srcp] << 8) | src[srcp+1]) / 65536;
      srcp += 2;
      if (this.flags & 0x02) {
        this.inivhi = ((src[srcp] << 8) | src[srcp+1]) / 65536;
        srcp += 2;
      } else {
        this.inivhi = this.inivlo;
      }
    }
    
    if (this.flags & 0x04) {
      this.susp = src[srcp++];
    } else {
      this.susp = -1;
    }
    
    let pointc;
    if (srcp < src.length) {
      pointc = src[srcp++];
    } else {
      pointc = 0;
    }
    
    this.points = [];
    for (let i=pointc; i-->0; ) {
      let tlo = src[srcp++];
      if (tlo & 0x80) {
        tlo = ((tlo & 0x7f) << 7) | src[srcp++];
      }
      const vlo = (src[srcp] << 8) | src[srcp+1];
      srcp += 2;
      const point = {
        tlo: tlo / 1000,
        vlo: vlo / 65536,
      };
      if (this.flags & 0x02) {
        let thi = src[srcp++];
        if (thi & 0x80) {
          thi = ((thi & 0x7f) << 7) | src[srcp++];
        }
        const vhi = (src[srcp] << 8) | src[srcp+1];
        srcp += 2;
        point.thi = thi / 1000;
        point.vhi = vhi / 65536;
      } else {
        point.thi = point.tlo;
        point.vhi = point.vlo;
      }
      this.points.push(point);
    }
    
    return srcp;
  }
  
  setConstant(v) {
    this.inivlo = this.inivhi = v;
    this.flags = 0x01;
    this.susp = -1;
    this.points = [];
  }
  
  bias(d) {
    this.inivlo += d;
    this.inivhi += d;
    for (const point of this.points) {
      point.vlo += d;
      point.vhi += d;
    }
  }
  
  scale(d) {
    this.inivlo *= d;
    this.inivhi *= d;
    for (const point of this.points) {
      point.vlo *= d;
      point.vhi *= d;
    }
  }
  
  /* (velocity) in 0..1, (when,dur) in seconds.
   * Returns array of { t, v } with (t) in absolute seconds and (v) in 0..1.
   */
  apply(velocity, when, dur) {
    const dst = [];
    if (!(this.flags & 0x02)) velocity = 0;
    let susp = -1, termt = 0;
    if (this.flags & 0x04) {
      susp = this.susp;
      termt = when + dur;
    }
    let t = when;
    if (velocity <= 0) {
      dst.push({ t, v: this.inivlo });
      for (const pt of this.points) {
        t += pt.tlo;
        dst.push({ t, v: pt.vlo });
        if (!susp--) {
          if (t < termt) {
            t = termt;
            dst.push({ t, v: pt.vlo });
          }
        }
      }
    } else if (velocity >= 1) {
      dst.push({ t, v: this.inivhi });
      for (const pt of this.points) {
        t += pt.thi;
        dst.push({ t, v: pt.vhi });
        if (!susp--) {
          if (t < termt) {
            t = termt;
            dst.push({ t, v: pt.vhi });
          }
        }
      }
    } else {
      const yticolev = 1.0 - velocity;
      dst.push({ t, v: this.inivlo * yticolev + this.inivhi * velocity });
      for (const pt of this.points) {
        t += pt.tlo * yticolev + pt.thi * velocity;
        const v = pt.vlo * yticolev + pt.vhi * velocity;
        dst.push({ t, v });
        if (!susp--) {
          if (t < termt) {
            t = termt;
            dst.push({ t, v });
          }
        }
      }
    }
    return dst;
  }
}
