/* PoiModal.js
 * For editing map annotations.
 */
 
import { Dom } from "../Dom.js";

export class PoiModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, "nonce"];
  }
  constructor(element, dom, nonce) {
    this.element = element;
    this.dom = dom;
    this.nonce = nonce;
    
    this.annotation = null;
  }
  
  /* Response will be:
   *  { annotation: verbatim, newText: string } or { delete: true }
   */
  setupExisting(annotation) {
    this.annotation = annotation;
    const cmd = annotation.map.commands.find(c => c.mapCommandId === annotation.mapCommandId);
    this.buildUi(cmd.encode());
  }
  
  /* Response will be:
   *  { newText: string }
   */
  setupNewPositioned(x, y) {
    this.buildUi(`OPCODE @${x},${y}`);
  }
  
  /* Response will be:
   *  { newText: string, createRemote: boolean }
   */
  setupNewDoor(x, y, map, mapStore) {
    this.buildUi(`door @${x},${y} MAPID @${x},${y}`, "createRemote");
  }
  
  buildUi(text, ...options) {
    this.element.innerHTML = "";
    //TODO Fancier UI for known commands?
    
    if (this.annotation) {
      if (this.annotation.opcode === "door:exit") {
        this.dom.spawn(this.element, "DIV", `Editing command for ${this.annotation.map.path}:`);
      } else {
        this.dom.spawn(this.element, "DIV", "Edit command:");
      }
    } else {
      this.dom.spawn(this.element, "DIV", "New command:");
    }
    
    const input = this.dom.spawn(this.element, "INPUT", { type: "text", name: "text", value: text, "on-keydown": e => {
      if (e.code === "Enter") this.onSubmit();
    }});
    
    for (const option of options) {
      switch (option) {
        case "createRemote": {
            const id = `PoiModal-${this.nonce}-createRemote`;
            this.dom.spawn(this.element, "DIV",
              this.dom.spawn(null, "INPUT", { type: "checkbox", id, name: "createRemote" }),
              this.dom.spawn(null, "LABEL", { for: id }, "Create remote")
            );
          } break;
      }
    }
    
    if (this.annotation) {
      this.dom.spawn(this.element, "INPUT", { type: "button", value: "Delete", "on-click": () => this.resolve({ delete: true }) });
    }
    
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "OK", "on-click": () => this.onSubmit() });
    input.focus();
    input.select(0, text.length);
  }
  
  onSubmit() {
    const result = {
      newText: this.element.querySelector("input[name='text']").value,
    };
    if (this.annotation) result.annotation = this.annotation;
    const createRemote = this.element.querySelector("input[name='createRemote']");
    if (createRemote) result.createRemote = createRemote.checked;
    this.resolve(result);
  }
}
