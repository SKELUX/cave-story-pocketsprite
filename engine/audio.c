#include "common.h"
#include "gamemode.h"
#include "system.h"
#include "tables.h"
#include "md/comp.h"
#include "md/sys.h"
#include "md/vdp.h"
#include "md/xgm.h"
#include "audio.h"

// ID of the currently playing song, and backup of the previous,
// used for song_resume()
uint8_t songPlaying, songResume;
static uint8_t songFading;
// soundChannel cycles between 1-3 to allow 3 sounds to play at once
uint8_t soundChannel;

void sound_init(void) {
    songPlaying = songResume = songFading = 0;
#ifdef NSOUND
	return;
#endif
    // Here we are pointing the XGM driver to each sound effect in the game
    // and their length (in frames) indexed in sfx_info
    disable_ints();
    z80_pause();
    for(uint8_t i = 1; i < SOUND_COUNT; i++) {
        if(!sfx_info[i].sound || !sfx_info[i].end) continue;
        uint32_t len = (uintptr_t)sfx_info[i].end - (uintptr_t)sfx_info[i].sound;
        xgm_pcm_set(0x40 + i, sfx_info[i].sound, len);
    }
    z80_resume();
    enable_ints();
    soundChannel = 1;
}

void sound_play(uint8_t id, uint8_t priority) {
#ifdef NSOUND
	return;
#endif
	if(cfg_sfx_mute && gamemode != GM_SOUNDTEST) return;
	if(id >= 0x90 && id < 0xA0) id -= 0x40;
	if(id >= SOUND_COUNT || sfx_info[id].end == 0) return;
	xgm_pcm_play(0x40 + id, priority, soundChannel++);
	if(soundChannel > 3) soundChannel = 1;
}

void sound_propeller(uint8_t enabled) {
#ifndef NSOUND
    xgm_pcm_loop(0x40 + SND_PROPELLOR,0,enabled && !cfg_sfx_mute?14000:0);
#endif
}

void sound_stream(uint16_t pitch) {
#ifndef NSOUND
    /* TSC pitch is frequency/10 at the original 22050-Hz sample rate.
     * MD provides one stream sample (40); slot 41 has no asset.
     * These MD samples are already resampled to 14000 Hz. */
    uint32_t rate=cfg_sfx_mute?0:(uint32_t)pitch*14000/2205;
    xgm_pcm_loop(0x40 + SND_STREAM1,4,rate);
#endif
}

void sound_ambient_stop(void) {
    sound_propeller(0);
    sound_stream(0);
}

void song_play(uint8_t id) {
#ifdef NSOUND
	return;
#endif
    if(id>=SONG_COUNT) return;
	// Muted?
	if(cfg_music_mute && gamemode != GM_SOUNDTEST) {
		if(songPlaying || songFading) {
            xgm_music_pause();
			songPlaying = songFading = 0;
		}
		return;
	}
	if(id == songPlaying && !songFading) return;
	songResume = songPlaying;
	// Track 0 in bgm_info is NULL, but others could be potentially
	if(bgm_info[id].song == NULL) {
		id = 0;
        xgm_music_pause();
	} else {
        xgm_music_pause();
        sys_wait_vblank(); aftervsync();
        sys_wait_vblank();
		xgm_music_play(bgm_info[id].song);
	}
	songPlaying = id;
    songFading=0;
}

void song_stop(void) {
    song_play(0);
}

void song_fade(void) {
#ifndef NSOUND
    if(!songPlaying) return;
    xgm_music_fade(5000);
    songResume=songPlaying;songPlaying=0;songFading=1;
#endif
}

void song_resume(void) {
	song_play(songResume);
}

uint8_t song_get_playing(void) {
	return songPlaying;
}
