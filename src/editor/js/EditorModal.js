/* EditorModal.js
 * Agnostic wrapper to hold one standard Editor in a standard Modal.
 * We do some voodoo to intercept the editor's calls to Data.dirty(), and return the latest serial via the modal's Promise.
 * The actual Data singleton does not get touched by us or by the editor.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";

class RedirectingData extends Data {
  dirty(path, cb) {
    // Clients depend on being able to spam this. We have to debounce at least a little.
    if (this.rdTimeout) return;
    this.rdTimeout = window.setTimeout(() => {
      this.rdTimeout = null;
      this.serial = cb();
    }, 100);
  }
}

export class EditorModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    this.data = null; 
  }
  
  setupWithResource(clazz, res) {
    this.element.innerHTML = "";
    this.data = new RedirectingData();
    this.data.serial = serial;
    const editor = this.dom.spawnController(this.element, clazz, [this.data]);
    editor.setup(res);
    this.spawnControls();
    return editor;
  }
  
  setupWithSerial(clazz, serial) {
    this.element.innerHTML = "";
    this.data = new RedirectingData();
    this.data.serial = serial;
    const editor = this.dom.spawnController(this.element, clazz, [this.data]);
    editor.setup({ serial });
    this.spawnControls();
    return editor;
  }
  
  spawnControls() {
    const controlsRow = this.dom.spawn(this.element, "DIV", ["controlsRow"]);
    this.dom.spawn(controlsRow, "INPUT", { type: "button", value: "Save", "on-click": () => this.onSave() });
  }
  
  onSave() {
    this.resolve(this.data.serial);
  }
}
