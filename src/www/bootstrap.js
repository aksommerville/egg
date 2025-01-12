import { Rom } from "./js/Rom.js";
import { Runtime } from "./js/Runtime.js";
import { ImageDecoder } from "./js/ImageDecoder.js";

const DEFAULT_ROM_PATH = "/demo.egg";

let runtime = null;

function reprError(error) {
  if (!error) return "A fatal error occurred (no detail).";
  if (typeof(error) === "string") return error;
  if (error.stack) return error.stack;
  if (error.message) return error.message;
  return JSON.stringify(error, null, 2);
}

function reportError(error) {
  console.error(error);
  const element = document.querySelector(".error");
  if (!element) return;
  element.innerText = reprError(error);
  element.style.display = "block";
}

function waitForFirstInteraction(runtime) {
  if (runtime.audio.autoplayPolicy === "allowed") {
    //console.log(`Skipping waitForFirstInteraction due to audio policy 'allowed'`);
    return;
  }
  const element = document.querySelector(".require-click");
  if (!element) {
    //console.log(`Skipping waitForFirstInteraction due to no 'require-click' element`);
    return;
  }
  element.style.display = "block";
  return new Promise((resolve, reject) => {
    element.addEventListener("click", () => {
      element.style.display = "none";
      resolve();
    }, { once: true });
  });
}

function registerResizeHandler() {
  const canvas = document.getElementById("egg-canvas");
  if (!canvas) return;
  const observer = new ResizeObserver(() => {
    const bodyBounds = document.body.getBoundingClientRect();
    const xscale = Math.max(1, Math.floor(bodyBounds.width / canvas.width));
    const yscale = Math.max(1, Math.floor(bodyBounds.height / canvas.height));
    const scale = Math.min(xscale, yscale);
    canvas.style.width = (canvas.width * scale) + "px";
    // Canvas height is always 100vh, and it has object-fit:contain, so the rest of aspect correction is all taken care of.
  });
  observer.observe(document.body);
}

function launchEgg(serial) {
  return Promise.resolve().then(() => {
    const canvas = document.getElementById("egg-canvas");
    if (!canvas) throw new Error("Canvas not found.");
    const rom = new Rom(serial);
    runtime = new Runtime(rom, canvas);
    //console.log(`launchEgg`, { rom, serial, runtime, canvas });
    return runtime.load();
  }).then(() => {
    window.eggRuntime = runtime;
    registerResizeHandler();
    return waitForFirstInteraction(runtime);
  }).then(() => {
    runtime.start();
  });
}

function terminateEgg() {
  if (!runtime) return;
  runtime.stop();
  runtime = null;
  window.eggRuntime = null;
}

/* Main bootstrap.
 * Acquire the encoded ROM (possibly base64-encoded too), and give it to launchEgg.
 * Alternately, the host may set `window.eggIsMultiLauncher` true, and we'll do nothing but install launchEgg on window.
 */
if (window.eggIsMultiLauncher) {
  window.launchEgg = launchEgg;
  window.terminateEgg = terminateEgg;
  window.Rom = Rom;
  window.ImageDecoder = ImageDecoder;
  window.eggRuntime = null;
} else {
  window.addEventListener("load", () => {
    const romElement = document.querySelector("egg-rom");
    let launchPromise;
    if (romElement) {
      launchPromise = launchEgg(romElement.innerText);
    /*IGNORE{*/
    } else if (DEFAULT_ROM_PATH) {
      launchPromise = fetch("/api/make" + DEFAULT_ROM_PATH).then(rsp => {
        if (!rsp.ok) return rsp.text().then(body => { throw body; });
        return rsp.arrayBuffer();
      }).then(rom => {
        return launchEgg(rom);
      });
    /*}IGNORE*/
    } else {
      launchPromise = Promise.reject('ROM not found');
    }
    launchPromise.then(() => {
    }).catch(e => reportError(e));
  }, { once: true });
}
