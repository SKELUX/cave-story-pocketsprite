/* Build-time soundtrack conversion only. libgme is NEVER linked on the device.
 * Runtime music will decode Opus PCM; no console/chip emulation on ESP32. */
#include <gme/gme.h>
#include <opus/opus.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define RATE 16000
#define FRAME 320
static void fail(const char *s) { fprintf(stderr,"%s\n",s); exit(1); }
static void check(gme_err_t error) { if(error) fail(error); }
static uint32_t le32(const uint8_t *p) {
    return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void put32(FILE *f,uint32_t x) {
    uint8_t b[4]={x,x>>8,x>>16,x>>24};
    if(fwrite(b,1,4,f)!=4) fail("Output write failed");
}
int main(int argc,char **argv) {
    if(argc!=4) fail("usage: render-music input.vgm output.csm bitrate");
    int bitrate=atoi(argv[3]);
    if(bitrate<6000 || bitrate>64000 || bitrate%400) fail("Invalid CBR bitrate");
    FILE *in=fopen(argv[1],"rb"); if(!in) fail("Cannot open VGM");
    uint8_t header[64];
    if(fread(header,1,64,in)!=64 || memcmp(header,"Vgm ",4)) fail("Invalid VGM");
    fclose(in);
    uint32_t total44=le32(header+0x18), loop44=le32(header+0x20);
    if(!total44 || loop44>total44) fail("Invalid VGM duration");
    uint32_t total=(uint64_t)total44*RATE/44100;
    uint32_t loop=loop44?(uint64_t)(total44-loop44)*RATE/44100:UINT32_MAX;
    Music_Emu *emu=NULL;
    check(gme_open_file(argv[1],&emu,RATE));
    gme_ignore_silence(emu,1);
    check(gme_start_track(emu,0));
    gme_set_fade(emu,1000000000);
    int error=0,skip=0;
    OpusEncoder *encoder=opus_encoder_create(RATE,1,OPUS_APPLICATION_RESTRICTED_LOWDELAY,&error);
    if(!encoder || error) fail("Opus init failed");
    if(opus_encoder_ctl(encoder,OPUS_SET_BITRATE(bitrate)) ||
       opus_encoder_ctl(encoder,OPUS_SET_VBR(0)) ||
       opus_encoder_ctl(encoder,OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC)) ||
       opus_encoder_ctl(encoder,OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_WIDEBAND)) ||
       opus_encoder_ctl(encoder,OPUS_SET_COMPLEXITY(10)) ||
       opus_encoder_ctl(encoder,OPUS_GET_LOOKAHEAD(&skip))) fail("Opus config failed");
    uint32_t count=(total+skip+FRAME-1)/FRAME, bytes=bitrate/400;
    FILE *out=fopen(argv[2],"wb"); if(!out) fail("Cannot open output");
    /* Header: magic, rate, frame size, count, packet bytes, duration, loop, skip. */
    if(fwrite("CSM1",1,4,out)!=4) fail("Output write failed");
    put32(out,RATE);put32(out,FRAME);put32(out,count);put32(out,bytes);
    put32(out,total);put32(out,loop);put32(out,skip);
    int16_t *rendered=calloc((size_t)count*FRAME,sizeof(*rendered));
    if(!rendered) fail("PCM allocation failed");
    int16_t stereo[FRAME*2],mono[FRAME]; uint8_t packet[1275];
    double squares=0;int peak=0;
    for(uint32_t n=0;n<count;n++) {
        check(gme_play(emu,FRAME*2,stereo));
        for(unsigned j=0;j<FRAME && n*FRAME+j<total;j++) {
            int sample=((int32_t)stereo[2*j]+stereo[2*j+1])/2;
            rendered[n*FRAME+j]=sample;
            squares+=(double)sample*sample;
            int magnitude=sample<0?-sample:sample;if(magnitude>peak) peak=magnitude;
        }
    }
    /* Balance complete tracks to -16 dBFS RMS where peak headroom permits.
     * One constant gain per track preserves dynamics and never clips the input. */
    double rms=sqrt(squares/total),gain=rms>0?5193.0/rms:1.0;
    if(peak && gain>30000.0/peak) gain=30000.0/peak;
    printf("Music level: rms=%.1f peak=%d gain=%.3f\n",rms,peak,gain);
    for(uint32_t n=0;n<count;n++) {
        for(unsigned j=0;j<FRAME;j++) mono[j]=(int16_t)lrint(rendered[n*FRAME+j]*gain);
        int length=opus_encode(encoder,mono,FRAME,packet,sizeof(packet));
        if(length!=(int)bytes) fail("Unexpected CBR packet size");
        if(fwrite(packet,1,length,out)!=(size_t)length) fail("Output write failed");
    }
    if(fclose(out)) fail("Output close failed");
    free(rendered);opus_encoder_destroy(encoder);gme_delete(emu);
    printf("%s: %u samples, loop %u, %u bytes\n",argv[1],total,loop,32+count*bytes);
}
