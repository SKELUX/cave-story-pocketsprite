#ifndef CS_VIEWPORT_H
#define CS_VIEWPORT_H
#include <stdint.h>
typedef struct {int32_t x,y,look_x,look_y,previous_x;int initialized,was_cutscene;} CsViewport;
typedef struct {
    int32_t x,y; /* World position, 1/512 pixel. */
    int16_t vx,vy;
    int width,height; /* Map size in pixels. */
    uint8_t right,up,down,grounded,cutscene,snap;
} CsViewTarget;
void cs_viewport_update(CsViewport *view,const CsViewTarget *target);
#endif
