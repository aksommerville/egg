import { Injector } from "./js/Injector.js";
import { Dom } from "./js/Dom.js";
import { RootUi } from "./js/RootUi.js";
import { Data } from "./js/Data.js";

window.addEventListener("load", () => {
  const injector = new Injector(window, document);
  const dom = injector.getInstance(Dom);
  const rootUi = dom.spawnController(document.body, RootUi);
  const data = injector.getInstance(Data);
  data.load().then(() => {
    rootUi.openInitialResource();
  }).catch(error => {
    console.log(`error fetching resources`, error);
  });
}, { once: true });
