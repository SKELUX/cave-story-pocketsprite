#ifndef CS_FIRE_REPEAT_H
#define CS_FIRE_REPEAT_H
#include <stdint.h>
typedef struct {uint16_t elapsed;uint8_t held;} FireRepeat;
/* Native game runs at 50 ticks/s. Carry the half-frame remainder so quarter
 * seconds alternate 13/12 ticks rather than drifting to 260 ms per shot. */
static inline int fire_repeat_tick(FireRepeat *r,int held,int pressed,int enabled) {
    if(!enabled || !held) {r->held=0;r->elapsed=0;return 0;}
    if(pressed || !r->held) {r->held=1;r->elapsed=0;return 1;}
    r->elapsed+=20;
    if(r->elapsed>=250) {r->elapsed-=250;return 1;}
    return 0;
}
#endif
