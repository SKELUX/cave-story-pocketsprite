/* Native UFTC tile decoder. Source format: upstream tools/uftc and md/comp.s.
 * Tile bytes remain in packed-pixel order, independent of CPU endianness.
 * The caller supplies trusted generated assets and a valid tile range.
 */
#include "md/comp.h"

static uint16_t read_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

void uftc_unpack(const void *input, void *output, uint16_t start, uint16_t count) {
    const uint8_t *data = input;
    unsigned packed=read_be16(data)==65535;
    unsigned bits=packed?data[4]:16;
    const uint8_t *dictionary=data+(packed?5:2);
    const uint8_t *indices=dictionary+read_be16(data+(packed?2:0));
    unsigned bit=(unsigned)start*4*bits;
    if(!packed) indices+=(size_t)start*8;
    uint8_t *out = output;
    for (uint16_t tile = 0; tile < count; ++tile) {
        for (unsigned half = 0; half < 2; ++half) {
            unsigned offsets[2];
            if(packed) {
                for(unsigned i=0;i<2;i++) {
                    const uint8_t *p=indices+(bit>>3);
                    unsigned word=p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16);
                    offsets[i]=((word>>(bit&7))&((1u<<bits)-1))*2;
                    bit+=bits;
                }
            } else {
                offsets[0]=read_be16(indices);offsets[1]=read_be16(indices+2);
                indices+=4;
            }
            const uint8_t *left=dictionary+offsets[0];
            const uint8_t *right=dictionary+offsets[1];
            for (unsigned row = 0; row < 4; ++row) {
                *out++ = *left++;
                *out++ = *left++;
                *out++ = *right++;
                *out++ = *right++;
            }
        }
    }
}
