/* RootUi.js
 * Top of the view hierarchy, lives forever.
 * We'll be in charge of high-level coordination.
 * Actual UI stuff, keep it to a minimum. Defer to SidebarUi whenever possible.
 */
 
import { Dom } from "./Dom.js";
import { SidebarUi } from "./SidebarUi.js";
import { Data } from "./Data.js";
import { Actions } from "./Actions.js";

export class RootUi {
  static getDependencies() {
    return [HTMLElement, Dom, Window, Data, Actions];
  }
  constructor(element, dom, window, data, actions) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    this.data = data;
    this.actions = actions;
    
    this.sidebar = null;
    this.editor = null;
    
    this.buildUi();
    
    this.hashListener = e => this.onHashChange(e.newURL?.split('#')[1] || "");
    this.window.addEventListener("hashchange", this.hashListener);
  }
  
  onRemoveFromDom() {
    this.window.removeEventListener("hashchange", this.hashListener);
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.sidebar = this.dom.spawnController(this.element, SidebarUi);
    this.dom.spawn(this.element, "DIV", ["workspace"]);
    this.editor = null;
  }
  
  // Our bootstrap will call this after the TOC is loaded. It is not loaded when our ctor runs.
  openInitialResource() {
    const hash = this.window.location.hash?.substring(1) || "";
    this.onHashChange(hash);
  }
  
  onHashChange(path) {
    const workspace = this.element.querySelector(".workspace");
    workspace.innerHTML = "";
    this.editor = null;
    let arg = [];
    const qp = path.indexOf("?");
    if (qp >= 0) {
      arg = path.substring(qp + 1).split(',');
      path = path.substring(0, qp);
    }
    const res = this.data.resv.find(r => r.path === path);
    if (!res) return;
    this.sidebar.setHighlight(path);
    const clazz = this.actions.editorByName(arg[0]) || this.actions.getEditor(res);
    this.editor = this.dom.spawnController(workspace, clazz);
    this.editor.setup(res);
  }
}
