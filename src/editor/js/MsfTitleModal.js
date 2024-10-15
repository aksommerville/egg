import { Dom } from "./Dom.js";

export class MsfTitleModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.id = 0;
    this.name = "";
  }
  
  setup(id, name) {
    this.id = id;
    this.name = name;
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const table = this.dom.spawn(this.element, "TABLE");
    let tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", "ID");
    this.dom.spawn(tr, "TD",
      this.dom.spawn(null, "INPUT", { name: "id", value: this.id })
    );
    tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", "Name");
    this.dom.spawn(tr, "TD",
      this.dom.spawn(null, "INPUT", { name: "name", value: this.name })
    );
    const buttonsRow = this.dom.spawn(this.element, "DIV", ["buttonsRow"]);
    this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: "Save", "on-click": () => this.onSave() });
  }
  
  onSave() {
    const id = +this.element.querySelector("input[name='id']").value;
    const name = this.element.querySelector("input[name='name']").value;
    this.resolve({ id, name });
  }
}
