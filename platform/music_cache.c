#include "music_cache.h"
#include <string.h>
void cs_music_cache_init(cs_music_cache *cache,cs_music_reader reader,void *context,size_t size) {
    cache->reader=reader;cache->context=context;cache->size=size;
    cache->base=cache->valid=0;
}
int cs_music_cache_read(cs_music_cache *cache,size_t offset,void *out,size_t length) {
    if(offset>cache->size || length>cache->size-offset) return 0;
    uint8_t *dest=out;
    while(length) {
        if(!cache->valid || offset<cache->base || offset-cache->base>=cache->valid) {
            /* Aligned 4 KiB reads never straddle AppFS's 64 KiB pages. */
            cache->base=offset-offset%CS_MUSIC_CACHE_BYTES;
            cache->valid=0;
            size_t n=cache->size-cache->base;
            if(n>CS_MUSIC_CACHE_BYTES) n=CS_MUSIC_CACHE_BYTES;
            if(!cache->reader(cache->context,cache->base,cache->bytes,n)) return 0;
            cache->valid=n;
        }
        size_t within=offset-cache->base,n=cache->valid-within;
        if(n>length) n=length;
        memcpy(dest,cache->bytes+within,n);
        offset+=n;dest+=n;length-=n;
    }
    return 1;
}
