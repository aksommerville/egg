/* MapStore.js
 * Instantiates and indexes all "map" resources.
 * The map editor should use us for all storage concerns, rather than encoding on its own and talking to Data.
 * Reason for this is that maps are tightly related to each other: There are doors and neighbor relationships between them.
 * So we want to keep the entire set of maps instantiated at all times to answer questions like "which maps point to this one?".
 */
 
import { Data } from "../Data.js";
import { MapRes } from "./MapRes.js";

export class MapStore {
  static getDependencies() {
    return [Data];
  }
  constructor(data) {
    this.data = data;
    
    this.maps = [];
    this.tocListener = this.data.listenToc(e => this.onTocChange(e));
    
    this.refresh();
  }
  
  getMapByPath(path) {
    return this.maps.find(m => m.path === path);
  }
  
  getMapByRid(rid) {
    return this.maps.find(m => m.rid === rid);
  }
  
  getMapByString(src) {
    const res = this.data.resByString(src, "map");
    if (!res) return null;
    return this.maps.find(m => m.path === res.path);
  }
  
  dirty(path) {
    // Allow an exception at encode time if we don't find the map or encode fails. Data knows what to do with it.
    this.data.dirty(path, () => this.maps.find(m => m.path === path).encode());
  }
  
  /* Begin private.
   ****************************************************************************************/
  
  /* Drop the entire state and rebuild synchronously from Data.
   * This happens at construction, and shouldn't be necessary any time else.
   */
  refresh() {
    this.maps = [];
    for (const res of this.data.resv) {
      if (res.type !== "map") continue;
      this.maps.push(new MapRes(res));
    }
  }
  
  myTocDisagreesWithDatas() {
    const pathsFromData = this.data.resv.filter(r => r.type === "map").map(r => r.path);
    for (const map of this.maps) {
      const p = pathsFromData.indexOf(map.path);
      if (p < 0) return true; // We have something Data doesn't. User deleted a map.
      pathsFromData.splice(p, 1);
    }
    if (pathsFromData.length) return true; // Data has something we don't. User created a map.
    // We're not privy to content changes that happen elsewhere. I expect that will be rare, and not going to bother about it.
    return false;
  }
  
  onTocChange(event) {
    if (this.myTocDisagreesWithDatas()) {
      console.log(`*** refreshing MapStore cache due to Data TOC change ***`);
      this.refresh();
    }
  }
}

MapStore.singleton = true;
