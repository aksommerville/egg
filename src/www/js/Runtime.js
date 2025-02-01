/* Runtime.js
 * Top level coordinator.
 */

import { Rom } from "./Rom.js"; 
import { Exec } from "./Exec.js";
import { Video } from "./Video.js";
import { Audio } from "./synth/Audio.js";
import { Input } from "./Input.js";
import { Incfg } from "./Incfg.js";

/* The ideal update interval is 1/60 seconds, about 0.017.
 * If they come in too fast, we skip some frames.
 * If they come in too slow, we lie about the elapsed time and let the game run slow.
 */
const MINIMUM_UPDATE_TIME = 0.010;
const MAXIMUM_UPDATE_TIME = 0.040;

export class Runtime {
  constructor(rom, canvas) {
    this.rom = rom;
    this.canvas = canvas;
    this.exec = new Exec(this);
    this.video = new Video(this);
    this.audio = new Audio(this);
    this.input = new Input(this);
    this.incfg = null; // Instance of Incfg if configuring.
    this.status = "new"; // "new" "loaded" "running" "stopped"
    this.exitStatus = 0;
    this.terminate = false;
    this.lang = this.langEval("en"); // It has to start somewhere. Will revise at load.
    this.pendingUpdate = null;
    this.lastFrameTime = 0; // s
    this.storePrefix = (this.rom.getMetadata("title") || "eggGame") + ".";
    this.hardPause = false;
    this.step = 0; // >0 to advance so many video frames despite (hardPause).
  }
  
  /* Public: Startup and shutdown.
   ***********************************************************************/
  
  load() {
    this.selectDefaultLanguage();
    this.replaceTitleAndIcon();
    this.video.load();
    const code1 = this.rom.getResource(Rom.TID_code, 1);
    return this.exec.load(code1).then(() => {
      this.status = "loaded";
    });
  }
  
  start() {
    if (this.status !== "loaded") throw new Error(`Can't start from state ${JSON.stringify(this.status)}`);
    this.video.start();
    this.input.start();
    this.audio.start();
    const err = this.exec.egg_client_init();
    if (err < 0) throw new Error(`egg_client_init() failed with error ${err}`);
    this.status = "running";
    if (this.terminate) return this.stop();
    this.lastFrameTime = Date.now() / 1000;
    this.scheduleUpdate();
  }
  
  stop() {
    if (this.incfg) {
      this.incfg.cancel();
      this.incfg = null;
    }
    if (this.pendingUpdate) {
      window.cancelAnimationFrame(this.pendingUpdate);
      this.pendingUpdate = null;
    }
    if (this.status === "running") {
      this.exec.egg_client_quit(this.exitStatus);
      this.video.stop();
      this.audio.stop();
      this.input.stop();
    }
    this.status = "stopped";
  }
  
  toggleFullscreen() {
    if (!this.canvas) return;
    // Browser should provide its own means of leaving fullscreen (eg Esc). If we got this request, assume it's *to* fullscreen only.
    this.canvas.requestFullscreen();
  }
  
  toggleHardPause() {
    this.hardPause = !this.hardPause;
    this.step = 0;
  }
  
  saveState() {
    console.log(`TODO Runtime.saveState`);
  }
  
  loadState() {
    console.log(`TODO Runtime.loadState`);
  }
  
  screencap() {
    console.log(`TODO Runtime.screencap`);
  }
  
  /* Private: Maintenance.
   ***********************************************************************/
   
  scheduleUpdate() {
    this.pendingUpdate = window.requestAnimationFrame(() => this.update());
  }
  
  update() {
    this.pendingUpdate = null;
    if (this.terminate) return this.stop();
    if (this.status !== "running") return this.stop();
    
    // Update Audio before the other things -- it is not subject to hard-pause, and regulates its own timing.
    this.audio.update();
    
    // Update clock, and maybe skip a frame.
    const now = Date.now() / 1000;
    let elapsed = now - this.lastFrameTime;
    if (elapsed < 0) {
      // Impossible, clock must be broken.
      elapsed = 0.016;
    } else if (elapsed < MINIMUM_UPDATE_TIME) {
      // Too short, eg high-frequency monitor. Skip this frame.
      return this.scheduleUpdate();
    } else if (elapsed > MAXIMUM_UPDATE_TIME) {
      // Clamp to the longest reasonable interval.
      elapsed = MAXIMUM_UPDATE_TIME;
    }
    this.lastFrameTime = now;
    
    // Enforce hard-pause.
    if (this.hardPause) {
      if (this.step < 1) return this.scheduleUpdate();
      this.step--;
    }
    
    // Update all the things.
    if (this.incfg) {
      if (!this.incfg.update(elapsed)) {
        this.incfg = null;
        this.input.start();
      }
    } else {
      this.input.update();
      this.exec.egg_client_update(elapsed);
      if (this.terminate) return this.stop(); // Client-initiated termination. Don't bother rendering.
      this.video.beginFrame();
      this.exec.egg_client_render();
      this.video.endFrame();
    }
    
    this.scheduleUpdate();
  }
  
  /* Private: Setup.
   ********************************************************************/
  
