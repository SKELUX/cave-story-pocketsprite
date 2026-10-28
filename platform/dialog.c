#include "dialog.h"
#include <string.h>
uint8_t cs_dialog_enabled=1;
void cs_dialog_layout(CsDialog *out,const uint8_t text[3][36]) {
    memset(out,0,sizeof(*out));
    for(unsigned row=0;row<3;row++) {
        unsigned start=0,end=36;
        for(unsigned i=0;i<36;i++) if(!text[row][i]) {end=i;break;}
        while(end && text[row][end-1]==' ') end--;
        while(start<end && text[row][start]==' ') start++;
        while(start<end) {
            unsigned length=end-start, take=length>19?19:length;
            if(length>19) {
                /* Keep a source row within two native rows; wrap on a word
                 * boundary whenever both halves still fit. */
                for(unsigned i=19;i>=length-19;i--) {
                    if(text[row][start+i]==' ') {take=i;break;}
                }
            }
            memcpy(out->line[out->count++],text[row]+start,take);
            start+=take;
            while(start<end && text[row][start]==' ') start++;
        }
    }
}
