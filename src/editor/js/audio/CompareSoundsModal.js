/* CompareSoundsModal.js
 * Shows all sound resources, with two "play" buttons for each.
 * One button plays it directly in the web synthesizer.
 * The other sends it to the server to be printed by the native synthesizer, then plays the output client-side.
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
import { Data } from "../Data.js";
import { AudioService } from "./AudioService.js";
 
export class CompareSoundsModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, Comm, Data, AudioService];
  }
  constructor(element, dom, comm, data, audioService) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    this.data = data;
    this.audioService = audioService;
    
    this.audioService.setOutputMode("client");
    
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const scroller = this.dom.spawn(this.element, "DIV", ["scroller"]);
    const resv = this.data.resv.filter(r => ((r.type === "sound") || (r.type === "song")));
    resv.sort((a, b) => {
      if (a.type < b.type) return -1;
      if (a.type > b.type) return 1;
      if (a.rid < b.rid) return -1;
      if (a.rid > b.rid) return 1;
      return 0;
    });
    for (const res of resv) {
      this.spawnSoundRow(scroller, res);
    }
  }
  
  spawnSoundRow(parent, res) {
    const row = this.dom.spawn(parent, "DIV", ["row"]);
    this.dom.spawn(row, "INPUT", { type: "button", value: "Native", "on-click": () => this.playNative(res) });
    this.dom.spawn(row, "INPUT", { type: "button", value: "Web", "on-click": () => this.playWeb(res) });
    this.dom.spawn(row, "DIV", ["name"], res.path);
  }
  
  playNative(res) {
    // Send it to the server first, not just to compile, but to print PCM for us.
    this.comm.httpBinary("POST", "/api/synth", { rate: 44100 }, null, res.serial).then(rsp => {
      this.audioService.play(rsp);
    }).catch(e => this.dom.modalError(e));
  }
  
  playWeb(res) {
    // This is easy, it's what AudioService was already built for (note that we set outputMode "client" at init).
    this.audioService.play(res.serial);
  }
}
