/* RootUi.js
 * Top of the view hierarchy, installed directly on <body>.
 */
 
import { Dom } from "./Dom.js";
import { RomSet } from "./RomSet.js";
import { GameList } from "./GameList.js";

export class RootUi {
  static getDependencies() {
    return [HTMLElement, Dom, Window, RomSet];
  }
  constructor(element, dom, window, romSet) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    this.romSet = romSet;
    
    this.element.addEventListener("dragover", e => this.onDragOver(e));
    this.element.addEventListener("drop", e => this.onDrop(e));
    this.gameList = null;
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const topRow = this.dom.spawn(this.element, "DIV", ["topRow"]);
    this.dom.spawn(topRow, "CANVAS", { id: "egg-canvas" });
    const controls = this.dom.spawn(topRow, "DIV", ["controls"]);
    this.dom.spawn(controls, "DIV", "Drag-and-drop ROM or click here:");
    this.dom.spawn(controls, "INPUT", { type: "file", value: "Add File", "on-input": e => this.onFileChanged(e) });
    //this.dom.spawn(controls, "INPUT", { type: "button", value: "From URL", "on-click": () => this.onFetchRom() });
    this.dom.spawn(controls, "INPUT", { type: "button", value: "Terminate", "on-click": () => this.onTerminate() });
    this.dom.spawn(controls, "INPUT", { type: "button", value: "Fullscreen", "on-click": () => this.onFullscreen() });
    this.dom.spawn(controls, "INPUT", { type: "button", value: "Configure Input", "on-click": () => this.onConfigureInput() });
    //TODO Runtime also exports toggleHardPause, saveState, loadState, and screencap. (mostly not implemented yet).
    this.gameList = this.dom.spawnController(this.element, GameList);
    this.gameList.onLaunch = r => this.onLaunch(r);
  }
  
  onDragOver(event) {
    event.preventDefault();
    event.dataTransfer.dropEffect = "copy";
  }
  
  onDrop(event) {
    event.preventDefault();
    for (const file of event.dataTransfer.files) this.onAddFile(file);
  }
  
  onTerminate() {
    this.window.terminateEgg();
  }
  
  onFileChanged(event) {
    for (const file of event.target.files) this.onAddFile(file);
  }
  
  onFullscreen() {
    if (!this.window.eggRuntime) return;
    this.window.eggRuntime.toggleFullscreen();
  }
  
  onConfigureInput() {
    //TODO We ought to allow this even when no ROM running (ie no Runtime).
    if (!this.window.eggRuntime) return;
    this.window.eggRuntime.egg_input_configure();
  }
  
  onAddFile(file) {
    this.romSet.removeByName(file.name);
    const fileReader = new FileReader();
    fileReader.addEventListener("load", () => {
      try {
        const serial = this.extractSerialFromHtml(fileReader.result);
        this.romSet.add(file.name, serial);
      } catch (e) { this.dom.modalError(e); }
    }, { once: true });
    if (file.type === "text/html") {
      fileReader.readAsText(file);
    } else {
      fileReader.readAsArrayBuffer(file);
    }
  }
  
  onLaunch(rom) {
    this.window.terminateEgg();
    this.window.launchEgg(rom);
  }
  
  /* This appears in the main HTML for Season of Penance on Itch (https://aksommerville.itch.io/the-season-of-penance):
   *   <div data-iframe="&lt;iframe allowfullscreen=&quot;true&quot; src=&quot;https://html-classic.itch.zone/html/12216782/index.html&quot; allow=&quot;autoplay; fullscreen *; 
   *   geolocation; microphone; camera; midi; monetization; xr-spatial-tracking; gamepad; gyroscope; accelerometer; xr; cross-origin-isolated; web-share&quot; allowtransparency=&quot;true&quot; 
   *   webkitallowfullscreen=&quot;true&quot; mozallowfullscreen=&quot;true&quot; msallowfullscreen=&quot;true&quot; scrolling=&quot;no&quot; frameborder=&quot;0&quot; 
   *   id=&quot;game_drop&quot;&gt;&lt;/iframe&gt;" class="iframe_placeholder">
   * (that whole thing is the [data-iframe] attribute)
   * Importantly, the bit after "src" is the actual embedded Egg HTML URL: https://html-classic.itch.zone/html/12216782/index.html
   * ...I'm getting empty responses for the first call. Probably a CORS thing or some kind of security on Itch's end.
   * Which is all good I guess, we're probably outside their ToS anyway.
   * If this isn't going to work against Itch, it's probably not worth doing at all.
   *
  onFetchRom() {
    this.dom.modalInput("URL (eg https://aksommerville.itch.io/the-season-of-penance):", "").then(url => {
      if (!url) return;
      console.log(`Load from ${JSON.stringify(url)}...`);
      return this.window.fetch(url, { mode: "no-cors" });
    }).then(rsp => {
      if (!rsp.ok) throw rsp;
      return rsp.text();
    }).then(html => {
      console.log(`Got first layer of HTML`, html);
      const nextUrl = extractItchInnerUrlFromHtml(html);
      if (!nextUrl) {
        console.log(`Failed to extract Itch Inner URL. Using first layer HTML.`);
        return html;
      }
      console.log(`Fetch inner HTML from ${JSON.stringify(nextUrl)}`);
      return this.window.fetch(nextUrl, { mode: "no-cors" }).then(rsp => {
        if (!rsp.ok) throw rsp;
        return rsp.text();
      });
    }).then(html => {
      console.log(`ok final html, ${html.length} bytes`);
      const serial = this.extractSerialFromHtml(html);
      this.romSet.add(name, serial);
    }).catch(e => {
      console.error(e);
      this.dom.modalError(e);
    });
  }
  /**/
  
  extractSerialFromHtml(src) {
    if (typeof(src) !== "string") return src;
    const openp = src.indexOf("<egg-rom");
    if (openp < 0) throw new Error("<egg-rom> tag not found in HTML file.");
    const startp = src.indexOf(">", openp) + 1;
    const endp = src.indexOf("</egg-rom>", startp);
    if ((startp < 0) || (endp < 0)) throw new Error("Malformed HTML.");
    const b64 = src.substring(startp, endp);
    // No need to decode the base64 at this point; Rom takes care of it.
    return b64;
  }
}
