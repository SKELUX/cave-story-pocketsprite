#include "mixer.h"
#include <string.h>
void cs_mixer_init(cs_mixer *m) { memset(m,0,sizeof(*m)); }
void cs_mixer_play(cs_mixer *m,unsigned channel,uint8_t priority,
                   const int8_t *samples,uint32_t length,uint8_t packed) {
    if(channel>=CS_VOICES) return;
    cs_voice *v=&m->voice[channel];
    if(samples && length && v->samples && priority<v->priority) return;
    *v=(cs_voice){.samples=length?samples:NULL,.length=length,.priority=priority,.rate=CS_PCM_RATE,.packed=packed};
}
void cs_mixer_loop(cs_mixer *m,unsigned channel,const int8_t *samples,
                   uint32_t length,uint32_t rate,uint8_t packed) {
    if(channel>=CS_VOICES) return;
    m->voice[channel]=(cs_voice){.samples=length && rate?samples:NULL,
        .length=length,.rate=rate,.loop=1,.packed=packed};
}
void cs_mixer_render(cs_mixer *m,const int16_t *music,uint8_t *out,size_t count) {
    for(size_t i=0;i<count;i++) {
        int32_t sum=music?music[i]:0;
        for(unsigned c=0;c<CS_VOICES;c++) {
            cs_voice *v=&m->voice[c];
            if(!v->samples) continue;
            int32_t a=cs_pcm_read(v->samples,v->position,v->packed);
            int32_t b=v->position+1<v->length?cs_pcm_read(v->samples,v->position+1,v->packed):(v->loop?cs_pcm_read(v->samples,0,v->packed):0);
            sum+=(a+(b-a)*(int32_t)v->fraction/CS_AUDIO_RATE)*320;
            v->fraction+=v->rate;
            v->position+=v->fraction/CS_AUDIO_RATE;
            v->fraction%=CS_AUDIO_RATE;
            if(v->position>=v->length) {
                if(v->loop) v->position%=v->length;
                else v->samples=NULL;
            }
        }
        /* Music is unity gain (4x the previous output); effects are 1.25x
         * (+1.94 dB relative to music). A static soft knee preserves quiet
         * detail and rounds loud overlaps without time-varying pumping. */
        int32_t magnitude=sum<0?-sum:sum;
        if(magnitude>24576) {
            int32_t excess=magnitude-24576;
            magnitude=24576+excess*8191/(excess+8191);
            sum=sum<0?-magnitude:magnitude;
        }
        sum=sum/256+128;
        if(sum<0) sum=0;
        if(sum>255) sum=255;
        out[i]=(uint8_t)sum;
    }
}
