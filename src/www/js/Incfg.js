/* Incfg.js
 * Interactive input configurator.
 */
 
const srcbits_png = [
0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,
0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x40,0x02,0x03,0x00,0x00,0x00,0xd7,0x07,0x99,
0x4d,0x00,0x00,0x00,0x09,0x50,0x4c,0x54,0x45,0x00,0x00,0x00,0x00,0x00,0x00,0xff,
0xff,0xff,0x83,0xdd,0xcf,0xd2,0x00,0x00,0x00,0x01,0x74,0x52,0x4e,0x53,0x00,0x40,
0xe6,0xd8,0x66,0x00,0x00,0x01,0x4d,0x49,0x44,0x41,0x54,0x38,0xcb,0x8d,0x92,0x41,
0x6e,0x85,0x30,0x0c,0x44,0xad,0xac,0x2c,0x4e,0x61,0x75,0x85,0x38,0xa5,0x97,0x3e,
0x86,0x97,0x56,0x4e,0xd9,0xb1,0x43,0x02,0xfc,0xb6,0x2a,0xf3,0xbf,0x00,0xbd,0x38,
0xe3,0xc1,0x84,0x88,0xa8,0x77,0xc7,0x95,0xb6,0xde,0xa9,0xd4,0x88,0x38,0xef,0x42,
0xa4,0x13,0xb4,0xbc,0xeb,0x04,0x5b,0x4c,0x70,0xd4,0x56,0xf2,0x36,0x81,0x6e,0x55,
0x80,0xc7,0x01,0x70,0xf5,0xd3,0x6b,0x01,0x3e,0xbd,0x16,0x18,0x7f,0x04,0x18,0xf2,
0x5c,0x86,0xe3,0xf0,0x4e,0x3b,0x2d,0xb0,0xd3,0xd2,0x5e,0x06,0x95,0x66,0xab,0xb2,
0x96,0x66,0x5f,0xb5,0x26,0x95,0x9e,0x8c,0xb2,0xd3,0xd6,0x07,0x38,0x34,0x33,0x00,
0x08,0x76,0x26,0x50,0x29,0x20,0x13,0x34,0xac,0xf1,0x02,0x30,0x9d,0x40,0x0b,0xd4,
0x18,0xb8,0xde,0xf2,0x13,0x34,0x9a,0x20,0x4d,0x33,0x69,0xeb,0x98,0x4c,0x64,0xb4,
0x43,0x3b,0x00,0xf3,0x39,0xc6,0xea,0x83,0xa2,0x05,0x34,0x3d,0xe4,0x0d,0x90,0x27,
0xe0,0x0b,0x0c,0x53,0x80,0x67,0x5b,0xce,0x4c,0xf7,0x60,0xed,0x06,0xd2,0x63,0x81,
0x23,0x8e,0x78,0x00,0x6d,0xda,0xfe,0x07,0x97,0x29,0x86,0x9c,0x1e,0x39,0xe4,0x01,
0xee,0x6a,0x35,0xe7,0x9b,0xf8,0x05,0xe0,0x3a,0x97,0x43,0xf5,0x51,0xb8,0x0e,0xd5,
0x90,0x27,0xa8,0xa9,0xaf,0xfa,0x71,0x36,0xf5,0xea,0x99,0x1f,0xea,0xe8,0x97,0x65,
0x0f,0xfa,0x29,0x71,0x77,0x0b,0x09,0x99,0x27,0xcb,0x84,0xd9,0x54,0xd9,0x6c,0x01,
0x91,0x70,0xe7,0x5b,0x85,0x32,0x6a,0xf0,0x9b,0x1e,0x11,0xce,0x2e,0x2c,0x4c,0x7f,
0x48,0x7c,0xf7,0x08,0xb1,0x80,0x75,0xb0,0x38,0x99,0x29,0x9a,0x28,0x9a,0x25,0x40,
0xaf,0x70,0xb5,0x4a,0xc2,0xae,0x11,0x36,0x2b,0xcc,0xa4,0x1e,0x4c,0xc9,0xd2,0x43,
0x77,0x4b,0x1b,0x53,0xd1,0x5f,0xbb,0x84,0xa5,0xe6,0x8b,0x62,0xab,0x14,0x98,0xc1,
0xcc,0x05,0x20,0xf0,0x46,0x27,0x08,0x63,0x31,0x09,0x5b,0x76,0x3b,0x4a,0x22,0x41,
0xd0,0x4b,0x7d,0x03,0x82,0xe2,0x96,0x09,0x02,0x37,0xdb,0x56,0x00,0x00,0x00,0x00,
0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82
];

