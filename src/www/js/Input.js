/* Input.js
 */
 
export class Input {
  constructor(rt) {
    this.rt = rt;
  }
  
  /* Public entry points.
   *******************************************************************************/
  
  start() {
    this.keyListener = e => this.onKey(e);
    window.addEventListener("keydown", this.keyListener);
    window.addEventListener("keyup", this.keyListener);
  }
  
  stop() {
    if (this.keyListener) {
      window.removeEventListener("keydown", this.keyListener);
      window.removeEventListener("keyup", this.keyListener);
      this.keyListener = null;
    }
  }
  
  update() {
  }
  
  /* Keyboard.
   ***************************************************************************/
   
  onKey(event) {
    console.log(`Input.onKey`, event);
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
