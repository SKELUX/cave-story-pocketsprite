#ifndef CS_VIDEO_H
#define CS_VIDEO_H
#include <stdint.h>
/* Render a logical viewport to RGB565. Output is width*height pixels.
 * Caller chooses viewport for gameplay, menus, or diagnostic full-frame views. */
void cs_video_render(uint16_t *out, unsigned width, unsigned height,
                     int x, int y, unsigned view_width, unsigned view_height);
void cs_video_render_world(uint16_t *out,int x,int y);
void cs_video_render_world_parallax(uint16_t *out,int x,int y,int bgx,int bgy);
/* Compact interface artwork only; never used by the world renderer. */
void cs_video_ui_icon(uint16_t *out,int x,int y,uint16_t attr,
                      unsigned tiles_w,unsigned tiles_h,unsigned width,unsigned height);
void cs_video_ui_portrait(uint16_t *out,int x,int y,uint16_t attr);
void cs_video_credit_scene(uint16_t *out,int x,int y);
void cs_video_credit_image(uint16_t *out,int x,int y);
void cs_video_menu_background(uint16_t *out);
uint16_t cs_video_color(unsigned index);
#endif
