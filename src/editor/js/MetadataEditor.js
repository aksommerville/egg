/* MetadataEditor.js
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";

export class MetadataEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    
    this.res = null;
    this.model = null;
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
  }
  
  static checkResource(res) {
    if (res.type === "metadata") return 2;
    return 0;
  }
  
  setup(res) {
    this.res = res;
    try {
      this.model = new Metadata(res?.serial);
    } catch (e) {
      this.dom.modalError(e);
      this.model = new Metadata();
    }
    this.populateUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const table = this.dom.spawn(this.element, "TABLE", { "on-input": e => this.onInput(e) });
    const hdrRow = this.dom.spawn(table, "TR", ["header"]);
    this.dom.spawn(hdrRow, "TH");
    this.dom.spawn(hdrRow, "TH", "Key");
    this.dom.spawn(hdrRow, "TH", "String");
    this.dom.spawn(hdrRow, "TH", "Value");
    const addRow = this.dom.spawn(table, "TR", ["add"]);
    const addCol = this.dom.spawn(addRow, "TD");
    this.dom.spawn(addCol, "INPUT", { type: "button", value: "+", "on-click": () => this.onAdd() });
  }
  
  populateUi() {
    const table = this.element.querySelector("table");
    for (const row of table.querySelectorAll("tr.field")) row.remove();
    const addRow = table.querySelector("tr.add");
    if (!this.model) return;
    for (const field of this.model.fields) {
      const row = this.dom.spawn(null, "TR", ["field"], { "data-fieldId": field.fieldId });
      
      const tdctl = this.dom.spawn(row, "TD", ["ctl"]);
      this.dom.spawn(tdctl, "INPUT", { type: "button", value: "?", "on-click": () => this.onHelp(field.fieldId) });
      //TODO We should also have a button to at least view the content of strings, when in use.

      const tdk = this.dom.spawn(row, "TD", ["k"]);
      this.dom.spawn(tdk, "INPUT", { type: "text", name: "k", value: field.k });

      const tdstr = this.dom.spawn(row, "TD", ["str"]);
      this.dom.spawn(tdstr, "INPUT", { type: "number", min: -1, max: 32767, value: (field.hasOwnProperty("stringIndex") ? field.stringIndex : "") });

      const tdv = this.dom.spawn(row, "TD", ["v"]);
      const inv = this.dom.spawn(tdv, "INPUT", { type: "text", name: "v", value: field.v });
      if (this.validateField(field)) inv.classList.add("invalid");

      table.insertBefore(row, addRow);
    }
  }
  
  onHelp(fieldId) {
    if (!this.model) return;
    const field = this.model.fields.find(f => f.fieldId === fieldId);
    if (!field) return;
    const desc = this.describeKey(field.k);
    const message = this.validateField(field);
    this.dom.modalMessage(desc + "\n" + (message || "Valid."));
  }
  
  onAdd() {
    if (!this.model) return;
    this.model.fields.push({ k: "", v: "", fieldId: this.model.idNext++ });
    this.populateUi();
    const rows = Array.from(this.element.querySelectorAll("tr.field"));
    if (rows.length > 0) {
      const row = rows[rows.length - 1];
      const input = row.querySelector("input[name='k']");
      if (input) input.focus();
    }
  }
  
  onInput(event) {
    if (!this.model || !this.res) return;
    const { k, v, stringIndex, fieldId, vinput } = this.readFieldForEvent(event);
    const field = this.model.fields.find(f => f.fieldId === fieldId);
    if (!field) return;
    field.k = k;
    field.v = v;
    if (stringIndex >= 0) field.stringIndex = stringIndex;
    else delete field.stringIndex;
    if (vinput) {
      if (this.validateField(field)) vinput.classList.add("invalid");
      else vinput.classList.remove("invalid");
    }
    this.data.dirty(this.res.path, () => this.model.encode());
  }
  
  // {k,v,stringIndex,fieldId,vinput} Missing stringIndex will always be -1
  readFieldForEvent(event) {
    let element = event?.target;
    for (; element; element = element.parentNode) {
      if ((element.tagName === "TR") && element.classList.contains("field")) {
        const fieldId = +element.getAttribute("data-fieldId");
        const k = element.querySelector("input[name='k']")?.value || "";
        const vinput = element.querySelector("input[name='v']");
        const v = vinput?.value || "";
        let stringIndex = element.querySelector("input[name='str']");
        if (stringIndex) stringIndex = +stringIndex;
        else stringIndex = -1;
        if (isNaN(stringIndex)) stringIndex = -1;
        return { k, v, stringIndex, fieldId, vinput };
      }
    }
    return { k: "", v: "", stringIndex: -1, fieldId: -1 };
  }
  
  describeKey(k) {
    if (!k) {
      const keys = [
        "fb", "audioRate", "audioChanc", "title", "author", "desc", "lang", "freedom", "required", "optional",
        "players", "copyright", "advisory", "rating", "genre", "tags", "time", "version", "persistKey",
        "iconImage", "posterImage",
      ];
      for (const field of this.model.fields) {
        const p = keys.indexOf(field.k);
        if (p >= 0) keys.splice(p, 1);
      }
      if (!keys.length) return "Wow you've used all the standard keys. Nothing left for this field unless you make something up.";
      return `Missing standard keys: ${keys.join(' ')}`;
    }
    
    switch (k) {
      case "fb": return "'WIDTHxHEIGHT', dimensions of main framebuffer.";
      case "audioRate": return "Preferred audio rate in Hz. Only if you are using a custom synthesizer.";
      case "audioChanc": return "Preferred audio channel count. Only if you are using a custom synthesizer.";
      case "title": return "Full title of game for human consumption. Recommend using string too.";
      case "author": return "Your name.";
      case "desc": return "Brief description for human consumption. Recommend using string too.";
      case "lang": return "Comma-delimited ISO 639 language codes (two lowercase letters). Your preferred language first.";
      case "freedom": return "One of:\n" +
        "restricted: Even just playing the game might violate a license.\n" +
        "limited (default): Assume redistribution is not allowed.\n" +
        "intact: Redistribution is allowed freely if unmodifed.\n" +
        "free: Users may yoink assets.\n" +
        "This is a summary of your license terms. You should also include a license file when distributing.";
      case "required":
      case "optional": return "Comma-delimited list of: gl, keyboard, mouse, touch, accelerometer, gamepad, gamepad(BTNMASK), audio.";
      case "players": return "How many can play at once? 'N' or 'N..N'. Beware '2' means 'only two, not one'.";
      case "copyright": return "Short copyright notice eg '(c) YEAR MY_NAME'";
      case "advisory": return "Briefly describe any potentially objectionable content. eg 'nudity, profanity, gore'. Strings recommended.";
      case "rating": return "Formal ratings from official agencies, machine-readable. TODO Haven't settled on a format yet.";
      case "genre": return "What kind of game? Recommend using strings that have been seen before on itch.io, so we have a common reference.";
      case "tags": return "Comma-delimited list. Recommend using strings that have been seen before on itch.io, so we have a common reference.";
      case "time": return "Publication date, ISO 8601, eg '2024' or '2024-08-23'.";
      case "version": return "Version of your game. Recommend 'vN.N.N'";
      case "persistKey": return "Like version, but controls compatibility of saved games. If unset, we assume different versions are incompatible.";
      case "iconImage": return "ID of image resource. Recommend PNG 16x16 with transparency.";
      case "posterImage": return "ID of image resource. Recommend PNG with aspect 2:1 and no text.";
    }
    
    if (k.match(/^[a-z][a-zA-Z0-9_]*$/)) {
      return `Unknown key ${JSON.stringify(k)} but shaped like a standard one. If this is a custom field, please don't start with a lowercase letter.`;
    }
    return `Nonstandard key ${JSON.stringify(k)}`;
  }
  
  /* Empty string if valid.
   * IMPORTANT: Validation failures DO NOT prevent us from saving the resource.
   */
  validateField(field) {
    if (!field.k.length) return "Key can't be empty. This field will be dropped on save.";
    if (field.k.length > 255) return `Key too long (${field.k.length}, limit 255)`;
    if (field.v.length > 255) return `Value too long (${field.v.length}, limit 255)`;
    if (field.k.match(/^\s/)) return "Illegal leading whitespace in key.";
    if (field.k.match(/\s$/)) return "Illegal trailing whitespace in key.";
    if (field.v.match(/^\s/)) return "Illegal leading whitespace in value.";
    if (field.v.match(/\s$/)) return "Illegal trailing whitespace in value.";
    switch (field.k) {
      case "fb": return this.validate_fb(field.v);
      case "audioRate": return this.validate_int(field.v, 200, 200000);
      case "audioChanc": return this.validate_int(field.v, 1, 8);
      case "lang": return this.validate_lang(field.v);
      case "freedom": return this.validate_freedom(field.v);
      case "required": return this.validate_features(field.v);
      case "optional": return this.validate_features(field.v);
      case "players": return this.validate_players(field.v);
      case "rating": return this.validate_rating(field.v);
      case "time": return this.validate_8601(field.v);
      case "iconImage": return this.validate_int(field.v, 1, 65535); // Not validating that image actually exist or anything like that.
      case "posterImage": return this.validate_int(field.v, 1, 65535);
      // Freeform keys with no validation:
      case "title":
      case "author":
      case "desc":
      case "copyright":
      case "advisory":
      case "genre":
      case "tags":
      case "version":
      case "persistKey":
        return "";
    }
    if (field.k.match(/^[a-z][a-zA-Z0-9_]*$/)) {
      return "Nonstandard key must not begin with lowercase letter, or must contain punctuation.";
    }
    // If there's a stringIndex, we could check that it exist. I think that's overkill.
    return "";
  }
  
  validate_fb(src) {
    const match = src.match(/^(\d+)x(\d+)$/);
    if (!match) return "Must be 'WIDTHxHEIGHT'.";
    const w = +match[1];
    const h = +match[2];
    if ((w < 1) || (w > 4096) || (h < 1) || (h > 4096)) return "Framebuffer dimensions must be in 1..4096.";
    return "";
  }
  
  validate_int(src, lo, hi) {
    const v = +src;
    if (isNaN(v)) return `Expected integer in ${lo}..${hi}`;
    if (v < lo) return `Below minimum (${lo})`;
    if (v > hi) return `Above maximum (${hi})`;
    return "";
  }
  
  validate_lang(src) {
    const tokens = src.split(',').map(v => v.trim());
    for (const token of tokens) {
      if (!token.match(/^[a-z]{2}$/)) return `Invalid language ${JSON.stringify(token)}, must be 2 lowercase letters.`;
    }
    return "";
  }
  
  validate_freedom(src) {
    switch (src) {
      case "restricted":
      case "limited":
      case "intact":
      case "free":
        return "";
    }
    return "Expected one of: restricted, limited, intact, free";
  }
  
  validate_features(src) {
    for (const feature of src.split(',').map(v => v.trim())) {
      switch (feature) {
        case "gl":
        case "keyboard":
        case "mouse":
        case "touch":
        case "accelerometer":
        case "gamepad":
        case "audio":
          continue;
      }
      if (feature.match(/^gamepad\((0x)?[0-9a-fA-F]+\)$/)) continue;
      return `Unknown feature: ${feature}`;
    }
    return "";
  }
  
  validate_players(src) {
    if (src.startsWith("..")) src = src.substring(2);
    else if (src.endsWith("..")) src = src.substring(0, src.length - 2);
    else {
      const sepp = src.indexOf("..");
      if (sepp >= 0) {
        const lo = +src.substring(0, sepp);
        const hi = +src.substring(sepp + 2);
        if (isNaN(lo) || isNaN(hi) || (lo > hi)) return "Expected 'LO..HI', two integers.";
        return "";
      }
    }
    const n = +src;
    if (isNaN(n) || (n < 1)) return "Expected positive integer.";
    return "";
  }
  
  validate_rating(src) {
    return "";//TODO 'rating' format
  }
  
  validate_8601(src) {
    if (src.match(/^\d{4}(-\d{2}(-\d{2}(T\d{2}(:\d{2}(:\d{2}(\.\d+)?)?)?)?)?)?$/)) return "";
    return "Expected ISO 8601: 'yyyy-mm-ddThh:mm:ss.fff' (can stop before any separator, year alone is typical)";
  }
}

