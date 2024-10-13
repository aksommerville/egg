/* WavEditor.js
 * We're not going deep on this one, but do show the levels and allow simple adjustments like dropping channels and truncating silence.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";
 
export class WavEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
  }
  
  static checkResource(res) {
    if (
      (res.serial.length >= 12) &&
      (res.serial[0] === 0x52) && // "RIFF"
      (res.serial[1] === 0x49) &&
      (res.serial[2] === 0x46) &&
      (res.serial[3] === 0x46) &&
      (res.serial[8] === 0x57) && // "WAVE"
      (res.serial[9] === 0x41) &&
      (res.serial[10] === 0x56) &&
      (res.serial[11] === 0x45)
    ) return 2;
    return 0;
  }
  
  setup(res) {
    console.log(`WavEditor.setup`, res);
  };
}
