#ifndef CS_MUSIC_H
#define CS_MUSIC_H
#include <stddef.h>
#include <stdint.h>
typedef struct cs_music cs_music;
typedef int (*cs_music_reader)(void *context,size_t offset,void *out,size_t length);
cs_music *cs_music_create(void);
void cs_music_destroy(cs_music *music);
/* Data must remain alive until stop/replacement. Invalid streams are rejected. */
int cs_music_start(cs_music *music,const void *data,size_t length);
int cs_music_start_reader(cs_music *music,cs_music_reader reader,void *context,size_t length);
/* Fade only music over this many output samples (maximum 60 seconds).
 * Repeated fade requests do not restart an in-progress fade. */
void cs_music_fade(cs_music *music,uint32_t samples);
void cs_music_stop(cs_music *music);
/* Fill all output, silence after a non-looping end. Returns 0 on decode error. */
int cs_music_read(cs_music *music,int16_t *out,size_t count);
#endif
