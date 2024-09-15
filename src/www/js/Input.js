/* Input.js
 */
 
export const
  EGG_BTN_SOUTH = 0x0001,
  EGG_BTN_EAST  = 0x0002,
  EGG_BTN_WEST  = 0x0004,
  EGG_BTN_NORTH = 0x0008,
  EGG_BTN_L1    = 0x0010,
  EGG_BTN_R1    = 0x0020,
  EGG_BTN_L2    = 0x0040,
  EGG_BTN_R2    = 0x0080,
  EGG_BTN_AUX2  = 0x0100,
  EGG_BTN_AUX1  = 0x0200,
  EGG_BTN_UP    = 0x0400,
  EGG_BTN_DOWN  = 0x0800,
  EGG_BTN_LEFT  = 0x1000,
  EGG_BTN_RIGHT = 0x2000,
  EGG_BTN_AUX3  = 0x4000,
  EGG_BTN_CD    = 0x8000,
  EGG_SIGNAL_BIT             = 0x40000000,
  EGG_SIGNAL_QUIT            = 0x40000001,
  EGG_SIGNAL_FULLSCREEN      = 0x40000002,
  EGG_SIGNAL_PAUSE           = 0x40000003,
  EGG_SIGNAL_STEP            = 0x40000004,
  EGG_SIGNAL_SAVESTATE       = 0x40000005,
  EGG_SIGNAL_LOADSTATE       = 0x40000006,
  EGG_SIGNAL_SCREENCAP       = 0x40000007,
  EGG_SIGNAL_CONFIGIN        = 0x40000008;
 
export class Input {
  constructor(rt) {
    this.rt = rt;
    this.state = []; // Indexed by playerid, including zero.
    
    // [KeyEvent.code]: EGG_BTN_* or EGG_SIGNAL_*
    this.keyMap = {
      Escape: EGG_SIGNAL_QUIT,
      Pause: EGG_SIGNAL_PAUSE,
      F1: EGG_SIGNAL_CONFIGIN,
      F7: EGG_SIGNAL_SCREENCAP,
      F8: EGG_SIGNAL_LOADSTATE,
      F9: EGG_SIGNAL_SAVESTATE,
      F10: EGG_SIGNAL_STEP,
      F11: EGG_SIGNAL_FULLSCREEN,
      F12: EGG_SIGNAL_PAUSE,
      Tab: EGG_BTN_L1,
      Backquote: EGG_BTN_L2,
      Backslash: EGG_BTN_R1,
      Backspace: EGG_BTN_R2,
      Enter: EGG_BTN_AUX1,
      BracketRight: EGG_BTN_AUX2,
      BracketLeft: EGG_BTN_AUX3,
      KeyW: EGG_BTN_UP,
      KeyA: EGG_BTN_LEFT,
      KeyS: EGG_BTN_DOWN,
      KeyD: EGG_BTN_RIGHT,
      Space: EGG_BTN_SOUTH,
      Comma: EGG_BTN_WEST,
      Period: EGG_BTN_EAST,
      Slash: EGG_BTN_NORTH,
      KeyQ: EGG_BTN_WEST,
      KeyE: EGG_BTN_EAST,
      KeyR: EGG_BTN_NORTH,
      ArrowLeft: EGG_BTN_LEFT,
      ArrowRight: EGG_BTN_RIGHT,
      ArrowUp: EGG_BTN_UP,
      ArrowDown: EGG_BTN_DOWN,
      KeyZ: EGG_BTN_SOUTH,
      KeyX: EGG_BTN_WEST,
      KeyC: EGG_BTN_EAST,
      KeyV: EGG_BTN_NORTH,
      Numpad8: EGG_BTN_UP,
      Numpad4: EGG_BTN_LEFT,
      Numpad5: EGG_BTN_DOWN,
      Numpad6: EGG_BTN_RIGHT,
      Numpad2: EGG_BTN_DOWN,
      Numpad7: EGG_BTN_L1,
      Numpad9: EGG_BTN_R1,
      Numpad1: EGG_BTN_L2,
      Numpad3: EGG_BTN_R2,
      Numpad0: EGG_BTN_SOUTH,
      NumpadEnter: EGG_BTN_WEST,
      NumpadAdd: EGG_BTN_EAST,
      NumpadDecimal: EGG_BTN_NORTH,
      NumpadDivide: EGG_BTN_AUX1,
      NumpadMultiply: EGG_BTN_AUX2,
      NumpadSubtract: EGG_BTN_AUX3,
    };
    
    this.gamepadStandardMapping = [
      EGG_BTN_SOUTH,
      EGG_BTN_EAST,
      EGG_BTN_WEST,
      EGG_BTN_NORTH,
      EGG_BTN_L1,
      EGG_BTN_R1,
      EGG_BTN_L2,
      EGG_BTN_R2,
      EGG_BTN_AUX2,
      EGG_BTN_AUX1,
      0, // lp
      0, // rp
      EGG_BTN_UP,
      EGG_BTN_DOWN,
      EGG_BTN_LEFT,
      EGG_BTN_RIGHT,
      EGG_BTN_AUX3,
    ];
  }
  
