/* EnvPointModal.js
 */
 
import { Dom } from "../Dom.js";

export class EnvPointModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.hint = "";
  }
  
  /* (name) is "lo" or "hi" for commentary.
   * (index) also just commentary.
   * (t) in absolute ms.
   * (v) in 0..0xffff.
   * (hint) is "level", "pitch", or "range".
   *
   * We resolve with one of:
   *  - null, cancelled, as usual.
   *  - {t,v}
   *  - "delete"
   */
  setup(name, index, t, v, hint) {
    this.hint = hint;
    this.element.innerHTML = "";
    const table = this.dom.spawn(this.element, "TABLE");
    
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", ["title"], `Point ${index} of ${hint}.${name}`)
    );
    
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", "Time"),
      this.dom.spawn(null, "TD",
        this.dom.spawn(null, "INPUT", { type: "number", name: "t", value: t })
      )
    );
    
    if (hint === "pitch") {
      this.dom.spawn(table, "TR",
        this.dom.spawn(null, "TD", "Bend, cents"),
        this.dom.spawn(null, "TD",
          this.dom.spawn(null, "INPUT", { type: "number", name: "v", value: v - 0x8000 })
        )
      );
    } else {
      this.dom.spawn(table, "TR",
        this.dom.spawn(null, "TD", "Value"),
        this.dom.spawn(null, "TD",
          this.dom.spawn(null, "INPUT", { type: "number", name: "v", value: v })
        )
      );
    }
    
    let delbtn;
    this.dom.spawn(this.element, "DIV", ["controls"],
      this.dom.spawn(null, "INPUT", { type: "button", value: "OK", "on-click": () => this.onSubmit() }),
      delbtn = this.dom.spawn(null, "INPUT", { type: "button", value: "Delete", "on-click": () => this.resolve("delete") })
    );
    if (!index) delbtn.disabled = true;
  }
  
  readModelFromUi() {
    const model = {
      t: +this.element.querySelector("input[name='t']")?.value || 0,
      v: +this.element.querySelector("input[name='v']")?.value || 0,
    };
    if (this.hint === "pitch") model.v += 0x8000;
    return model;
  }
  
  onSubmit() {
    this.resolve(this.readModelFromUi());
  }
}
