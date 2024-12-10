/* MapEditor.js
 * This is the big kahuna, the main entry point and also coordinator for a bunch of other map-related classes.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { Namespaces } from "../Namespaces.js";
import { MapToolbar } from "./MapToolbar.js";
import { MapCanvas } from "./MapCanvas.js";
import { MapStore } from "./MapStore.js";
import { MapPaint } from "./MapPaint.js";
import { TilesheetEditor } from "./TilesheetEditor.js";

export class MapEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, MapStore, Namespaces, MapPaint, Window];
  }
  constructor(element, dom, data, mapStore, namespaces, mapPaint, window) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.mapStore = mapStore;
    this.namespaces = namespaces;
    this.mapPaint = mapPaint;
    this.window = window;
    
    this.tilesize = 16;
    this.res = null;
    this.map = null;
    this.tilesheet = null;
    this.annotations = [];
    this.mapToolbar = null;
    this.mapCanvas = null;
    
    this.keyListener = e => this.onKey(e);
    this.window.addEventListener("keydown", this.keyListener);
    this.window.addEventListener("keyup", this.keyListener);
    
    try {
      this.workbenchState = JSON.parse(this.window.localStorage.getItem("MapEditorState"));
      if (!this.workbenchState || (typeof(this.workbenchState) !== "object")) throw null;
    } catch (e) {
      this.workbenchState = {};
    }
    if (!this.workbenchState.tool) this.workbenchState.tool = "rainbow";
    if (!this.workbenchState.tile) this.workbenchState.tile = 0;
    if (!this.workbenchState.visibility) this.workbenchState.visibility = ["image", "grid", "poi", "regions"];
    if (!this.workbenchState.zoom) this.workbenchState.zoom = 4;
  }
  
  onRemoveFromDom() {
    if (this.keyListener) {
      this.window.removeEventListener("keydown", this.keyListener);
      this.window.removeEventListener("keyup", this.keyListener);
      this.keyListener = null;
    }
  }
  
  static checkResource(res) {
    if (res.type === "map") return 2;
    return 0;
  }
  
  setup(res) {
    /* The two main UI bits, MapToolbar and MapCanvas, will not be instantiated until our model is stable.
     * I want as little "wait until everything's loaded..." as possible, and ideally do that junk up here instead of places where work gets done.
     */
    this.namespaces.whenLoaded.then(() => {
      this.tilesize = this.namespaces.idFromName("NS", "sys", "tilesize");
      if (this.tilesize < 1) this.tilesize = 16;
      this.res = res;
      this.map = this.mapStore.getMapByPath(res.path);
      if (!this.map) throw new Error(`${JSON.stringify(res.path)} not found in MapStore`);
      this.annotations = this.mapStore.generateAnnotations(this.map);
      this.acquireTilesheet();
      this.mapPaint.setup(this, this.map);
      this.mapToolbar = this.dom.spawnController(this.element, MapToolbar, [this]);
      this.mapCanvas = this.dom.spawnController(this.element, MapCanvas, [this]);
      this.mapToolbar.setup();
    });
  }
  
  acquireTilesheet() {
    this.tilesheet = {};
    const imageName = this.map.getFirstCommand("image");
    if (!imageName) return;
    const imageRes = this.data.resByString(imageName, "image");
    if (!imageRes) return;
    const tsRes = this.data.resv.find(r => ((r.type === "tilesheet") && (r.rid === imageRes.rid)));
    if (!tsRes) return;
    this.tilesheet = TilesheetEditor.decode(tsRes.serial);
  }
  
  dirty() {
    this.mapStore.dirty(this.res.path);
  }
  
  refreshAnnotations() {
    this.annotations = this.mapStore.generateAnnotations(this.map);
    this.mapCanvas.renderSoon();
  }
  
  workbenchStateDirty() {
    this.window.localStorage.setItem("MapEditorState", JSON.stringify(this.workbenchState));
  }
  
  onKey(event) {
    //console.log(`MapEditor.onKey ${event.code}=${event.type==="keydown"?1:0}`);
    
    /* If a modal is open, don't touch it, just get out.
     */
    if (this.window.document.querySelector(".modal")) return;
    
    /* MapPaint likes to know about the shift and control keys.
     */
    if ((event.code === "ShiftLeft") || (event.code === "ShiftRight")) {
      this.mapPaint.setShiftKey(event.type === "keydown");
      this.mapToolbar.setShiftKey(event.type === "keydown");
      return;
    }
    if ((event.code === "ControlLeft") || (event.code === "ControlRight")) {
      this.mapPaint?.setControlKey(event.type === "keydown");
      this.mapToolbar?.setControlKey(event.type === "keydown");
      return;
    }
    
    /* Simple keystroke actions.
     */
    let did = true;
    if (event.type === "keydown") switch (event.code) {
    
      case "Escape": this.mapPaint.dropSelection(); break;
      case "Backspace":
      case "Delete": this.mapPaint.deleteSelection(); break;
      case "Backquote": this.mapToolbar.selectToolIndex(0); break;
      case "Digit1": this.mapToolbar.selectToolIndex(1); break;
      case "Digit2": this.mapToolbar.selectToolIndex(2); break;
      case "Digit3": this.mapToolbar.selectToolIndex(3); break;
      case "Digit4": this.mapToolbar.selectToolIndex(4); break;
      case "Digit5": this.mapToolbar.selectToolIndex(5); break;
      case "Digit6": this.mapToolbar.selectToolIndex(6); break;
      case "Digit7": this.mapToolbar.selectToolIndex(7); break;
      case "Digit8": this.mapToolbar.selectToolIndex(8); break;
      case "Digit9": this.mapToolbar.selectToolIndex(9); break;
      case "Digit0": this.mapToolbar.selectToolIndex(10); break;
    
      default: did = false;
    }
    if (did) {
      event.stopPropagation();
      event.preventDefault();
      return;
    }
  }
  
  /* Any method on this class starting "action_" will appear in MapToolbar's actions menu.
   * If its first argument is "label", return a string to put in the menu.
   ****************************************************************************************/
   
  action_resize(arg) {
    if (arg === "label") return "Resize...";
    this.dom.modalInput("Map size:", `${this.map.w} ${this.map.h}`).then(result => {
      if (!result) return;
      const match = result.match(/^\s*(\d+)\s+(\d+)\s*$/);
      if (!match) return this.dom.modalError("Expected 'W H'");
      this.mapPaint.resize(+match[1], +match[2]);
    }).catch(e => this.dom.modalError(e));
  }
  
  action_healAll(arg) {
    if (arg === "label") return "Heal All";
    let changed = false;
    if (this.mapPaint.selection) {
      const pvselection = this.mapPaint.captureSelection();
      for (const [x, y] of pvselection) {
        if (this.mapPaint.healCell(x, y)) changed = true;
      }
      this.mapPaint.restoreSelection(pvselection);
    } else {
      for (let y=0; y<this.map.h; y++) {
        for (let x=0; x<this.map.w; x++) {
          if (this.mapPaint.healCell(x, y)) changed = true;
        }
      }
    }
    if (changed) {
      this.dirty();
      this.mapCanvas.renderSoon();
    }
  }
}
