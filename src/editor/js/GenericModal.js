/* GenericModal.js
 */
 
import { Dom } from "./Dom.js";

export class GenericModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
  }
  
  setupPickOne(options, prompt) {
    this.element.innerText = "";
    if (prompt) {
      this.dom.spawn(this.element, "DIV", prompt);
    }
    for (const option of options) {
      this.dom.spawn(this.element, "INPUT", { type: "button", value: option, "on-click": () => this.resolve(option) });
    }
  }
  
  setupError(error) {
    this.element.innerHTML = "";
    if (!error) this.element.innerText = "Unspecified error";
    else if (error.stack) this.element.innerText = error.stack;
    else if (error.message) this.element.innerText = error.message;
    else this.element.innerText = error;
    this.element.classList.add("error");
  }
  
  setupIframe(url) {
    this.element.innerHTML = "";
    const iframe = this.dom.spawn(this.element, "IFRAME", { src: url });
  }
  
  setupMessage(text) {
    this.element.innerHTML = "";
    this.element.innerText = text;
  }
  
  setupInput(prompt, preset) {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", ["prompt"], prompt);
    const form = this.dom.spawn(this.element, "FORM", { action: "POST", href: "javascript:0", "on-submit": event => {
      event.preventDefault();
      // This would be the right place to handle it, but somehow we get removed from the DOM before the event reaches us?
      // Whatever. Handling on click of submit also works.
    }});
    const input = this.dom.spawn(form, "INPUT", { type: "text", value: preset || "" });
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": (event) => {
      this.resolve(this.element.querySelector("input").value);
      event.stopPropagation();
      event.preventDefault();
    }});
    input.focus();
  }
}
