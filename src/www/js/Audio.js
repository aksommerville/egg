/* Audio.js
 */
 
export class Audio {
  constructor(rt) {
    this.rt = rt;
  }
  
  start() {
  }
  
  stop() {
  }
  
  egg_play_sound(rid, p) {
    console.log(`TODO Audio.egg_play_sound(${rid}, ${p})`);
  }
  
  egg_play_song(rid, force, repeat) {
    console.log(`TODO Audio.egg_play_song rid=${rid} force=${force} repeat=${repeat}`);
  }
  
  egg_audio_event(chid, opcode, a, b, durms) {
    console.log(`TODO Audio.egg_audio_event(${chid}, ${opcode}, ${a}, ${b}, ${durms})`);
  }
  
  egg_audio_get_playhead() {
    console.log(`TODO Audio.egg_audio_get_playhead`);
    return 0.0;
  }
  
  egg_audio_set_playhead(s) {
    console.log(`TODO Audio.egg_audio_set_playhead(${s})`);
  }
}
