#ifndef CS_MUSIC_CACHE_H
#define CS_MUSIC_CACHE_H
#include "music.h"
#define CS_MUSIC_CACHE_BYTES 4096
/* Owned by the audio mutex. Cache offsets are absolute within the data file. */
typedef struct {
    cs_music_reader reader;
    void *context;
    size_t size,base,valid;
    uint8_t bytes[CS_MUSIC_CACHE_BYTES];
} cs_music_cache;
void cs_music_cache_init(cs_music_cache *cache,cs_music_reader reader,void *context,size_t size);
int cs_music_cache_read(cs_music_cache *cache,size_t offset,void *out,size_t length);
#endif
