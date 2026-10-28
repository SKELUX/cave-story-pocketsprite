/* Shared desktop/diagnostic durable save backend. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "save_io.h"
static FILE *save;
int cs_save_io_init(void) {
    if(save) {fclose(save);save=NULL;}
    const char *name=getenv("CS_HOST_SAVE");if(!name) name="artifacts/host-diagnostic.state";
    save=fopen(name,"r+b");
    if(!save) {
        save=fopen(name,"w+b");if(!save) return 0;
        uint8_t erased[256];memset(erased,255,sizeof(erased));
        for(unsigned n=0;n<CS_SAVE_FILE_BYTES;n+=sizeof(erased))
            if(fwrite(erased,1,sizeof(erased),save)!=sizeof(erased)) return 0;
        if(fflush(save)||fsync(fileno(save))) return 0;
    }
    if(fseek(save,0,SEEK_END)) return 0;
    return ftell(save)==CS_SAVE_FILE_BYTES;
}
int cs_save_io_read(size_t off,void *data,size_t len) {
    return save && off<=CS_SAVE_FILE_BYTES && len<=CS_SAVE_FILE_BYTES-off &&
           fseek(save,off,SEEK_SET)==0 && fread(data,1,len,save)==len;
}
int cs_save_io_write(size_t off,const void *data,size_t len) {
    if(getenv("CS_HOST_FAIL_WRITES")) return 0; /* Host fault injection only. */
    return save && off<=CS_SAVE_FILE_BYTES && len<=CS_SAVE_FILE_BYTES-off &&
           fseek(save,off,SEEK_SET)==0 && fwrite(data,1,len,save)==len &&
           fflush(save)==0 && fsync(fileno(save))==0;
}
int cs_save_io_erase(size_t off,size_t len) {
    uint8_t erased[256];memset(erased,255,sizeof(erased));
    if(len%sizeof(erased)) return 0;
    while(len) {
        if(!cs_save_io_write(off,erased,sizeof(erased))) return 0;
        off+=sizeof(erased);len-=sizeof(erased);
    }
    return 1;
}
void cs_save_io_error(void) {fprintf(stderr,"Host save I/O failure\n");}
