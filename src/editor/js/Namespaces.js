/* Namespaces.js
 * Calls GET /api/symbols and caches the response.
 * We call out for it immediately upon construction.
 * We have a member (ns) that is always available, and exactly the /api/symbols response:
 *   {
 *     name: string;
 *     mode: "CMD" | "NS";
 *     sym: {
 *       name: string;
 *       id: number;
 *       args?: string;
 *     }[];
 *   }[];
 * And also a Promise (whenLoaded) that resolves to the same.
 */
 
import { Comm } from "./Comm.js";

export class Namespaces {
  static getDependencies() {
    return [Comm];
  }
  constructor(comm) {
    this.comm = comm;
    this.ns = [];
    this.status = 0; // -1,0,1
    this.whenLoaded = this.comm.httpJson("GET", "/api/symbols")
      .catch(e => {
        console.error(`Failed to load symbols`, e);
        this.status = -1;
        return [];
      }).then(rsp => {
        this.ns = rsp?.ns || [];
        if (!this.status) this.status = 1;
        return rsp;
      });
  }
  
  // Empty if undefined.
  getSymbols(mode, nsname) {
    const ns = this.ns.find(n => ((!mode || (mode === n.mode)) && (nsname === n.name)));
    if (!ns) return [];
    return ns.sym || [];
  }
  
  // Undefined if undefined.
  entryByName(mode, nsname, name) {
    return this.getSymbols(mode, nsname).find(s => s.name === name);
  }
  
  // Undefined if undefined.
  entryById(mode, nsname, id) {
    return this.getSymbols(mode, nsname).find(s => s.id === id);
  }
  
  // "" if undefined.
  nameFromId(mode, nsname, id) {
    return this.entryById(mode, nsname, id)?.name || "";
  }
  
  // 0 if undefined.
  idFromName(mode, nsname, name) {
    return this.entryByName(mode, nsname, name)?.id || 0;
  }
}

Namespaces.singleton = true;
