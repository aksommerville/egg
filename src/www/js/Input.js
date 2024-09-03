/* Input.js
 */
 
export class Input {
  constructor(rt) {
    this.rt = rt;
  }
  
  /* Public entry points.
   *******************************************************************************/
  
  start() {
  }
  
  stop() {
  }
  
  update() {
  }
  
  /* Egg Platform API.
   * We don't handle egg_input_configure(); Runtime does that.
   ******************************************************************************/
  
  egg_input_get_all(dstp, dsta) {
    console.log(`TODO Input.egg_input_get_all(${dsta})`);
    return 0;
  }
  
  egg_input_get_one(playerid) {
    console.log(`TODO Input.egg_input_get_one(${playerid})`);
    return 0;
  }
}
