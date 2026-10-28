/* Native PCM/Opus output. The legacy xgm names are API adapters only. */
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "8bkc-hal.h"
#include "md/sys.h"
#include "md/xgm.h"
#include "assets.h"
#include "mixer.h"
#include "music.h"
#include "music_cache.h"
#include "appfs.h"
#include "esp_timer.h"
#include "esp_system.h"
static cs_mixer mixer;
static cs_music *music;
static appfs_handle_t music_file=APPFS_INVALID_FD;
static uint32_t music_base,music_length;
static int music_file_size;
static cs_music_cache music_cache;
static uint32_t le32(const uint8_t *p) {
    return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static int music_file_read(void *context,size_t offset,void *out,size_t length) {
    (void)context;
    return appfsRead(music_file,offset,out,length)==ESP_OK;
}
static int music_read(void *context,size_t offset,void *out,size_t length) {
    (void)context;
    return offset<=music_length && length<=music_length-offset &&
        cs_music_cache_read(&music_cache,music_base+offset,out,length);
}
static struct {const int8_t *data;uint32_t size:31;uint32_t packed:1;} samples[256];
static void audio_task(void *arg) {
    (void)arg;
    int16_t decoded[320];uint8_t output[320];
    uint32_t blocks=0,worst_us=0;
    for(;;) {
        disable_ints();
        int64_t begin=esp_timer_get_time();
        if(!cs_music_read(music,decoded,320)) {
            printf("Cave Story: music decode failed\n");cs_music_stop(music);
        }
        cs_mixer_render(&mixer,decoded,output,320);
        uint32_t elapsed=esp_timer_get_time()-begin;
        if(elapsed>worst_us) worst_us=elapsed;
        enable_ints();
        kchal_sound_push(output,320);
        if(++blocks%250==0) {
            printf("CS audio block max=%uus budget=20000 heap=%u stack=%u\n",
                worst_us,esp_get_free_heap_size(),(unsigned)uxTaskGetStackHighWaterMark(NULL));
            worst_us=0;
        }
    }
}
void xgm_init(void) {
    music_file=appfsOpen("cavestory.dat");
    if(music_file==APPFS_INVALID_FD) {printf("Cave Story: cavestory.dat missing\n");abort();}
    appfsEntryInfo(music_file,NULL,&music_file_size);
    cs_music_cache_init(&music_cache,music_file_read,NULL,music_file_size);
    cs_mixer_init(&mixer);
    music=cs_music_create();
    if(!music) abort();
    kchal_sound_start(CS_AUDIO_RATE,4096);
    if(xTaskCreatePinnedToCore(audio_task,"cave-audio",32768,NULL,5,NULL,1)!=pdPASS) abort();
}
void xgm_pcm_set(uint8_t id,const uint8_t *data,uint32_t length) {
    disable_ints();
    samples[id].data=(const int8_t *)data;samples[id].size=length;samples[id].packed=0;
    for(size_t i=0;i<cs_asset_count;i++) if(cs_assets[i].data==data && cs_assets[i].expanded) {
        samples[id].size=cs_assets[i].expanded;samples[id].packed=1;break;
    }
    enable_ints();
}
void xgm_pcm_play(uint8_t id,uint8_t priority,uint16_t channel) {
    disable_ints();
    cs_mixer_play(&mixer,channel,priority,samples[id].data,samples[id].size,samples[id].packed);
    enable_ints();
}
void xgm_pcm_loop(uint8_t id,uint16_t channel,uint32_t rate) {
    disable_ints();
    cs_mixer_loop(&mixer,channel,samples[id].data,samples[id].size,rate,samples[id].packed);
    enable_ints();
}
void xgm_music_fade(uint32_t milliseconds) {
    if(milliseconds>60000) milliseconds=60000;
    disable_ints();cs_music_fade(music,milliseconds*16);enable_ints();
}
void xgm_music_pause(void) {
    disable_ints();cs_music_stop(music);enable_ints();
}
void xgm_music_play(const uint8_t *data) {
    size_t length=0;
    for(size_t i=0;i<cs_asset_count;i++) if(cs_assets[i].data==data) {length=cs_assets[i].size;break;}
    disable_ints();
    cs_music_stop(music);
    int ok=0;
    if(length==16 && data[0]=='C' && data[1]=='S' && data[2]=='E' && data[3]=='1') {
        music_base=le32(data+4);music_length=le32(data+8);
        if(music_base<=(uint32_t)music_file_size && music_length<=(uint32_t)music_file_size-music_base)
            ok=cs_music_start_reader(music,music_read,NULL,music_length);
    }
    enable_ints();
    if(!ok) {printf("Cave Story: invalid music asset\n");abort();}
}
