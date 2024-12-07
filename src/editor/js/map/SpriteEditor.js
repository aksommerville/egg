/* SpriteEditor.js
 * Sprites are just command lists, with a couple standard fields.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { CommandListEditor } from "./CommandListEditor.js";

export class SpriteEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    
    this.res = null;
    this.commandList = null;
  }
  
  static checkResource(res) {
    if (res.type === "sprite") return 2;
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.element.innerHTML = "";
    this.commandList = this.dom.spawnController(this.element, CommandListEditor);
    this.commandList.clientValidate = src => this.onValidate(src);
    this.commandList.clientAssist = src => this.onAssist(src);
    this.commandList.setup(res);
  }
  
  onAssist(words) {
    console.log(`SpriteEditor.onAssist ${JSON.stringify(words)}`);
    //TODO image picker
    //TODO tile picker
    //TODO ...or maybe these should belong to some outer non-generic ui
    //TODO In any case, we definitely want a preview of the tile, and a clicky interface to pick image and tile.
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
}
