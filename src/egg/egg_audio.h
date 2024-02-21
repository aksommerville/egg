/* egg_audio.h
 */
 
#ifndef EGG_AUDIO_H
#define EGG_AUDIO_H

/* Simple summary of audio capability.
 * This will not change during the program's run.
 *   IMPOSSIBLE: Platform doesn't support audio at all.
 *   DISABLED: Theoretically supported but turned off.
 *   ENABLED: Normal, running.
 *   ERROR: Tried and failed to initialize.
 * Most games won't care.
 * But if you're a rhythm game, maybe you want to check this at init and bail out if not ENABLED.
 */
int egg_audio_status();
#define EGG_AUDIO_STATUS_IMPOSSIBLE 0
#define EGG_AUDIO_STATUS_DISABLED   1
#define EGG_AUDIO_STATUS_ENABLED    2
#define EGG_AUDIO_STATUS_ERROR      3

/* Begin a song.
 * Null or empty (path) to turn off music.
 * (force) nonzero to start from the beginning even if it's already playing.
 */
void egg_audio_play_song(const char *path,int force,int repeat);

/* Play a fire-and-forget sound effect.
 * (trim) in 0..1, (pan) in -1..1.
 */
void egg_audio_play_sound(const char *path,double trim,double pan);

/* Send an event to the MIDI bus.
 * Beware that this might interfere with background music.
 * Advisable to reserve a channel for programmatic use, and it's on you to not use it in songs.
 * You can't really play music this way: Events will be processed only between driver cycles.
 * Those cycles could be coarse, like 200 ms, depends on the platform and the user.
 */
void egg_audio_event(int opcode,int chid,int a,int b);

/* Return current position in the song, in milliseconds.
 * Always zero if no song playing.
 * Returns to zero on auto-repeat.
 * Platform should make a reasonable attempt to judge how much of the last buffer has been delivered.
 * So this is hopefully fine-grained enough to build rhythm games with.
 * Mind that our song format does not record tempo information like MIDI,
 * so you're on your own figuring out where your cues are, in absolute time.
 */
int egg_audio_get_playhead();

#endif
