/* MapToolbar.js
 * Ribbon at the top of MapEditor with all the tools and such.
 * This is the main dommy part of MapEditor.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { TileModal } from "../TileModal.js";
import { MapEditor } from "./MapEditor.js";
import { MapPaint } from "./MapPaint.js";
import { CommandListModal } from "./CommandListEditor.js";

export class MapToolbar {
  static getDependencies() {
    return [HTMLElement, Dom, MapEditor, "nonce", Data];
  }
  constructor(element, dom, mapEditor, nonce, data) {
    this.element = element;
    this.dom = dom;
    this.mapEditor = mapEditor;
    this.nonce = nonce;
    this.data = data;
    
    this.selectedTileid = this.mapEditor.workbenchState.tile;
    this.image = null;
    this.manualTool = this.mapEditor.workbenchState.tool;
    this.shiftKey = false;
    this.controlKey = false;
    
    this.buildUi();
    
    this.icons = null;
    const img = new Image();
    img.addEventListener("load", () => { this.icons = img; this.renderIcons(); }, { once: true });
    img.src = "/egg-editor-icons.png";
  }
  
  /* We must assume that MapCanvas doesn't exist during construction, because it doesn't.
   * MapEditor will call this as soon as both of the pieces are instantiated.
   */
  setup() {
    this.data.getImageAsync(this.mapEditor.map.getFirstCommand("image")).then(img => {
      this.image = img;
      this.renderPalette();
    }).catch(e => {});
  }
  
  /* The selected tool, accounting for modifier keys and all.
   */
  getToolName() {
    const element = this.element.querySelector(".tools input:checked");
    if (!element) return "";
    return element.value;
  }
  
  selectTile(tileid) {
    this.selectedTileid = tileid;
    this.renderPalette();
    this.mapEditor.workbenchState.tile = tileid;
    this.mapEditor.workbenchStateDirty();
  }
  
  selectToolIndex(p) {
    const tool = MapPaint.TOOLS[p];
    if (!tool) return;
    for (const element of this.element.querySelectorAll(".tools input:checked")) element.checked = false;
    this.element.querySelector(`.tools input[value='${tool.name}']`).checked = true;
    this.onToolChanged();
  }
  
  setShiftKey(v) {
    if (v) {
      if (this.shiftKey) return;
      this.shiftKey = true;
    } else {
      if (!this.shiftKey) return;
      this.shiftKey = false;
    }
    this.checkOverrideTool();
  }
  
  setControlKey(v) {
    if (v) {
      if (this.controlKey) return;
      this.controlKey = true;
    } else {
      if (!this.controlKey) return;
      this.controlKey = false;
    }
    this.checkOverrideTool();
  }
  
  clearTattle() {
    this.element.querySelector(".tattle").innerText = "";
  }
  
  setTattle(x, y) {
    this.element.querySelector(".tattle").innerText = `${x},${y}`;
  }
  
  setNote(msg) {
    this.element.querySelector(".note").innerText = msg;
  }
  
  getVisibility() {
    return Array.from(this.element.querySelectorAll(".visibility input:checked")).map(e => e.name);
  }
  
  checkOverrideTool() {
    let override = this.manualTool;
    if (this.controlKey) {
      switch (this.manualTool) {
        case "verbatim":
        case "rainbow":
        case "monalisa": override = "pickup"; break;
        case "poimove": override = "poiedit"; break;
      }
    }
    for (const element of this.element.querySelectorAll(".tools input:checked")) element.checked = false;
    this.element.querySelector(`.tools input[value='${override}']`).checked = true;
  }
  
  buildUi() {
    this.element.innerHTML = "";
    
    // Palette.
    this.dom.spawn(this.element, "DIV", ["palette"],
      this.dom.spawn(null, "CANVAS", { width: this.mapEditor.tilesize, height: this.mapEditor.tilesize, "on-click": () => this.onPaletteClick() })
    );
    
    // Tools.
    const tools = this.dom.spawn(this.element, "DIV", ["tools"], { "on-click": () => this.onToolChanged() });
    for (const tool of MapPaint.TOOLS) {
      const id = `MapToolbar-${this.nonce}-tool-${tool.name}`;
      this.dom.spawn(tools, "INPUT", { id, type: "radio", value: tool.name, name: "tool" });
      this.dom.spawn(tools, "LABEL", { for: id, "data-icon": tool.icon, "on-click": e => e.stopPropagation() }, tool.name);
    }
    tools.querySelector(`input[value='${this.manualTool}']`).checked = true;
    
    // Tattle and Note.
    this.dom.spawn(this.element, "DIV",
      this.dom.spawn(this.element, "DIV", ["tattle"]),
      this.dom.spawn(this.element, "DIV", ["note"])
    );
    
    // Commands.
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "Commands...", "on-click": () => this.onEditCommands() });
    
    // Visibility and actions.
    const va = this.dom.spawn(this.element, "DIV");
    const visibility = this.dom.spawn(va, "DIV", ["visibility"], { "on-change": () => this.onVisibilityChanged() });
    for (const k of ["image", "grid", "physics", "poi", "regions"]) {
      const id = `MapToolbar-${this.nonce}-visibility-${k}`;
      this.dom.spawn(visibility, "INPUT", { id, type: "checkbox", name: k });
      this.dom.spawn(visibility, "LABEL", { for: id }, k);
    }
    for (const k of this.mapEditor.workbenchState.visibility) {
      const toggle = visibility.querySelector(`input[name='${k}']`);
      if (toggle) toggle.checked = true;
    }
    const actions = this.dom.spawn(va, "SELECT", { name: "actions", "on-change": () => this.onSelectAction() },
      this.dom.spawn(null, "OPTION", { value: "", disabled: "disabled" }, "Actions...")
    );
    actions.value = "";
    for (const k of Object.getOwnPropertyNames(this.mapEditor.__proto__)) {
      if (!k.startsWith("action_")) continue;
      this.dom.spawn(actions, "OPTION", { value: k }, this.mapEditor[k]("label"));
    }
  }
  
  renderPalette() {
    if (!this.image) return;
    const ts = this.mapEditor.tilesize;
    const srcx = (this.selectedTileid & 15) * ts;
    const srcy = (this.selectedTileid >> 4) * ts;
    const canvas = this.element.querySelector(".palette > canvas");
    const ctx = canvas.getContext("2d");
    ctx.drawImage(this.image, srcx, srcy, ts, ts, 0, 0, ts, ts);
  }
  
  renderIcons() {
    for (const label of this.element.querySelectorAll(".tools label")) {
      const iconid = +label.getAttribute("data-icon");
      label.innerHTML = "";
      const canvas = this.dom.spawn(label, "CANVAS", { width: 16, height: 16 });
      const ctx = canvas.getContext("2d");
      ctx.drawImage(this.icons, iconid * 16, 0, 16, 16, 0, 0, 16, 16);
    }
  }
  
  onPaletteClick() {
    const modal = this.dom.spawnModal(TileModal);
    modal.setup(this.mapEditor.map.getFirstCommand("image"), this.selectedTileid);
    modal.result.then(result => {
      if (typeof(result) === "number") {
        this.selectedTileid = result;
        this.renderPalette();
        this.mapEditor.workbenchState.tile = result;
        this.mapEditor.workbenchStateDirty();
      }
    }).catch(e => this.dom.modalError(e));
  }
  
  onEditCommands() {
    const modal = this.dom.spawnModal(CommandListModal);
    modal.setup(this.mapEditor.map.commands);
    modal.result.then(result => {
      if (!result) return;
      this.mapEditor.map.replaceCommands(result);
      this.mapEditor.dirty();
      this.mapEditor.mapCanvas.renderSoon();
      this.mapEditor.refreshAnnotations();
    }).catch(e => this.dom.modalError(e));
  }
  
  onToolChanged() {
    // For most purposes, effective tool is read straight off the UI.
    // But we do need a sense of "which was clicked on last", and we fire off some things on explicit changes.
    const newSelection = this.element.querySelector(".tools input:checked")?.value;
    if (!newSelection) return;
    this.manualTool = newSelection;
    this.mapEditor.mapPaint.onToolChanged(this.manualTool);
    this.mapEditor.workbenchState.tool = this.manualTool;
    this.mapEditor.workbenchStateDirty();
  }
  
  onVisibilityChanged() {
    this.mapEditor.mapCanvas.renderSoon();
    this.mapEditor.workbenchState.visibility = this.getVisibility();
    this.mapEditor.workbenchStateDirty();
  }
  
  onSelectAction() {
    const select = this.element.querySelector("select[name='actions']");
    const action = select.value;
    select.value = "";
    this.mapEditor[action]();
  }
}