  /* Public entry points.
   *******************************************************************************/
  
  start() {
    this.gamepads = [];
    this.state = [];
    if (!this.keyListener) {
      this.keyListener = e => this.onKey(e);
      window.addEventListener("keydown", this.keyListener);
      window.addEventListener("keyup", this.keyListener);
    }
    if (!this.gamepadListener) {
      this.gamepadListener = e => this.onGamepad(e);
      window.addEventListener("gamepadconnected", this.gamepadListener);
      window.addEventListener("gamepaddisconnected", this.gamepadListener);
    }
    this.setPlayerButton(1, EGG_BTN_CD, 1);
  }
  
  stop() {
    if (this.keyListener) {
      window.removeEventListener("keydown", this.keyListener);
      window.removeEventListener("keyup", this.keyListener);
      this.keyListener = null;
    }
    if (this.gamepadListener) {
      window.removeEventListener("gamepadconnected", this.gamepadListener);
      window.removeEventListener("gamepaddisconnected", this.gamepadListener);
      this.gamepadListener = null;
    }
    this.gamepads = [];
    this.state = [];
  }
  
  update() {
    this.updateGamepads();
  }
  
  /* States and signals.
   ***************************************************************************/
   
  dispatchSignal(signal) {
    switch (signal) {
      case EGG_SIGNAL_QUIT: this.rt.terminate = true; break;
      case EGG_SIGNAL_FULLSCREEN: this.rt.toggleFullscreen(); break;
      case EGG_SIGNAL_PAUSE: this.rt.toggleHardPause(); break;
      case EGG_SIGNAL_STEP: this.rt.step++; break;
      case EGG_SIGNAL_SAVESTATE: this.rt.saveState(); break;
      case EGG_SIGNAL_LOADSTATE: this.rt.loadState(); break;
      case EGG_SIGNAL_SCREENCAP: this.rt.screencap(); break;
      case EGG_SIGNAL_CONFIGIN: this.rt.egg_input_configure(); break;
    }
  }
  
  setPlayerButton(playerid, btnid, value) {
    if (!this.state[playerid]) this.state[playerid] = 0;
    if (value) {
      if (this.state[playerid] & btnid) return;
      this.state[playerid] |= btnid;
    } else {
      if (!(this.state[playerid] & btnid)) return;
      this.state[playerid] &= ~btnid;
    }
    if (playerid) this.setPlayerButton(0, btnid, value);
  }
  
  dropState(playerid, state) {
    if (!state) return;
    for (let btnid=0x8000; btnid; btnid>>=1) {
      if (!(state & btnid)) continue;
      this.setPlayerButton(playerid, btnid, 0);
    }
    //TODO Reassess CDs
  }
  
  /* Keyboard.
   ***************************************************************************/
   
  onKey(event) {
    const btnid = this.keyMap[event.code];
    if (!btnid) return;
    event.preventDefault();
    event.stopPropagation();
    if (event.repeat) return;
    const value = (event.type === "keydown") ? 1 : 0;
    if (btnid < 0x10000) {
      this.setPlayerButton(1, btnid, value);
    } else if (value) {
      this.dispatchSignal(btnid);
    }
  }
  
