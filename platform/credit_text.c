#include "credit_text.h"
#include <string.h>
void cs_credit_text_clear(CsCreditText *v) {memset(v,0,sizeof(*v));}
void cs_credit_text_add(CsCreditText *v,const uint8_t *text,unsigned length) {
    unsigned end=0;
    while(end<length && text[end]) end++;
    while(end && text[end-1]==' ') end--;
    unsigned start=0;
    while(start<end || (!start && !end)) {
        unsigned stop=start+19;if(stop>end) stop=end;
        if(stop<end) {
            unsigned word=stop;while(word>start && text[word]!=' ') word--;
            if(word>start) stop=word;
        }
        if(v->count==6) {memmove(v->line,v->line+1,5*sizeof(v->line[0]));v->count--;}
        memset(v->line[v->count],0,20);
        memcpy(v->line[v->count++],text+start,stop-start);
        if(stop==end) break;
        start=stop;while(start<end && text[start]==' ') start++;
    }
}
