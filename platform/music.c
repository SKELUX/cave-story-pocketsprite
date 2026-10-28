#include "music.h"
#include <opus.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define FRAME 320
struct cs_music {
    OpusDecoder *decoder;
    void *loop_state;
    size_t state_size;
    const uint8_t *data;
    cs_music_reader reader;
    void *context;
    uint32_t packets,packet_bytes,total,loop,skip,raw_pos,next_packet,cached;
    uint32_t dictionary_offset,unique_packets,index_block;
    uint8_t packet_indices[256];
    int loop_ready,failed;
    uint32_t fade_left,fade_total,fade_gain,fade_step,fade_remainder,fade_error;
    int16_t pcm[FRAME];
};
static uint32_t le32(const uint8_t *p) {
    return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
cs_music *cs_music_create(void) {
    cs_music *m=calloc(1,sizeof(*m));
    if(!m) return NULL;
    m->state_size=opus_decoder_get_size(1);
    m->decoder=malloc(m->state_size);m->loop_state=malloc(m->state_size);
    if(!m->decoder || !m->loop_state) {cs_music_destroy(m);return NULL;}
    return m;
}
void cs_music_destroy(cs_music *m) {
    if(m) {free(m->decoder);free(m->loop_state);free(m);}
}
void cs_music_stop(cs_music *m) {m->reader=NULL;m->failed=0;m->fade_left=0;}
void cs_music_fade(cs_music *m,uint32_t samples) {
    if(!samples) {cs_music_stop(m);return;}
    if(!m->reader || m->fade_left) return;
    if(samples>16000*60) samples=16000*60;
    m->fade_left=m->fade_total=samples;m->fade_gain=65536;
    m->fade_step=65536/samples;m->fade_remainder=65536%samples;m->fade_error=0;
}
static int memory_read(void *context,size_t offset,void *out,size_t length) {
    cs_music *m=context;memcpy(out,m->data+offset,length);return 1;
}
int cs_music_start(cs_music *m,const void *data,size_t length) {
    m->data=data;
    return cs_music_start_reader(m,data?memory_read:NULL,m,length);
}
int cs_music_start_reader(cs_music *m,cs_music_reader reader,void *context,size_t length) {
    cs_music_stop(m);
    uint8_t p[32];
    if(!reader || length<32 || !reader(context,0,p,32) || (memcmp(p,"CSM1",4) && memcmp(p,"CSM2",4)) || le32(p+4)!=16000 || le32(p+8)!=FRAME) return 0;
    uint32_t packets=le32(p+12),bytes=le32(p+16),total=le32(p+20),loop=le32(p+24),skip=le32(p+28);
    if(!packets || !bytes || bytes>1275 || !total || skip>=FRAME ||
       total>UINT32_MAX-skip || packets!=(total+skip-1)/FRAME+1 ||
       (loop!=UINT32_MAX && loop>=total)) return 0;
    m->dictionary_offset=0;m->unique_packets=0;m->index_block=UINT32_MAX;
    if(p[3]=='2') {
        if(packets>(length-32)/2) return 0;
        size_t dictionary=32+(size_t)packets*2;
        size_t unique=(length-dictionary)/bytes;
        if(!unique || unique>packets || unique>65536 || (length-dictionary)%bytes) return 0;
        m->dictionary_offset=dictionary;m->unique_packets=unique;
    } else if(packets>(length-32)/bytes || length-32!=(size_t)packets*bytes) return 0;
    if(opus_decoder_init(m->decoder,16000,1)!=OPUS_OK) return 0;
    m->reader=reader;m->context=context;m->packets=packets;m->packet_bytes=bytes;
    m->total=total;m->loop=loop;m->skip=skip;m->raw_pos=skip;
    m->next_packet=0;m->cached=UINT32_MAX;m->loop_ready=0;
    return 1;
}
int cs_music_read(cs_music *m,int16_t *out,size_t count) {
    memset(out,0,count*sizeof(*out));
    if(m->failed) return 0;
    if(!m->reader) return 1;
    for(size_t i=0;i<count;) {
        if(m->raw_pos==m->skip+m->total) {
            if(m->loop==UINT32_MAX) {m->reader=NULL;break;}
            if(!m->loop_ready) {m->failed=1;return 0;}
            memcpy(m->decoder,m->loop_state,m->state_size);
            m->raw_pos=m->skip+m->loop;
            m->next_packet=m->raw_pos/FRAME;m->cached=UINT32_MAX;
        }
        uint32_t target=m->raw_pos/FRAME;
        if(m->cached!=target) {
            while(m->next_packet<=target) {
                if(m->next_packet>=m->packets) {m->failed=1;return 0;}
                if(m->loop!=UINT32_MAX && m->next_packet==(m->skip+m->loop)/FRAME) {
                    memcpy(m->loop_state,m->decoder,m->state_size);m->loop_ready=1;
                }
                size_t packet_offset=32+(size_t)m->next_packet*m->packet_bytes;
                if(m->dictionary_offset) {
                    uint32_t block=m->next_packet/128;
                    if(m->index_block!=block) {
                        uint32_t n=m->packets-block*128;if(n>128) n=128;
                        if(!m->reader(m->context,32+(size_t)block*256,m->packet_indices,n*2)) {
                            m->failed=1;return 0;
                        }
                        m->index_block=block;
                    }
                    unsigned at=(m->next_packet%128)*2;
                    unsigned index=m->packet_indices[at]|((unsigned)m->packet_indices[at+1]<<8);
                    if(index>=m->unique_packets) {m->failed=1;return 0;}
                    packet_offset=m->dictionary_offset+(size_t)index*m->packet_bytes;
                }
                uint8_t packet[1275];
                if(!m->reader(m->context,packet_offset,packet,m->packet_bytes)) {
                    m->failed=1;return 0;
                }
                int decoded=opus_decode(m->decoder,packet,m->packet_bytes,m->pcm,FRAME,0);
                if(decoded!=FRAME) {m->failed=1;return 0;}
                m->cached=m->next_packet++;
            }
        }
        uint32_t offset=m->raw_pos%FRAME;
        size_t n=FRAME-offset;
        if(n>count-i) n=count-i;
        if(n>m->skip+m->total-m->raw_pos) n=m->skip+m->total-m->raw_pos;
        if(m->fade_left) {
            if(n>m->fade_left) n=m->fade_left;
            for(size_t j=0;j<n;j++) {
                out[i+j]=(int32_t)m->pcm[offset+j]*(int32_t)m->fade_gain/65536;
                m->fade_gain-=m->fade_step;
                m->fade_error+=m->fade_remainder;
                if(m->fade_error>=m->fade_total) {m->fade_error-=m->fade_total;m->fade_gain--;}
            }
            m->fade_left-=n;
            if(!m->fade_left) {cs_music_stop(m);break;}
        } else memcpy(out+i,m->pcm+offset,n*sizeof(*out));
        m->raw_pos+=n;i+=n;
    }
    return 1;
}
