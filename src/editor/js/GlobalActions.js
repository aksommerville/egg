/* GlobalActions.js
 * One-off actions that can be taken from anywhere in the app.
 * These get listed by SidebarUi, at the time. Mixed with custom ones from the game.
 */
 
import { Resmgr } from "./Resmgr.js";
import { Bus } from "./Bus.js";
import { Dom } from "./Dom.js";
import { ServerAudioModal } from "./ServerAudioModal.js";
import { Comm } from "./Comm.js";
 
export class GlobalActions {
  static getDependencies() {
    return [Resmgr, Bus, Dom, Window, Comm];
  }
  constructor(resmgr, bus, dom, window, comm) {
    this.resmgr = resmgr;
    this.bus = bus;
    this.dom = dom;
    this.window = window;
    this.comm = comm;
  }
  
  list() {
    return [
      { name: "Server-side audio", value: "serverSideAudio", action: () => this.serverSideAudio() },
      { name: "Missing Strings", value: "missingStrings", action: () => this.missingStrings() },
    ];
  }
  
  /* Server-side audio.
   ***************************************************************************/
   
  serverSideAudio() {
    const modal = this.dom.spawnModal(ServerAudioModal);
    modal.result.then((rsp) => {
      return this.comm.httpJson("POST", "/api/audio", null, null, JSON.stringify(rsp)).then(hrsp => {
        console.log(`Initialized audio`, hrsp);
      });
    }).catch(error => {
      if (error) this.bus.broadcast({ type: "error", error });
    });
  }
  
  /* Missing Strings.
   **************************************************************************/
  
  missingStrings() {
    const declaredLanguages = this.resmgr.getMetadata("language").split(",").map(v => v.trim()).filter(v => v);
    const existing = {}; // key:language, value:Set(id) where length>0
    for (const file of this.bus.toc) {
      if (file.type !== "string") continue;
      if (!existing[file.qual]) existing[file.qual] = new Set();
      this.addStringIdsToList(existing[file.qual], file.serial);
    }
    
    const errors = [];
    for (const lang of declaredLanguages) {
      if (!existing[lang]) errors.push(`Language ${JSON.stringify(lang)} declared in metadata but has no strings.`);
    }
    for (const lang of Object.keys(existing)) {
      if (declaredLanguages.indexOf(lang) < 0) {
        errors.push(`Language ${JSON.stringify(lang)} has ${existing[lang].size} strings but is not declared in metadata.`);
      }
    }
    let allIds = new Set();
    for (const lang of Object.keys(existing)) {
      allIds = allIds.union(existing[lang]);
    }
    for (const lang of Object.keys(existing)) {
      let diff = allIds.difference(existing[lang]);
      if (!diff.size) continue;
      diff = Array.from(diff);
      if (diff.length > 4) { // 4 completely arbitrary, threshold where we report count instead of enumerating.
        errors.push(`Language ${JSON.stringify(lang)} missing ${diff.length} strings.`);
      } else {
        errors.push(`Language ${JSON.stringify(lang)} missing: ${diff.join(' ')}`);
      }
    }
    
    if (errors.length < 1) {
      this.dom.modalMessage(`Valid. ${allIds.size} strings in ${declaredLanguages.length} languages.`);
    } else {
      this.dom.modalMessage(errors.join("\n"));
    }
  }
  
  addStringIdsToList(dst, serial) {
    const src = new this.window.TextDecoder("utf8").decode(serial);
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      if (!line) continue;
      if (line.startsWith("#")) continue;
      const match = line.match(/^([^\s]*)\s+(.*)$/);
      if (!match) continue;
      const id = match[1];
      const v = match[2].match(/^".*"$/) ? JSON.parse(match[2]) : match[2];
      if (!v.length) continue; // Empty is the same as missing.
      dst.add(id);
    }
  }
}

GlobalActions.singleton = true;
