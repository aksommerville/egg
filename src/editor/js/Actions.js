/* Actions.js
 * Coordinates the global actions and selection of editors for a given resource.
 * Those things are both overridable. We handle it.
 */
 
import { Custom } from "../override/Custom.js";
import { TextEditor } from "./TextEditor.js";
import { HexEditor } from "./HexEditor.js";
import { ImageEditor } from "./ImageEditor.js";
import { MetadataEditor } from "./MetadataEditor.js";
import { StringsEditor } from "./StringsEditor.js";
import { MsfEditor } from "./MsfEditor.js";
import { EgsEditor } from "./EgsEditor.js";
import { WavEditor } from "./WavEditor.js";
import { MidiEditor } from "./MidiEditor.js";
 
export class Actions {
  static getDependencies() {
    return [Custom];
  }
  constructor(custom) {
    this.ops = [
      ...custom.getActions(),
      //TODO Standard Actions. {op,label,fn}
    ];
    this.editors = [
      ...custom.getEditors(),
      MetadataEditor,
      StringsEditor,
      ImageEditor,
      TextEditor,
      HexEditor,
      MsfEditor,
      EgsEditor,
      WavEditor,
      MidiEditor,
    ];
  }
  
  /* Returns array of {op,label} for all of the global actions.
   */
  getOps() {
    return this.ops;
  }
  
  /* Execute one global action.
   */
  op(id) {
    this.ops.find(o => o.op === id)?.fn?.();
  }
  
  /* Return a list of editor classes for a given resource (from Data.resv: {path,serial}).
   * Array of {clazz,recommended}.
   * (recommended) is boolean, our opinion of whether it would work to open with that editor class.
   * We always return every known editor.
   */
  getEditors(res) {
    return this.editors.map(clazz => ({
      clazz,
      recommended: (clazz.checkResource?.(res) >= 1),
    }));
  }
  
  /* Return one editor class, the one we feel best suits (res).
   * There are generic hex and text editors, so we always have something valid to return.
   */
  getEditor(res) {
    let fallback = null;
    for (const clazz of this.editors) {
      const weight = clazz.checkResource?.(res) || 0;
      if (weight >= 2) return clazz;
      if ((weight >= 1) && !fallback) fallback = clazz;
    }
    // It's not possible for fallback to be unset by this point, but let's be sure.
    return fallback || HexEditor;
  }
  
  editorByName(name) {
    return this.editors.find(e => e.name === name);
  }
}

Actions.singleton = true;