const button_rects = [
  [[ 44, 28, 6, 6]], // SOUTH
  [[ 51, 21, 6, 6]], // EAST
  [[ 37, 21, 6, 6]], // WEST
  [[ 44, 14, 6, 6]], // NORTH
  [[5,6,3,7], [4,7,7,4], [8,4,8,5]], // L1
  [[48,4,8,5], [53,8,8,3], [56,6,3,7]], // R1
  [[12,1,11,3], [16,4,7,4]], // L2
  [[41,1,11,3], [41,4,7,4]], // R2
  [[ 25, 31, 6, 3]], // AUX2
  [[ 33, 31, 6, 3]], // AUX1
  [[ 14, 14, 6, 6]], // UP
  [[ 14, 28, 6, 6]], // DOWN
  [[  7, 21, 6, 6]], // LEFT
  [[ 21, 21, 6, 6]], // RIGHT
  [[ 30, 12, 4, 4]], // AUX3
];
 
export class Incfg {
  constructor(rt) {
    this.rt = rt;
    this.assignments = []; // {srcbtnid,dstbtnid}
    this.deviceName = ""; // "Keyboard", or something derived from a gamepad. Displayed, and flags the fact we've attached to a device.
    this.gamepadIndex = -1;
    this.gamepadId = "";
    this.gamepadAxes = []; // {index,value:-1|1} for all held axes
    this.gamepadButtons = []; // index
    this.btnid = 0; // 0..14, which Egg button are we asking for.
    this.confirm = false; // false or btnid
    this.error = false;
    this.abort = false;
    this.clock = 10;
    this.loadImage();
    this.createCanvas();
    this.listen();
  }
  
  /* Signal that you're going to stop using this Incfg.
   */
  cancel() {
    this.destroyCanvas();
    this.unlisten();
  }
  
  /* Call the same way you would update the game.
   * We do our own rendering.
   * Returns false if configuration is finished.
   */
  update(elapsed) {
    if ((this.clock -= elapsed) <= 0) {
      this.expire();
    }
    if (this.abort) {
      this.destroyCanvas();
      this.unlisten();
      return false;
    }
    this.pollGamepads();
    this.render();
    return true;
  }
  
  /* Nothing public below this point.
   *******************************************************************************/
   
  listen() {
    this.keyListener = e => this.onKey(e);
    window.addEventListener("keydown", this.keyListener);
    window.addEventListener("keyup", this.keyListener);
  }
  
  unlisten() {
    window.removeEventListener("keydown", this.keyListener);
    window.removeEventListener("keyup", this.keyListener);
    this.keyListener = null;
  }
  
  nextButton() {
    this.btnid++;
    while (this.shouldSkipButton(this.btnid)) this.btnid++;
    if (this.btnid >= 15) {
      if (this.deviceName === "Keyboard") this.rt.input.setKeyMapButtonsOnly(this.assignments);
      else this.rt.input.setGamepadMap(this.gamepadId, this.assignments);
      this.abort = true;
    } else {
      this.clock = 10;
      this.confirm = false;
      this.error = false;
    }
  }
  
  shouldSkipButton(btnid) {
    if (this.assignments.find(a => a.dstbtnid === btnid)) return true;
    const missing = (bid) => !this.assignments.find(a => a.dstbtnid === bid);
    switch (btnid) {
    
      // SOUTH, EAST, WEST, NORTH: Skip if the prior button unset.
      case 0: return false;
      case 1: return missing(0);
      case 2: return missing(1);
      case 3: return missing(2);
      
      // L1, R1, L2, R2: Skip if the prior button unset.
      case 4: return false;
      case 5: return missing(4);
      case 6: return missing(5);
      case 7: return missing(6);
      
      // AUX2, AUX1: Standard Mapping has them in the wrong order (AUX1 should be the primary), so just ask always.
      case 8: return false;
      case 9: return false;
      
      // UP, DOWN, LEFT, RIGHT: Don't skip.
      case 10: return false;
      case 11: return false;
      case 12: return false;
      case 13: return false;
      
      // AUX3: Skip if AUX2 unset.
      case 14: return missing(8);
    }
    // Important that we *not* skip OOB buttons; caller will manage termination.
    return false;
  }
  