  /* Gamepad.
   ****************************************************************************/
   
  selectPlayerid() {
    const countByPlayerid = [];
    for (const gp of this.gamepads) {
      if (!countByPlayerid[gp.playerid]) countByPlayerid[gp.playerid] = 0;
      countByPlayerid[gp.playerid]++;
    }
    const limit = 4; // TODO Get from metadata:1 "players"
    let best=1, bestCount=999;
    for (let playerid=1; playerid<=limit; playerid++) {
      const c = countByPlayerid[playerid] || 0;
      if (!c) return playerid;
      if (c < bestCount) {
        best = playerid;
        bestCount = c;
      }
    }
    return best;
  }
   
  onGamepad(event) {
    if (event.type === "gamepadconnected") {
      const gp = {
        id: event.gamepad.id,
        index: event.gamepad.index,
        axes: event.gamepad.axes.map(() => 0),
        buttons: event.gamepad.buttons.map(() => 0),
        playerid: this.selectPlayerid(),
        state: EGG_BTN_CD,
      };
      this.gamepads.push(gp);
      this.setPlayerButton(gp.playerid, EGG_BTN_CD, 1);
    } else {
      const p = this.gamepads.findIndex(gp => gp.index === event.gamepad.index);
      if (p >= 0) {
        const gp = this.gamepads[p];
        this.gamepads.splice(p, 1);
        this.dropState(gp.playerid, gp.state);
      }
    }
  }
  
  updateGamepads() {
    if (this.gamepads.length < 1) return;
    if (!navigator?.getGamepads) return;
    const nstates = navigator.getGamepads();
    for (let i=this.gamepads.length; i-->0; ) {
      const gp = this.gamepads[i];
      const nstate = nstates[gp.index];
      if (!nstate) {
        this.gamepads.splice(i, 1);
        this.dropState(gp.playerid, gp.state);
        continue;
      }
      
      for (let ai=0; ai<2; ai++) {
        const thresh = 0.200;
        const pv = (gp.axes[ai] < -thresh) ? -1 : (gp.axes[ai] > thresh) ? 1 : 0;
        const nv = (nstate.axes[ai] < -thresh) ? -1 : (nstate.axes[ai] > thresh) ? 1 : 0;
        if (pv !== nv) {
          let btnidlo, btnidhi;
          if (ai) {
            btnidlo = EGG_BTN_UP;
            btnidhi = EGG_BTN_DOWN;
          } else {
            btnidlo = EGG_BTN_LEFT;
            btnidhi = EGG_BTN_RIGHT;
          }
          if (pv < 0) this.setPlayerButton(gp.playerid, btnidlo, 0);
          else if (pv > 0) this.setPlayerButton(gp.playerid, btnidhi, 0);
          if (nv < 0) this.setPlayerButton(gp.playerid, btnidlo, 1);
          else if (nv > 0) this.setPlayerButton(gp.playerid, btnidhi, 1);
        }
        gp.axes[ai] = nstate.axes[ai];
      }
      
      for (let bi=0; bi<gp.buttons.length; bi++) {
        const pv = gp.buttons[bi];
        const nv = nstate.buttons[bi].value;
        if (pv === nv) continue;
        gp.buttons[bi] = nv;
        const btnid = this.gamepadStandardMapping[bi];
        if (!btnid) continue;
        this.setPlayerButton(gp.playerid, btnid, nv);
      }
    }
  }
  
  /* Egg Platform API.
   * We don't handle egg_input_configure(); Runtime does that.
   ******************************************************************************/
  
  egg_input_get_all(dstp, dsta) {
    const cpc = Math.min(dsta, this.state.length);
    dstp >>= 2;
    const dst = this.rt.exec.mem32;
    for (let i=0; i<cpc; i++, dstp++) {
      dst[dstp] = this.state[i];
    }
    return cpc;
  }
  
  egg_input_get_one(playerid) {
    return this.state[playerid] || 0;
  }
}