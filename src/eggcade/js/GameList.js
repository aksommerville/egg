/* GameList.js
 * UI element covering the bottom half of RootUi, shows a list of all available ROMs.
 */
 
import { RomSet } from "./RomSet.js";
import { Dom } from "./Dom.js";
import { GameCard } from "./GameCard.js";

export class GameList {
  static getDependencies() {
    return [HTMLElement, Dom, RomSet, Window];
  }
  constructor(element, dom, romSet, window) {
    this.element = element;
    this.dom = dom;
    this.romSet = romSet;
    this.window = window;
    
    // Owner may replace directly:
    this.onLaunch = (rom) => {};
    
    this.romSetListener = this.romSet.listen(e => this.onRomSetChanged(e));
    this.onRomSetChanged(this.romSet.roms);
  }
  
  onRemoveFromDom() {
    this.romSet.unlisten(this.romSetListener);
  }
  
  onRomSetChanged(roms) {
    this.element.innerHTML = "";
    for (const base of Object.keys(roms)) {
      const rom = roms[base];
      const gameCard = this.dom.spawnController(this.element, GameCard);
      gameCard.setup(rom, base, r => this.onLaunch(r));
    }
  }
}