  expire() {
    this.nextButton();
  }
  
  onStroke(btnid) {
    this.clock = 10;
    if (this.confirm === btnid) {
      this.assignments.push({ dstbtnid: this.btnid, srcbtnid: btnid });
      this.autoAssign(this.btnid, btnid);
      this.nextButton();
    } else if (this.confirm) {
      this.confirm = btnid;
      this.error = !this.error;
    } else {
      this.confirm = btnid;
      this.error = false;
    }
  }
  
  // Add missing assignments that could be inferred from this one.
  autoAssign(dstbtnid, srcbtnid) {
    let match, partner;
    const ifMissing = (obtnid) => {
      if (this.assignments.find(a => a.dstbtnid === obtnid)) return;
      this.assignments.push({ dstbtnid: obtnid, srcbtnid: partner });
    };
    
    // Signed axes.
    if (match = srcbtnid.match(/^a(\d+)([-+])$/)) {
      partner = `a${match[1]}${(match[2] === '-') ? '+' : '-'}`;
      if (!this.assignments.find(a => a.srcbtnid === partner)) {
        switch (dstbtnid) {
          case 10: ifMissing(11); break; // UP => DOWN
          case 11: ifMissing(10); break; // DOWN => UP
          case 12: ifMissing(13); break; // LEFT => RIGHT
          case 13: ifMissing(12); break; // RIGHT => LEFT
        }
      }
      return;
    }
    
    // Arrow keys.
    if (match = srcbtnid.match(/^Arrow([a-zA-Z]+)$/)) {
      switch (match[1]) {
        case "Left": partner = "ArrowRight"; break;
        case "Right": partner = "ArrowLeft"; break;
        case "Up": partner = "ArrowDown"; break;
        case "Down": partner = "ArrowUp"; break;
        default: partner = "";
      }
      if (partner && !this.assignments.find(a => a.srcbtnid === partner)) {
        switch (dstbtnid) {
          case 10: ifMissing(11); break; // UP => DOWN
          case 11: ifMissing(10); break; // DOWN => UP
          case 12: ifMissing(13); break; // LEFT => RIGHT
          case 13: ifMissing(12); break; // RIGHT => LEFT
        }
      }
      return;
    }
  }
  
  pollGamepads() {
    if (!navigator?.getGamepads) return;
    if (this.deviceName === "Keyboard") return;
    for (const gp of navigator.getGamepads()) {
      if (!gp) continue;
      
      // No device attached yet, just check for any nonzero state.
      // Don't bother processing the event; we'll catch it at the next update.
      if (!this.deviceName) {
        if (
          gp.buttons.find(b => b.value) ||
          gp.axes.find(a => (a < -0.5) || (a > 0.5))
        ) {
          this.deviceName = this.deviceNameForGamepad(gp);
          this.gamepadIndex = gp.index;
          this.gamepadId = gp.id;
          return;
        }
        continue;
      }
      if (gp.index !== this.gamepadIndex) continue;
      
      for (let ai=gp.axes.length; ai-->0; ) {
        const v = (gp.axes[ai] < -0.5) ? -1 : (gp.axes[ai] > 0.5) ? 1 : 0;
        if (v) {
          const axis = this.gamepadAxes.find(v => v.index === ai);
          if (!axis) {
            this.gamepadAxes.push({ index: ai, value: v });
            this.onStroke(`a${ai}${(v < 0) ? '-' : '+'}`);
          }
        } else {
          const ap = this.gamepadAxes.findIndex(v => v.index === ai);
          if (ap >= 0) {
            this.gamepadAxes.splice(ap, 1);
          }
        }
      }
      
      for (let bi=gp.buttons.length; bi-->0; ) {
        const v = gp.buttons[bi].value;
        const p = this.gamepadButtons.indexOf(bi);
        if (v) {
          if (p < 0) {
            this.gamepadButtons.push(bi);
            this.onStroke(`b${bi}`);
          }
        } else {
          if (p >= 0) {
            this.gamepadButtons.splice(p, 1);
          }
        }
      }
    }
  }
  