  replaceTitleAndIcon() {
    const title = this.rom.getMetadata("title", this.lang);
    if (title) document.title = title;
    const iconRid = +this.rom.getMetadata("iconImage", 0);
    if (iconRid) {
      const serial = this.rom.getResource(Rom.TID_image, iconRid);
      const b64 = this.rom.encodeBase64(serial);
      for (const element of document.querySelectorAll("link[rel='icon']")) element.remove();
      const element = document.createElement("LINK");
      element.setAttribute("rel", "icon");
      element.setAttribute("href", "data:image/png;base64," + b64);
      document.head.appendChild(element);
    }
  }
  
  selectDefaultLanguage() {
    const user = Array.from(new Set(
      (navigator.languages?.map(l => this.langEval(l)) || [])
        .concat(this.langEval(navigator.language)).filter(v => v)
    ));
    const game = this.rom.getMetadata("lang").split(',').map(v => this.langEval(v.trim()));
    // Both lists are hopefully in preference order.
    // Use the first from (user) which is present anywhere in (game).
    for (const lang of user) {
      if (game.includes(lang)) {
        this.lang = lang;
        return;
      }
    }
    // None? OK, that happens. Use the game's most preferred.
    if (game.length > 0) {
      this.lang = game[0];
      return;
    }
    // Game didn't express a preference. Whatever, use the user's first preference.
    if (user.length > 0) {
      this.lang = user[0];
      return;
    }
    // Neither user nor game expressed a preference, so leave it unset. (it initializes to English)
  }
  
  langEval(src) {
    if (!src || (src.length < 2)) return 0;
    const hi = src.charCodeAt(0) - 0x61;
    const lo = src.charCodeAt(1) - 0x61;
    if ((hi < 0) || (hi >= 26) || (lo < 0) || (lo >= 26)) return 0;
    return (hi << 5) | lo;
  }
  
  langRepr(lang) {
    const hi = 0x61 + ((lang >> 5) & 0x1f);
    const lo = 0x61 + (lang & 0x1f);
    return String.fromCharCode(hi) + String.fromCharCode(lo);
  }
  
  /* Egg Platform API (partial).
   ******************************************************************************/
  
  egg_log(msgp) {
    const msg = this.exec.getString(msgp);
    console.log("GAME: " + msg);
  }
  
  egg_terminate(status) {
    this.exitStatus = status;
    this.terminate = true;
  }
  
  egg_time_local(dstp, dsta) {
    if (dsta < 1) return;
    if (dsta > 7) dsta = 7;
    if ((dstp < 0) || (dstp > this.exec.mem8.length - dsta * 4)) return;
    const now = new Date();
    dstp >>= 2;
    this.exec.mem32[dstp++] = now.getFullYear(); if (dsta < 2) return;
    this.exec.mem32[dstp++] = 1 + now.getMonth(); if (dsta < 3) return;
    this.exec.mem32[dstp++] = now.getDate(); if (dsta < 4) return;
    this.exec.mem32[dstp++] = now.getHours(); if (dsta < 5) return;
    this.exec.mem32[dstp++] = now.getMinutes(); if (dsta < 6) return;
    this.exec.mem32[dstp++] = now.getSeconds(); if (dsta < 7) return;
    this.exec.mem32[dstp] = now.getMilliseconds();
  }
  
  egg_set_language(lang) {
    if (lang === this.lang) return;
    if (lang & ~0x3ff) return;
    const hi = lang >> 5;
    const lo = lang & 0x1f;
    if ((hi >= 26) || (lo >= 26)) return;
    console.log(`Runtime.egg_set_language: ${this.lang} => ${lang} per game request`);
    this.lang = lang;
    const title = this.rom.getMetadata("title", this.lang);
    if (title) document.title = title;
    //TODO Will anything else need updated too?
  }
  
  egg_get_rom(dstp, dsta) {
    const dst = this.exec.getMemory(dstp, dsta);
    if (dst && (dst.length >= this.rom.serial.length)) {
      dst.set(this.rom.serial);
    }
    return this.rom.serial.length;
  }
  
  egg_store_get(vp, va, kp, kc) {
    const k = this.exec.getString(kp, kc);
    if (!k) return 0;
    const v = localStorage.getItem(this.storePrefix + k) || "";
    return this.exec.setString(vp, va, v);
  }
  
  egg_store_set(kp, kc, vp, vc) {
    const k = this.exec.getString(kp, kc);
    if (!k) return -1;
    const v = this.exec.getString(vp, vc);
    if (v) {
      localStorage.setItem(this.storePrefix + k, v);
    } else {
      localStorage.removeItem(this.storePrefix + k);
    }
    return 0;
  }
  
  egg_store_key_by_index(kp, ka, p) {
    if (p < 0) return 0;
    for (let i=0; ; i++) {
      const k = localStorage.key(i);
      if (!k) return 0;
      if (!k.startsWith(this.storePrefix)) continue;
      if (p--) continue;
      return this.exec.setString(kp, ka, k.substring(this.storePrefix.length));
    }
    return 0;
  }
  
  egg_input_configure() {
    if (this.incfg) return 0;
    this.incfg = new Incfg(this);
    this.input.stop();
    return 0;
  }
}
