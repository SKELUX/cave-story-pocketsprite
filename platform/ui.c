#include "ui.h"
#include "font4x6.h"
void cs_ui_frame(uint16_t *p,int x,int y,int w,int h,
                 uint16_t fill,uint16_t rim,uint16_t line,uint16_t shadow) {
    cs_ui_rect(p,x,y,w,h,shadow);
    cs_ui_rect(p,x,y,w-1,h-1,rim);
    cs_ui_rect(p,x+1,y+1,w-3,h-3,line);
    cs_ui_rect(p,x+2,y+2,w-5,h-5,fill);
}
void cs_ui_rect(uint16_t *p,int x,int y,int w,int h,uint16_t color) {
    if(w<=0 || h<=0 || x>=80 || y>=64) return;
    int64_t right=(int64_t)x+w,bottom=(int64_t)y+h;
    if(right<=0 || bottom<=0) return;
    if(x<0) x=0;
    if(y<0) y=0;
    if(right>80) right=80;
    if(bottom>64) bottom=64;
    for(int yy=y;yy<bottom;yy++) for(int xx=x;xx<right;xx++) p[yy*80+xx]=color;
}
void cs_ui_text(uint16_t *p,int x,int y,const char *text,uint16_t color) {
    if(y<=-6 || y>=64) return;
    while(*text && x<80) {
        uint8_t c=(uint8_t)*text++;
        if(c<32 || c>127) c='?';
        if(x>-4) for(int yy=0;yy<6;yy++) {
            if(y+yy<0 || y+yy>=64) continue;
            for(int xx=0;xx<4;xx++)
                if(x+xx>=0 && x+xx<80 && (cs_font4x6[c-32][yy]&(1u<<xx)))
                    p[(y+yy)*80+x+xx]=color;
        }
        x+=4;
    }
}

/* Hand-drawn 3x5 HUD alphabet; four-pixel advance keeps adjacent digits clear. */
void cs_ui_tiny(uint16_t *p,int x,int y,const char *text,uint16_t color) {
    static const char alphabet[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/-:.";
    static const uint8_t rows[][5]={
        {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
        {7,4,7,1,7},{7,4,7,5,7},{7,1,2,2,2},{7,5,7,5,7},{7,5,7,1,7},
        {2,5,7,5,5},{6,5,6,5,6},{3,4,4,4,3},{6,5,5,5,6},{7,4,6,4,7},
        {7,4,6,4,4},{3,4,5,5,3},{5,5,7,5,5},{7,2,2,2,7},{1,1,1,5,2},
        {5,5,6,5,5},{4,4,4,4,7},{5,7,7,5,5},{5,7,7,7,5},{2,5,5,5,2},
        {6,5,6,4,4},{2,5,5,3,1},{6,5,6,5,5},{3,4,2,1,6},{7,2,2,2,2},
        {5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},{5,5,2,2,2},
        {7,1,2,4,7},{1,1,2,4,4},{0,0,7,0,0},{0,2,0,2,0},{0,0,0,0,2}
    };
    if(y<=-5 || y>=64) return;
    while(*text && x<80) {
        unsigned char c=*text++;
        if(c>='a' && c<='z') c-=32;
        unsigned i=0;while(alphabet[i] && alphabet[i]!=c) i++;
        if(alphabet[i] && x>-3) for(int yy=0;yy<5;yy++) for(int xx=0;xx<3;xx++)
            if(y+yy>=0 && y+yy<64 && x+xx>=0 && x+xx<80 && (rows[i][yy]&(4>>xx)))
                p[(y+yy)*80+x+xx]=color;
        x+=4;
    }
}