  deviceNameForGamepad(gp) {
    const name = gp.id.split("(")[0].trim();
    if (name) return name;
    return gp.index.toString();
  }
  
  onKey(event) {
    const value = (event.type === "keydown") ? 1 : 0;
  
    // ESC always aborts, we ignore F keys, and we ignore everything if a modifier is down.
    if (event.code === "Escape") {
      if (value) {
        this.abort = true;
        event.stopPropagation();
        event.preventDefault();
        return;
      }
    }
    if ((event.code.length > 1) && (event.code[0] === 'F')) {
      return;
    }
    if (value && (event.shiftKey || event.controlKey || event.altKey)) {
      return;
    }
    
    event.stopPropagation();
    event.preventDefault();
    
    // First DOWN event attaches the device, and we also process it normally.
    if (!this.deviceName) {
      if (value) {
        this.deviceName = "Keyboard";
      } else {
        return;
      }
    }
    if (this.deviceName !== "Keyboard") return;
    
    if (value) this.onStroke(event.code);
  }
   
  loadImage() {
    this.image = null;
    const image = new Image();
    const blob = new Blob([new Uint8Array(srcbits_png)], { type: "image/png" });
    const url = URL.createObjectURL(blob);
    image.src = url;
    image.addEventListener("load", () => {
      this.image = image;
      URL.revokeObjectURL(url);
    }, { once: true });
    image.addEventListener("error", () => {
      URL.revokeObjectURL(url);
    }, { once: true });
  }
   
  createCanvas() {
    this.canvas = document.createElement("CANVAS");
    this.canvas.width = 160;
    this.canvas.height = 90;
    this.canvas.style.objectFit = "contain";
    this.canvas.style.backgroundColor = "#000";
    this.canvas.style.position = "absolute";
    const gameBounds = this.rt.canvas.getBoundingClientRect();
    this.canvas.style.left = gameBounds.x + "px";
    this.canvas.style.top = gameBounds.y + "px";
    this.canvas.style.width = gameBounds.width + "px";
    this.canvas.style.height = gameBounds.height + "px";
    document.body.appendChild(this.canvas);
    this.ctx = this.canvas.getContext("2d");
  }
  
  destroyCanvas() {
    this.canvas.remove();
  }
  
  render() {
    this.ctx.fillStyle = "#334";
    this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
    if (!this.image) return;
    
    const rects = button_rects[this.btnid];
    if (rects) {
      if (this.error) this.ctx.fillStyle = "#f00";
      else if (this.confirm) this.ctx.fillStyle = "#080";
      else this.ctx.fillStyle = "#ff0";
      for (const [x, y, w, h] of rects) {
        this.ctx.fillRect(48 + x, 23 + y, w, h);
      }
    }
    
    this.ctx.drawImage(this.image, 0, 0, 64, 44, 48, 23, 64, 44);
    
    if (this.deviceName) {
      const namew = this.deviceName.length * 4;
      let dstx = 80 - (namew >> 1);
      const dsty = 8;
      for (let i=0; i<this.deviceName.length; i++, dstx+=4) {
        let ch = this.deviceName.charCodeAt(i);
        if ((ch >= 0x30) && (ch <= 0x39)) {
          this.ctx.drawImage(this.image, (ch - 0x30) * 4, 45, 3, 5, dstx, dsty, 3, 5);
        } else {
          if ((ch >= 0x61) && (ch <= 0x7a)) ch -= 0x20;
          if ((ch >= 0x41) && (ch <= 0x50)) {
            this.ctx.drawImage(this.image, (ch - 0x41) * 4, 51, 3, 5, dstx, dsty, 3, 5);
          } else if ((ch >= 0x51) && (ch <= 0x5a)) {
            this.ctx.drawImage(this.image, (ch - 0x51) * 4, 57, 3, 5, dstx, dsty, 3, 5);
          }
        }
      }
    }
    
    if (this.clock < 5) {
      const digit = Math.ceil(this.clock);
      this.ctx.drawImage(this.image, digit * 4, 45, 3, 5, 78, 76, 3, 5);
    }
  }
}
