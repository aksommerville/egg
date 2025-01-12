/* RomSet.js
 * Service owning all available ROM files.
 */
 
export class RomSet {
  static getDependencies() {
    return [Window];
  }
  constructor(window) {
    this.window = window;
    
    this.roms = {}; // [name]:Rom
    this.listeners = []; // {cb,id}
    this.nextListenerId = 1;
  }
  
  listen(cb) {
    const id = this.nextListenerId++;
    this.listeners.push({ cb, id });
    return id;
  }
  
  unlisten(id) {
    const p = this.listeners.findIndex(l => l.id === id);
    if (p < 0) return;
    this.listeners.splice(p, 1);
  }
  
  broadcast() {
    for (const { cb } of this.listeners) cb(this.roms);
  }
  
  removeByName(name) {
    if (!this.roms[name]) return;
    delete this.roms[name];
    this.broadcast();
  }
  
  add(name, serial) {
    const rom = new this.window.Rom(serial);
    this.roms[name] = rom;
    this.broadcast();
  }
}

RomSet.singleton = true;
