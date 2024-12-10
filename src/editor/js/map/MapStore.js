/* MapStore.js
 * Instantiates and indexes all "map" resources.
 * The map editor should use us for all storage concerns, rather than encoding on its own and talking to Data.
 * Reason for this is that maps are tightly related to each other: There are doors and neighbor relationships between them.
 * So we want to keep the entire set of maps instantiated at all times to answer questions like "which maps point to this one?".
 */
 
import { Data } from "../Data.js";
import { Namespaces } from "../Namespaces.js";
import { MapRes } from "./MapRes.js";
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
    this.maps = [];
    for (const res of this.data.resv) {
      if (res.type !== "map") continue;
      if (!res.serial?.length) res.serial = this.generateDefaultMap();
      this.maps.push(new MapRes(res));
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