/* Live model, since we want to be a rich, context-sensitive editor.
 */
class Metadata {
  constructor(src) {
    this.idNext = 1;
    this.fields = []; // {k,v,stringIndex?}
    if (src) this.decode(new TextDecoder("utf8").decode(src));
  }
  
  decode(src) {
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).split("#")[0].trim();
      srcp = nlp + 1;
      if (!line) continue;
      const sepp = line.indexOf("=");
      let k, v;
      if (sepp < 0) {
        k = line;
        v = "";
      } else {
        k = line.substring(0, sepp).trim();
        v = line.substring(sepp + 1).trim();
      }
      this.addField(k, v);
    }
  }
  
  addField(k, v) {
    let stringIndex = 0, isString = false;
    if (k.endsWith("$")) {
      k = k.substring(0, k.length - 1);
      stringIndex = +v || 0;
      isString = true;
    }
    let field = this.fields.find(f => f.k === k);
    if (field) {
      if (isString) {
        field.stringIndex = stringIndex;
      } else {
        field.v = v;
      }
    } else {
      field = { k, v, fieldId: this.idNext++ };
      if (isString) {
        field.v = "";
        field.stringIndex = stringIndex;
      }
      this.fields.push(field);
    }
  }
  
  encode() {
    let dst = "";
    for (const field of this.fields) {
      if (!field.k) continue;
      dst += `${field.k}=${field.v}\n`;
      if (field.hasOwnProperty("stringIndex")) {
        const ix = +field.stringIndex;
        if (!isNaN(ix) && (ix >= 0)) {
          dst += `${field.k}\$=${ix}\n`;
        }
      }
    }
    return new TextEncoder("utf8").encode(dst);
  }
}
