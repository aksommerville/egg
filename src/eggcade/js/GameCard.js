/* GameCard.js
 * UI element under GameList for a single game.
 */
 
import { Dom } from "./Dom.js";

export class GameCard {
  static getDependencies() {
    return [HTMLElement, Dom, Window];
  }
  constructor(element, dom, window) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    
    this.name = "";
    this.rom = null;
    this.metadata = null;
    this.cbLaunch = null;
    
    this.element.addEventListener("click", e => this.onClick(e));
    
    this.buildUi();
  }
  
  setup(rom, name, cbLaunch) {
    this.name = name || "";
    this.rom = rom;
    this.metadata = null;
    this.cbLaunch = cbLaunch;
    this.populateUi();
  }
  
  onClick(event) {
    event.preventDefault();
    event.stopPropagation();
    if (!this.rom) return;
    if (!this.cbLaunch) return;
    this.cbLaunch(this.rom);
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", ["titleRow"],
      this.dom.spawn(null, "IMG", ["icon"]),
      this.dom.spawn(null, "DIV", ["title"], "")
    );
    this.dom.spawn(this.element, "DIV", ["vitals"],
      this.dom.spawn(null, "DIV", ["time"], ""),
      this.dom.spawn(null, "DIV", ["author"], ""),
      this.dom.spawn(null, "DIV", ["genre"], ""),
      this.dom.spawn(null, "DIV", ["players"], ""),
      //this.dom.spawn(null, "DIV", ["tags"], "")
    );
    this.dom.spawn(this.element, "DIV", ["desc"], "");
  }
  
  populateUi() {
    this.element.querySelector(".icon").src = this.getIconSrc();
    this.element.querySelector(".title").innerText = this.reprTitle();
    this.element.querySelector(".time").innerText = this.reprTime();
    this.element.querySelector(".author").innerText = this.reprAuthor();
    this.element.querySelector(".genre").innerText = this.reprGenre();
    this.element.querySelector(".players").innerText = this.reprPlayers();
    this.element.querySelector(".desc").innerText = this.reprDesc();
  }
  
  /* Metadata.
   * Anyone can null out (this.metadata) and the accessors below refresh as needed.
   * Everything in metadata are strings prepared for display.
   ****************************************************************************/
   
  getIconSrc() { this.requireMetadata(); return this.metadata.iconSrc; }
  reprTitle() { this.requireMetadata(); return this.metadata.title; }
  reprTime() { this.requireMetadata(); return this.metadata.time; }
  reprAuthor() { this.requireMetadata(); return this.metadata.author; }
  reprGenre() { this.requireMetadata(); return this.metadata.genre; }
  reprPlayers() { this.requireMetadata(); return this.metadata.players; }
  reprDesc() { this.requireMetadata(); return this.metadata.desc; }
  
  requireMetadata() {
    if (this.metadata) return;
    this.metadata = {
      iconSrc: "",
      title: this.name || "",
      time: "",
      author: "",
      genre: "",
      players: "",
      desc: "",
    };
    if (!this.rom) return;
    
    const iconImageId = this.rom.getMetadata("iconImage");
    if (iconImageId) this.metadata.iconSrc = this.dataUrlFromResource(this.rom.getResource(Rom.TID_image, +iconImageId));
    
    const lang = this.window.eggRuntime?.lang || 141; // 141 == "en"
    const ifPresent = (k, repr) => {
      const v = this.rom.getMetadata(k, lang);
      if (v) this.metadata[k] = repr(v);
    };
    ifPresent("title", v => this.sanitizeTitle(v));
    ifPresent("time", v => this.sanitizeTime(v));
    ifPresent("author", v => this.sanitizeAuthor(v));
    ifPresent("genre", v => this.sanitizeGenre(v));
    ifPresent("players", v => this.sanitizePlayers(v));
    ifPresent("desc", v => this.sanitizeDesc(v));
  }
  
  dataUrlFromResource(serial) {
    if (serial?.length) {
      const imageDecoder = new this.window.ImageDecoder(); // Egg bootstrap installs this on window.
      if (imageDecoder.isPng(serial)) {
        return "data:image/png;base64," +  this.rom.encodeBase64(serial);
      }
    }
    return "";
  }
  
  sanitizeTitle(src) {
    if (src.length > 30) return src.substring(0, 25) + "...";
    return src;
  }
  
  sanitizeTime(src) {
    const yearMatch = src.match(/^(\d{4})/);
    if (yearMatch) return yearMatch[1];
    return "";
  }
  
  sanitizeAuthor(src) {
    if (src.length > 30) return src.substring(0, 25) + "...";
    return src;
  }
  
  sanitizeGenre(src) {
    if (src.length > 30) return src.substring(0, 25) + "...";
    return src;
  }
  
  sanitizePlayers(src) {
    // Nominally "N" or "N..N". We'll also allow omitted endpoints.
    const sepp = src.indexOf("..");
    if (sepp >= 0) {
      const lo = +src.substring(0, sepp) || 1;
      const hi = +src.substring(sepp + 2) || 8;
      if (lo < hi) return `${lo}-${hi}p`;
    }
    if (src.match(/^\d{1,2}$/)) return src + "p";
    return "";
  }
  
  sanitizeDesc(src) {
    return src;
  }
}
