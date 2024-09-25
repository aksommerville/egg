import { Rom } from "./js/Rom.js";
import { Runtime } from "./js/Runtime.js";

const DEFAULT_ROM_PATH = "/demo.egg";

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

function launchEgg(serial) {
  let runtime;
  return Promise.resolve().then(() => {
    const canvas = document.getElementById("egg-canvas");
    if (!canvas) throw new Error("Canvas not found.");
    const rom = new Rom(serial);
    runtime = new Runtime(rom, canvas);
    //console.log(`launchEgg`, { rom, serial, runtime });
    return runtime.load();
  }).then(() => {
    return waitForFirstInteraction(runtime);
  }).then(() => {
    runtime.start();
  });
}

/* Main bootstrap.
 * Acquire the encoded ROM (possibly base64-encoded too), and give it to launchEgg.
 */
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
