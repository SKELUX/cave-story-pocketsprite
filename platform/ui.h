#ifndef CS_UI_H
#define CS_UI_H
#include <stdint.h>
/* Native 80x64 RGB565 drawing, before HAL byte swapping. */
void cs_ui_rect(uint16_t *pixels,int x,int y,int width,int height,uint16_t color);
void cs_ui_tiny(uint16_t *pixels,int x,int y,const char *text,uint16_t color);
void cs_ui_text(uint16_t *pixels,int x,int y,const char *text,uint16_t color);
void cs_ui_frame(uint16_t *pixels,int x,int y,int width,int height,
                 uint16_t fill,uint16_t rim,uint16_t line,uint16_t shadow);
#endif
