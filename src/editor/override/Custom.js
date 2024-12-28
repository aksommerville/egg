/* Custom.js
 * This exists only to be overridden by games' editors.
 */
 
export class Custom {
  static getDependencies() {
    return [];
  }
  constructor() {
  }
  
  /* Return an array of {
   *   op: string // unique
   *   label: string // for display
   *   fn: () => void
   * }
   * Return in the order you want them to appear. Custom actions come before standard ones in the menu.
   */
  getActions() {
    return [];
  }
  
  /* Array of class implementing:
   *   static checkResource({path,type,rid,serial}): (false,1,2) = (no,yes,prefer)
   *   setup({path,type,rid,serial})
   *
   * These should inject Data and inform it when dirty.
   * See egg/src/editor/js/TextEditor.js for a nice simple example (and ignore its checkResource; yours should be much simpler).
   *
   * Order matters! When searching for a default editor, the first to return >=2 wins, then the first to return 1.
   * Standard editors will only win if all of your editors return false at checkResource().
   *
   * In most cases, checkResource() should just return 2 if type matches and 0 otherwise.
   * Doing a more involved format check is of course possible but kind of pointless, that's what type is for.
   */
  getEditors() {
    return [];
  }
  
  /* To get custom point commands appearing in MapEditor instead of the "?" tile,
   * pick off your interesting annotations (usually by annotiation.opcode), and return a Canvas or Image.
   * You can't override things the editor knows about: sprite, door, door:exit
   */
  renderMapEditorAnnotation(annotation, command, map) {
    return null;
  }
}

Custom.singleton = true;
