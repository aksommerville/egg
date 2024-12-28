/* SpriteEditor.js
 * Sprites are just command lists, with a couple standard fields.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { ImagePickerModal } from "../ImagePickerModal.js";
import { TilePickerModal } from "../TilePickerModal.js";
import { CommandListEditor } from "./CommandListEditor.js";

export class SpriteEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, Window];
  }
  constructor(element, dom, data, window) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.window = window;
    
    this.res = null;
    this.commandList = null;
    this.previewTimeout = null;
  }
  
  onRemoveFromDom() {
    if (this.previewTimeout) {
      this.window.clearTimeout(this.previewTimeout);
      this.previewTimeout = null;
    }
  }
  
  static checkResource(res) {
    if (res.type === "sprite") return 2;
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.element.innerHTML = "";
    this.preview = this.dom.spawn(this.element, "CANVAS", ["preview"], { "on-click": () => this.commandList.assist("tile") });
    this.commandList = this.dom.spawnController(this.element, CommandListEditor);
    this.commandList.clientValidate = src => this.onValidate(src);
    this.commandList.clientAssist = (rowid, words) => this.onAssist(rowid, words);
    this.commandList.clientChange = (rowid, words) => this.onChange(rowid, words);
    this.commandList.setup(res);
    this.renderPreviewSoon();
  }
  
  onAssist(rowid, words) {
    switch (words[0]) {
      case "image": {
          const modal = this.dom.spawnModal(ImagePickerModal);
          modal.result.then(imageName => {
            if (!imageName) return;
            const ncmd = [...words];
            ncmd[1] = "image:" + imageName;
            this.commandList.setValue(rowid, ncmd);
          }).catch(e => this.dom.modalError(e));
        } break;
      case "tile": {
          const modal = this.dom.spawnModal(TilePickerModal);
          modal.setup(this.commandList.getFirstParams("image"));
          modal.result.then(tileid => {
            if (tileid === null) return;
            const ncmd = [...words];
            ncmd[1] = "0x" + tileid.toString(16).padStart(2, "0");
            this.commandList.setValue(rowid, ncmd);
          }).catch(e => this.dom.modalError(e));
        } break;
    }
  }
  
  onValidate(words) {
    switch (words[0]) {
      case "image": {
          if (words.length !== 2) return "'image' takes one argument: imageid";
          if (!words[1].match(/^image:[a-zA-Z0-9_]+$/)) return "First argument to 'image' is normally 'image:NAME'";
        } break;
      case "tile": {
          if (words.length !== 3) return "'tile' takes two arguments: tileid xform";
          const tileid = +words[1];
          const xform = +words[2];
          if (isNaN(tileid) || (tileid < 0) || (tileid > 0xff)) return `Expected tileid in 0..255, found ${JSON.stringify(words[1])}`;
          if (isNaN(xform) || (xform < 0) || (xform > 7)) return `Expected xform in 0..7, found ${JSON.stringify(words[2])}`;
        } break;
    }
    return "";
  }
  
  onChange(rowid, words) {
    switch (words[0]) {
      case "image":
      case "tile":
        this.renderPreviewSoon();
    }
  }
  
  renderPreviewSoon() {
    if (this.previewTimeout) return;
    this.previewTimeout = this.window.setTimeout(() => {
      this.previewTimeout = null;
      this.renderPreviewNow();
    }, 100);
  }
  
  renderPreviewNow() {
    const canvas = this.element.querySelector(".preview");
    const imageName = this.commandList.getFirstParams("image");
    let tileid=0, xform=0;
    const tileAndXform = this.commandList.getFirstParams("tile");
    const txmatch = tileAndXform.match(/^\s*([^\s]+)(\s+([^\s]+))?\s*$/);
    if (txmatch) {
      tileid = +txmatch[1];
      xform = +txmatch[3] || 0;
    }
    this.data.getImageAsync(imageName).then(src => {
      this.renderRealPreview(canvas, src, tileid, xform);
    }).catch(() => {
      this.renderNoPreview(canvas);
    });
  }
  
  renderNoPreview(canvas) {
    canvas.width = 1;
    canvas.height = 1;
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#f00";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
  }
  
  renderRealPreview(canvas, src, tileid, xform) {
    const tilesize = src.naturalWidth >> 4;
    const srcx = (tileid & 0x0f) * tilesize;
    const srcy = (tileid >> 4) * tilesize;
    canvas.width = tilesize;
    canvas.height = tilesize;
    const ctx = canvas.getContext("2d");
    ctx.clearRect(0, 0, tilesize, tilesize);
    ctx.save();
    switch (xform & 7) {
      case 0: ctx.setTransform(1, 0, 0, 1, 0, 0); break; // No xform.
      case 1: ctx.setTransform(-1, 0, 0, 1, tilesize, 0); break; // XREV
      case 2: ctx.setTransform(1, 0, 0, -1, 0, tilesize); break; // YREV
      case 3: ctx.setTransform(-1, 0, 0, -1, tilesize, tilesize); break; // XREV|YREV ie 180
      case 4: ctx.setTransform(0, 1, 1, 0, 0, 0); break; // SWAP
      case 5: ctx.setTransform(0, -1, 1, 0, 0, tilesize); break; // SWAP|XREV
      case 6: ctx.setTransform(0, 1, -1, 0, tilesize, 0); break; // SWAP|YREV
      case 7: ctx.setTransform(0, -1, -1, 0, tilesize, tilesize); break; // SWAP|XREV|YREV
    }
    ctx.drawImage(src, srcx, srcy, tilesize, tilesize, 0, 0, tilesize, tilesize);
    ctx.restore();
  }
}
