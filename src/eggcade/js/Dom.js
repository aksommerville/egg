/* Dom.js
 * Helpers for building DOM elements programmatically.
 */
 
import { Injector } from "./Injector.js";
import { GenericModal } from "./GenericModal.js";

export class Dom {
  static getDependencies() {
    return [Window, Document, Injector];
  }
  constructor(window, document, injector) {
    this.window = window;
    this.document = document;
    this.injector = injector;
    
    this.removalIgnored = [];
    this.mutationObserver = new MutationObserver(e => this.onMutation(e));
    this.mutationObserver.observe(document.body, { childList: true, subtree: true });
  }
  
  /* Create an element and append it to (parent).
   * (parent) may be null to create without adding.
   * Returns the new element.
   * (args) may contain any of:
   *   string|number|null|undefined: innerText (null and undefined become the empty string)
   *   string[]: CSS class names
   *   object: HTML attributes, or "on-EVENT":function
   *   HTMLElement: Append grandchild.
   */
  spawn(parent, tagName, ...args) {
    const element = this.document.createElement(tagName);
    for (const arg of args) {
      if (arg instanceof HTMLElement) {
        element.appendChild(arg);
      } else if (arg instanceof Array) {
        for (const clsname of arg) {
          element.classList.add(clsname);
        }
      } else if (arg && (typeof(arg) === "object")) {
        for (const key of Object.keys(arg)) {
          if (key.startsWith("on-")) element.addEventListener(key.substring(3), arg[key]);
          else element.setAttribute(key, arg[key]);
        }
      } else if (typeof(arg) === "string") {
        element.innerText = arg;
      } else if (typeof(arg) === "number") {
        element.innerText = arg.toString();
      } else if ((arg === null) || (arg === undefined)) {
        element.innerText = "";
      } else {
        throw new Error(`Unexpected argument ${JSON.stringify(arg)} to Dom.spawn`);
      }
    }
    if (parent) parent.appendChild(element);
    return element;
  }
  
  /* Instantiate a controller class, append an element for it, and return the controller.
   */
  spawnController(parent, clazz, overrides) {
    const element = this.document.createElement(this.tagNameForControllerClass(clazz));
    if (overrides) overrides = [...overrides, element];
    else overrides = [element];
    const controller = this.injector.getInstance(clazz, overrides);
    element.classList.add(clazz.name);
    element.__egg_controller = controller;
    if (parent) parent.appendChild(element);
    return controller;
  }
  
  replaceContentWithController(parent, clazz, overrides) {
    parent.innerHTML = "";
    return this.spawnController(parent, clazz, overrides);
  }
  
  /* Present a modal and return its controller instance.
   * That instance will have a Promise "result", which resolves null on cancellation.
   * The controller class may resolve it.
   */
  spawnModal(clazz) {
    const controller = this.spawnController(this.document.body, clazz);
    if (!(controller.element instanceof HTMLDialogElement)) {
      throw new Error(`Modal controller ${clazz.name} should have produced HTMLDialogElement, but found ${controller.element.constructor.name}`);
    }
    controller.result = new Promise((res, rej) => {
      controller.resolve = res;
      controller.reject = rej;
    }).then(r => { controller.element.remove(); return r; }).catch(r => { controller.element.remove(); throw r; });
    controller.element.addEventListener("mousedown", (event) => {
      const bounds = controller.element.getBoundingClientRect();
      if ((event.clientX >= bounds.x) && (event.clientY >= bounds.y) && (event.clientX < bounds.x + bounds.width) && (event.clientY < bounds.y + bounds.height)) {
        // In bounds, let the modal play with it.
      } else {
        controller.resolve(null);
      }
    });
    controller.element.addEventListener("close", (event) => {
      controller.resolve(null);
    });
    controller.element.classList.add("modal");
    controller.element.showModal();
    return controller;
  }
  
  modalPickOne(options, prompt) {
    if (options.length < 1) return Promise.reject();
    const controller = this.spawnModal(GenericModal);
    controller.setupPickOne(options, prompt);
    return controller.result;
  }
  
  modalError(error) {
    const controller = this.spawnModal(GenericModal);
    controller.setupError(error);
    return controller.result.catch(() => {});
  }
  
  modalIframe(url) {
    const controller = this.spawnModal(GenericModal);
    controller.setupIframe(url);
    return controller;
  }
  
  modalMessage(text) {
    const controller = this.spawnModal(GenericModal);
    controller.setupMessage(text);
    return controller.result.catch(() => {});
  }
  
  modalInput(prompt, preset) {
    const controller = this.spawnModal(GenericModal);
    controller.setupInput(prompt, preset);
    return controller.result;
  }
  
  // Stupid hack to prevent onRemoveFromDom firing when elements are only being shuffled in the order.
  ignoreNextRemoval(element) {
    this.removalIgnored.push(element);
  }
  
  tagNameForControllerClass(clazz) {
    if (clazz.getDependencies) {
      for (const dcls of clazz.getDependencies()) {
        if (dcls === HTMLElement) return "DIV";
        if (HTMLElement.isPrototypeOf(dcls)) return this.tagNameForElementClass(dcls);
      }
    }
    return "DIV";
  }
  
  tagNameForElementClass(clazz) {
    switch (clazz) {
      case HTMLCanvasElement: return "CANVAS";
      case HTMLInputElement: return "INPUT";
      case HTMLDialogElement: return "DIALOG";
      // There are many others, and the names aren't always obvious. Pick off as we find them.
    }
    return "DIV";
  }
  
  onMutation(events) {
    for (const event of events) {
      if (!event.removedNodes) continue;
      for (const node of event.removedNodes) {
        this.checkRemovedNodesRecursively(node);
      }
    }
  }
  
  checkRemovedNodesRecursively(parent) {
    const p = this.removalIgnored.indexOf(parent);
    if (p >= 0) {
      this.removalIgnored.splice(p, 1);
      return;
    }
    if (parent.childNodes) {
      for (const child of parent.childNodes) {
        this.checkRemovedNodesRecursively(child);
      }
    }
    if (parent.__egg_controller) {
      const controller = parent.__egg_controller;
      delete parent.__egg_controller;
      if (controller.onRemoveFromDom) controller.onRemoveFromDom();
    }
  }
}

Dom.singleton = true;
