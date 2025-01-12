import { Injector } from "./js/Injector.js";
import { Dom } from "./js/Dom.js";
import { RootUi } from "./js/RootUi.js";

window.addEventListener("load", () => {
  const injector = new Injector(window, document);
  const dom = injector.getInstance(Dom);
  const rootUi = dom.spawnController(document.body, RootUi);
}, { once: true });
