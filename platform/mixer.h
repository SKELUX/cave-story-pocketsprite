#ifndef CS_MIXER_H
#define CS_MIXER_H
#include <stdint.h>
#include <stddef.h>
#define CS_AUDIO_RATE 16000
#define CS_PCM_RATE 14000
/* Access is serialized by the native audio adapter. */
#define CS_VOICES 5
typedef struct {
    const int8_t *samples;
    uint32_t length, position, fraction, rate;
    uint8_t priority, loop, packed;
} cs_voice;
typedef struct { cs_voice voice[CS_VOICES]; } cs_mixer;
/* Packed assets use little-endian offsets into 128-sample blocks. Each
 * block stores its signed minimum and exact bit width, then LSB-first values.
 * No heap, sample approximation or decode buffer is needed. */
static inline int8_t cs_pcm_read(const int8_t *samples,uint32_t position,uint8_t packed) {
    if(!packed) return samples[position];
    const uint8_t *data=(const uint8_t *)samples;
    unsigned table=(position>>7)*2;
    unsigned offset=data[table]|((unsigned)data[table+1]<<8);
    const uint8_t *block=data+offset;
    unsigned bits=block[1],bit=(position&127)*bits;
    if(!bits) return (int8_t)block[0];
    unsigned value=block[2+(bit>>3)];
    if((bit&7)+bits>8) value|=(unsigned)block[3+(bit>>3)]<<8;
    return (int8_t)block[0]+((value>>(bit&7))&((1u<<bits)-1));
}
void cs_mixer_init(cs_mixer *m);
void cs_mixer_play(cs_mixer *m, unsigned channel, uint8_t priority,
                   const int8_t *samples, uint32_t length, uint8_t packed);
void cs_mixer_loop(cs_mixer *m, unsigned channel, const int8_t *samples,
                   uint32_t length, uint32_t rate, uint8_t packed);
/* Optional mono music is signed 16-bit. Output is the HAL's unsigned 8-bit PCM.
 * Music uses unity gain; SFX use 1.25x gain, with a soft knee for peaks. */
void cs_mixer_render(cs_mixer *m, const int16_t *music, uint8_t *out, size_t count);
#endif
