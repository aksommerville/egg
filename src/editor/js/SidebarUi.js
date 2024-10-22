/* SidebarUi.js
 * Left side of the top level view.
 * Lists all resources, and some extra controls at the top.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";
import { ResOptionsModal } from "./ResOptionsModal.js";
import { Actions } from "./Actions.js";
import { AudioService } from "./AudioService.js";

export class SidebarUi {
  static getDependencies() {
    return [HTMLElement, Dom, Data, Window, Actions, AudioService];
  }
  constructor(element, dom, data, window, actions, audioService) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.window = window;
    this.actions = actions;
    this.audioService = audioService;
    
    this.dataListener = this.data.listenToc(() => this.onTocChanged());
    this.dirtyListener = this.data.listenDirty(state => this.onDirtyStateChanged(state));
    this.audioListener = this.audioService.listen(e => this.onAudioEvent(e));
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    this.data.unlistenToc(this.dataListener);
    this.data.unlistenDirty(this.dirtyListener);
    this.audioService.unlisten(this.audioListener);
  }
  
  setHighlight(path) {
    for (const element of this.element.querySelectorAll("li.highlight")) element.classList.remove("highlight");
    let element = this.element.querySelector(`.typelist li[data-path='${path}']`);
    if (element) {
      element.classList.add("highlight");
      for (; element; element=element.parentNode) {
        if (element.classList.contains("reslist")) {
          element.classList.remove("hidden");
          if (element = element.parentNode?.querySelector(".tname")) {
            element.classList.add("open");
          }
          break;
        }
      }
    }
  }
  
  buildUi() {
    this.element.innerHTML = "";
    
    const actionsMenu = this.dom.spawn(this.element, "SELECT", ["actions"], { "on-change": () => {
      const action = actionsMenu.value;
      actionsMenu.value = "";
      this.actions.op(action);
    }});
    this.dom.spawn(actionsMenu, "OPTION", { disabled: "disabled", value: "" }, "Actions...");
    for (const { op, label } of this.actions.getOps()) {
      this.dom.spawn(actionsMenu, "OPTION", { value: op }, label);
    }
    actionsMenu.value = "";
    
    let nativeOption = null;
    const audioMenu = this.dom.spawn(this.element, "SELECT", ["audio"], { "on-change": () => this.onAudioChanged() },
      this.dom.spawn(null, "OPTION", { value: "none" }, "No audio"),
      nativeOption = this.dom.spawn(null, "OPTION", { value: "server" }, "Native audio"),
      this.dom.spawn(null, "OPTION", { value: "client" }, "Web audio")
    );
    if (!this.audioService.serverAvailable) nativeOption.disabled = true;
    
    const toolbar = this.dom.spawn(this.element, "DIV", ["toolbar"]);
    this.dom.spawn(toolbar, "DIV", ["dirtyState", "clean"])
    this.dom.spawn(toolbar, "INPUT", { type: "button", value: "+", "on-click": () => this.onAddRes() });
    this.dom.spawn(toolbar, "DIV", ["tip"], "Right click for options.");
    
    const typelist = this.dom.spawn(this.element, "UL", ["typelist"]);
  }
  
  onTocChanged() {
  
    // Stash the previous state of open types and highlighted resource.
    const typelist = this.element.querySelector(".typelist");
    const openTypes = Array.from(typelist.querySelectorAll(".reslist"))
      .filter(e => !e.classList.contains("hidden"))
      .map(e => e.getAttribute("data-type"));
    const highlightPath = typelist.querySelector("li.highlight")?.getAttribute("data-path") || "";
    typelist.innerHTML = "";
    
    for (const type of this.data.types) {
      const typeli = this.dom.spawn(typelist, "LI", { "data-type": type });
      this.dom.spawn(typeli, "DIV", ["tname"], type, { "on-click": () => this.toggleType(type) });
      const reslist = this.dom.spawn(typeli, "UL", ["reslist"], { "data-type": type });
      if (!openTypes.includes(type)) reslist.classList.add("hidden");
      const resv = this.data.resv.filter(r => r.type === type);
      resv.sort((a, b) => a.rid - b.rid);
      for (const res of resv) {
        const resli = this.dom.spawn(reslist, "LI", res.path.replace(/^.*\//, ""), {
          "data-path": res.path,
          "on-click": e => this.onResClick(e, res.path),
          "on-mousedown": e => this.onResClick(e, res.path),
          "on-contextmenu": e => { e.preventDefault(); e.stopPropagation(); },
        });
        if (res.path === highlightPath) {
          resli.classList.add("highlight");
          reslist.classList.remove("hidden");
        }
      }
    }
  }
  
  onDirtyStateChanged(state) {
    const indicator = this.element.querySelector(".dirtyState");
    indicator.classList.remove("clean");
    indicator.classList.remove("dirty");
    indicator.classList.remove("saving");
    indicator.classList.add(state);
  }
  
  toggleType(type) {
    const typeli = this.element.querySelector(`.typelist > li[data-type='${type}']`);
    const tname = typeli.querySelector(".tname");
    const reslist = typeli.querySelector(".reslist");
    if (reslist.classList.contains("hidden")) {
      reslist.classList.remove("hidden");
      tname.classList.add("open");
    } else {
      reslist.classList.add("hidden");
      tname.classList.remove("open");
    }
  }
  
  onResClick(event, path) {
    if (event.button === 2) {
      event.stopPropagation();
      event.preventDefault();
      this.onResOptions(path);
    } else if (event.type === "click") {
      const res = this.data.resv.find(r => r.path === path);
      if (res) {
        this.onOpen(res, this.actions.getEditor(res));
      }
    }
  }
  
  onResOptions(path) {
    const res = this.data.resv.find(r => r.path === path);
    if (!res) return;
    // Weird bug in Chrome Linux: If we spawn the modal immediately, preventing the right-click's default doesn't work, a popup menu appears.
    // Kicking the modal instantiation to a future frame seems to fix it.
    this.window.setTimeout(() => {
      const modal = this.dom.spawnModal(ResOptionsModal);
      modal.setup(res);
      modal.result.then((result) => {
        if (!result) return;
        if (result.hasOwnProperty("rename")) this.onRename(res, result.rename);
        else if (result.clazz) this.onOpen(res, result.clazz);
        else throw new Error(`Unexpected result from ResOptionsModal: ${JSON.stringify(result)}`);
      }).catch((error) => this.dom.modalError(error));
    }, 0);
  }
  
  onAddRes() {
    this.dom.modalInput("Path:").then((path) => {
      if (!path) return;
      if (this.data.resv.find(r => r.path === path)) {
        this.dom.modalError(`Path ${JSON.stringify(path)} already in use. Delete it first if you want, we won't automatically.`);
      } else {
        return this.data.newResource(path).then(() => {
          const res = this.data.resv.find(r => r.path === path);
          if (res) {
            this.onOpen(res, this.actions.getEditor(res));
          }
        })
      }
    }).catch(e => this.dom.modalError(e));
  }
  
  // Rename, delete, or noop.
  onRename(res, newPath) {
    newPath = newPath.trim();
    if (newPath === res.path) return;
    if (!newPath) {
      this.data.deleteResource(res.path).then(() => {
      }).catch(error => this.dom.modalError(error));
    } else if (this.data.resv.find(r => r.path === newPath)) {
      this.dom.modalError(`Path ${JSON.stringify(newPath)} already in use. Delete it first if you want, we won't automatically.`);
    } else {
      // Write the new one first, then delete the old one.
      this.data.newResource(newPath, res.serial).then(() => {
        return this.data.deleteResource(res.path);
      }).catch(error => this.dom.modalError(error));
    }
  }
  
  onOpen(res, clazz) {
    
    // Update highlight, and force the type panel is open.
    for (const element of this.element.querySelectorAll("li.highlight")) element.classList.remove("highlight");
    let element = this.element.querySelector(`.typelist li[data-path='${res.path}']`);
    if (element) {
      element.classList.add("highlight");
      for (; element; element=element.parentNode) {
        if (element.classList.contains("reslist")) {
          element.classList.remove("hidden");
          if (element = element.parentNode?.querySelector(".tname")) {
            element.classList.add("open");
          }
          break;
        }
      }
    }
    
    // Load for real by setting location fragment. RootUi will notice.
    this.window.location = "#" + res.path + "?" + clazz.name;
  }
  
  onAudioChanged() {
    const value = this.element.querySelector("select.audio")?.value;
    if (!value) return;
    this.audioService.setOutputMode(value);
  }
  
  onAudioEvent(event) {
    switch (event.id) {
      case "availability": {
          const option = this.element.querySelector("select.audio option[value='server']");
          if (option) option.disabled = !this.audioService.serverAvailable;
        } break;
      case "outputMode": {
          const select = this.element.querySelector("select.audio");
          if (select) select.value = this.audioService.outputMode;
        } break;
    }
  }
}
