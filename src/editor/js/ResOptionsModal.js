/* ResOptionsModal.js
 * Resolves with one of:
 *   null: Cancelled.
 *   {rename:string}: Rename or delete.
 *   {clazz}: Open with explicit editor.
 */
 
import { Dom } from "./Dom.js";
import { Actions } from "./Actions.js";
 
export class ResOptionsModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, Actions];
  }
  constructor(element, dom, actions) {
    this.element = element;
    this.dom = dom;
    this.actions = actions;
  }
  
  setup(res) {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", res.path);
    
    this.dom.spawn(this.element, "DIV", ["tip"], "Rename empty to delete.");
    const renameForm = this.dom.spawn(this.element, "FORM", { "on-submit": e => e.preventDefault() });
    const renameInput = this.dom.spawn(renameForm, "INPUT", { name: "path", value: res.path, autofocus: "autofocus" });
    this.dom.spawn(renameForm, "INPUT", { type: "submit", value: "Rename", "on-click": (e) => {
      e.stopPropagation();
      e.preventDefault();
      this.resolve({ rename: renameInput.value });
    }});
    renameInput.focus();
    renameInput.select();
    
    this.dom.spawn(this.element, "DIV", "Open with:");
    const editorList = this.dom.spawn(this.element, "DIV", ["editorList"]);
    for (const { clazz, recommended } of this.actions.getEditors(res)) {
      this.dom.spawn(editorList, "INPUT", {
        type: "button",
        value: clazz.name,
        "on-click": () => this.resolve({ clazz }),
      }, [recommended ? "recommended" : "discouraged"]);
    }
  }
}
