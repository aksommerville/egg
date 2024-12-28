/* MapStore.js
 * Instantiates and indexes all "map" resources.
 * The map editor should use us for all storage concerns, rather than encoding on its own and talking to Data.
 * Reason for this is that maps are tightly related to each other: There are doors and neighbor relationships between them.
 * So we want to keep the entire set of maps instantiated at all times to answer questions like "which maps point to this one?".
 */
 
import { Data } from "../Data.js";
import { Namespaces } from "../Namespaces.js";
import { MapRes, MapCommand } from "./MapRes.js";
import { CommandListEditor } from "./CommandListEditor.js";
import { Encoder } from "../Encoder.js";

export class MapStore {
  static getDependencies() {
    return [Data, Namespaces];
  }
  constructor(data, namespaces) {
    this.data = data;
    this.namespaces = namespaces;
    
    this.maps = [];
    this.tocListener = this.data.listenToc(e => this.onTocChange(e));
    this.neighborRegime = ""; // "", "none", "neighbors", "location". Empty string means not yet determined.
    this.planes = []; // { z, x, y, w, h, v:MapRes[] }
    
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
  
  /* Neighbors.
   * Games may track map neighbor relationships either by the `neighbors` command or the `location` command.
   * These two commands must not be mixed.
   * We store maps in a separate list `planes` where those neighbor relationships are baked in.
   *****************************************************************************/
  
  /* Call any time a 'neighbors' or 'location' command might have changed.
   * We'll rebuild the planes from scratch next time we need them.
   */
  neighborsDirty() {
    this.planes = [];
    this.neighborRegime = "";
  }
  
  // Returns path or null, and may throw.
  createNeighborMap(from, dx, dy) {
    this.requireNeighbors();
    if (this.neighborRegime === "none") {
      // This is a little awkward. The very first neighbor relationship you create for a game, you have to enter the command manually.
      throw new Error(`No neighbor regime has been established by 'neighbors' or 'location' commands.`);
    }
    for (const plane of this.planes) {
      const p = plane.v.indexOf(from);
      if (p < 0) continue;
      const fromx = plane.x + p % plane.w;
      const fromy = plane.y + Math.floor(p / plane.w);
      const dstx = fromx + dx;
      const dsty = fromy + dy;
      while (dstx < plane.x) this.growPlane(plane, -1, 0);
      while (dsty < plane.y) this.growPlane(plane, 0, -1);
      while (dstx >= plane.x + plane.w) this.growPlane(plane, 1, 0);
      while (dsty >= plane.y + plane.h) this.growPlane(plane, 0, 1);
      const dstp = (dsty - plane.y) * plane.w + (dstx - plane.x);
      if (plane.v[dstp]) throw new Error(`World position (${dstx},${dsty},${plane.z}) already occupied by ${plane.v[dstp].path}`);
      
      const to = new MapRes();
      to.w = from.w;
      to.h = from.h;
      to.v = new Uint8Array(to.w * to.h);
      to.rid = this.data.unusedRid("map");
      to.path = `map/${to.rid}`;
      
      const commandsToCopy = ["image", "song"];
      for (const cmd of from.commands) {
        if (!commandsToCopy.includes(cmd.tokens[0])) continue;
        to.commands.push(new MapCommand(cmd));
      }
      
      switch (this.neighborRegime) {

        case "neighbors": {
            const lmap = (dstx > plane.x) ? plane.v[dstp - 1] : null;
            const rmap = (dstx < plane.x + plane.w - 1) ? plane.v[dstp + 1] : null;
            const umap = (dsty > plane.y) ? plane.v[dstp - plane.w] : null;
            const dmap = (dsty < plane.y + plane.h - 1) ? plane.v[dstp + plane.w] : null;
            to.commands.push(new MapCommand(["neighbors", lmap?.rid || 0, rmap?.rid || 0, umap?.rid || 0, dmap?.rid || 0]));
            this.setNeighbor(lmap, 1, to.rid);
            this.setNeighbor(rmap, 0, to.rid);
            this.setNeighbor(umap, 3, to.rid);
            this.setNeighbor(dmap, 2, to.rid);
          } break;
          
        case "location": {
            to.commands.push(new MapCommand(`location ${dstx} ${dsty} ${plane.z.startsWith?.("orphan:") ? "" : plane.z}`));
          } break;
      }
      this.maps.push(to);
      plane.v[dstp] = to; // Doesn't actually matter; we're about to get notified of a TOC change, and rebuild the whole plane set.
      this.data.newResourceSync(to.path, to.encode());
      return to.path;
    }
    return null;
  }
  
  // Returns array of { map, dx, dy }.
  getNeighbors(from) {
    this.requireNeighbors();
    for (const plane of this.planes) {
      const p = plane.v.indexOf(from);
      if (p < 0) continue;
      const neighbors = [];
      const x = p % plane.w; // Normalized. (x - plane.x) is the logical value but we don't care about that.
      const y = Math.floor(p / plane.w);
      for (let dy=-1; dy<=1; dy++) {
        const oy = y + dy;
        if ((oy < 0) || (oy >= plane.h)) continue;
        for (let dx=-1; dx<=1; dx++) {
          if (!dx && !dy) continue;
          const ox = x + dx;
          if ((ox < 0) || (ox >= plane.w)) continue;
          const map = plane.v[oy * plane.w + ox];
          if (map) {
            neighbors.push({ map, dx, dy });
          }
        }
      }
      return neighbors;
    }
    return [];
  }
  
  /* Neighbors, private bits.
   ******************************************************************************/
  
  requireNeighbors() {
    if (this.neighborRegime) return;
    switch (this.neighborRegime = this.selectNeighborRegime()) {
      case "neighbors": this.rebuildPlanes_neighbors(); break;
      case "location": this.rebuildPlanes_location(); break;
    }
  }
  
  selectNeighborRegime() {
    let neighborsc=0, locationc=0;
    let neighbors_any="", location_any="";
    for (const map of this.maps) {
      for (const cmd of map.commands) {
        switch (cmd.tokens[0]) {
          case "neighbors": neighborsc++; neighbors_any = map.path; break;
          case "location": locationc++; location_any = map.path; break;
        }
      }
    }
    if (!neighborsc && !locationc) return "none";
    if (neighborsc > locationc) {
      if (locationc) console.warn(`Using 'neighbors' commands, but we also have ${locationc} 'location' commands, eg in ${JSON.stringify(location_any)}`);
      return "neighbors";
    } else {
      if (neighborsc) console.warn(`Using 'location' commands, but we also have ${neighborsc} 'neighbors' commands, eg in ${JSON.stringify(neighbors_any)}`);
      return "location";
    }
  }
  
  rebuildPlanes_neighbors() {
    /* Building planes from 'neighbors' commands is complex.
     * We pick any unplaced map, put in on a new plane, then recursively enter its neighbors, growing the plane as needed.
     * It is easily possible for the commands to be inconsistent. We'll issue warnings when we detect that.
     */
    const unplaced = [...this.maps];
    while (unplaced.length > 0) {
      const map = unplaced[0];
      unplaced.splice(0, 1);
      const plane = { z: this.planes.length + 1, x: 0, y: 0, w: 1, h: 1, v: [map] };
      this.planes.push(plane);
      this.fillPlane_neighbors(plane, 0, 0, unplaced);
    }
  }
  
  fillPlane_neighbors(plane, x, y, unplaced) {
    if ((x < plane.x) || (y < plane.w) || (x >= plane.x + plane.w) || (y >= plane.y + plane.h)) return;
    const map = plane.v[(y - plane.y) * plane.w + (x - plane.x)];
    if (!map) return;
    const tokens = map.getFirstCommandTokens("neighbors");
    if (!tokens.length) return;
    const lrid = +tokens[1];
    const rrid = +tokens[2];
    const urid = +tokens[3];
    const drid = +tokens[4];
    // Get all growth cases out of the way first.
    if (lrid && (x === plane.x)) this.growPlane(plane, -1, 0);
    if (urid && (y === plane.y)) this.growPlane(plane, 0, -1);
    if (rrid && (x >= plane.x + plane.w - 1)) this.growPlane(plane, 1, 0);
    if (drid && (y >= plane.y + plane.h - 1)) this.growPlane(plane, 0, 1);
    // Warn if a slot is already occupied, and recur where we place a new map.
    const neighbor = (rid, dx, dy) => {
      if (!rid) return;
      const nx = x + dx;
      const ny = y + dy;
      const p = (ny - plane.y) * plane.w + (nx - plane.x);
      let nmap = plane.v[p];
      if (nmap) {
        if (nmap.rid === rid) {
          // Already visited, no problemo. This will happen often.
        } else {
          console.warn(`World map position claimed by both map:${nmap.rid} and map:${rid}. Dropping map:${rid} from the world map.`);
        }
      } else {
        const upp = unplaced.findIndex(m => m.rid === rid);
        if (upp < 0) {
          console.warn(`map:${rid} not found, named as neighbor (${dx},${dy}) of map:${map.rid}`);
        } else {
          plane.v[p] = unplaced[upp];
          unplaced.splice(upp, 1);
          this.fillPlane_neighbors(plane, nx, ny, unplaced);
        }
      }
    };
    neighbor(lrid, -1, 0);
    neighbor(rrid, 1, 0);
    neighbor(urid, 0, -1);
    neighbor(drid, 0, 1);
  }
  
  rebuildPlanes_location() {
    /* Compared to the "neighbors" regime, "location" is a breeze.
     * Each map tells us exactly what we need to know.
     */
    for (const map of this.maps) {
      const cmd = map.getFirstCommandTokens("location");
      if (!cmd.length) { // No location. Give it a unique plane.
        const z = `orphan:${this.planes.length}`;
        const plane = { z, x: 0, y: 0, w: 1, h: 1, v: [map] };
        this.planes.push(plane);
      } else {
        const x = +cmd[1];
        const y = +cmd[2];
        const z = +cmd[3] || 0; // z is optional. If omitted, all maps are on a single plane.
        let plane = this.planes.find(p => p.z === z);
        if (plane) {
          while (x < plane.x) this.growPlane(plane, -1, 0);
          while (y < plane.y) this.growPlane(plane, 0, -1);
          while (x >= plane.x + plane.w) this.growPlane(plane, 1, 0);
          while (y >= plane.y + plane.h) this.growPlane(plane, 0, 1);
          const p = (y - plane.y) * plane.w + (x - plane.x);
          if (plane[p]) {
            console.warn(`World map position (${x},${y},${z}) claimed by both map:${map.rid} and ${plane[p].rid}. Dropping map:${map.rid} from the world map.`);
          } else {
            plane.v[p] = map;
          }
        } else {
          plane = { z, x, y, w: 1, h: 1, v: [map] };
          this.planes.push(plane);
        }
      }
    }
  }
  
  growPlane(plane, gx, gy) {
    const increment = 4;
    let nw = plane.w;
    let nh = plane.h;
    let dstx = 0;
    let dsty = 0;
    if (gx < 0) { nw += increment; dstx = increment; plane.x -= increment; }
    else if (gx > 0) nw += increment;
    if (gy < 0) { nh += increment; dsty = increment; plane.y -= increment; }
    else if (gy > 0) nh += increment;
    const nv = [];
    for (let i=nw*nh; i-->0; ) nv.push(null);
    this.copyPlane(nv, dstx, dsty, nw, plane.v, 0, 0, plane.w, plane.w, plane.h);
    plane.v = nv;
    plane.w = nw;
    plane.h = nh;
  }
  
  copyPlane(dst, dstx, dsty, dststride, src, srcx, srcy, srcstride, w, h) {
    for (let dstrowp=dsty*dststride+dstx, srcrowp=srcy*srcstride+srcx, yi=h; yi-->0; dstrowp+=dststride, srcrowp+=srcstride) {
      for (let dstp=dstrowp, srcp=srcrowp, xi=w; xi-->0; dstp++, srcp++) {
        dst[dstp] = src[srcp];
      }
    }
  }
  
  // Having just added a map, if this neighbor exists, update its "neighbors" command.
  setNeighbor(map, np, nrid) {
    if (!map) return;
    let cmd = map.getFirstCommandTokens("neighbors");
    if (!cmd) {
      const co = new MapCommand("neighbors 0 0 0 0");
      map.commands.push(co);
      cmd = co.tokens;
    }
    cmd[1 + np] = nrid.toString();
    this.dirty(map.path);
  }
  
  /* Annotations: Points and regions of interest for the editor, based on map commands.
   * Basically, any command with a "@X,Y" or "@X,Y,W,H" argument becomes an annotation.
   * These are MapStore's concern because they can include things from other maps (eg door exit points).
   *****************************************************************************************/
   
  generateAnnotations(map) {
    const annotations = [];
    if (map) {
      for (const command of map.commands) {
        // Take only the first point and region from each command.
        // eg "door" has two point arguments, and the second one is for the destination map.
        let gotpoint=false, gotregion=false;
        for (const arg of command.tokens) {
          const match = arg.match(/^@(\d+),(\d+)(,\d+,\d+)?$/);
          if (!match) continue;
          if (match[3]) { // Region.
            if (!gotregion) {
              gotregion = true;
              const [w, h] = match[3].split(',').slice(1).map(v => +v);
              annotations.push({
                map,
                mapCommandId: command.mapCommandId,
                opcode: command.tokens[0],
                desc: command.tokens[0],
                x: +match[1],
                y: +match[2],
                w, h,
              });
            }
          } else { // Point.
            if (!gotpoint) {
              gotpoint = true;
              annotations.push({
                map,
                mapCommandId: command.mapCommandId,
                opcode: command.tokens[0],
                desc: command.tokens[0],
                x: +match[1],
                y: +match[2],
              });
            }
          }
        }
      }
      this.appendExitDoorsIfApplicable(annotations, map);
      this.generateAnnotationThumbnails(annotations, map);
    }
    return annotations;
  }
  
  /* Add (thumbnail) and (desc) in place where we can.
   * We might arrange for it to happen async, and for now there's no notification when that completes.
   * TODO I'd like some means for the game's editor to insert overrides here and produce its own icons.
   */
  generateAnnotationThumbnails(annotations, map) {
    for (const an of annotations) {
      switch (an.opcode) {
      
        case "sprite": {
            const cmd = map.commands.find(c => c.mapCommandId === an.mapCommandId);
            if (!cmd) break;
            if (!cmd.tokens[2]) break;
            an.desc = cmd.tokens[2];
            const sres = this.data.resByString(cmd.tokens[2], "sprite");
            if (!sres) break;
            let imageName="", tileid=0, xform=0;
            CommandListEditor.forEachCommand(sres.serial, scmd => {
              if ((scmd[0] === "image") && (scmd.length >= 2)) imageName = scmd[1];
              else if ((scmd[0] === "tile") && (scmd.length >= 3)) {
                tileid = +scmd[1];
                xform = +scmd[2];
              }
            });
            if (!imageName) break;
            this.data.getImageAsync(imageName).then(image => {
              if (!image) return;
              const ts = image.naturalWidth >> 4;
              an.thumbnail = document.createElement("CANVAS");
              an.thumbnail.width = ts;
              an.thumbnail.height = ts;
              const ctx = an.thumbnail.getContext("2d");
              ctx.fillStyle = "#0f0";
              ctx.fillRect(0, 0, ts, ts); // In case the tile image is sparse.
              ctx.drawImage(image, (tileid & 15) * ts, (tileid >> 4) * ts, ts, ts, 0, 0, ts, ts);
            }).catch(e => console.error(e));
          } break;
        
        case "door": {
            const cmd = map.commands.find(c => c.mapCommandId === an.mapCommandId);
            if (!cmd) break;
            if (!cmd.tokens[2]) break;
            an.desc = `to ${cmd.tokens[2]}`;
          } break;
          
        case "door:exit": {
            an.desc = `from ${an.map.path}`;
          } break;
      }
    }
  }
  
  /* Client app must define a "door" command for maps.
   * If it exists, the first three arguments must be: "@SRCX,SRCY" "DSTMAP" "@DSTX,DSTY"
   */
  appendExitDoorsIfApplicable(annotations, map) {
    if (!this.namespaces.nameExists("CMD", "map", "door")) return;
    for (const remote of this.maps) {
      if (remote.path === map.path) continue;
      for (const command of remote.commands) {
        if (command.tokens[0] !== "door") continue;
        const dstMapRes = this.data.resByString(command.tokens[2], "map");
        if (dstMapRes?.rid !== map.rid) continue;
        try {
          const [_, x, y] = command.tokens[3]?.match(/^@(\d+),(\d+)$/);
          annotations.push({
            map: remote,
            mapCommandId: command.mapCommandId,
            opcode: "door:exit",
            x: +x,
            y: +y,
          });
        } catch (e) {}
      }
    }
  }
  
  /* Begin private.
   ****************************************************************************************/
  
  /* Drop the entire state and rebuild synchronously from Data.
   * This happens at construction, and also on relevant TOC changes.
   */
  refresh() {
    this.neighborsDirty();
    this.maps = [];
    for (const res of this.data.resv) {
      if (res.type !== "map") continue;
      if (!res.serial?.length) res.serial = this.generateDefaultMap();
      const map = new MapRes(res);
      this.maps.push(map);
    }
  }
  
  /* Empty is legal but weird.
   * When we find something empty, generate a sensible default instead.
   * Returns Uint8Array.
   */
  generateDefaultMap() {
    let mapw = this.namespaces.idFromName("NS", "sys", "mapw");
    let maph = this.namespaces.idFromName("NS", "sys", "maph");
    if ((mapw < 1) || (maph < 1)) {
      mapw = 20;
      maph = 15;
    }
    const dst = new Encoder();
    for (let yi=maph; yi-->0; ) {
      for (let xi=mapw*2; xi-->0; ) {
        dst.u8(0x30);
      }
      dst.u8(0x0a);
    }
    dst.u8(0x0a);
    if (this.namespaces.nameExists("CMD", "map", "image")) {
      const anyTilesheet = this.data.resv.find(r => r.type === "tilesheet");
      if (anyTilesheet) {
        dst.raw(`image image:${anyTilesheet.rid}\n`);
      }
    }
    return dst.finish();
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
      //console.log(`*** refreshing MapStore cache due to Data TOC change ***`);
      this.refresh();
    }
  }
}

MapStore.singleton = true;
