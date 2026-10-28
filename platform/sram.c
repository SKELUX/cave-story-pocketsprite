/* Persistent native SRAM API. Two independently committed banks retain the
 * previous complete snapshot if a write is interrupted. Save payload keeps the
 * original MD big-endian byte layout (four slots, backups, settings, counter). */
#include "md/sram.h"
#include "save_io.h"
#include <string.h>

static uint8_t memory[CS_SAVE_BYTES];
static int initialized, writable, dirty, healthy, active, storage_ready;
static uint32_t sequence;
static const uint8_t magic[4]={'C','S','S','1'};
static uint32_t be32(const uint8_t *p) {
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}
static void put32(uint8_t *p, uint32_t n) {
    p[0]=n>>24; p[1]=n>>16; p[2]=n>>8; p[3]=n;
}
static uint32_t crc_step(uint32_t crc, const uint8_t *p, size_t n) {
    while(n--) {
        crc ^= *p++;
        for(unsigned i=0;i<8;i++) crc=(crc>>1)^((0u-(crc&1))&0xedb88320u);
    }
    return crc;
}
static int valid_bank(unsigned bank, uint32_t *seq) {
    uint8_t header[20], block[256];
    size_t base=bank*CS_SAVE_BANK_BYTES;
    if(!cs_save_io_read(base,header,sizeof(header))) return -1;
    if(memcmp(header,magic,4) || be32(header+8)!=CS_SAVE_BYTES ||
       be32(header+16)!=~crc_step(~0u,header+4,12)) return 0;
    uint32_t crc=~0u;
    for(size_t off=0;off<CS_SAVE_BYTES;off+=sizeof(block)) {
        if(!cs_save_io_read(base+32+off,block,sizeof(block))) return -1;
        crc=crc_step(crc,block,sizeof(block));
    }
    if(~crc!=be32(header+12)) return 0;
    *seq=be32(header+4);
    return 1;
}
static int load(void) {
    uint32_t seq0=0,seq1=0;
    int good0=valid_bank(0,&seq0),good1=valid_bank(1,&seq1);
    if(good0<0 || good1<0) return 0;
    active=-1; sequence=0;
    if(good0) {active=0;sequence=seq0;}
    /* Serial arithmetic also handles the uint32 generation wrap. */
    if(good1 && (!good0 || (int32_t)(seq1-seq0)>0)) {active=1;sequence=seq1;}
    memset(memory,0xff,sizeof(memory));
    return active<0 || cs_save_io_read(active*CS_SAVE_BANK_BYTES+32,memory,sizeof(memory));
}
void cs_sram_init(void) {
    writable=dirty=0;
    memset(memory,0xff,sizeof(memory));
    healthy=storage_ready=cs_save_io_init() && load();
    initialized=1;
    if(!healthy) cs_save_io_error();
}
int cs_sram_ok(void) { return healthy; }
void sram_enable(void) { if(!initialized) cs_sram_init(); writable=1; }
void sram_enable_ro(void) { if(!initialized) cs_sram_init(); writable=0; }
static int commit(void) {
    /* Never erase an unverified snapshot after a failed initial read/open. */
    if(!storage_ready) return 0;
    unsigned bank=active==0 ? 1 : 0;
    size_t base=bank*CS_SAVE_BANK_BYTES;
    uint8_t header[20]={0}, check[20];
    memcpy(header,magic,4);
    put32(header+4,sequence+1);
    put32(header+8,CS_SAVE_BYTES);
    put32(header+12,~crc_step(~0u,memory,sizeof(memory)));
    put32(header+16,~crc_step(~0u,header+4,12));
    if(!cs_save_io_erase(base,CS_SAVE_BANK_BYTES) ||
       !cs_save_io_write(base+32,memory,sizeof(memory)) ||
       !cs_save_io_write(base+4,header+4,16) ||
       !cs_save_io_write(base,header,4) ||
       !cs_save_io_read(base,check,20) || memcmp(header,check,20)) return 0;
    uint32_t seq;
    if(valid_bank(bank,&seq)!=1 || seq!=sequence+1) return 0;
    active=bank; ++sequence;
    return 1;
}
void sram_disable(void) {
    if(dirty) {
        healthy=commit();
        if(!healthy) {
            /* Revert the RAM cache to durable state; never report failed flash
             * writes as successful merely because reads hit the RAM cache. */
            if(storage_ready) storage_ready=load();
            if(!storage_ready) memset(memory,0xff,sizeof(memory));
            cs_save_io_error();
        }
        dirty=0;
    }
    writable=0;
}
uint8_t sram_read_byte(uint32_t off) { return off<CS_SAVE_BYTES ? memory[off] : 0xff; }
uint16_t sram_read_word(uint32_t off) {
    if(off>CS_SAVE_BYTES-2) return 0xffff;
    return (uint16_t)memory[off]<<8 | memory[off+1];
}
uint32_t sram_read_long(uint32_t off) { return off<=CS_SAVE_BYTES-4 ? be32(memory+off) : ~0u; }
void sram_write_byte(uint32_t off,uint8_t value) {
    if(writable && off<CS_SAVE_BYTES && memory[off]!=value) {memory[off]=value;dirty=1;}
}
void sram_write_word(uint32_t off,uint16_t value) {
    if(off>CS_SAVE_BYTES-2) return;
    sram_write_byte(off,value>>8);sram_write_byte(off+1,value);
}
void sram_write_long(uint32_t off,uint32_t value) {
    if(off>CS_SAVE_BYTES-4) return;
    sram_write_word(off,value>>16);sram_write_word(off+2,value);
}
