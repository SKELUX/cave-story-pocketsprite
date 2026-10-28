/* Small-screen follow camera, informed by CaveThtory's direction/aim/fall
 * look-ahead. Independent native C implementation; sprites remain 1:1 pixels. */
#include "viewport.h"
static int32_t approach(int32_t current,int32_t goal,int32_t step) {
    if(goal-current>step) return current+step;
    if(current-goal>step) return current-step;
    return goal;
}
static int32_t bound(int32_t pos,int dimension,int screen) {
    if(dimension<screen) return (dimension-screen)*256;
    int32_t end=(dimension-screen)*512;
    return pos<0?0:pos>end?end:pos;
}
void cs_viewport_update(CsViewport *v,const CsViewTarget *t) {
    int32_t lx=0,ly=0;
    if(!t->cutscene) {
        lx=(t->right?30:-30)*512;
        if(t->up || (!t->grounded && t->vy<-512)) ly=-16*512;
        else if(t->down) ly=16*512;
        else if(!t->grounded && t->vy>0) {
            ly=t->vy*4;if(ly>20*512) ly=20*512;
        }
    }
    v->look_x=approach(v->look_x,lx,2*512);
    v->look_y=approach(v->look_y,ly,2*512);
    int32_t x=t->x+v->look_x-40*512,y=t->y+v->look_y-32*512;
    if(!v->initialized || t->snap) {v->x=x;v->y=y;v->initialized=1;}
    else {
        /* Follow translation immediately; ease the look offset, not running
         * velocity. Otherwise seven frames of lag consume the forward view. */
        if(!t->cutscene && !v->was_cutscene) v->x+=t->x-v->previous_x;
        int32_t dx=x-v->x,dy=y-v->y;
        v->x+=(dx>-8 && dx<8)?dx:dx/8;
        v->y+=(dy>-8 && dy<8)?dy:dy/8;
    }
    v->previous_x=t->x;v->was_cutscene=t->cutscene;
    if(!t->cutscene) {
        /* Never let speed or camera easing push the player out of view. */
        if(t->x-v->x<8*512) v->x=t->x-8*512;
        if(t->x-v->x>72*512) v->x=t->x-72*512;
        if(t->y-v->y<12*512) v->y=t->y-12*512;
        if(t->y-v->y>52*512) v->y=t->y-52*512;
    }
    v->x=bound(v->x,t->width,80);v->y=bound(v->y,t->height,64);
}
